#pragma once

#include "ascii.hpp"
#include "codec.hpp"
#include "codepoint.hpp"
#include "decimal_digits.hpp"
#include "fold.hpp"
#include <cstdint>
#include <limits>
#include <variant>

namespace strings {

enum class compare_fallback {
    none,
    lexicographical,
};

template <compare_fallback Fallback = compare_fallback::lexicographical>
constexpr auto compare(codepoint_source auto&& lhs, codepoint_source auto&& rhs, codepoint_folding auto f) -> int
{
    constexpr auto fallback = Fallback;
    constexpr auto lex_fallback = (fallback == compare_fallback::lexicographical);
    using lex_status_t = std::conditional_t<lex_fallback, int, int const>;
    lex_status_t lex_status = 0;

    auto a = lhs();
    auto b = rhs();
    while (true) {
        if (!a)
            return b ? -1 : lex_status;
        if (!b)
            return +1;
        if (*a != *b) {
            if constexpr (lex_fallback)
                if (!lex_status)
                    lex_status = int(*a - *b);

            *a = f(*a);
            *b = f(*b);
            if (*a != *b)
                return int(*a - *b);
        }
        a = lhs();
        b = rhs();
    }
    return lex_status; // this line should be unreacheable
}

template <compare_fallback Fallback = compare_fallback::lexicographical>
constexpr auto compare(string_like_input auto const& lhs, string_like_input auto const& rhs, codepoint_folding auto f) -> int
{
    return compare<Fallback>(utf::make_decoder(lhs), utf::make_decoder(rhs), f);
}

template <compare_fallback Fallback = compare_fallback::lexicographical>
constexpr auto compare(string_like_input auto const& lhs, string_like_input auto const& rhs) -> int
{
    return compare<Fallback>(utf::make_decoder(lhs), utf::make_decoder(rhs), fold::unicode_simple);
}

namespace detail {

inline void add_decimal_digit(uint64_t& v, unsigned d)
{
    // returns v * 10 + d, saturated
    constexpr auto sat_max = std::numeric_limits<uint64_t>::max();
    constexpr auto tenth_of_sat = sat_max / 10u;
    if (v > tenth_of_sat)
        v = sat_max;
    else {
        v *= 10;
        if (v > sat_max - d)
            v = sat_max;
        else
            v += d;
    }
};

template <bool UnicodeDigits> inline auto digit_from(codepoint cp) -> unsigned
{
    auto v = cp.value - unsigned('0');
    if constexpr (UnicodeDigits)
        if (v >= 10u)
            v = unicode::decimal(cp.value);

    return v; // valid if v < 10u
}

} // namespace detail

template <bool UnicodeDigits = true, compare_fallback Fallback = compare_fallback::lexicographical>
constexpr auto natural_compare(codepoint_source auto&& lhs, codepoint_source auto&& rhs, codepoint_folding auto f) -> int
{
    constexpr auto fallback = Fallback;
    constexpr auto lex_fallback = (fallback == compare_fallback::lexicographical);
    using lex_status_t = std::conditional_t<lex_fallback, int, int const>;
    constexpr auto digit_from = detail::digit_from<UnicodeDigits>;

    lex_status_t lex_status = 0;
    int fld_status = 0;

    auto update_status = [&](codepoint a, codepoint b) {
        if (a == b)
            return;

        if constexpr (lex_fallback)
            if (!lex_status)
                lex_status = a < b ? -1 : a - b;

        if (!fld_status) {
            a = f(a);
            b = f(b);
            fld_status = a < b ? -1 : a - b;
        }
    };

    auto compose_result = [&]() { return (fld_status != 0 || !lex_fallback) ? fld_status : lex_status; };

    auto a = lhs();
    auto b = rhs();

    auto check_done = [&]() -> std::optional<int> {
        if (!a)
            return b.has_value() ? -1 : compose_result();
        else if (!b)
            return +1;
        else
            return {};
    };

    while (true) {
        if (auto done = check_done())
            return *done;

        if (*a != *b)
            update_status(*a, *b);

        unsigned d_a = digit_from(*a);
        unsigned d_b = digit_from(*b);

        if (d_a >= 10u || d_b >= 10u) {
            // not a numeric pair
            if (fld_status)
                return fld_status;
            else {
                a = lhs();
                b = rhs();
                continue;
            }
        }

        // handle numeric pair
        uint64_t num_a = d_a;
        uint64_t num_b = d_b;
        while (true) {
            // track both numbers while possible
            a = lhs();
            b = rhs();
            d_a = a ? digit_from(*a) : 10u;
            d_b = b ? digit_from(*b) : 10u;
            if (d_a < 10u)
                detail::add_decimal_digit(num_a, d_a);
            if (d_b < 10u)
                detail::add_decimal_digit(num_b, d_b);
            if (d_a < 10u && d_b < 10u) {
                if constexpr (UnicodeDigits)
                    if (*a != *b)
                        update_status(*a, *b);
            }
            else
                break;
        }

        // at most one of the numbers still needs tracking
        auto extra_digits_a = std::size_t{0};
        while (d_a < 10u) {
            ++extra_digits_a;
            a = lhs();
            d_a = a ? digit_from(*a) : 10u;
            if (d_a < 10u)
                detail::add_decimal_digit(num_a, d_a);
        }
        auto extra_digits_b = std::size_t{0};
        while (d_b < 10u) {
            ++extra_digits_b;
            b = rhs();
            d_b = b ? digit_from(*b) : 10u;
            if (d_b < 10u)
                detail::add_decimal_digit(num_b, d_b);
        }

        // done with both numbers
        if (num_a != num_b)
            return num_a < num_b ? -1 : +1;
        if (extra_digits_a != extra_digits_b)
            return extra_digits_a < extra_digits_b ? -1 : +1;
        if (auto done = check_done())
            return *done;
        if (*a != *b)
            update_status(*a, *b);
    }

    return compose_result();
};

template <bool UnicodeDigits = true, compare_fallback Fallback = compare_fallback::lexicographical>
constexpr auto natural_compare(string_like_input auto&& lhs, string_like_input auto&& rhs, codepoint_folding auto f) -> int
{
    return natural_compare<UnicodeDigits, Fallback>(utf::make_decoder(lhs), utf::make_decoder(rhs), f);
}

template <bool UnicodeDigits = true, compare_fallback Fallback = compare_fallback::lexicographical>
constexpr auto natural_compare(string_like_input auto&& lhs, string_like_input auto&& rhs) -> int
{
    return natural_compare<UnicodeDigits, Fallback>(
        utf::make_decoder(lhs), utf::make_decoder(rhs), fold::unicode_simple);
}

} // namespace strings
