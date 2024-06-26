#pragma once

#include "ascii.hpp"
#include "codec.hpp"
#include "trim.hpp"
#include <string_view>

namespace strings {

// split - calls SegmentHandler for each of subviews while splitting the input
// string with the specified separator
template <typename Input, typename Separator, typename U = codeunit_type_of<Input>>
requires(std::convertible_to<Input, std::basic_string_view<U>> && std::convertible_to<Input, std::basic_string_view<U>>)
void split(Input const& input, Separator const& separator, std::invocable<std::basic_string_view<U>> auto&& f)
{
    using string_view_type = typename std::basic_string_view<U>;
    using char_type = typename string_view_type::value_type;
    auto sv = string_view_type(input);
    auto sep = string_view_type(separator);
    auto cur = std::size_t{0};
    auto pos = sv.find(sep, cur);
    while (pos != string_view_type::npos) {
        f(sv.substr(cur, pos - cur));
        cur = pos + sep.size();
        pos = sv.find(sep, cur);
    }
    f(sv.substr(cur));
}

template <typename Input, typename Separator, typename U = codeunit_type_of<Input>>
void split(Input const& input, Separator const& separator, trimming_predicate auto tr, trim_side side,
    std::invocable<std::basic_string_view<U>> auto&& f)
{
    using string_view_type = typename std::basic_string_view<U>;
    using char_type = typename string_view_type::value_type;
    auto sv = string_view_type(input);
    auto sep = string_view_type(separator);
    auto cur = std::size_t{0};
    auto pos = sv.find(sep, cur);
    while (pos != string_view_type::npos) {
        f(trim(sv.substr(cur, pos - cur), tr, side));
        cur = pos + sep.size();
        pos = sv.find(sep, cur);
    }
    f(trim(sv.substr(cur), tr, side));
}

template <typename Input, typename Separator, typename U = codeunit_type_of<Input>>
void split(Input const& input, Separator const& separator, trimming_predicate auto tr,
    std::invocable<std::basic_string_view<U>> auto&& f)
{
    using ft = std::remove_cvref_t<decltype(f)>;
    split<Input, Separator, U>(input, separator, tr, trim_side::front_and_back, std::forward<ft&&>(f));
}

struct numeric_section_result {
    using size_type = std::string_view::size_type;
    size_type pos = 0;
    size_type count = 0;
    friend auto operator==(numeric_section_result const&, numeric_section_result const&) -> bool = default;
};

inline auto find_last_numeric_section(std::string_view s) -> numeric_section_result
{
    auto last = s.size();
    while (last)
        if (ascii::is_decimal(s[last - 1]))
            break;
        else
            --last;

    auto first = last;
    while (first)
        if (ascii::is_decimal(s[first - 1]))
            --first;
        else
            break;

    if (first == last)
        return {std::string_view::npos, 0};
    else
        return {first, last - first};
}

} // namespace strings