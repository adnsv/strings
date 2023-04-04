#pragma once

#include "ascii.hpp"
#include <algorithm>
#include <charconv>
#include <clocale>
#include <cmath>
#include <concepts>
#include <optional>
#include <string>
#include <string_view>

#include "charconv_stubs.hpp"

namespace strings::fp {

namespace appearance {

struct separators {
    char decimal = '.'; // we only support single-byte decimal separators here
};

struct sign {
    std::string_view plus = "";   // optional prefix for positive values
    std::string_view minus = "-"; // U+2212 (can be replaced with ascii hyphen U+002D)
};

struct specials {
    std::string_view infinity = "inf";   // U+221E
    std::string_view notanumber = "nan"; // override "nan", "nan(ind)", and nan(snan) with this
};

struct scientific {
    std::string_view exp_prefix = "e"; // exponent prefix in the scientific mode
    bool exp_super = false;            // exponent superscript in the scientific mode
};

template <typename T>
concept modifier =
    std::same_as<T, separators> || std::same_as<T, sign> || std::same_as<T, specials> || std::same_as<T, scientific>;

} // namespace appearance

inline auto get_user_decimal() -> char
{
    auto ret = char('.');
    auto prev = std::setlocale(LC_NUMERIC, "");
    auto ld = std::localeconv();
    if (ld && ld->decimal_point && ld->decimal_point[0] == ',')
        ret = ',';
    std::setlocale(LC_NUMERIC, prev);
    return ret;
}

inline char const user_decimal = get_user_decimal();

struct locale : public appearance::separators,
                public appearance::sign,
                public appearance::specials,
                public appearance::scientific {

    constexpr locale() noexcept = default;

    static constexpr auto ascii(char decimal = '.')
    {
        auto ret = locale{};
        ret.decimal = decimal;
        return ret;
    }

    static constexpr auto unicode(char decimal = '.')
    {
        auto ret = locale{};
        ret.decimal = decimal;
        ret.plus = "";      // no prefix for positive numbers
        ret.minus = "−";    // U+2212 - use unicode mathematical minus instead of ascii dash
        ret.infinity = "∞"; // U+221E
        ret.notanumber = "NaN";
        ret.exp_prefix = " × 10"; // use " × 10" as exponent prefix
        ret.exp_super = true;     // use superscript for exponent digits
        return ret;
    }

    friend constexpr auto operator|(locale const& l, appearance::modifier auto const& a) -> locale
    {
        using modifier_type = std::remove_cvref_t<decltype(a)>;
        auto ret = l;
        auto& m = static_cast<modifier_type&>(ret);
        m = a;
        return ret;
    }
};

struct settings {
    int frac_precision = 2;
    int frac_significant = 2;
    int sci_precision = 2;   // fraction precision when using chars_format::scientific
    float sci_below = 1e-6f; // use chars_format::scientific when abs(v) is above this value
    float sci_above = 1e+6f; // use chars_format::scientific when abs(v) is below this value
};

namespace detail {

static constexpr auto npos = std::string_view::npos;

inline auto to_chars(char* first, char* last, std::string_view s) -> std::to_chars_result
{
    if (first + s.size() > last)
        return {last, std::errc::value_too_large};
    for (auto c : s)
        *first++ = c;
    return {first, std::errc{}};
}

constexpr auto digit_val(char c) -> int
{
    auto u = unsigned(c) - unsigned('0');
    return u < 10u ? int(u) : -1;
}

constexpr auto is_digit(char c) { return unsigned(c) - unsigned('0') < 10u; }

inline auto find_trim_pos(char const* first, char const* last) -> char const*
{
    if (first == last || last[-1] != '0')
        return last;

    for (; first != last; ++first)
        if (*first == '.' || *first == ',')
            break;

    auto z = last;
    while (z[-1] == '0')
        --z;

    if (z == first + 1)
        return first;

    ++first;
    while (first != z && *first >= '0' && *first <= '9')
        ++first;

    return (first == z) ? z : last;
}

inline auto sci_exp_value(char const* first, char const* last) -> std::optional<int>
{
    auto const n = last - first;
    if (n < 2 || first[0] != 'e' || !is_digit(last[-1]))
        return {};
    auto sign = 1;
    ++first;
    if (*first == '+')
        ++first;
    else if (*first == '-') {
        ++first;
        sign = -1;
    }
    auto v = 0;
    for (; first != last; ++first) {
        auto d = digit_val(*first);
        if (d < 0)
            return {};
        v = v * 10 + d;
    }
    return v * sign;
}

inline auto exp_to_chars(char* first, char* last, std::string_view prefix, int exp, std::string_view plus,
    std::string_view minus, bool use_superscript) -> std::to_chars_result
{
    // count ascii symbols required (assuming exp is less than 100)
    auto sign = exp < 0 ? minus : plus;
    if (exp < 0)
        exp = -exp;
    if (exp >= 100)
        return {last, std::errc::value_too_large};

    auto d1 = exp / 10;
    auto d2 = exp % 10;

    auto n = prefix.size() + sign.size();

    static constexpr std::string_view ss[10] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};

    if (!use_superscript)
        n += d1 ? 2 : 1;
    else
        n += (d1 ? ss[d1].size() : 0) + ss[d2].size();

    if (first + n > last)
        return {last, std::errc::value_too_large};
    for (auto c : prefix)
        *first++ = c;
    for (auto c : sign)
        *first++ = c;

