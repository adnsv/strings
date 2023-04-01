#pragma once

#include "codepoint.hpp"
#include "utf.hpp"
#include <concepts>
#include <ranges>

// see for reference: https://github.com/tcbrindle/utf_ranges

namespace strings {

template <typename Iter, typename Sentinel>
constexpr auto unknown_to_codepoint(Iter first, Sentinel last, codepoint& output, unexpected_policy utp) -> Iter
{
    if (first == last) {
        output = errcp::insufficient;
        return first;
    }
    ++first;
    output = errcp::unexpected;
    if (utp == unexpected_policy::consume_all)
        while (first != last)
            ++first;
    return first;
}

template <encoding Enc, typename Iter, typename Sentinel>
constexpr auto to_codepoint(Iter first, Sentinel last, codepoint& output, unexpected_policy utp) -> Iter
{
    constexpr auto enc = Enc;
    if constexpr (enc == encoding::utf8)
        return utf::u8_to_codepoint(first, last, output, utp);
    else if constexpr (enc == encoding::utf16)
        return utf::u16_to_codepoint(first, last, output, utp);
    else if constexpr (enc == encoding::utf32)
        return utf::u32_to_codepoint(first, last, output, utp);
    else
        return unknown_to_codepoint(first, last, output, utp);
}

template <encoding Enc, typename U> void to_codeunits(codepoint const& cp, std::invocable<U> auto put)
{
    constexpr auto enc = Enc;
    if constexpr (enc == encoding::utf8)
        utf::u8_to_codeunits<U>(cp, put);
    else if constexpr (enc == encoding::utf16)
        utf::u16_to_codeunits<U>(cp, put);
    else if constexpr (enc == encoding::utf32)
        utf::u32_to_codeunits<U>(cp, put);
    else
        put(U{});
}


template <encoding Enc, typename Iter, typename Sentinel>
requires std::sentinel_for<Sentinel, Iter> && codeunit<std::iter_value_t<Iter>>
struct decoder final {
    // static inline constexpr encoding_type = encoding_of<std::ranges::value_t<Range>>;
    using iterator_type = Iter;
    using sentinel_type = Sentinel;
    using codeunit_type = std::iter_value_t<Iter>;
    static constexpr auto enc = Enc;

    decoder(Iter first, Sentinel last, std::optional<codepoint> replacement)
        : first_{first}
        , last_{last}
        , replacement_{replacement}
    {
    }

    auto empty() const -> bool { return first_ == last_; }

    auto operator()() -> std::optional<codepoint>
    {
        if (first_ == last_) // input is empty
            return {};

        codepoint cp;
        first_ = to_codepoint<enc>(first_, last_, cp, unexpected_policy::consume_all);

        auto have_err = cp.value & errcp::error_bit.value;
        return (!have_err && unicode::is_valid(cp)) ? cp : replacement_;
    }

private:
    iterator_type first_;
    sentinel_type last_;
    std::optional<codepoint> replacement_;
};

template <encoding Enc, typename S>
requires string_like_input<S>
auto make_decoder(S&& s, std::optional<codepoint> replacement)
{
    using codeunit_type = codeunit_type_of<S>;
    constexpr auto enc = strings::utf::encoding_of<codeunit_type>;
    if constexpr (string_view_input<S>) {
        using iter_type = S::iterator;
        return decoder<enc, iter_type, iter_type>(s.begin(), s.end(), replacement);
    }
    else if constexpr (convertible_to_string_view_input<S>) {
        using sv_type = string_view_type_of<S>;
        using iter_type = sv_type::iterator;
        auto sv = sv_type(s);
        return decoder<enc, iter_type, iter_type>(sv.begin(), sv.end(), replacement);
    }
    else {
        using iter_type = std::ranges::iterator_t<S>;
        using sent_type = std::ranges::sentinel_t<S>;
        return decoder<enc, iter_type, sent_type>(std::ranges::begin(s), std::ranges::end(s), replacement);
    }
}

template <encoding Enc, typename U> auto make_encoder(std::basic_string<U>& s)
{
    return [&s](codepoint cp) { to_codeunits<Enc, U>(cp, [&s](U cu) { s += cu; }); };
}

namespace utf {

// strings::utf::make_decoder is a version of strings::make_decoder<UTF> that automatically chooses
// input encoding from utf8, utf16, or utf32 based on the input codeunit size
auto make_decoder(string_like_input auto&& s, std::optional<codepoint> replacement = unicode::replacement_character)
{
    using codeunit_type = codeunit_type_of<decltype(s)>;
    constexpr auto enc = strings::utf::encoding_of<codeunit_type>;
    return strings::make_decoder<enc>(s, replacement);
}

template <typename U> auto make_encoder(std::basic_string<U>& s) { return strings::make_encoder<encoding_of<U>>(s); }

void decode(string_like_input auto&& s, std::optional<codepoint> replacement, std::invocable<codepoint> auto put)
{
    auto dec = make_decoder(s, replacement);
    auto cp = dec();
    while (cp) {
        put(*cp);
        cp = dec();
    }
}

template <codeunit U>
auto to_string_of(string_like_input auto&& s, std::optional<codepoint> replacement = unicode::replacement_character)
    -> std::basic_string<U>
{
    auto ret = std::basic_string<U>{};
    decode(s, replacement, make_encoder(ret));
    return ret;
}

} // namespace utf
} // namespace strings
