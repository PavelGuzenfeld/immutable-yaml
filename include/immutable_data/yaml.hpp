#pragma once

#include <immutable_data/detail/yaml_lexer.hpp>
#include <immutable_data/detail/yaml_parser.hpp>
#include <immutable_data/detail/types.hpp>

#include <string_view>
#include <variant>

#ifndef DATA_CT_MAX_TOKENS
#define DATA_CT_MAX_TOKENS 1024
#endif

namespace data::yaml
{

    using data::detail::document;
    using data::error_code;

    template <typename T>
    using result = std::variant<T, error_code>;

    template <std::size_t N>
    constexpr auto parse(const char (&str)[N]) noexcept -> result<document>
    {
        if constexpr (N <= 1)
            return error_code::invalid_syntax;

        std::string_view view{str, N - 1};
        detail::lexer<DATA_CT_MAX_TOKENS> lex{};
        auto tokens_result = lex.tokenize(view);

        if (std::holds_alternative<error_code>(tokens_result))
            return std::get<error_code>(tokens_result);

        auto const &tokens = std::get<data::detail::token_array<DATA_CT_MAX_TOKENS>>(tokens_result);
        data::detail::document doc{};
        auto parser = detail::parser<DATA_CT_MAX_TOKENS>{tokens, doc};
        return parser.parse_document();
    }

    template <std::size_t N>
    constexpr auto parse_or_throw(const char (&str)[N]) -> document
    {
        auto r = parse(str);
        if (std::holds_alternative<document>(r))
            return std::get<document>(r);
        throw "YAML parse error";
    }

    template <std::size_t N>
    constexpr auto is_valid(const char (&str)[N]) noexcept -> bool
    {
        auto r = parse(str);
        return std::holds_alternative<document>(r);
    }

} // namespace data::yaml
