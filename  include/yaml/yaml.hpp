#pragma once
#include "detail/lexer.hpp"
#include "detail/parser.hpp"
#include "detail/types.hpp"

#include <array>
#include <optional>
#include <string_view>
#include <variant>
#include <cstdint>

namespace yaml::ct
{

    enum class [[nodiscard]] error_code : std::uint8_t
    {
        none = 0,
        invalid_syntax,
        unexpected_token,
        invalid_indentation,
        unterminated_string,
        invalid_escape_sequence,
        duplicate_key,
        invalid_document_start,
        invalid_document_end,
        cyclic_reference,
        unsupported_feature
    };

    template <typename T>
    using result = std::variant<T, error_code>;

    template <std::size_t N>
    constexpr auto parse(const char (&yaml_str)[N]) noexcept -> result<document>
    {
        if constexpr (N <= 1)
        {
            return error_code::invalid_syntax;
        }

        constexpr std::string_view yaml_view{yaml_str, N - 1}; // remove null terminator because we're not animals

        auto lexer = detail::lexer{yaml_view};
        auto tokens_result = lexer.tokenize();

        if (std::holds_alternative<error_code>(tokens_result))
        {
            return std::get<error_code>(tokens_result);
        }

        auto const &tokens = std::get<detail::token_array>(tokens_result);
        auto parser = detail::parser{tokens};

        return parser.parse_document();
    }

    template <std::size_t N>
    constexpr auto parse_or_throw(const char (&yaml_str)[N]) -> document
    {
        constexpr auto result = parse(yaml_str);
        static_assert(std::holds_alternative<document>(result),
                      "yaml parsing failed at compile time - fix your yaml, genius");
        return std::get<document>(result);
    }

    template <std::size_t N>
    constexpr auto is_valid(const char (&yaml_str)[N]) noexcept -> bool
    {
        constexpr auto result = parse(yaml_str);
        return std::holds_alternative<document>(result);
    }

} // namespace yaml::ct

#define YAML_CT(str) yaml::ct::parse_or_throw(str)
#define YAML_CT_VALID(str) yaml::ct::is_valid(str)