#pragma once

#include "fold.hpp"
#include "string_traits.hpp"
#include "utf.hpp"
#include <string>

namespace strings {

// searcher implements a simple substring search algorithm using unicode case folding
//
// returns search score 0..6 (see impl for details)
//
struct searcher {
    std::u32string nc;
    std::u32string nf;

    using score = unsigned;

    static constexpr auto no_match = score{0};
    static constexpr auto inner_ci = score{1};
    static constexpr auto inner_cs = score{2};
    static constexpr auto front_ci = score{3};
    static constexpr auto front_cs = score{4};
    static constexpr auto full_ci = score{5};
    static constexpr auto full_cs = score{6};

    searcher(searcher const&) = delete;

    template <typename Input> searcher(Input const& needle)
    {
        auto u = utf::decoder{as_string_view(needle)};
        while (!u.empty()) {
            auto c = u.get();
            nc += c;
            nf += fold<folding::simple>(c);
        }
    }

    template <typename Input> auto operator()(Input const& haystack) -> score
    {
        auto sv = as_string_view(haystack);

        if (nf.empty())
            return sv.empty() ? full_cs : no_match;

        auto u = utf::decoder{sv};
        auto hc = std::u32string{};
        auto hf = std::u32string{};
        while (!u.empty()) {
            auto c = u.get();
            hc += c;
            hf += fold<folding::simple>(c);
        }

        // case sensitive
        auto p = hc.find(nc);
        if (p == 0) {
            if (hc.size() == nc.size())
                return full_cs; // full match
            else
                return front_cs; // prefix match
        }
        else if (p != std::u32string::npos) {
            return inner_cs;
        }

        // case insenitive
        p = hf.find(nf);
        if (p == 0) {
            if (hc.size() == nc.size())
                return full_ci; // full match
            else
                return front_ci; // prefix match
        }
        else if (p != std::u32string::npos) {
            return inner_ci;
        }

        return no_match;
    }
};

} // namespace strings