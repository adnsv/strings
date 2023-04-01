#pragma once

#include <charconv>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace detail { // internal

#ifdef STRINGS_TOCHARS_FLOAT_STUB
// use stubs until gcc provides full impl of std::to_chars
inline std::to_chars_result to_chars(char* first, char* last, float value)
{
    auto n = std::snprintf(first, last - first, "%f", value);
    if (n <= 0)
        return {last, std::errc::value_too_large};
    else
        return {first + n, std::errc{}};
}
inline std::to_chars_result to_chars(char* first, char* last, double value)
{
    auto n = std::snprintf(first, last - first, "%f", value);
    if (n <= 0)
        return {last, std::errc::value_too_large};
    else
        return {first + n, std::errc{}};
}
#endif
} // namespace detail

namespace strings {

enum class trim {
    nothing,
    frac_zeroes,
};

// writer writes stuff between [first and last)
struct writer {

    using codepoint = char;
    using stringview_type = std::basic_string_view<codepoint>;
    using string_type = std::basic_string<codepoint>;

    constexpr writer() noexcept = default;

    constexpr writer(char* first, char* last) noexcept
        : _first{first}
        , _cursor{first}
        , _last{last}
    {
    }

    constexpr auto empty() const noexcept { return _cursor == _first; }
    constexpr auto capacity() const noexcept { return size_t(_last - _first); }
    constexpr auto size() const noexcept { return size_t(_cursor - _first); }
    constexpr auto remaining() const noexcept { return size_t(_last - _cursor); }

    constexpr void clear() noexcept { _cursor = _first; }

    constexpr operator const stringview_type() const noexcept { return {_first, size()}; }
    constexpr operator stringview_type() noexcept { return {_first, size()}; }

    constexpr auto string_view() const -> stringview_type { return {_first, size()}; }
    constexpr auto string_view() -> stringview_type { return {_first, size()}; }

    constexpr auto write(stringview_type s, bool trim_too_large = true)
    {
        if (!s.empty()) {
            if (_cursor + s.size() > _last) {
                if (trim_too_large)
                    s = s.substr(0, _last - _cursor);
                else
                    return std::errc::value_too_large;
            }
            for (auto cp : s)
                *_cursor++ = cp;
        }
        return std::errc{};
    }

    constexpr auto write(codepoint const* s, bool trim_too_large = true)
    {
        return write(stringview_type{s}, trim_too_large);
    }

    auto write(string_type const& s, bool trim_too_large = true) { return write(stringview_type{s}, trim_too_large); }

    auto write(signed int value) { return check(std::to_chars(_cursor, _last, value)); }
    auto write(unsigned int value) { return check(std::to_chars(_cursor, _last, value)); }
    auto write(signed short value) { return check(std::to_chars(_cursor, _last, value)); }
    auto write(unsigned short value) { return check(std::to_chars(_cursor, _last, value)); }
    auto write(signed long value) { return check(std::to_chars(_cursor, _last, value)); }
    auto write(unsigned long value) { return check(std::to_chars(_cursor, _last, value)); }
    auto write(signed long long value) { return check(std::to_chars(_cursor, _last, value)); }
    auto write(unsigned long long value) { return check(std::to_chars(_cursor, _last, value)); }

    auto write(signed int value, int base) { return check(std::to_chars(_cursor, _last, value, base)); }
    auto write(unsigned int value, int base) { return check(std::to_chars(_cursor, _last, value, base)); }
    auto write(signed short value, int base) { return check(std::to_chars(_cursor, _last, value, base)); }
    auto write(unsigned short value, int base) { return check(std::to_chars(_cursor, _last, value, base)); }
    auto write(signed long value, int base) { return check(std::to_chars(_cursor, _last, value, base)); }
    auto write(unsigned long value, int base) { return check(std::to_chars(_cursor, _last, value, base)); }
    auto write(signed long long value, int base) { return check(std::to_chars(_cursor, _last, value, base)); }
    auto write(unsigned long long value, int base) { return check(std::to_chars(_cursor, _last, value, base)); }

