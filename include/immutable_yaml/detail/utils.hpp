#pragma once
#include <array>   // for token storage
#include <cstdint> // for std::uint8_t

namespace yaml::ct::detail
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

} // namespace yaml::ct::detail