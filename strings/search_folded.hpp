#pragma once

#include "codec.hpp"
#include "fold.hpp"
#include <string>
#include <vector>

namespace strings {

// searcher implements a simple substring search algorithm using unicode case folding
//
// returns search score 0..6 (see impl for details)
//
struct searcher {
    using carrier_string = std::basic_string<codepoint::carrier_type>;
    carrier_string nc;
    carrier_string nf;

    using score = unsigned;
    static inline constexpr auto npos = carrier_string::npos;

    static constexpr auto no_match = score{0};
    static constexpr auto inner_ci = score{1};
    static constexpr auto inner_cs = score{2};
    static constexpr auto front_ci = score{3};
    static constexpr auto front_cs = score{4};
    static constexpr auto full_ci = score{5};
    static constexpr auto full_cs = score{6};

    searcher(searcher const&) = delete;

    searcher(string_like_input auto const& needle)
    {
        auto dec = utf::make_decoder(needle);
        auto c = dec();
        while (c) {
            nc.push_back(c->value);
            nf.push_back(fold::unicode_simple(*c).value);
            c = dec();
        }
    }

    auto operator()(string_like_input auto const& haystack) -> score
    {
        auto dec = utf::make_decoder(haystack);

        auto c = dec();

        if (!c.has_value())
            return nf.empty() ? full_cs : no_match;

        auto hc = carrier_string{c->value};
        auto hf = carrier_string{fold::unicode_simple(*c).value};
        c = dec();
        while (c.has_value()) {
            hc.push_back(c->value);
            hf.push_back(fold::unicode_simple(*c).value);
            c = dec();
        }

        // case sensitive
        auto p = hc.find(nc);
        if (p == 0) {
            if (hc.size() == nc.size())
                return full_cs; // full match
            else
                return front_cs; // prefix match
        }
        else if (p != npos) {
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
        else if (p != npos) {
            return inner_ci;
        }

        return no_match;
    }
};

} // namespace strings