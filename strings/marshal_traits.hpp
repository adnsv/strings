#pragma once

#include "format_spec.hpp"
#include "charconv_stubs.hpp"
#include <concepts>
#include <string_view>
#include <utility>
#include <type_traits>

namespace strings {

// customization for user types
template <typename T> struct string_marshaler;
template <typename T> struct chars_marshaler;
template <typename T> struct formatter;

template <class From, class To>
concept convertible_to_ = std::is_convertible_v<From, To>;

template <typename T, typename... Args>
concept string_marshalable = requires(T const& v, Args&&... args) {
    { string_marshaler<T>{}(v, std::forward<Args>(args)...) } -> convertible_to_<std::string_view>;
};

template <typename T, typename... Args>
concept chars_marshalable = requires(char* buf, T const& v, Args&&... args) {
    { chars_marshaler<T>{}(buf, buf, v, std::forward<Args>(args)...) } -> std::same_as<std::to_chars_result>;
};

template <typename T, typename... Args>
concept marshalable = 
    string_marshalable<T, Args...> || 
    chars_marshalable<T, Args...>;

template <typename T>
concept formattable = requires(char* buf, T const& v, fmt::arg const& a) {
    { formatter<T>{}(buf, buf, v, a) } -> std::same_as<std::to_chars_result>;
};

template <typename T, typename... Args>
concept to_chars_convertible = requires(char* buf, T const& v, Args&&... args) {
    { std::to_chars(buf, buf, v, std::forward<Args>(args)...) } -> std::same_as<std::to_chars_result>;
};

namespace detail {
template <typename T>
concept supported_format_arg = convertible_to_<T, std::string_view> ||
    std::is_integral_v<T> || std::is_floating_point_v<T> || to_chars_convertible<T> ||
    string_marshalable<T> || chars_marshalable<T> || formattable<T> ;
}

}