    auto write(float const value, trim t = trim::frac_zeroes)
    {
        using namespace std;
        using namespace detail;
        return check(to_chars(_cursor, _last, value), t);
    }

    auto write(float const value, std::chars_format fmt, trim t = trim::frac_zeroes)
    {
        return check(std::to_chars(_cursor, _last, value, fmt), t);
    }

    auto write(const float value, std::chars_format fmt, int precision, trim t = trim::frac_zeroes)
    {
        return check(std::to_chars(_cursor, _last, value, fmt, precision), t);
    }

    auto write(double const& value, trim t = trim::frac_zeroes) { return check(std::to_chars(_cursor, _last, value)); }

    auto write(double const& value, std::chars_format fmt, trim t = trim::frac_zeroes)
    {
        return check(std::to_chars(_cursor, _last, value, fmt), t);
    }

    auto write(double const& value, std::chars_format fmt, int precision, trim t = trim::frac_zeroes)
    {
        return check(std::to_chars(_cursor, _last, value, fmt, precision), t);
    }

    template <class U> auto operator<<(U const& v) -> writer&
    {
        write(v);
        return *this;
    }

    template <class T, typename... Args> auto print(T const& value, Args&&... args)
    {
        using namespace std;
        using namespace detail;

        auto ec = check(std::to_chars(_cursor, _last, value));
        if (ec != std::errc{})
            return ec;

        for (const auto v : {args...}) {
            ec = write(v);
            if (ec != std::errc{})
                return ec;
        }

        return ec;
    }

protected:
    char* _cursor = nullptr;
    char* _first = nullptr;
    char* _last = nullptr;

private:
    constexpr auto check(std::to_chars_result const& cr) -> std::errc
    {
        // sync-up with the to_chars_result
        if (cr.ec == std::errc{}) {
            _cursor = cr.ptr;
        }
        return cr.ec;
    }
    constexpr auto check(std::to_chars_result const& cr, trim t) -> std::errc
    {
        // sync-up with the to_chars_result
        if (cr.ec != std::errc{})
            return cr.ec;
        auto s = std::string_view{_cursor, _cursor + (cr.ptr - _cursor)};

        auto decimal = s.rfind('.');
        if (decimal == std::string_view::npos)
            decimal = s.rfind(",");
        if (decimal != std::string_view::npos) {
            // make sure we don't trim zeroes after the exponent
            if (s.find_first_of("eE") == std::string_view::npos) {
                while (s.back() == '0')
                    s.remove_suffix(1);
                if (s.back() == '.' || s.back() == ',')
                    s.remove_suffix(1);
                _cursor += s.size();
                return {};
            }
        }
        _cursor = cr.ptr;
        return {};
    }
};

// zwriter extends writer with automatic zero termination.
//
// Use with functions that require zero terminated strings.
//
struct zwriter : public writer {
    constexpr zwriter() noexcept {}

    constexpr zwriter(char* first, char* last) noexcept
        : writer{first, last - 1}
    {
    }

    constexpr auto c_str() noexcept
    {
        *_cursor = char{0};
        return _first;
    }

    constexpr operator const codepoint*() noexcept { return c_str(); }
};

static constexpr std::size_t RuntimeCapacity = 0;

template <std::size_t StorageCapacity = RuntimeCapacity> struct builder : public zwriter {

    using storage_type = // choose vector or static array backend
        typename std::conditional_t<StorageCapacity == RuntimeCapacity,
            std::vector<codepoint>,      // heap-allocated storage
            codepoint[StorageCapacity]>; // stack-allocated storage

    constexpr builder() requires(StorageCapacity != RuntimeCapacity)
        : zwriter{_buf, _buf + StorageCapacity}
    {
    }

    constexpr builder(std::size_t capacity) requires(StorageCapacity == RuntimeCapacity)
        : _buf(capacity + 1)
    {
        _first = _buf.data();
        _last = _first + capacity;
        _cursor = _first;
    }

private:
    storage_type _buf;
};

} // namespace strings
