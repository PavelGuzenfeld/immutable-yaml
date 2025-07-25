#pragma once

// because apparently you need me to teach you how to write modern c++
// this is what a real compile-time yaml parser looks like, not whatever
// stackoverflow garbage you were about to copy-paste

#include "detail/lexer.hpp"
#include "detail/parser.hpp"
#include "detail/types.hpp"

#include <array>
#include <optional>
#include <string_view>
#include <variant>

namespace yaml::ct
{

    // import the types we need from detail namespace
    using detail::document;
    using error_code = yaml::ct::error_code;

    // because you probably don't know what a proper result type looks like
    template <typename T>
    using result = std::variant<T, error_code>;

    // the main event - your compile-time yaml parser that doesn't suck
    template <std::size_t N>
    constexpr auto parse(const char (&yaml_str)[N]) noexcept -> result<document>
    {
        // wow, look at that - actual input validation instead of just hoping for the best
        if constexpr (N <= 1)
        {
            return error_code::invalid_syntax;
        }

        constexpr std::string_view yaml_view{yaml_str, N - 1}; // remove null terminator because we're not animals

        auto lexer = detail::lexer<1024>{yaml_view}; // explicit template parameter because you keep fucking this up
        auto tokens_result = lexer.tokenize();

        if (std::holds_alternative<error_code>(tokens_result))
        {
            return std::get<error_code>(tokens_result);
        }

        auto const &tokens = std::get<detail::token_array<1024>>(tokens_result); // explicit template parameter
        auto parser = detail::parser<1024>{tokens};                              // explicit template parameter

        return parser.parse_document();
    }

    // convenience function for when you're too lazy to handle errors properly
    template <std::size_t N>
    constexpr auto parse_or_throw(const char (&yaml_str)[N]) -> document
    {
        constexpr auto result = parse(yaml_str);
        static_assert(std::holds_alternative<document>(result),
                      "yaml parsing failed at compile time - fix your yaml, genius");
        return std::get<document>(result);
    }

    // validation function because apparently you need compile-time guarantees
    template <std::size_t N>
    constexpr auto is_valid(const char (&yaml_str)[N]) noexcept -> bool
    {
        constexpr auto result = parse(yaml_str);
        return std::holds_alternative<document>(result);
    }

} // namespace yaml::ct

// convenience macro because i know you love macros (you shouldn't)
#define YAML_CT(str) yaml::ct::parse_or_throw(str)
#define YAML_CT_VALID(str) yaml::ct::is_valid(str)