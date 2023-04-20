#pragma once

#include <charconv>
#include <string_view>

namespace strings::fmt {

struct arg {
    char align = ' ';          // (ignored) "<" | ">" | "^" (" " if unspecified)
    char sign = '-';           // "+" | "-" | " "
    bool zero_padding = false; // "0"
    int width = 0;
    int precision = -1;
    bool alternate_form = false; // "#"
    bool use_locale = false;
    char type = ' ';
};

constexpr auto parse_arg(char const* first, char const* last, int& idx, bool& dflt, fmt::arg& arg)
    -> std::from_chars_result
{
    if (last - first < 2 || *first != '{')
        return {first, std::errc::invalid_argument};

    ++first;

    if (*first == '}') {
        dflt = true;
        return {first + 1, std::errc{}};
    }

    constexpr auto supported_types = std::string_view{"sdxXeEfFgG"}; // todo: o, b B

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
    if (auto i = read_int(); i >= 0) {
        idx = std::size_t(i);
        if (first == last)
            return {first, std::errc::invalid_argument};
    }

    if (*first == ':') {
        // read format spec
        dflt = false;
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

        if (auto v = read_int(); v >= 0) {
            arg.width = v; // only numeric widths are currently supported
            if (first == last)
                return {first, std::errc::invalid_argument};
        }

        if (*first == '.') {
            ++first;
            arg.precision = read_int(); // only numeric precisions are currently supported
            if (arg.precision < 0 || first == last)
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
    else {
        dflt = true;
    }
    if (*first == '}')
        return {first + 1, std::errc{}};
    else
        return {first, std::errc::invalid_argument};
}

template <typename PutStr, typename PutArg>
constexpr auto parse_spec(char const* first, char const* last, PutStr&& putstr, PutArg&& putarg) -> std::errc
{
    while (first != last) {
        auto curr = first;
        while (curr != last && (*curr != '{' && *curr != '}'))
            ++curr;

        if (curr != first) {
            std::errc ec = putstr(first, curr);
            if (ec != std::errc{})
                return ec;
            first = curr;
            continue;
        }

        if (first + 1 == last) {
            return std::errc::invalid_argument;
        }

        if (*first == '}') {
            if (first[1] != '}')
                return std::errc::invalid_argument;
            auto ec = putstr(first, first + 1);
            if (ec != std::errc{})
                return ec;
            first += 2;
            continue;
        }

        if (first[1] == '{') {
            auto ec = putstr(first, first + 1);
            if (ec != std::errc{})
                return ec;
            first += 2;
            continue;
        }

        auto arg_index = -1;
        auto arg_fmt = fmt::arg{};
        auto arg_dflt = true;

        auto [pos, ec] = parse_arg(first, last, arg_index, arg_dflt, arg_fmt);
        if (ec != std::errc{})
            return ec;
        first = pos;
        ec = putarg(arg_index, arg_dflt, arg_fmt);
        if (ec != std::errc{})
            return ec;
    }
    return std::errc{};
}

template <typename T> constexpr auto convert_printf_spec(fmt::arg const& a, char* pfspec) -> bool
{
    *pfspec++ = '%';
    if (a.sign != '-')
        *pfspec++ = a.sign;
    if (a.alternate_form)
        *pfspec++ = '#';
    if (a.zero_padding)
        *pfspec++ = '0';

    if (a.width > 0 && a.width < 100) {
        if (a.width >= 10)
            *pfspec++ = '0' + a.width / 10;
        *pfspec++ = '0' + a.width % 10;
    }

    auto t = a.type;
    auto precision = a.precision;
    if constexpr (std::floating_point<T>) {
        if (t == ' ') {
            // auto precision
            t = 'g';
            if (precision < 0)
                precision = std::numeric_limits<T>::max_digits10 - 3;
        }
    }

    if (precision >= 0 && precision < 100) {
        *pfspec++ = '.';
        if (precision >= 10)
            *pfspec++ = '0' + precision / 10;
        *pfspec++ = '0' + precision % 10;
    }

    if constexpr (std::integral<T>) {
        // size specifier for integral inputs
        if constexpr (sizeof(T) == sizeof(long long)) {
            *pfspec++ = 'l';
            *pfspec++ = 'l';
        }
        else if constexpr (sizeof(T) == sizeof(long)) {
            *pfspec++ = 'l';
        }
        else if constexpr (sizeof(T) == sizeof(short)) {
            *pfspec++ = 'h';
        }
        else if constexpr (sizeof(T) == sizeof(char)) {
            *pfspec++ = 'h';
            *pfspec++ = 'h';
        }
        if constexpr (std::signed_integral<T>) {
            if (t == ' ')
                t = 'd';
        }
        else {
            if (t == ' ' || t == 'd')
                t = 'u';
        }
        *pfspec++ = t;
        *pfspec++ = 0;
        return true;
    }
    else if constexpr (std::floating_point<T>) {
        if constexpr (std::same_as<std::remove_cv_t<T>, long double>)
            *pfspec++ = 'L';
        else if constexpr (std::same_as<std::remove_cv_t<T>, double>)
            *pfspec++ = 'l';
        *pfspec++ = t;
        *pfspec++ = 0;
        return true;
    }
    else
        return false;
}

} // namespace strings::fmt