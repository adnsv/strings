#pragma once

#include "fmtspec.hpp"
#include "fp.hpp"
#include <array>
#include <charconv>
#include <concepts>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "charconv_stubs.hpp"

namespace strings {

inline auto string_view_to_chars(char* first, char* last, std::string_view s) -> std::to_chars_result
{
    if (first + s.size() > last)
        return {last, std::errc::value_too_large};
    for (auto c : s)
        *first++ = c;
    return {first, std::errc{}};
};

namespace writer {

template <typename T, typename... Args> struct traits;

template <typename T, typename... Args>
concept writable = requires(
    char* buf, T const& v, Args&&... args) { traits<T, Args...>{}(buf, buf, v, static_cast<Args&&>(args)...); };

template <typename T>
concept supported_type = std::integral<std::remove_cvref_t<T>> || std::floating_point<std::remove_cvref_t<T>> ||
                         writable<std::remove_cvref_t<T>>;
} // namespace writer

// writer_base writes stuff between [first and last)
struct writer_base {
    constexpr writer_base() noexcept = default;

    constexpr writer_base(char* first, char* last) noexcept
        : first_{first}
        , cursor_{first}
        , last_{last}
    {
    }

    constexpr auto empty() const noexcept { return cursor_ == first_; }
    constexpr auto capacity() const noexcept { return size_t(last_ - first_); }
    constexpr auto size() const noexcept { return size_t(cursor_ - first_); }
    constexpr auto remaining() const noexcept { return size_t(last_ - cursor_); }

    constexpr auto clear() noexcept -> writer_base&
    {
        cursor_ = first_;
        return *this;
    }

    constexpr operator const std::string_view() const noexcept { return {first_, size()}; }
    constexpr auto string_view() const -> std::string_view { return {first_, size()}; }
    auto string() const -> std::string { return std::string{first_, size()}; }

    constexpr auto write(std::string_view const& sv) -> std::errc
    {
        if (sv.empty())
            return std::errc{};
        else if (cursor_ + sv.size() <= last_) {
            for (auto cp : sv)
                *cursor_++ = cp;
            return std::errc{};
        }
        else {
            for (auto cp : sv.substr(0, last_ - cursor_))
                *cursor_++ = cp;
            return std::errc::value_too_large;
        }
    }

    constexpr auto write_codeunit(char codeunit) -> std::errc
    {
        if (cursor_ == last_)
            return std::errc::value_too_large;
        *cursor_++ = codeunit;
        return std::errc{};
    }

    template <typename T>
    requires(!std::same_as<T, std::string_view> && std::convertible_to<T, std::string_view> && !writer::writable<T>)
    constexpr auto write(T const& s)
    {
        return write(std::string_view(s));
    }

    template <typename T, typename... Args>
    requires writer::writable<T, Args...>
    auto write(T const& value, Args&&... args)
    {
        using traits = writer::traits<T, Args...>;
        return check(traits{}(cursor_, last_, value, static_cast<Args&&>(args)...));
    }

    template <typename T, typename... Args>
    requires(!writer::writable<T, Args...> && !std::convertible_to<T, std::string_view>)
    auto write(T const& value, Args&&... args)
    {
        using namespace std;
        return check(to_chars(cursor_, last_, value, static_cast<Args&&>(args)...));
    }

    template <typename... Ts> constexpr auto format(std::string_view spec, Ts&&... values) -> std::errc;

protected:
    char* cursor_ = nullptr;
    char* first_ = nullptr;
    char* last_ = nullptr;

    char fp_decimal = '.';

    constexpr void mk_printf_spec(
        fmtarg const& fmt, char const* sizespec, char t, char* buf, int override_precision = -1);

    template <typename T> constexpr auto vfmt(T const& v, fmtarg const& fmt) -> std::errc;

private:
    constexpr auto check(std::to_chars_result const& cr) -> std::errc
    {
        // sync-up with the to_chars_result
        if (cr.ec == std::errc{}) {
            cursor_ = cr.ptr;
        }
        return cr.ec;
    }
};

// zwriter extends writer_base with automatic zero termination.
//
// Use with functions that require zero terminated strings.
//
struct zwriter : public writer_base {
    constexpr zwriter() noexcept {}

