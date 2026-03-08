#pragma once

// Compile-time JSON lexer — tokenizes JSON input into a fixed-size token array

#include <immutable_data/detail/types.hpp>
#include <array>
#include <string_view>
#include <variant>

namespace data::json::detail
{

    using namespace data::detail;

    template <std::size_t MaxTokens = 1024>
    class lexer
    {
    public:
        struct state
        {
            std::size_t position{0};
            std::size_t line{1};
            std::size_t column{1};
        };

        constexpr lexer() noexcept = default;

        constexpr auto tokenize(std::string_view input) noexcept -> std::variant<token_array<MaxTokens>, data::parse_error>
        {
            token_array<MaxTokens> tokens{};
            std::size_t token_count = 0;
            state s{};

            while (!at_end(input, s) && token_count < MaxTokens)
            {
                skip_whitespace(input, s);

                if (at_end(input, s))
                    break;

                auto token_result = next_token(input, s);
                if (std::holds_alternative<data::parse_error>(token_result))
                    return std::get<data::parse_error>(token_result);

                tokens[token_count++] = std::get<token>(token_result);
            }

            if (token_count < MaxTokens)
                tokens[token_count] = token{token_type::eof, {}, s.line, s.column};

            return tokens;
        }

    private:
        static constexpr auto at_end(std::string_view input, const state &s) noexcept -> bool
        {
            return s.position >= input.size();
        }

        static constexpr auto peek(std::string_view input, const state &s) noexcept -> char
        {
            return at_end(input, s) ? '\0' : input[s.position];
        }

        static constexpr auto advance(std::string_view input, state &s) noexcept -> char
        {
            if (at_end(input, s))
                return '\0';
            char c = input[s.position++];
            if (c == '\n')
            {
                ++s.line;
                s.column = 1;
            }
            else
            {
                ++s.column;
            }
            return c;
        }

        static constexpr auto skip_whitespace(std::string_view input, state &s) noexcept -> void
        {
            while (!at_end(input, s))
            {
                char c = peek(input, s);
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
                    advance(input, s);
                else
                    break;
            }
        }

        static constexpr auto next_token(std::string_view input, state &s) noexcept -> std::variant<token, data::parse_error>
        {
            char c = peek(input, s);
            std::size_t start_line = s.line;
            std::size_t start_column = s.column;

            switch (c)
            {
            case '{':
                advance(input, s);
                return token{token_type::mapping_start, "{", start_line, start_column};
            case '}':
                advance(input, s);
                return token{token_type::mapping_end, "}", start_line, start_column};
            case '[':
                advance(input, s);
                return token{token_type::sequence_start, "[", start_line, start_column};
            case ']':
                advance(input, s);
                return token{token_type::sequence_end, "]", start_line, start_column};
            case ':':
                advance(input, s);
                return token{token_type::mapping_key, ":", start_line, start_column};
            case ',':
                advance(input, s);
                return token{token_type::comma, ",", start_line, start_column};
            case '"':
                return parse_string(input, s);
            case 't':
            case 'f':
                return parse_bool(input, s);
            case 'n':
                return parse_null(input, s);
            default:
                if (c == '-' || is_digit(c))
                    return parse_number(input, s);
                break;
            }

            return data::parse_error{data::error_code::unexpected_token, s.line, s.column};
        }

        static constexpr auto parse_string(std::string_view input, state &s) noexcept -> std::variant<token, data::parse_error>
        {
            std::size_t start_line = s.line;
            std::size_t start_column = s.column;
            std::size_t start_pos = s.position;

            advance(input, s); // skip opening "
            while (!at_end(input, s) && peek(input, s) != '"')
            {
                if (peek(input, s) == '\\')
                {
                    advance(input, s);
                    if (at_end(input, s))
                        return data::parse_error{data::error_code::unterminated_string, start_line, start_column};
                    advance(input, s); // skip escaped char
                }
                else
                {
                    advance(input, s);
                }
            }

            if (at_end(input, s))
                return data::parse_error{data::error_code::unterminated_string, start_line, start_column};

            advance(input, s); // skip closing "
            std::string_view value = input.substr(start_pos, s.position - start_pos);
            return token{token_type::quoted_string, value, start_line, start_column};
        }

        static constexpr auto parse_number(std::string_view input, state &s) noexcept -> std::variant<token, data::parse_error>
        {
            std::size_t start_line = s.line;
            std::size_t start_column = s.column;
            std::size_t start_pos = s.position;

            if (peek(input, s) == '-')
                advance(input, s);

            while (!at_end(input, s) && is_digit(peek(input, s)))
                advance(input, s);

            bool is_float = false;
            if (!at_end(input, s) && peek(input, s) == '.')
            {
                is_float = true;
                advance(input, s);
                while (!at_end(input, s) && is_digit(peek(input, s)))
                    advance(input, s);
            }
            if (!at_end(input, s) && (peek(input, s) == 'e' || peek(input, s) == 'E'))
            {
                is_float = true;
                advance(input, s);
                if (!at_end(input, s) && (peek(input, s) == '+' || peek(input, s) == '-'))
                    advance(input, s);
                while (!at_end(input, s) && is_digit(peek(input, s)))
                    advance(input, s);
            }

            std::string_view value = input.substr(start_pos, s.position - start_pos);
            token_type type = is_float ? token_type::float_literal : token_type::integer_literal;
            return token{type, value, start_line, start_column};
        }

        static constexpr auto parse_bool(std::string_view input, state &s) noexcept -> std::variant<token, data::parse_error>
        {
            std::size_t start_line = s.line;
            std::size_t start_column = s.column;
            std::size_t start_pos = s.position;

            if (peek(input, s) == 't')
            {
                // expect "true"
                for (int i = 0; i < 4 && !at_end(input, s); ++i)
                    advance(input, s);
                std::string_view value = input.substr(start_pos, s.position - start_pos);
                if (value == "true")
                    return token{token_type::boolean_literal, value, start_line, start_column};
            }
            else
            {
                // expect "false"
                for (int i = 0; i < 5 && !at_end(input, s); ++i)
                    advance(input, s);
                std::string_view value = input.substr(start_pos, s.position - start_pos);
                if (value == "false")
                    return token{token_type::boolean_literal, value, start_line, start_column};
            }

            return data::parse_error{data::error_code::unexpected_token, start_line, start_column};
        }

        static constexpr auto parse_null(std::string_view input, state &s) noexcept -> std::variant<token, data::parse_error>
        {
            std::size_t start_line = s.line;
            std::size_t start_column = s.column;
            std::size_t start_pos = s.position;

            for (int i = 0; i < 4 && !at_end(input, s); ++i)
                advance(input, s);

            std::string_view value = input.substr(start_pos, s.position - start_pos);
            if (value == "null")
                return token{token_type::null_literal, value, start_line, start_column};

            return data::parse_error{data::error_code::unexpected_token, start_line, start_column};
        }
    };

} // namespace data::json::detail
