#pragma once

#include "ascii.hpp"
#include "fold.hpp"
#include "utf.hpp"

namespace strings {

template <typename Input, folding Folding = folding::simple>
constexpr auto compare(Input const& lhs, Input const& rhs) -> int
{
    if constexpr (Folding == folding::none) {
        // straight simple lexicographical compare
        if (lhs == rhs)
            return 0;
        return lhs < rhs ? -1 : +1;
    }

    auto l = utf::decoder(as_string_view(lhs));
    auto r = utf::decoder(as_string_view(rhs));

    while (!l.empty()) {
        if (r.empty())
            return +1;

        auto lc = l.get();
        auto rc = r.get();

        lc = fold<Folding>(lc);
        rc = fold<Folding>(rc);

        if (lc == rc)
            continue;
        if (lc < rc)
            return -1;
        if (rc < lc)
            return +1;
    }

    return 0;
}

template <typename Input, folding Folding = folding::simple,
    bool CompareLexicographicallyIfOtherwiseEqual = true>
constexpr auto natural_compare(Input const& lhs, Input const& rhs) -> int
{
    auto l = utf::decoder(as_string_view(lhs));
    auto r = utf::decoder(as_string_view(rhs));

    while (!l.empty()) {
        if (r.empty())
            return +1;

        auto lc = l.get();
        auto rc = r.get();

        // very simple numeric comparizon, does not handle overflow (yet)
        auto ld = ascii::decimal(lc);
        auto rd = ascii::decimal(rc);
        if (ld < 10 && rd < 10) {
            while (!l.empty()) {
                lc = l.get();
                if (auto d = ascii::decimal(lc); d < 10)
                    ld = ld * 10 + d;
                else
                    break;
            }
            while (!r.empty()) {
                rc = r.get();
                if (auto d = ascii::decimal(rc); d < 10)
                    rd = rd * 10 + d;
                else
                    break;
            }
            if (ld < rd)
                return -1;
            if (rd < ld)
                return +1;
            if (lc < rc)
                return +1;
            if (rc < lc)
                return -1;
        }

        lc = fold<Folding>(lc);
        rc = fold<Folding>(rc);

        if (lc == rc) {
            continue;
        }
        if (lc < rc)
            return -1;
        if (rc < lc)
            return +1;
    }

    if (!r.empty())
        return -1;

    if constexpr (CompareLexicographicallyIfOtherwiseEqual) {
        if (lhs == rhs)
            return 0;
        return lhs < rhs ? -1 : +1;
    }

    return 0;
}

} // namespace strings