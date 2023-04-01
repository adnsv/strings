#pragma once

namespace strings::ascii {

// is_alpha(c) := c in ['A'..'Z', 'a'..'z']
constexpr auto is_alpha(unsigned c) -> bool { return (c | 32) - 'a' < 26; }

// is_upper_alpha(c) := c in ['A'..'Z']
constexpr auto is_upper_alpha(unsigned c) -> bool { return c - 'A' < 26; }

// is_lower_alpha(c) := c in ['a'..'z']
constexpr auto is_lower_alpha(unsigned c) -> bool { return c - 'a' < 26; }

// lower(c) := is_upper_alpha(c) ? c + 'a' - 'A' : c
constexpr auto lower(unsigned c) -> unsigned { return is_upper_alpha(c) ? c + 'a' - 'A' : c; }

// upper(c) := is_lower_alpha(c) ? c + 'a' - 'A' : c
constexpr auto upper(unsigned c) -> unsigned { return is_lower_alpha(c) ? c + 'A' - 'a' : c; }

// decimal returns 0..9 for c in ['0'..'9']
// for all other values, returns a value >= 10
constexpr auto decimal(unsigned c) -> unsigned { return unsigned(c) - unsigned('0'); }

// decimal returns 0..15 for c in ['0'..'9', 'A'..'F', 'a'..'f']
// for all other values, returns a value >= 10
constexpr auto hex(unsigned c) -> unsigned
{
    c -= unsigned('0');
    if (c < 10)
        return c;
    c -= unsigned('A' - '0');
    if (c < 6)
        return c + 10;
    c -= unsigned('a' - 'A');
    return c < 6 ? c + 10 : unsigned(-1);
};

} // namespace strings::ascii