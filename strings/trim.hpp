#pragma once

#include "codec.hpp"
#include <string_view>

namespace strings {

namespace trim_detail {
constexpr auto is_space(unsigned c) -> bool { return c == ' '; }
constexpr auto is_space_or_tab(unsigned c) -> bool { return c == '\t' || c == ' '; }
constexpr auto is_non_printable(unsigned c) -> bool { return c <= 32; }
} // namespace trim_detail

// aliases to use in trim-like functions (to make the code look nicer)
constexpr auto trim_spaces_only(unsigned c) { return trim_detail::is_space(c); }
constexpr auto trim_spaces_and_tabs(unsigned c) { return trim_detail::is_space_or_tab(c); }
constexpr auto trim_non_printables(unsigned c) { return trim_detail::is_non_printable(c); }

template <typename F>
concept trimming_predicate = requires(F&& f, unsigned c) {
                                 {
                                     static_cast<F&&>(f)(c)
                                     } -> std::same_as<bool>;
                             };

enum class trim_side {
    front_and_back,
    front,
    back,
};

// trim - produces a subview int the Intput string with content trimmed at the specified sides
[[nodiscard]] constexpr auto trim(convertible_to_string_view_input auto const& s, trimming_predicate auto tr,
    trim_side side = trim_side::front_and_back)
{
    using string_view_type = string_view_type_of<decltype(s)>;
    using ct = typename string_view_type::value_type;
    using cu = std::make_unsigned_t<ct>;
    
    auto sv = string_view_type(s);

    auto b = std::size_t{0};
    auto e = sv.size();
    if (side != trim_side::back)
        while (b != e && tr(cu(sv[b])))
            ++b;

    if (side != trim_side::front)
        while (b != e && tr(cu(sv[e - 1])))
            --e;
    return sv.substr(b, e - b);
}

} // namespace strings