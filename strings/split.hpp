#pragma once

#include "codec.hpp"
#include "trim.hpp"
#include <string_view>

namespace strings {

// split - calls SegmentHandler for each of subviews while splitting the input
// string with the specified separator
template <typename Input, typename Separator, typename U = codeunit_type_of<Input>>
requires                                                     
    std::convertible_to<Input, std::basic_string_view<U>> && 
    std::convertible_to<Input, std::basic_string_view<U>>    
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

} // namespace strings