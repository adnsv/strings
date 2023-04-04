#pragma once

#include "codepoint.hpp"
#include <concepts>
#include <ranges>

namespace strings::utf {

constexpr auto u8 = encoding::utf8;
constexpr auto u16 = encoding::utf16;
constexpr auto u32 = encoding::utf32;

// encoding_of
//
// - utf8 for char, char8_t
// - utf16 for char16_t, wchar_t if sizeof(wchar_t) == 2
// - utf32 for char32_t, wchar_t if sizeof(wchar_t) == 4
//
template <codeunit U> constexpr auto encoding_of = static_cast<encoding>(sizeof(U));

template <encoding Enc> constexpr auto codeunit_size_of = static_cast<std::size_t>(Enc);

template <typename Iter, typename Sentinel>
constexpr auto u8_to_codepoint(Iter first, Sentinel last, codepoint& output, unexpected_policy utp) -> Iter
{
    using cp = codepoint::carrier_type;
    using cu = std::make_unsigned_t<std::iter_value_t<Iter>>;
    static_assert(sizeof(cu) == 1);

    output = errcp::insufficient;

    if (first == last) // insufficient_input
        return first;

    auto c0 = cp(cu(*first++));

    if (c0 < 0b10000000u) {
        // 1-byte sequence [U+0000..U+007F]
        output = codepoint{c0};
        return first;
    }

    auto is_trail = [](cu c) { return (c & 0b11000000u) == 0b10000000u; };

    if (c0 < 0b11000000u) {
        output = errcp::unexpected;
        if (utp == unexpected_policy::consume_all)
            while (first != last && is_trail(cp(cu(*first))))
                ++first;
        return first;
    }

    cp c2, c3, c4, c5, c6;
    constexpr cp trail_mask = 0b00111111;

    auto is_starter = [](cp c) { return (c & 0b11000000u) != 0b10000000u; };

    // byte #2
    if (first == last) // insufficient_input
        return first;
    c2 = cp(cu(*first));
    if (is_starter(c2)) {
        output = errcp::incomplete;
        return first;
    }
    ++first;
    if (c0 < 0b11100000u) {
        // 2-byte sequence [U+0080..U+07FF]
        output = codepoint{
            ((c0 & 0b00011111) << 06) | // #1
            (c2 & trail_mask)           // #2
        };
        if (output.value < 0x80)
            output = errcp::overlong;
        return first;
    }

    // byte #3
    if (first == last) // insufficient_input
        return first;
    c3 = cp(cu(*first));
    if (is_starter(c3)) {
        output = errcp::incomplete;
        return first;
    }
    ++first;
    if (c0 < 0b11110000u) {
        // 3-byte sequence [U+00800..U+FFFF]
        output = codepoint{
            ((c0 & 0b00001111) << 12) | // #1
            ((c2 & trail_mask) << 06) | // #2
            (c3 & trail_mask)           // #3
        };
        if (output.value < 0x800)
            output = errcp::overlong;
        return first;
    }

    // byte #4
    if (first == last) // insufficient_input
        return first;
    c4 = cp(cu(*first));
    if (is_starter(c4)) {
        output = errcp::incomplete;
        return first;
    }
    ++first;
    if (c0 < 0b11111000u) {
        // 4-byte sequence [U+00010000..U+001FFFFF]
        output = codepoint{
            ((c0 & 0b00000111) << 18) | // #1
            ((c2 & trail_mask) << 12) | // #2
            ((c3 & trail_mask) << 06) | // #3
            (c4 & trail_mask)           // #4
        };
        if (output.value < 0x00010000)
            output = errcp::overlong;
        return first;
    }

    // byte #5
    if (first == last) // insufficient_input
        return first;
    c5 = cp(cu(*first));
    if (is_starter(c5)) {
        output = errcp::incomplete;
        return first;
    }
    ++first;
    if (c0 < 0b11111100u) {
        // 5-byte sequence [U+00200000..U+03FFFFFF]
        output = codepoint{
            ((c0 & 0b00000011) << 24) | // #1
            ((c2 & trail_mask) << 18) | // #2
            ((c3 & trail_mask) << 12) | // #3
            ((c4 & trail_mask) << 06) | // #4
            (c5 & trail_mask)           // #5
        };
        if (output.value < 0x00200000)
            output = errcp::overlong;
        return first;
    }

    // byte #6
    if (first == last) // insufficient_input
        return first;
    c6 = cp(cu(*first));
    if (is_starter(c6)) {
        output = errcp::incomplete;
        return first;
    }
    ++first;
    // 6-byte sequence [U+04000000..U+7FFFFFFF]
    output = codepoint{
        ((c0 & 0b00000001) << 30) | // #1
        ((c2 & trail_mask) << 24) | // #2
        ((c3 & trail_mask) << 18) | // #3
        ((c4 & trail_mask) << 12) | // #4
        ((c5 & trail_mask) << 06) | // #5
        (c6 & trail_mask)           // #6
    };
    if (output.value < 0x04000000)
        output = errcp::overlong;
    return first;
}

template <typename Iter, typename Sentinel>
constexpr auto u16_to_codepoint(Iter first, Sentinel last, codepoint& output, unexpected_policy utp) -> Iter
{
    using cp = codepoint::carrier_type;
    using cu = std::make_unsigned_t<std::iter_value_t<Iter>>;
    static_assert(sizeof(cu) == 2);

    output = errcp::insufficient;
    if (first == last)
        return first;

    auto c0 = cp(cu(*first++));

    if (!unicode::is_surrogate(codepoint{c0})) {
        output = codepoint{c0};
        return first;
    }

    // must start with a high surrogate:
    if (c0 >= unicode::low_surrogate_first.value) {
        output = errcp::unexpected;
        if (utp == unexpected_policy::consume_all)
            while (first != last && unicode::is_low_surrogate(codepoint{cp(cu(*first))}))
                ++first;
        return first;
    }
    if (first == last) // insufficient_input
        return first;

    // must be followed by a low surrogate
    auto c1 = cp(cu(*first));
    if (!unicode::is_low_surrogate(codepoint{c1})) {
        output = errcp::incomplete;
        return first;
    }

    output = codepoint{(((c0 & 0x3FFu) << 10) | (c1 & 0x3FFu)) + 0x10000};
    ++first;
    return first;
}

template <typename Iter, typename Sentinel>
constexpr auto u32_to_codepoint(Iter first, Sentinel last, codepoint& output, unexpected_policy dummy) -> Iter
{
    using cp = codepoint::carrier_type;
    using cu = std::make_unsigned_t<std::iter_value_t<Iter>>;
    static_assert(sizeof(cu) == 4);

    output = (first == last) ? output = errcp::insufficient : output = codepoint{cp(cu(*first++))};
    return first;
}

template <typename T> void u8_to_codeunits(codepoint const& cp, std::invocable<T> auto put)
{
    static_assert(sizeof(T) == 1);
    auto const c = cp.value; // shortcut
    if (c < 0x80) {
        put(c);
    }
    else if (c < 0x800) {
        put((c >> 6) | 0xc0);
        put((c & 0x3f) | 0x80);
    }
    else if (c < 0x10000) {
        put((c >> 12) | 0xe0);
        put(((c >> 6) & 0x3f) | 0x80);
        put((c & 0x3f) | 0x80);
    }
    else {
        put((c >> 18) | 0xF0);
        put(((c >> 12) & 0x3F) | 0x80);
        put(((c >> 6) & 0x3F) | 0x80);
        put((c & 0x3F) | 0x80);
    }
}

template <typename T> void u16_to_codeunits(codepoint const& cp, std::invocable<T> auto put)
{
    static_assert(sizeof(T) == 2);
    auto c = cp.value; // shortcut
    if (c < 0x10000) {
        put(c);
    }
    else {
        c -= 0x10000;
        put(0xD800 | (c >> 10));
        put(0xDC00 | (c & 0x3FF));
    }
}

template <typename T> void u32_to_codeunits(codepoint const& cp, std::invocable<T> auto put)
{
    static_assert(sizeof(T) == 4);
    put(cp.value);
}

} // namespace strings::utf