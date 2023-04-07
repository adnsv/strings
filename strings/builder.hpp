#pragma once

#include "format_locale.hpp"
#include "format_spec.hpp"
#include "marshal_traits.hpp"
#include "charconv_stubs.hpp"
#include <array>
#include <charconv>
#include <concepts>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace strings {

// writer writes stuff between [first and last)
struct writer {
    writer() noexcept
        : fp_decimal_{user_decimal}
    {
    }
    writer(char* first, char* last) noexcept
        : first_{first}
        , cursor_{first}
        , last_{last}
        , fp_decimal_{user_decimal}
    {
    }

    constexpr writer(char fp_decimal) noexcept
        : fp_decimal_{fp_decimal}
    {
    }
    constexpr writer(char* first, char* last, char fp_decimal) noexcept
        : first_{first}
        , cursor_{first}
        , last_{last}
        , fp_decimal_{fp_decimal}
    {
    }

    constexpr auto empty() const noexcept { return cursor_ == first_; }
    constexpr auto capacity() const noexcept { return size_t(last_ - first_); }
    constexpr auto size() const noexcept { return size_t(cursor_ - first_); }
    constexpr auto remaining() const noexcept { return size_t(last_ - cursor_); }

    constexpr operator const std::string_view() const noexcept { return {first_, size()}; }
    constexpr auto string_view() const -> std::string_view { return {first_, size()}; }
    auto string() const -> std::string { return std::string{first_, size()}; }

    constexpr auto clear() -> writer&;

    constexpr auto write_codeunit(char codeunit) -> std::errc;
    constexpr auto write(std::string_view sv) -> std::errc;

    template <typename T>
    requires(!std::same_as<T, std::string_view> && std::convertible_to<T, std::string_view> && !marshalable<T>)
    constexpr auto write(T const& s) -> std::errc;

    template <typename T, typename... Args>
    requires(to_chars_convertible<T, Args...> && !marshalable<T>)
    constexpr auto write(T const& v, Args&&... args) -> std::errc;

    template <typename T, typename... Args>
    requires marshalable<T, Args...>
    constexpr auto write(T const& value, Args&&... args) -> std::errc;

    template <detail::supported_format_arg... Ts>
    constexpr auto format(std::string_view spec, Ts&&... values) -> std::errc;

protected:
    char* cursor_ = nullptr;
    char* first_ = nullptr;
    char* last_ = nullptr;
    char fp_decimal_ = '.';

    template <detail::supported_format_arg T> constexpr auto vfmt(T const& v, fmt::arg const&) -> std::errc;

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

// zwriter extends writer with automatic zero termination.
//
// Use with functions that require zero terminated strings.
//
struct zwriter : public writer {
    constexpr zwriter(char fp_decimal) noexcept
        : writer(fp_decimal)
    {
    }
    constexpr zwriter(char* first, char* last, char fp_decimal) noexcept
        : writer{first, last - 1, fp_decimal}
    {
    }

    zwriter() noexcept
        : writer()
    {
    }
    zwriter(char* first, char* last) noexcept
        : writer{first, last - 1}
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

    builder()
    requires(StorageCapacity != RuntimeCapacity)
        : zwriter{buf_, buf_ + StorageCapacity}
    {
    }

    builder(std::size_t capacity)
    requires(StorageCapacity == RuntimeCapacity)
        : zwriter{}
        , buf_(capacity + 1)
    {
        first_ = buf_.data();
        last_ = first_ + capacity;
        cursor_ = first_;
    }

    constexpr builder(char fp_decimal)
    requires(StorageCapacity != RuntimeCapacity)
        : zwriter{buf_, buf_ + StorageCapacity, fp_decimal}
    {
    }

