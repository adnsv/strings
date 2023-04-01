#pragma once

namespace strings::ascii {

// is_alpha(c) := c in ['A'..'Z', 'a'..'z']
constexpr auto is_alpha(unsigned c) -> bool { return (c | 32u) - unsigned('a') < 26u; }

// is_upper_alpha(c) := c in ['A'..'Z']
constexpr auto is_upper_alpha(unsigned c) -> bool { return c - unsigned('A') < 26u; }

// is_lower_alpha(c) := c in ['a'..'z']
constexpr auto is_lower_alpha(unsigned c) -> bool { return c - unsigned('a') < 26u; }

constexpr auto is_decimal(unsigned c) -> bool { return c - unsigned('0') < 10u; }

// lower(c) := is_upper_alpha(c) ? c + 'a' - 'A' : c
constexpr auto lower(unsigned c) -> unsigned { return is_upper_alpha(c) ? c + 'a' - 'A' : c; }

// upper(c) := is_lower_alpha(c) ? c + 'a' - 'A' : c
constexpr auto upper(unsigned c) -> unsigned { return is_lower_alpha(c) ? c + 'A' - 'a' : c; }

// decimal returns 0..9 for c in ['0'..'9']
// for all other values, returns a value >= 10
constexpr auto decimal(unsigned c) -> unsigned {
    c -= unsigned('0'); 
    return c < 10u ? c : unsigned(-1);
}

// decimal returns 0..15 for c in ['0'..'9', 'A'..'F', 'a'..'f']
// for all other values, returns a value >= 10
constexpr auto hex(unsigned c) -> unsigned
{
    c -= unsigned('0');
    if (c < 10u)
        return c;
    c -= unsigned('A' - '0');
    if (c < 6u)
        return c + 10u;
    c -= unsigned('a' - 'A');
    return c < 6u ? c + 10u : unsigned(-1);
};

} // namespace strings::ascii