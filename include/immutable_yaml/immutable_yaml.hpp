#pragma once

#include <immutable_yaml/detail/lexer.hpp>
#include <immutable_yaml/detail/parser.hpp>
#include <immutable_yaml/detail/types.hpp>

#include <array>
#include <optional>
#include <string_view>
#include <variant>

#ifndef YAML_CT_MAX_TOKENS
#define YAML_CT_MAX_TOKENS 1024
#endif

namespace yaml::ct
{

    using detail::document;
    using error_code = yaml::ct::error_code;

    template <typename T>
    using result = std::variant<T, error_code>;

    template <std::size_t N>
    constexpr auto make_yaml_view(const char (&yaml_str)[N]) noexcept -> std::string_view
    {
        if constexpr (N <= 1)
        {
            return std::string_view{};
        }
        else
        {
            return std::string_view{yaml_str, N - 1};
        }
    }

    template <std::size_t N>
    constexpr auto parse(const char (&yaml_str)[N]) noexcept -> result<document>
    {
        if constexpr (N <= 1)
        {
            return error_code::invalid_syntax;
        }

        auto yaml_view = make_yaml_view(yaml_str);

        detail::lexer<YAML_CT_MAX_TOKENS> lexer{};
        auto tokens_result = lexer.tokenize(yaml_view);

        if (std::holds_alternative<error_code>(tokens_result))
        {
            return std::get<error_code>(tokens_result);
        }

        auto const &tokens = std::get<detail::token_array<YAML_CT_MAX_TOKENS>>(tokens_result);
        detail::document doc{};
        auto parser = detail::parser<YAML_CT_MAX_TOKENS>{tokens, doc};

        return parser.parse_document();
    }

    template <std::size_t N>
    constexpr auto parse_or_throw(const char (&yaml_str)[N]) -> document
    {
        auto result = parse(yaml_str);

        if (std::holds_alternative<document>(result))
        {
            return std::get<document>(result);
        }
        else
        {
            return std::get<document>(result);
        }
    }

    template <std::size_t N>
    constexpr auto is_valid(const char (&yaml_str)[N]) noexcept -> bool
    {
        auto result = parse(yaml_str);
        return std::holds_alternative<document>(result);
    }

} // namespace yaml::ct

#define YAML_CT(str) yaml::ct::parse_or_throw(str)
#define YAML_CT_VALID(str) yaml::ct::is_valid(str)
