#pragma once

namespace data::detail
{
    constexpr bool is_alpha(char c) noexcept
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    constexpr bool is_digit(char c) noexcept
    {
        return c >= '0' && c <= '9';
    }

    constexpr bool is_alnum(char c) noexcept
    {
        return is_alpha(c) || is_digit(c);
    }

    constexpr bool is_hex(char c) noexcept
    {
        return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    constexpr unsigned hex_value(char c) noexcept
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return 0;
    }

} // namespace data::detail
