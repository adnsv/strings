#pragma once

#include "codepoint.hpp"
#include "fold_simple.hpp"
#include <concepts>

namespace strings {

namespace fold {

constexpr auto ascii(codepoint cp) -> codepoint
{
    // from ascii upper 'A-Z' make ascii lower 'a-z'
    return (cp.value - unsigned('A') < 26u) ? codepoint{cp.value + unsigned('a' - 'A')} : cp;
}

constexpr auto none(codepoint cp) -> codepoint
{
    // noop
    return cp;
}

} // namespace fold

[[nodiscard]] inline auto operator>>(codepoint_source auto&& s, codepoint_folding auto f)
{
    return [s = std::forward<decltype(s)>(s), f]() mutable {
        auto c = s();
        if (c.has_value())
            c = f(*c);
        return c;
    };
}

[[nodiscard]] inline auto operator>>(string_like_input auto&& s, codepoint_folding auto f)
{
    return utf::make_decoder(s) >> f;
}

inline void operator>>(codepoint_source auto&& src, codepoint_sink auto&& dst)
{
    auto c = src();
    while (c) {
        dst(*c);
        c = src();
    }
}

template <codeunit U> auto operator>>(codepoint_source auto&& src, std::basic_string<U>&& dst) -> std::basic_string<U>
{
    src >> utf::make_encoder(dst);
    return dst;
}

} // namespace strings