    constexpr builder(std::size_t capacity, char fp_decimal)
    requires(StorageCapacity == RuntimeCapacity)
        : zwriter{fp_decimal}
        , buf_(capacity + 1)
    {
        first_ = buf_.data();
        last_ = first_ + capacity;
        cursor_ = first_;
    }

private:
    storage_type buf_;
};

constexpr auto writer::clear() -> writer&
{
    cursor_ = first_;
    return *this;
}

constexpr auto writer::write_codeunit(char codeunit) -> std::errc
{
    if (cursor_ == last_)
        return std::errc::value_too_large;
    *cursor_++ = codeunit;
    return std::errc{};
}

constexpr auto writer::write(std::string_view sv) -> std::errc
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

template <typename T>
requires(!std::same_as<T, std::string_view> && std::convertible_to<T, std::string_view> && !marshalable<T>)
constexpr auto writer::write(T const& s) -> std::errc
{
    return write(std::string_view(s));
}

template <typename T, typename... Args>
requires(to_chars_convertible<T, Args...> && !marshalable<T>)
constexpr auto writer::write(T const& v, Args&&... args) -> std::errc
{
    constexpr auto nargs = sizeof...(Args);

    if constexpr (std::floating_point<T> && (nargs == 0)) {
        auto const spec = sizeof(T) >= 8 ? "%.16g" : "%.7g";
        auto const n = std::snprintf(cursor_, last_ - cursor_, spec, v);

        if (n <= 0)
            return std::errc::value_too_large;

        if (fp_decimal_ != '.')
            for (auto i = 0; i < n; ++i)
                if (cursor_[i] == '.') {
                    cursor_[i] = fp_decimal_;
                    break;
                }

        cursor_ += n;
        return std::errc{};
    }
    else {
        // integral, and other types that support std::to_chars
        return check(std::to_chars(cursor_, last_, v, static_cast<Args&&>(args)...));
    }
}

template <typename T, typename... Args>
requires marshalable<T, Args...>
constexpr auto writer::write(T const& value, Args&&... args) -> std::errc
{
    if constexpr (chars_marshalable<T, Args...>) {
        auto m = chars_marshaler<T>{};
        return check(m(cursor_, last_, value, std::forward<Args...>(args)...));
    }
    else if constexpr (string_marshalable<T, Args...>) {
        auto m = string_marshaler<T>{};
        return write(m(value, std::forward<Args...>(args)...));
    }
    else
        return std::errc::not_supported;
}

template <detail::supported_format_arg T> constexpr auto writer::vfmt(T const& v, fmt::arg const& a) -> std::errc
{
    if constexpr (formattable<T>) {
        auto f = formatter<T>{};
        auto [ptr, ec] = f(cursor_, last_, v, a);
        if (ec != std::errc{})
            return ec;
        cursor_ = ptr;
    }
    else if constexpr (marshalable<T>) {
        return write(v);
    }
    else if constexpr (std::convertible_to<T, std::string_view>) {
        if (a.type != ' ' && a.type != 's')
            return std::errc::invalid_argument;
        else
            return write(v);
    }
    else if constexpr (std::integral<T>) {
        char pfspec[16];
        if (!fmt::convert_printf_spec<T>(a, pfspec))
            return std::errc::invalid_argument;
        auto const nmax = last_ - cursor_;
        auto n = std::snprintf(cursor_, nmax, pfspec, v);
        if (n <= 0 || n >= nmax)
            return std::errc::value_too_large;
        cursor_ += n;
        return std::errc{};
    }
    else if constexpr (std::floating_point<T>) {
        if (auto fpc = std::fpclassify(v); fpc == FP_ZERO) {
            write_codeunit('0');
            return std::errc{};
        }
        char pfspec[16];
        if (!fmt::convert_printf_spec<T>(a, pfspec))
            return std::errc::invalid_argument;
        auto const nmax = last_ - cursor_;
        auto n = std::snprintf(cursor_, nmax, pfspec, v);
        if (n <= 0 || n >= nmax)
            return std::errc::value_too_large;

        if (fp_decimal_ != '.')
            for (auto i = 0; i < n; ++i)
                if (cursor_[i] == '.') {
                    cursor_[i] = fp_decimal_;
                    break;
                }

        cursor_ += n;
        return std::errc{};
    }
    return std::errc::not_supported;
}

template <detail::supported_format_arg... Ts>
constexpr auto writer::format(std::string_view spec, Ts&&... args) -> std::errc
{
    auto curr_index = 0;
    constexpr auto arg_count = sizeof...(args);
    auto t = std::forward_as_tuple(args...);

    return fmt::parse_spec(
        spec.data(), spec.data() + spec.size(), //
        [&](char const* first, char const* last) {
            // handle string segment between arguments
            return write(std::string_view{first, std::size_t(last - first)});
        },
        [&](int arg_index, bool arg_dflt, fmt::arg const& arg_fmt) {
            // handle argument
            if (arg_index >= 0)
                curr_index = arg_index;

            switch (curr_index) {
            case 0:
                if constexpr (arg_count < 1)
                    return std::errc::invalid_argument;
                else
                    vfmt(std::get<0>(t), arg_fmt);
                break;
            case 1:
                if constexpr (arg_count < 2)
                    return std::errc::invalid_argument;
                else
                    vfmt(std::get<1>(t), arg_fmt);
                break;
            case 2:
                if constexpr (arg_count < 3)
                    return std::errc::invalid_argument;
                else
                    vfmt(std::get<2>(t), arg_fmt);
                break;
            case 3:
                if constexpr (arg_count < 4)
                    return std::errc::invalid_argument;
                else
                    vfmt(std::get<3>(t), arg_fmt);
                break;
            case 4:
                if constexpr (arg_count < 5)
                    return std::errc::invalid_argument;
                else
                    vfmt(std::get<4>(t), arg_fmt);
                break;
            case 5:
                if constexpr (arg_count < 6)
                    return std::errc::invalid_argument;
                else
                    vfmt(std::get<5>(t), arg_fmt);
                break;
            case 6:
                if constexpr (arg_count < 7)
                    return std::errc::invalid_argument;
                else
                    vfmt(std::get<6>(t), arg_fmt);
                break;
            case 7:
                if constexpr (arg_count < 8)
                    return std::errc::invalid_argument;
                else
                    vfmt(std::get<7>(t), arg_fmt);
                break;
            case 8:
                if constexpr (arg_count < 9)
                    return std::errc::invalid_argument;
                else
                    vfmt(std::get<8>(t), arg_fmt);
                break;
            case 9:
                if constexpr (arg_count < 10)
                    return std::errc::invalid_argument;
                else
                    vfmt(std::get<9>(t), arg_fmt);
                break;

            default:
                return std::errc::invalid_argument;
            }
            ++curr_index;

            return std::errc{};
        });
}

} // namespace strings
