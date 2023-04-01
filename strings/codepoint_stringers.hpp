#pragma once

#include "codepoint.hpp"
#include <charconv>
#include <string_view>
#include <type_traits>

namespace strings {

inline auto to_chars(char* first, char* last, codepoint const& cp) -> std::to_chars_result
{
    auto const v = cp.value;
    auto n = 2 + v > 0x0FFFFFFFu ? 8 : v > 0x00FFFFFFu ? 7 : v > 0x000FFFFFu ? 6 : v > 0x0000FFFFu ? 5 : 4;
    if (first + n > last)
        return {last, std::errc::value_too_large};
    *first++ = 'U';
    *first++ = '+';

    auto digit = [](unsigned v) {
        v &= 0x0f;
        return char('0' + v + ('A' - 1 - '9') * unsigned(v >= 10));
    };
    if (v > 0x0FFFFFFF)
        *first++ = digit(v >> 28);
    if (v > 0x00FFFFFF)
        *first++ = digit(v >> 24);
    if (v > 0x000FFFFF)
        *first++ = digit(v >> 20);
    if (v > 0x0000FFFF)
        *first++ = digit(v >> 16);
    *first++ = digit(v >> 12);
    *first++ = digit(v >> 8);
    *first++ = digit(v >> 4);
    *first++ = digit(v);
    return {first, std::errc{}};
}

constexpr auto from_chars(char const* first, char const* last, codepoint& cp) -> std::from_chars_result
{
    auto decode_hex = [](unsigned c) -> char32_t {
        c -= unsigned('0');
        if (c < 10)
            return c;
        c -= unsigned('A' - '0');
        if (c < 6)
            return c + 10;
        c -= unsigned('a' - 'A');
        return c < 6 ? c + 10 : unsigned(-1);
    };

    auto const n = last - first;

    if (n < 3 || (first[0] != 'U' && first[0] != 'u') || first[1] != '+')
        return {first, std::errc::invalid_argument};

    auto v = decode_hex(first[2]);
    if (v >= 16u)
        return {first, std::errc::invalid_argument};

    auto i = 3;
    while (true) {
        if (i >= n)
            break;
        auto d = decode_hex(first[i]);
        if (d >= 16u)
            break;
        if (v >= 0x10000000)
            return {first, std::errc::result_out_of_range};
        v = (v << 4) + d;
        ++i;
    }
    cp.value = v;
    return {first + i, std::errc{}};
}

enum class codeunit_to_chars_format {
    hex,
    hex_0x,
};

// this to_chars overload is mostly useful for debugging
inline auto to_chars(char* first, char* last, codeunit auto c,
    codeunit_to_chars_format fmt = codeunit_to_chars_format::hex_0x) -> std::to_chars_result
{
    using input_t = std::remove_cv_t<decltype(c)>;
    using unsigned_t = std::make_unsigned_t<input_t>;

    auto u = static_cast<unsigned_t>(c);
    auto n = sizeof(unsigned_t) * 2;

    auto put_hex = [&]() {
        while (n) {
            --n;
            auto v = (u >> (n * 4)) & 0xf;
            auto d = char('0' + v + ('A' - 1 - '9') * unsigned(v >= 10));
            *first++ = d;
        }
    };

    switch (fmt) {
    case codeunit_to_chars_format::hex:
        if (first + n <= last) {
            put_hex();
            return {first, std::errc{}};
        }
        break;

    case codeunit_to_chars_format::hex_0x:
        if (first + n + 2 <= last) {
            *first++ = '0';
            *first++ = 'x';
            put_hex();
            return {first, std::errc{}};
        }
        break;
    }

    return {last, std::errc::value_too_large};
}

} // namespace strings