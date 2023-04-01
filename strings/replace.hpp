#pragma once

#include <initializer_list>
#include <string>
#include <string_view>

namespace strings {

// replace - replaces one specified wildcard with new_content
inline auto replace(std::string_view tmpl, std::string_view wildcard, std::string_view new_content)
    -> std::string
{
    auto ret = std::string{tmpl};
    if (auto pos = ret.find(wildcard); pos != std::string::npos) {
        ret.replace(pos, wildcard.size(), new_content);
    }
    return ret;
}

struct replacement_pair_view {
    std::string_view wildcard;
    std::string_view new_content;
};

// replace_all - replaces all occurences of the wildcards in a string with the
// matching replacement content
inline auto replace_all(
    std::string_view tmpl, std::initializer_list<replacement_pair_view> replacements) -> std::string
{
    auto find = [&](std::size_t start_pos) {
        auto pair_it = replacements.end();
        auto best = tmpl.size();
        for (auto it = replacements.begin(); it != replacements.end(); ++it)
            if (auto p = tmpl.find(it->wildcard, start_pos); p != std::string_view::npos)
                if (p < best) {
                    pair_it = it;
                    best = p;
                }
        return std::make_pair(best, pair_it);
    };

    auto ret = std::string{};
    auto start = std::size_t(0);
    while (true) {
        auto [pos, rep] = find(start);
        ret += tmpl.substr(start, pos - start);
        if (rep == replacements.end())
            break;
        ret += rep->new_content;
        start = pos + rep->wildcard.size();
    }

    return ret;
}

} // namespace strings
