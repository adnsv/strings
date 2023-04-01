#pragma once

#include "fold_simple.hpp"
#include "string_traits.hpp"
#include "ascii.hpp"
#include "utf.hpp"
#include <string>

namespace strings {

enum class folding { none, ascii, simple };

template <folding Folding = folding::simple> constexpr auto fold(char32_t cp) -> char32_t
{
    if constexpr (Folding == folding::simple)
        cp = simple_fold(cp);
    else if constexpr (Folding == folding::ascii) {
        cp = ascii::lower(cp);
    }
    return cp;
}

template <typename Input, folding Folding = folding::simple>
auto fold(Input const& s) -> std::u32string
{
    auto sv = as_string_view(s);
    auto ret = std::u32string{};
    utf::decode(sv, [&ret](char32_t cp) { ret += fold<Folding>(cp); });
    return ret;
}

} // namespace strings