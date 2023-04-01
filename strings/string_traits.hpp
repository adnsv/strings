#pragma once

#include <string>
#include <string_view>
#include <cstdint>

namespace strings {

template <typename T> constexpr auto is_string_like()
{
    return 
        std::convertible_to<T, std::string_view> || 
        std::convertible_to<T, std::wstring_view> ||
        std::convertible_to<T, std::u8string_view> ||
        std::convertible_to<T, std::u16string_view> ||
        std::convertible_to<T, std::u32string_view>;
}

template <typename T> struct string_traits;
template <typename T> requires(is_string_like<T>()) 
struct string_traits<T> {
    using string_view_type = 
        std::conditional_t<std::convertible_to<T, std::string_view>, std::string_view,
            std::conditional_t<std::convertible_to<T, std::wstring_view>, std::wstring_view,
                std::conditional_t<std::convertible_to<T, std::u8string_view>, std::u8string_view,
                    std::conditional_t<std::convertible_to<T, std::u16string_view>,
                        std::u16string_view, std::u32string_view>>>>;

    using char_type = typename string_view_type::value_type;
    using string_type = typename std::basic_string<char_type>;
};

template <typename T> constexpr auto as_string_view(T const& v) {
    static_assert(is_string_like<T>());
    return typename string_traits<T>::string_view_type(v);
}

template <typename T> constexpr auto codeunit_size() -> std::size_t {
    return sizeof(typename std::char_traits<T>::char_type);
}

} // namespace strings