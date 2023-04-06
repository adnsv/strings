#pragma once

#include "format_locale.hpp"
#include "marshal_traits.hpp"
#include <string>
#include <string_view>
#include <utility>
#include <cassert>

namespace strings {

template <detail::supported_format_arg... Ts>
auto format_ex(char fp_decimal, std::string_view spec, Ts&&... args) -> std::string
{
    auto curr_index = 0;

    auto ret = std::string{};
    ret.reserve(spec.size());

    constexpr auto arg_count = sizeof...(args);
    auto t = std::forward_as_tuple(args...);

    constexpr std::size_t buffer_size = 256;

    auto vfmt = [&ret, fp_decimal](auto const& v, fmt::arg const& arg_fmt) {
        using vtype = std::remove_cvref_t<decltype(v)>;

        if constexpr (formattable<vtype>) {
            char buf[buffer_size];
            auto r = formatter<vtype>{}(buf, buf + buffer_size, v, arg_fmt);
            if (r.ec == std::errc{}) {
                ret.append(buf, r.ptr);
                return std::errc{};
            }
            else
                return r.ec;
        }
        else if constexpr (string_marshalable<vtype>) {
            ret += string_marshaler<vtype>{}(v);
            return std::errc{};
        }
        else if constexpr (chars_marshalable<vtype>) {
            char buf[buffer_size];
            auto r = chars_marshaler<vtype>{}(buf, buf + buffer_size, v);
            if (r.ec == std::errc{}) {
                ret.append(buf, r.ptr);
                return std::errc{};
            }
            else
                return r.ec;
        }
        else if constexpr (std::convertible_to<vtype, std::string_view>) {
            ret += v;
            return std::errc{};
        }
        else if constexpr (std::is_arithmetic_v<vtype>) {
            char pfspec[16];
            if (!fmt::convert_printf_spec<vtype>(arg_fmt, pfspec))
                return std::errc::invalid_argument;
            char buf[128];
            auto n = std::snprintf(buf, 128, pfspec, v);
            assert(n > 0 && n < 128);

            if constexpr (std::floating_point<vtype>) {
                if (fp_decimal != '.')
                    for (auto i = 0; i < n; ++i)
                        if (buf[i] == '.') {
                            buf[i] = fp_decimal;
                            break;
                        }
            }

            ret.append(buf, buf + n);
            return std::errc{};
        }
        else if constexpr (to_char_convertible<vtype>) {
            // custom types that declare std::to_chars
            char buf[buffer_size];
            auto r = std::to_chars(buf, buf + buffer_size, v);
            if (r.ec == std::errc{}) {
                ret.append(buf, r.ptr);
                return std::errc{};
            }
            else
                return r.ec;
        }
        return std::errc::not_supported;
    };

    auto ec = fmt::parse_spec(
        spec.data(), spec.data() + spec.size(), //
        [&](char const* first, char const* last) {
            // handle string segment between arguments
            ret.append(first, last);
            return std::errc{};
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

    if (ec == std::errc{})
        return ret;
    
    return "#ERRFMT";
}

template <detail::supported_format_arg... Ts>
auto format(std::string_view spec, Ts&&... args) -> std::string
{
    return format_ex(user_decimal, spec, std::forward<Ts>(args)...);
}

} // namespace strings