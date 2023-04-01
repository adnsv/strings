#pragma once

#include "string_traits.hpp"
#include <string_view>

namespace strings {

constexpr auto is_space(unsigned c) -> bool { return c == ' '; }
constexpr auto is_space_or_tab(unsigned c) -> bool { return c == '\t' || c == ' '; }
constexpr auto is_non_printable(unsigned c) -> bool { return c <= 32; }

// aliases to use in trim-like functions (to make the code look nicer)
constexpr auto trim_spaces_only(unsigned c) { return is_space(c); }
constexpr auto trim_spaces_and_tabs(unsigned c) { return is_space_or_tab(c); }
constexpr auto trim_non_printables(unsigned c) { return is_non_printable(c); }

enum trim_side {
    trim_front_and_back,
    trim_front,
    trim_back,
};

// trim - produces a subview int the Intput string with content trimmed at the specified sides
template <typename Input, typename TrimmingPredicate>
[[nodiscard]] constexpr auto trim(
    Input const& s, TrimmingPredicate tr, trim_side side = trim_front_and_back)
{
    auto sv = as_string_view(s);
    auto b = std::size_t{0};
    auto e = sv.size();
    if (side != trim_back)
        while (b != e && tr(unsigned(s[b])))
            ++b;

    if (side != trim_front)
        while (b != e && tr(unsigned(s[e - 1])))
            --e;
    return sv.substr(b, e - b);
}

} // namespace strings