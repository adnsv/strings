#pragma once

#include "trim.hpp"
#include <string_view>
#include <vector>

namespace strings {

// split - calls SegmentHandler for each of subviews while splitting the input
// string with the specified separator
template <typename Input, typename Separator, typename SegmentHandler>
void split(Input const& input, Separator const& separator, SegmentHandler f)
{
    using sv_type = typename string_traits<Input>::string_view_type;
    using char_type = sv_type::value_type;
    auto sv = as_string_view(input);
    auto sep = as_string_view(separator);
    auto cur = std::size_t{0};
    auto pos = sv.find(sep, cur);
    while (pos != sv_type::npos) {
        f(sv.substr(cur, pos - cur));
        cur = pos + sep.size();
        pos = sv.find(sep, cur);
    }
    f(sv.substr(cur));
}

// split_view - splits the input string and produces a vector of string views
template <typename Input, typename Separator>
auto split_view(Input const& input, Separator const& separator)
{
    using sv_type = typename string_traits<Input>::string_view_type;
    auto v = std::vector<sv_type>{};
    split(input, separator, [&](sv_type seg) { v.push_back(seg); });
    return v;
}

// split_view - splits the input string and produces a vector of trimmed string views
template <typename Input, typename Separator, typename TrimmingPredicate>
auto split_view(Input const& input, Separator const& separator, TrimmingPredicate tr,
    trim_side side = trim_front_and_back)
{
    using sv_type = typename string_traits<Input>::string_view_type;
    auto v = std::vector<sv_type>{};
    split(input, separator,
        [&](sv_type seg) { v.push_back(trim<Input, TrimPredicate>(seg, tr, side)); });
    return v;
}

} // namespace strings