#pragma once

#include <clocale>

namespace strings {

inline auto get_user_decimal() -> char
{
    auto ret = char('.');
    auto prev = std::setlocale(LC_NUMERIC, "");
    auto ld = std::localeconv();
    if (ld && ld->decimal_point && ld->decimal_point[0] == ',')
        ret = ',';
    std::setlocale(LC_NUMERIC, prev);
    return ret;
}

inline char const user_decimal = get_user_decimal();

}