    constexpr zwriter(char* first, char* last) noexcept
        : writer_base{first, last - 1}
    {
    }

    constexpr auto c_str() noexcept
    {
        *cursor_ = char{0};
        return first_;
    }

    constexpr operator char const*() noexcept { return c_str(); }
};

static constexpr std::size_t RuntimeCapacity = 0;

template <std::size_t StorageCapacity = RuntimeCapacity> struct builder : public zwriter {

    using storage_type = // choose vector or static array backend
        typename std::conditional_t<StorageCapacity == RuntimeCapacity,
            std::vector<char>,      // heap-allocated storage
            char[StorageCapacity]>; // stack-allocated storage

    constexpr builder()
    requires(StorageCapacity != RuntimeCapacity)
        : zwriter{buf_, buf_ + StorageCapacity}
    {
    }

    constexpr builder(std::size_t capacity)
    requires(StorageCapacity == RuntimeCapacity)
        : buf_(capacity + 1)
    {
        first_ = buf_.data();
        last_ = first_ + capacity;
        cursor_ = first_;
    }

private:
    storage_type buf_;
};

constexpr void writer_base::mk_printf_spec(
    fmtarg const& fmt, char const* size_spec, char t, char* buf, int override_precision)
{
    *buf++ = '%';
    if (fmt.sign != '-')
        *buf++ = fmt.sign;
    if (fmt.alternate_form)
        *buf++ = '#';
    if (fmt.zero_padding)
        *buf++ = '0';

    auto const w = fmt.width;
    auto const p = override_precision >= 0 ? override_precision : fmt.precision;

    if (w > 0 && w < 100) {
        if (w >= 10)
            *buf++ = '0' + w / 10;
        *buf++ = '0' + w % 10;
    }

    if (p >= 0 && p < 100) {
        *buf++ = '.';
        if (p >= 10)
            *buf++ = '0' + p / 10;
        *buf++ = '0' + p % 10;
    }

    if (size_spec)
        while (*size_spec)
            *buf++ = *size_spec++;

    *buf++ = t;
    *buf++ = 0;
}

template <typename T> constexpr auto writer_base::vfmt(T const& v, fmtarg const& fmt) -> std::errc
{
    if constexpr (std::convertible_to<T, std::string_view>) {
        if (fmt.type != ' ' && fmt.type != 's')
            return std::errc::invalid_argument;
        else
            return write(v);
    }
    else if constexpr (std::integral<T>) {
        char pfspec[32];
        if constexpr (std::signed_integral<T>) {
            char const* sizespec = sizeof(T) == sizeof(long long)     ? "ll"
                                   : sizeof(T) == sizeof(long)        ? "l"
                                   : sizeof(T) == sizeof(short)       ? "h"
                                   : sizeof(T) == sizeof(signed char) ? "hh"
                                                                      : nullptr;
            mk_printf_spec(fmt, nullptr, 'd', pfspec);
        }
        else {
            char const* sizespec = sizeof(T) == sizeof(unsigned long long) ? "ll"
                                   : sizeof(T) == sizeof(unsigned long)    ? "l"
                                   : sizeof(T) == sizeof(unsigned short)   ? "h"
                                   : sizeof(T) == sizeof(unsigned char)    ? "hh"
                                                                           : nullptr;
            mk_printf_spec(fmt, sizespec, 'u', pfspec);
        }

        auto n = std::snprintf(cursor_, last_ - cursor_, pfspec, v);
        if (n <= 0)
            return std::errc::value_too_large;
        cursor_ += n;
        return std::errc{};
    }
    else if constexpr (std::floating_point<T>) {
        if (auto fpc = std::fpclassify(v); fpc == FP_ZERO) {
            write_codeunit('0');
            return std::errc{};
        }

        auto const auto_precision = fmt.type == ' ';
        auto const precision = auto_precision ? std::numeric_limits<T>::max_digits10 - 1 : fmt.precision;
        auto const t = fmt.type == ' ' ? 'g' : fmt.type;
        if (std::string_view{"eEfFgGaA"}.find(t) == std::string_view::npos)
            return std::errc::not_supported;

        char const* sizespec = sizeof(T) == sizeof(long double) ? "L" : nullptr;

        char pfspec[32];
        mk_printf_spec(fmt, sizespec, t, pfspec, precision);
        auto n = std::snprintf(cursor_, last_ - cursor_, pfspec, v);
        if (n <= 0)
            return std::errc::value_too_large;

        if (fp_decimal != '.')
            for (auto i = 0; i < n; ++i)
                if (cursor_[i] == '.') {
                    cursor_[i] = fp_decimal;
                    break;
                }

        cursor_ += n;
        return std::errc{};
    }
    return std::errc::not_supported;
}

template <typename... Ts> constexpr auto writer_base::format(std::string_view spec, Ts&&... args) -> std::errc
{
    auto arg_index = std::size_t{0};
    constexpr auto arg_count = sizeof...(args);

    auto t = std::forward_as_tuple(args...);

    while (!spec.empty()) {
        auto p = std::size_t{0};
        auto const n = spec.size();
        while (p < n)
            if (spec[p] == '{' || spec[p] == '}')
                break;
            else
                ++p;

        if (p) {
            if (auto ec = write(spec.substr(0, p)); ec != std::errc{})
                return ec;
            spec.remove_prefix(p);
            if (spec.empty())
                return std::errc{};
        }

        if (spec.size() < 2)
            return std::errc::invalid_argument;

        if (spec[0] == '}') {
            if (spec[1] != '}')
                return std::errc::invalid_argument;
            if (auto ec = write_codeunit('}'); ec != std::errc{})
                return ec;
            spec.remove_prefix(2);
            continue;
        }

        if (spec[1] == '{') {
            if (auto ec = write_codeunit('{'); ec != std::errc{})
                return ec;
            spec.remove_prefix(2);
            continue;
        }

        spec.remove_prefix(1);
        auto argspec = fmtarg{};

        // have an opening curly brace
        if (spec[0] == '}') {
            // default formatting
            spec.remove_prefix(1);
        }
        else {
            // fmtlib-like spec
            auto [pos, ec] = parse_fmt_arg(spec.data(), spec.data() + spec.size(), arg_index, argspec);
            if (ec != std::errc{})
                return ec;
            spec.remove_prefix(pos - spec.data());
        }

        switch (arg_index) {
        case 0:
            if constexpr (arg_count < 1)
                return std::errc::invalid_argument;
            else
                vfmt(std::get<0>(t), argspec);
            break;
        case 1:
            if constexpr (arg_count < 2)
                return std::errc::invalid_argument;
            else
                vfmt(std::get<1>(t), argspec);
            break;
        case 2:
            if constexpr (arg_count < 3)
                return std::errc::invalid_argument;
            else
                vfmt(std::get<2>(t), argspec);
            break;
        case 3:
            if constexpr (arg_count < 4)
                return std::errc::invalid_argument;
            else
                vfmt(std::get<3>(t), argspec);
            break;
        case 4:
            if constexpr (arg_count < 5)
                return std::errc::invalid_argument;
            else
                vfmt(std::get<4>(t), argspec);
            break;
        case 5:
            if constexpr (arg_count < 6)
                return std::errc::invalid_argument;
            else
                vfmt(std::get<5>(t), argspec);
            break;
        case 6:
            if constexpr (arg_count < 7)
                return std::errc::invalid_argument;
            else
                vfmt(std::get<6>(t), argspec);
            break;
        case 7:
            if constexpr (arg_count < 8)
                return std::errc::invalid_argument;
            else
                vfmt(std::get<7>(t), argspec);
            break;
        case 8:
            if constexpr (arg_count < 9)
                return std::errc::invalid_argument;
            else
                vfmt(std::get<8>(t), argspec);
            break;
        case 9:
            if constexpr (arg_count < 10)
                return std::errc::invalid_argument;
            else
                vfmt(std::get<9>(t), argspec);
            break;

        default:
            return std::errc::invalid_argument;
        }
        ++arg_index;
    }
    return std::errc{};
}

} // namespace strings
