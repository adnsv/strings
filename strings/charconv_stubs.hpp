#pragma once

#include <charconv>
#include <type_traits>

#ifndef __cpp_lib_to_chars
#ifndef STRINGS_USE_TOCHARS_FLOAT_STUB
#define STRINGS_USE_TOCHARS_FLOAT_STUB
#endif
#endif

#if defined(STRINGS_USE_TOCHARS_FLOAT_STUB)
#include <cstdio>
#endif

namespace std {

#ifdef STRINGS_USE_TOCHARS_FLOAT_STUB
template <typename T>
requires std::is_floating_point_v<T>
inline to_chars_result to_chars(char* first, char* last, T const& value)
{
    auto const nmax = last - first;
    auto n = std::snprintf(first, nmax, "%g", value);
    if (n <= 0 || n >= nmax)
        return {last, std::errc::value_too_large};
    else
        return {first + n, std::errc{}};
}
template <typename T>
requires std::is_floating_point_v<T>
inline to_chars_result to_chars(char* first, char* last, T const& value, std::chars_format fmt)
{
    auto spec = "%g";
    switch (fmt) {
    case std::chars_format::fixed:
        spec = "%f";
        break;
    case std::chars_format::scientific:
        spec = "%e";
        break;
    case std::chars_format::hex:
        spec = "%a";
        break;
    default:;
    }
    auto const nmax = last - first;
    auto n = std::snprintf(first, nmax, spec, value);
    if (n <= 0 || n >= nmax)
        return {last, std::errc::value_too_large};
    else
        return {first + n, std::errc{}};
}
template <typename T>
requires std::is_floating_point_v<T>
inline to_chars_result to_chars(char* first, char* last, T const& value, std::chars_format fmt, int precision)
{
    auto spec = "%.*g";
    switch (fmt) {
    case std::chars_format::fixed:
        spec = "%.*f";
        break;
    case std::chars_format::scientific:
        spec = "%.*e";
        break;
    case std::chars_format::hex:
        spec = "%.*a";
        break;
    default:;
    }
    auto const nmax = last - first;
    auto n = std::snprintf(first, nmax, spec, precision, value);
    if (n <= 0 || n >= nmax)
        return {last, std::errc::value_too_large};
    else
        return {first + n, std::errc{}};
}
#endif

} // namespace std
