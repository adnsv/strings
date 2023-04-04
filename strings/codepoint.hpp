#pragma once

#include <concepts>
#include <optional>
#include <ranges>
#include <string_view>

#ifndef STRINGS_APPLE_CLANG_RANGES
#include <ranges>
#else
#include <__ranges/all.h>
#endif

namespace strings {

// codepoint
// - normally matches unicode codepoint
// - can be used in other contexts as an intermediate character carrier
//
struct codepoint {
    using carrier_type = char32_t;
    using difference_type = int;
    
    char32_t value;

    constexpr auto operator++() -> codepoint& // pre
    {
        ++value;
        return *this;
    }
    constexpr auto operator++(int) -> codepoint // post
    {
        auto t = value;
        ++value;
        return codepoint{t};
    }
    constexpr auto operator--() -> codepoint& // pre
    {
        --value;
        return *this;
    }
    constexpr auto operator--(int) -> codepoint // post
    {
        auto t = value;
        --value;
        return codepoint{t};
    }

    friend constexpr auto operator==(codepoint const& a, codepoint const& b) { return a.value == b.value; };
    friend constexpr auto operator<=>(codepoint const& a, codepoint const& b) { return a.value <=> b.value; };
    friend constexpr auto operator-(codepoint const& a, codepoint const& b) -> int
    {
        return int(a.value) - int(b.value);
    }
    friend constexpr auto operator+(codepoint const& v, int n) -> codepoint
    {
        auto t = v;
        t.value += n;
        return t;
    }
    friend constexpr auto operator+(int n, codepoint const& v) -> codepoint
    {
        auto t = v;
        t.value += n;
        return t;
    }
};

// decode error sentinels
namespace errcp {
constexpr auto error_bit = codepoint{0x80000000};
constexpr auto insufficient = codepoint{0x80000000};
constexpr auto incomplete = codepoint{0xC0000000};
constexpr auto overlong = codepoint{0xA0000000};
constexpr auto unexpected = codepoint{0xB0000000};
} // namespace errcp

enum class unexpected_policy {
    consume_one, // consume one codeunit
    consume_all, // consume all codeunits
};

template <typename T>
concept codepoint_source = requires (T v) {
    { v() } -> std::same_as<std::optional<codepoint>>;
};

template <typename T>
concept codepoint_sink = requires (T v, codepoint c) {
    { v(c) } -> std::same_as<void>;
};

template <typename T>
concept codepoint_folding = requires(T v, codepoint c)
{
    { v(c) } -> std::same_as<codepoint>;
};


// codeunit
//
// - a concept that matches commonly used character data carrier types (char, wchar_t, etc...)
// - matches character types that are used in basic_string<T> and other string-like types
//
template <typename T>
concept codeunit = std::same_as<char, std::remove_cv_t<T>> || std::same_as<wchar_t, std::remove_cv_t<T>> ||
    std::same_as<char8_t, std::remove_cv_t<T>> || std::same_as<char16_t, std::remove_cv_t<T>> ||
    std::same_as<char32_t, std::remove_cv_t<T>>;

enum class encoding {
    unknown = 0,
    utf8 = 1,
    utf16 = 2,
    utf32 = 4,
};

namespace unicode {
constexpr auto codepoint_last = codepoint{0x10FFFF}; // inclusive
constexpr auto high_surrogate_first = codepoint{0xD800};
constexpr auto high_surrogate_last = codepoint{0xDBFF}; // inclusive
constexpr auto low_surrogate_first = codepoint{0xDC00};
constexpr auto low_surrogate_last = codepoint{0xDFFF}; // inclusive

constexpr auto is_surrogate(codepoint c) { return c >= high_surrogate_first && c <= low_surrogate_last; }
constexpr auto is_high_surrogate(codepoint c) { return c >= high_surrogate_first && c <= high_surrogate_last; };
constexpr auto is_low_surrogate(codepoint c) { return c >= low_surrogate_first && c <= low_surrogate_last; };
constexpr auto is_valid(codepoint c) { return c <= codepoint_last && !is_surrogate(c); }

// default_replacement_char - normally displayed as rhombus with a question mark inside
constexpr auto replacement_character = codepoint{0xFFFD};

} // namespace unicode

template <typename S>
concept string_view_input = std::same_as<std::remove_cv_t<S>, std::string_view> ||
    std::same_as<std::remove_cv_t<S>, std::wstring_view> || std::same_as<std::remove_cv_t<S>, std::u8string_view> ||
    std::same_as<std::remove_cv_t<S>, std::u16string_view> || std::same_as<std::remove_cv_t<S>, std::u32string_view>;

template <typename S>
concept convertible_to_string_view_input = std::convertible_to<S, std::string_view> ||
    std::convertible_to<S, std::wstring_view> || std::convertible_to<S, std::u8string_view> ||
    std::convertible_to<S, std::u16string_view> || std::convertible_to<S, std::u32string_view>;

template <typename S>
using string_view_type_of = std::conditional_t<std::convertible_to<S, std::string_view>, std::string_view,
    std::conditional_t<std::convertible_to<S, std::wstring_view>, std::wstring_view,
        std::conditional_t<std::convertible_to<S, std::u8string_view>, std::u8string_view,
            std::conditional_t<std::convertible_to<S, std::u16string_view>, std::u16string_view,
                std::u32string_view>>>>;

template <typename S>
concept range_input = std::ranges::range<S> && codeunit<std::ranges::range_value_t<S>>;

template <typename S>
concept string_like_input = convertible_to_string_view_input<S> || range_input<S>;

template <typename T> struct string_like_traits;

template <string_like_input T>
struct string_like_traits<T> {
    using codeunit_type = std::ranges::range_value_t<T>;
};

template <convertible_to_string_view_input T>
struct string_like_traits<T> {
    using codeunit_type = typename string_view_type_of<T>::value_type;
};


template <string_like_input S> using codeunit_type_of = typename string_like_traits<S>::codeunit_type;


} // namespace strings