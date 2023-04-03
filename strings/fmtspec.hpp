#pragma once

#include <charconv>
#include <string_view>

namespace strings {

struct fmtarg {
    int index = -1;
    char align = ' ';            // "<" | ">" | "^" (" " if unspecified)
    char sign = '-';             // "+" | "-" | " "
    bool alternate_form = false; // "#"
    bool zero_padding = false;   // "0"
    int width = -1;
    int precision = -1;
    bool use_locale = false;
    char type = ' ';
};

constexpr auto parse_fmt_arg(char const* first, char const* last, fmtarg& arg) -> std::from_chars_result
{
    if (first == last)
        return {first, std::errc::invalid_argument};
    if (*first == '}')
        return {first + 1, std::errc{}};

    constexpr auto supported_types = std::string_view{"sdbBxXeEfFgG"};

    auto read_int = [&]() -> int {
        if (first == last)
            return -1;
        if (auto c = *first; c >= '0' && c <= '9') {
            auto v = int(c - '0');
            ++first;
            while (first != last)
                if (auto c = *first; c >= '0' && c <= '9') {
                    v = v * 10 + int(c - '0');
                    ++first;
                }
                else
                    break;
            return v;
        }
        return -1;
    };

    // arg_id, only integers are supported at this point
    arg.index = read_int();
    if (arg.index >= 0 && first == last)
        return {first, std::errc::invalid_argument};
    if (*first == ':') {
        // read format spec
        ++first;
        if (first == last)
            return {first, std::errc::invalid_argument};

        if (*first == '<' || *first == '>' || *first == '^') {
            arg.align = *first++;
            if (first == last)
                return {first, std::errc::invalid_argument};
        }

        if (*first == ' ' || *first == '+' || *first == '-') {
            arg.sign = *first++;
            if (first == last)
                return {first, std::errc::invalid_argument};
        }

        if (*first == '#') {
            arg.alternate_form = true;
            ++first;
            if (first == last)
                return {first, std::errc::invalid_argument};
        }

        if (*first == '0') {
            arg.zero_padding = true;
            ++first;
            if (first == last)
                return {first, std::errc::invalid_argument};
        }

        arg.width = read_int(); // only numeric widths are currently supported
        if (arg.width >= 0 && first == last)
            return {first, std::errc::invalid_argument};

        if (*first == '.') {
            ++first;
            arg.precision = read_int(); // only numeric precisions are currently supported
            if (arg.precision < 0)
                return {first, std::errc::invalid_argument};
        }

        if (*first == 'L') {
            arg.use_locale = true;
            ++first;
            if (first == last)
                return {first, std::errc::invalid_argument};
        }

        if (supported_types.find(*first) != std::string_view::npos) {
            arg.type = *first++;
            if (first == last)
                return {first, std::errc::invalid_argument};
        }
    }
    if (*first == '}')
        return {first + 1, std::errc{}};
    else
        return {first, std::errc::invalid_argument};
}

} // namespace strings