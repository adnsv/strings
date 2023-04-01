#pragma once

#include <charconv>
#include <string>
#include <string_view>
#include <type_traits>

namespace strings {

// trim_frac_zeroes removes trainling zeroes in a fractional part of a
// stringified floating point representation, will also remove a decimal point
// (or comma) if necessary.
inline auto trim_frac_zeroes(std::string_view s) -> std::string_view
{
    auto decimal = s.rfind('.');
    if (decimal == std::string_view::npos) {
        decimal = s.rfind(",");
        if (decimal == std::string_view::npos)
            return s;
    }
    while (s.back() == '0')
        s.remove_suffix(1);
    if (s.back() == '.' || s.back() == ',')
        s.remove_suffix(1);
    return s;
}

template <typename T>
requires std::is_floating_point_v<T>
auto format_fp(T const& v, int prec, bool trim_frac_zeroes = true) -> std::string
{
    constexpr size_t bufsz = 32;
    static const auto badval = std::string{"####"};
    char buf[bufsz];
    auto [p, ec] = std::to_chars(buf, buf + bufsz, v, std::chars_format::fixed, prec);
    if (ec != std::errc{})
        return badval;
    auto s = std::string_view{buf, p};
    if (trim_frac_zeroes)
        s = strings::trim_frac_zeroes(s);
    return std::string{s};
}

} // namespace strings