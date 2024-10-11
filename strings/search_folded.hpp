#pragma once

#include "codec.hpp"
#include "fold.hpp"
#include <string>
#include <vector>

namespace strings {

inline auto levenshtein_distance(
    std::u32string_view const& s1, std::u32string_view const& s2, int const max_distance) -> int
{
    if (s1.empty())
        return s2.length();
    if (s2.empty())
        return s1.length();

    auto prev_row = std::vector<int>(s2.length() + 1);
    auto curr_row = std::vector<int>(s2.length() + 1);

    // initialize first row
    for (std::size_t j = 0; j <= s2.length(); ++j)
        prev_row[j] = j;

    auto exceeded_max = true;
    for (size_t i = 1; i <= s1.length(); ++i) {
        curr_row[0] = i;
        exceeded_max = true;

        for (size_t j = 1; j <= s2.length(); ++j) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            curr_row[j] = std::min({
                curr_row[j - 1] + 1,   // insertion
                prev_row[j] + 1,       // deletion
                prev_row[j - 1] + cost // substitution
            });

            if (curr_row[j] <= max_distance)
                exceeded_max = false;
        }

        if (exceeded_max)
            return max_distance + 1;

        std::swap(prev_row, curr_row);
    }

    return prev_row[s2.length()];
}

inline auto longest_common_substring(
    std::u32string_view const& s1, std::u32string_view const& s2) -> std::size_t
{
    if (s1.empty() || s2.empty())
        return 0;

    auto dp = std::vector<std::vector<std::size_t>>(
        s1.length() + 1, std::vector<size_t>(s2.length() + 1, 0));

    auto max_length = std::size_t{0};

    for (std::size_t i = 1; i <= s1.length(); ++i) {
        for (std::size_t j = 1; j <= s2.length(); ++j) {
            if (s1[i - 1] == s2[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1] + 1;
                max_length = std::max(max_length, dp[i][j]);
            }
        }
    }

    return max_length;
}

inline auto is_ctrl_or_space(char32_t codepoint) -> bool
{
    return codepoint <= U'\u0020' ||                             // Space
           codepoint == U'\u007f' ||                             // Delete character
           codepoint == U'\u00A0' ||                             // No-Break Space
           codepoint == U'\u1680' ||                             // Ogham Space Mark
           (codepoint >= U'\u2000' && codepoint <= U'\u200A') || // En Quad to Hair Space
           codepoint == U'\u202F' ||                             // Narrow No-Break Space
           codepoint == U'\u205F' ||                             // Medium Mathematical Space
           codepoint == U'\u3000' ||                             // Ideographic Space
           codepoint == U'\u200B';                               // Zero Width Space
}

inline auto is_word_boundary(char32_t c) -> bool
{
    return is_ctrl_or_space(c) || c == U',' || c == U'.' || c == U'?' || c == U'!' || c == U'-';
}

inline auto split_words(std::u32string const& text) -> std::vector<std::u32string>
{
    auto words = std::vector<std::u32string>{};
    auto word = std::u32string{};

    for (auto c : text) {
        if (is_ctrl_or_space(c)) {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }

    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

// searcher implements a simple substring search algorithm using unicode case folding
//
// returns search score 0..6 (see impl for details)
//
struct searcher {
    using carrier_string = std::basic_string<codepoint::carrier_type>;
    carrier_string nc; // original needle value
    carrier_string nf; // case-folded needle value

    using score = float;
    static inline constexpr auto npos = carrier_string::npos;

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
            return nf.empty() ? 1.0f : 0.0f;

        auto hc = carrier_string{c->value};
        auto hf = carrier_string{fold::unicode_simple(*c).value};
        c = dec();
        while (c.has_value()) {
            hc.push_back(c->value);
            hf.push_back(fold::unicode_simple(*c).value);
            c = dec();
        }

        if (nf.length() == 1) {
            auto query_char = nf[0];
            auto best_score = 0.0f;
            for (std::size_t i = 0; i < hf.size(); ++i) {
                if (hf[i] == query_char) {
                    // Higher score for matches at word boundaries
                    if (i == 0 || is_word_boundary(hf[i - 1])) {
                        return 0.9f; // Perfect match at the beginning of a word
                    }
                    best_score = std::max(best_score, 0.8f); // Regular match
                }
            }
            return best_score;
        }

        // case sensitive submatch
        if (auto p = hc.find(nc); p != npos) {
            if (p == 0) {
                if (hc.size() == nc.size())
                    return 1.0f; // Full match
                else
                    return 0.9f; // Prefix match
            }
            else {
                if (is_word_boundary(hf[p - 1]))
                    return 0.9f; // Word start
                else
                    return 0.8f; // Partial inner match
            }
        }

        // case insensitive submatch
        if (auto p = hf.find(nf); p != npos) {
            if (p == 0) {
                if (hf.size() == nf.size())
                    return 0.95f; // Full match
                else
                    return 0.85f; // Prefix match
            }
            else {
                if (is_word_boundary(hf[p - 1]))
                    return 0.85f; // Word start
                else
                    return 0.75f; // Partial inner match
            }
        }

        return 0.0f;
    }
};

template <typename T> struct search_scored_item {
    T ref;
    searcher::score max_score;
    searcher::score sum_score;
};

template <typename T> struct search_sorter : private std::vector<search_scored_item<T>> {
    using item_type = search_scored_item<T>;
    using base_vector = std::vector<item_type>;

    using base_vector::begin;
    using base_vector::end;

    void put(T&& v, searcher::score max_score, searcher::score sum_score)
    {
        auto item = search_scored_item{std::move(v), max_score, sum_score};
        auto pos = std::lower_bound(base_vector::begin(), base_vector::end(), item,
            [](item_type const& a, item_type const& b) {
                if (a.max_score > b.max_score)
                    return true;
                if (a.max_score < b.max_score)
                    return false;
                if (a.sum_score > b.sum_score)
                    return true;
                if (a.sum_score < b.sum_score)
                    return false;
                return false;
            });
        base_vector::insert(pos, std::move(item));
    }
};

} // namespace strings