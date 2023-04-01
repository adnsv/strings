#pragma once

#include "string_traits.hpp"
#include <string>
#include <string_view>

namespace strings::utf {

enum encoding {
    utf8 = 1,
    utf16 = 2,
    utf32 = 4,
};

constexpr auto replacement_char = char32_t{0xfffd};

constexpr auto valid_codepoint(char32_t c) -> bool
{
    return c < 0xd800 || (c > 0xdfff && c < 0x110000);
}

template <typename Codeunit> constexpr auto as_codepoint(Codeunit const cu) -> char32_t
{
    return static_cast<char32_t>(static_cast<std::make_unsigned_t<Codeunit>>(cu));
}

enum class errc {
    no_error,
    insufficient_input, // more codeunits required to produce a complete
                        // codepoint
    invalid_sequence,   // unexpected starter or unexpected trail, 5-byte and
                        // 6-byte sequences
    invalid_codepoint,  // a sequence decodes into an invalid codepoint
};

template <typename Iter> struct decode_result {
    Iter it;
    errc ec;
};

template <typename Codeunit> constexpr auto encoding_of() -> encoding
{
    constexpr auto sz = codeunit_size<Codeunit>();
    static_assert(sz == 1 || sz == 2 || sz == 4);
    if constexpr (sz == 1)
        return utf8;
    else if constexpr (sz == 2)
        return utf16;
    else
        return utf32;
}

template <typename Iter>
constexpr auto decode_u8(Iter first, Iter last, char32_t& cp) -> decode_result<Iter>
{
    auto ret = decode_result<Iter>{first, errc::no_error};

    if (ret.it == last) {
        ret.ec = errc::insufficient_input;
        return ret;
    }

    auto next_starter = [&] {
        while (ret.it != last && (*ret.it & 0b11000000) == 0b10000000)
            ++ret.it;
    };

    cp = as_codepoint(*ret.it);
    ++ret.it;
    if (cp < 0b10000000) {
        // one-byte sequence [U+0000..U+0FF]
        return ret;
    }
    if (cp < 0b11000000) {
        // unexpected trail instead of a starter char
        // before returning an error, skip to a next starter, if possible
        next_starter();
        ret.ec = errc::invalid_sequence;
        return ret;
    }

    auto next = [&](char32_t& u) -> bool {
        if (ret.it == last) {
            ret.ec = errc::insufficient_input;
            return false;
        }
        u = *ret.it;
        if ((*ret.it & 0b11000000) != 0b10000000) {
            ret.ec = errc::invalid_sequence;
            return false;
        }
        ++ret.it;
        return true;
    };

    // fetch second codeunit
    auto c2 = char32_t{};
    if (!next(c2))
        return ret;

    if (cp < 0b11100000) {
        // two-byte sequence [U+0080..U+07FF]
        cp = ((cp & 0x1f) << 6) | (c2 & 0x3f);
        // other than 0xC0 and 0xC1 overlongs (which we choose to ignore),
        // this sequence should produce valid codepoints. Therefore, we
        // do not validate it before returning.
        return ret;
    }

    // fetch third byte
    auto c3 = char32_t{};
    if (!next(c3))
        return ret;

    if (cp < 0b11110000) {
        // three-byte sequence [U+0800..U+FFFF]
        cp = ((cp & 0x0f) << 12) | ((c2 & 0x3f) << 6) | (c3 & 0x3f);
    }
    else if (cp < 0b11111000) {
        // four-byte sequence [U+10000..U+10FFFF]
        auto c4 = char32_t{};
        if (!next(c4))
            return ret;
        cp = ((cp & 0x07) << 18) | ((c2 & 0x3f) << 12) | ((c3 & 0x3f) << 6) | (c4 & 0x3f);
    }
    else {
        // more-than-four-byte sequences are invalid
        next_starter();
        ret.ec = errc::invalid_sequence;
        return ret;
    }
    if (!valid_codepoint(cp))
        ret.ec = errc::invalid_codepoint;

    return ret;
}

template <typename Iter>
constexpr auto decode_u16(Iter first, Iter last, char32_t& cp) -> decode_result<Iter>
{
    auto ret = decode_result{first, errc::no_error};

    if (ret.it == last) {
        ret.ec = errc::insufficient_input;
        return ret;
    }
    cp = as_codepoint(*ret.it);
    ++ret.it;

    auto surr = [](char32_t c) { return c >= 0xd800 && c <= 0xdfff; };
    auto lo_surr = [](char32_t c) { return c >= 0xdc00 && c <= 0xdfff; };

    if (surr(cp)) {
        // surrogate pair, must start with the hi surrogate:
        if (cp >= 0xdc00) {
            ret.ec = errc::invalid_sequence;
            return ret;
        }
        if (ret.it == last) {
            ret.ec = errc::insufficient_input;
            return ret;
        }
        auto c2 = as_codepoint(*ret.it);
        if (!lo_surr(c2)) {
            ret.ec = errc::invalid_sequence;
            return ret;
        }
        ++ret.it;
        cp = (((cp & 0x3ff) << 10) | (c2 & 0x3ff)) + 0x10000;
        if (!valid_codepoint(cp))
            ret.ec = errc::invalid_codepoint;
    }

    return ret;
}

template <typename Iter>
constexpr auto decode_u32(Iter first, Iter last, char32_t& cp) -> decode_result<Iter>
{
    auto ret = decode_result<Iter>{first, errc::no_error};

    if (ret.it == last) {
        ret.ec = errc::insufficient_input;
        return ret;
    }
    cp = as_codepoint(*ret.it);
    ++ret.it;

    if (!valid_codepoint(cp))
        ret.ec = errc::invalid_codepoint;
    return ret;
}

template <encoding Enc, typename Iter>
constexpr auto decode(Iter first, Iter last, char32_t& cp) -> decode_result<Iter>
{
    static_assert(Enc == utf8 || Enc == utf16 || Enc == utf32, "invalid encoding");
    if constexpr (Enc == utf8)
        return decode_u8(first, last, cp);
    else if constexpr (Enc == utf16)
        return decode_u16(first, last, cp);
    else
        return decode_u32(first, last, cp);
}

template <typename Input, std::invocable<char32_t> Putter>
constexpr void decode(Input const& s, Putter put)
{
    using sv_type = typename string_traits<Input>::string_view_type;
    using char_type = typename sv_type::value_type;
    auto sv = as_string_view(s);

    constexpr auto enc = encoding_of<char_type>();
    auto f = sv.begin();
    auto e = sv.end();
    char32_t cp;
    while (f != e) {
        auto r = decode<enc>(f, e, cp);
        if (r.ec == errc{})
            put(cp);
        f = r.it;
    }
}

template <typename Input> inline auto decode(Input const& s) -> std::u32string
{
    auto ret = std::u32string{};
    auto sv = as_string_view(s);
    ret.reserve(sv.size());
    decode(sv, [&ret](char32_t cp) { ret += cp; });
    return ret;
}

template <typename Codeunit> struct decoder {
    using codeunit = Codeunit;
    static constexpr auto enc = encoding_of<codeunit>();

    constexpr decoder(std::basic_string_view<codeunit> sv)
        : pos{sv.data()}
        , last{sv.data() + sv.size()}
    {
    }

    constexpr auto empty() const -> bool { return pos == last; }

    constexpr auto get() -> char32_t
    {
        auto ret = char32_t{};
        auto dr = decode<enc>(pos, last, ret);
        if (dr.ec == utf::errc::no_error) {
            pos = dr.it;
        }
        else {
            ret = replacement_char;
            pos = last;
        }
        return ret;
    }

private:
    Codeunit const* pos;
    Codeunit const* last;
};

template <encoding Enc, typename Putter> constexpr void encode(char32_t cp, Putter put)
{
    if constexpr (Enc == encoding::utf8) {
        if (cp < 0x80) {
            put(cp);
        }
        else if (cp < 0x800) {
            put((cp >> 6) | 0xc0);
            put((cp & 0x3f) | 0x80);
        }
        else if (cp < 0x10000) {
            put((cp >> 12) | 0xe0);
            put(((cp >> 6) & 0x3f) | 0x80);
            put((cp & 0x3f) | 0x80);
        }
        else {
            put((cp >> 18) | 0xF0);
            put(((cp >> 12) & 0x3F) | 0x80);
            put(((cp >> 6) & 0x3F) | 0x80);
            put((cp & 0x3F) | 0x80);
        }
    }
    else if constexpr (Enc == encoding::utf16) {
        if (cp < 0x10000) {
            put(cp);
        }
        else {
            cp -= 0x10000;
            put(0xD800 | (cp >> 10));
            put(0xDC00 | (cp & 0x3FF));
        }
    }
    else {
        // assume Encoding == encoding::utf32
        put(cp);
    }
}

template <typename Input32> inline auto encode_u8(Input32 const& inp) -> std::string
{
    static_assert(std::convertible_to<Input32, std::u32string_view>);
    auto sv = std::u32string_view(inp);
    auto ret = std::string{};
    ret.reserve(sv.size());
    for (auto cp : sv)
        encode<encoding::utf8>(cp, [&ret](char cu) { ret += cu; });
    return ret;
}

} // namespace strings::utf