    if (!use_superscript) {
        if (d1)
            *first++ = char('0') + d1;
        *first++ = char('0') + d2;
    }
    else {
        if (d1)
            for (auto c : ss[d1])
                *first++ = c;
        for (auto c : ss[d2])
            *first++ = c;
    }
    return {first, std::errc{}};
}

template <std::floating_point T>
inline auto for_trimming_(char* first, char* last, T const v, settings const& settings, std::size_t& trm_pos,
    std::size_t& exp_pos) -> std::to_chars_result
{
    trm_pos = std::string_view::npos;
    exp_pos = std::string_view::npos;

    if (auto fpc = std::fpclassify(v); fpc == FP_ZERO) {
        // optimize for pure zeroes
        if (first + 1 > last)
            return {last, std::errc::value_too_large};
        *first++ = '0';
        return {first, std::errc{}};
    }
    else if (fpc == FP_INFINITE || fpc == FP_NAN) {
        // infinities and nans
        return std::to_chars(first, last, v);
    }

    auto fmt = std::chars_format::fixed;
    auto p = settings.frac_precision;

    auto const u = std::abs(v);
    if (u < settings.sci_below || u > settings.sci_above) {
        fmt = std::chars_format::scientific;
        p = settings.sci_precision;
    }
    else if (u < T(1.0)) {
        auto exp = u >= T(0.1)      ? -1
                   : u >= T(0.01)   ? -2
                   : u >= T(0.001)  ? -3
                   : u >= T(0.0001) ? -4
                                    : int(std::floor(std::log10(u)));
        p = std::max(p, settings.frac_significant - exp - 1);
    }

    auto ret = std::to_chars(first, last, v, fmt, p);
    if (ret.ec != std::errc{})
        return ret;

    auto const n = ret.ptr - first;
    if (fmt == std::chars_format::scientific || fmt == std::chars_format::general) {
        exp_pos = 0;
        while (exp_pos < n)
            if (first[exp_pos] == 'e' || first[exp_pos] == 'E')
                break;
            else
                ++exp_pos;
    }
    else
        exp_pos = n;

    trm_pos = find_trim_pos(first, first + exp_pos) - first;
    return ret;
}

} // namespace detail

template <std::floating_point T>
inline auto to_chars(char* first, char* last, T const v, settings const& settings, bool trim = true)
    -> std::to_chars_result
{
    std::size_t trim_pos, exp_pos;
    auto r = detail::for_trimming_(first, last, v, settings, trim_pos, exp_pos);
    if (!trim || r.ec != std::errc{} || exp_pos == std::string_view::npos)
        return r;

    if (first + exp_pos == r.ptr) {
        if (first + trim_pos != r.ptr)
            r.ptr = first + trim_pos;
        return r;
    }

    auto exp = detail::sci_exp_value(first + exp_pos, r.ptr);
    if (!exp)
        return r;
    r.ptr = first + exp_pos;

    if (first + trim_pos != last)
        r.ptr = first + trim_pos;
    if (*exp == 0)
        return r; // trim zero exponents, e.g. "e+00"

    return detail::exp_to_chars(r.ptr, last, "e", *exp, "", "-", false);
}

template <std::floating_point T>
inline auto to_chars(char* first, char* last, T const v, settings const& settings, locale const& locale)
    -> std::to_chars_result
{
    auto fpc = std::fpclassify(v);

    if (fpc == FP_ZERO)
        return detail::to_chars(first, last, "0");

    if (fpc == FP_NAN)
        return detail::to_chars(first, last, locale.notanumber);

    auto const sign = std::signbit(v) ? std::string_view{locale.minus} : std::string_view{locale.plus};
    auto const u = std::abs(v);

    if (auto r = detail::to_chars(first, last, sign); r.ec != std::errc{})
        return r;
    else
        first = r.ptr;

    std::size_t trim_pos, exp_pos;
    auto r = detail::for_trimming_(first, last, u, settings, trim_pos, exp_pos);
    if (r.ec != std::errc{})
        return r;

    if (locale.decimal != '.')
        if (auto p = std::string_view{first, std::size_t(r.ptr - first)}.find('.'); p != std::string_view::npos)
            first[p] = locale.decimal;

    if (first + exp_pos == r.ptr) {
        if (first + trim_pos != r.ptr)
            r.ptr = first + trim_pos;
        return r;
    }

    auto exp = detail::sci_exp_value(first + exp_pos, r.ptr);
    if (!exp)
        return r;
    r.ptr = first + trim_pos;
    if (*exp == 0)
        return r; // trim zero exponents, e.g. "e+00"

    auto l_plus = std::string_view{locale.plus};
    auto l_minus = std::string_view{locale.minus};
    if (locale.exp_super) {
        l_plus = {};
        l_minus = "⁻";
    }

    return detail::exp_to_chars(r.ptr, last, locale.exp_prefix, *exp, l_plus, l_minus, locale.exp_super);
}

// to_chars_buffer_cap
//
// minimum recommended capacity to use with fp_to_chars methods
//
constexpr std::size_t to_chars_buffer_cap = 128;

static const auto badval = std::string{"####"};

template <std::floating_point T>
inline auto to_string(T const v, settings const& settings, bool trim = false) -> std::string
{
    char buf[to_chars_buffer_cap];
    auto r = to_chars(buf, buf + to_chars_buffer_cap, v, settings, trim);
    if (r.ec == std::errc{})
        return std::string(buf, r.ptr);
    return badval;
}

template <std::floating_point T>
inline auto to_string(T const v, settings const& settings, locale const& locale) -> std::string
{
    char buf[to_chars_buffer_cap];
    auto r = to_chars(buf, buf + to_chars_buffer_cap, v, settings, locale);
    if (r.ec == std::errc{})
        return std::string(buf, r.ptr);
    return badval;
}

} // namespace strings::fp