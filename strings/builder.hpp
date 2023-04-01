#pragma once

#include "fp.hpp"
#include <charconv>
#include <concepts>
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
    // constexpr operator std::string_view() noexcept { return {_first, size()}; }

    constexpr auto string_view() const -> std::string_view { return {first_, size()}; }
    auto string() const -> std::string { return std::string{first_, size()}; }
    // constexpr auto string_view() -> std::string_view { return {_first, size()}; }

    constexpr auto write(std::string_view const& sv)
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
    requires(!std::same_as<T, std::string_view> && std::convertible_to<T, std::string_view> && !writer::writable<T>)
    constexpr auto write(T const& s)
    {
        return write(std::string_view(s));
    }

    template <typename T>
    requires(std::integral<T> && !writer::writable<T>)
    auto write(T const value)
    {
        return check(std::to_chars(cursor_, last_, value));
    }

    template <typename T>
    requires(std::integral<T> && !writer::writable<T, int>)
    auto write(T const value, int base)
    {
        return check(std::to_chars(cursor_, last_, value, base));
    }

    template <typename T>
    requires(std::floating_point<T> && !writer::writable<T>)
    auto write(T const value)
    {
        return check(std::to_chars(cursor_, last_, value));
    }

    template <typename T>
    requires(std::floating_point<T> && !writer::writable<T, std::chars_format>)
    auto write(T const value, std::chars_format fmt)
    {
        return check(std::to_chars(cursor_, last_, value, fmt));
    }

    template <typename T>
    requires(std::floating_point<T> && !writer::writable<T, std::chars_format, int>)
    auto write(T const value, std::chars_format fmt, int precision)
    {
        return check(std::to_chars(cursor_, last_, value, fmt, precision));
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

    template <typename... Ts> void print(Ts&&... values) { (write(static_cast<Ts&&>(values)), ...); }

    template <typename T>
    requires(writer::supported_type<T> || std::convertible_to<T, std::string_view>)
    auto operator<<(T&& v) -> writer_base&
    {
        write(static_cast<T&&>(v));
        return *this;
    }

protected:
    char* cursor_ = nullptr;
    char* first_ = nullptr;
    char* last_ = nullptr;

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

} // namespace strings
