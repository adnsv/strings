#pragma once

#include <concepts>
#include <cstring>
#include <cwchar>
#include <string>

namespace strings {

namespace detail {
template <typename CharT, typename Traits>
inline auto length(std::basic_string_view<CharT, Traits> str)
{
    return str.size();
}

template <typename CharT, typename Traits>
inline auto length(std::basic_string<CharT, Traits> const& str)
{
    return str.size();
}

inline auto length(char const* str) { return std::strlen(str); }
inline auto length(wchar_t const* str) { return std::wcslen(str); }

template <typename CharT, std::size_t N> inline auto length(CharT (&)[N]) { return N; }
} // namespace detail

template <typename CharT, typename Traits,
    std::convertible_to<std::basic_string_view<CharT, Traits>>... Strs>
auto join(std::basic_string_view<CharT, Traits> sep, Strs&&... strs)
    -> std::basic_string<CharT, Traits>
{
    using detail::length;
    auto const total_length = (length(strs) + ...) + sizeof...(Strs) - 1;
    std::basic_string<CharT, Traits> result;
    result.reserve(total_length);

    auto masher = [&result, sep, count = 0](auto&& str) mutable {
        result += std::forward<decltype(str)>(str);
        ++count;
        if (count != sizeof...(Strs)) {
            result += sep;
        }
    };
    (masher(std::forward<Strs>(strs)), ...);
    return result;
}

template <typename CharT, typename Traits,
    std::convertible_to<std::basic_string_view<CharT, Traits>>... Strs>
auto join(std::basic_string<CharT, Traits> const& sep, Strs&&... strs)
{
    return join(std::basic_string_view<CharT, Traits>(sep), std::forward<Strs>(strs)...);
}

template <typename CharT,
    std::convertible_to<std::basic_string_view<CharT, std::char_traits<CharT>>>... Strs>
auto join(CharT const* sep, Strs&&... strs)
{
    return join(std::basic_string_view<CharT>(sep), std::forward<Strs>(strs)...);
}

} // namespace strings
