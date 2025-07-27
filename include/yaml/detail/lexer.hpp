#pragma once

// buckle up buttercup, this is where the real parsing magic happens
// and by magic i mean actual computer science instead of regex vomit

#include "types.hpp"
#include <array>
#include <cctype>
#include <string_view>
#include <variant>

namespace yaml::ct::detail
{

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

        // Changed: tokenize now takes the string directly instead of storing it
        constexpr auto tokenize(std::string_view input) noexcept -> std::variant<token_array<MaxTokens>, yaml::ct::error_code>
        {
            token_array<MaxTokens> tokens{};
            std::size_t token_count = 0;
            state s{};

            while (!at_end(input, s) && token_count < MaxTokens)
            {
                skip_whitespace_and_comments(input, s);

                if (at_end(input, s))
                    break;

                auto token_result = next_token(input, s);
                if (std::holds_alternative<yaml::ct::error_code>(token_result))
                {
                    return std::get<yaml::ct::error_code>(token_result);
                }

                tokens[token_count++] = std::get<token>(token_result);
            }

            // add eof token because we're civilized
            if (token_count < MaxTokens)
            {
                tokens[token_count] = token{token_type::eof, {}, s.line, s.column};
            }

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

        static constexpr auto peek_next(std::string_view input, const state &s) noexcept -> char
        {
            return (s.position + 1 >= input.size()) ? '\0' : input[s.position + 1];
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

        static constexpr auto skip_whitespace_and_comments(std::string_view input, state &s) noexcept -> void
        {
            while (!at_end(input, s))
            {
                char c = peek(input, s);

                if (c == ' ' || c == '\t' || c == '\n')
                {
                    advance(input, s);
                }
                else if (c == '#')
                {
                    // skip comment to end of line
                    while (!at_end(input, s) && peek(input, s) != '\n')
                    {
                        advance(input, s);
                    }
                }
                else
                {
                    break;
                }
            }
        }

        static constexpr auto next_token(std::string_view input, state &s) noexcept -> std::variant<token, yaml::ct::error_code>
        {
            char c = peek(input, s);
            std::size_t start_line = s.line;
            std::size_t start_column = s.column;

            switch (c)
            {
            case '\n':
                advance(input, s);
                return token{token_type::newline, "\n", start_line, start_column};

            case '-':
                if (peek_next(input, s) == '-' && s.position + 2 < input.size() && input[s.position + 2] == '-')
                {
                    // document start
                    advance(input, s);
                    advance(input, s);
                    advance(input, s);
                    return token{token_type::document_start, "---", start_line, start_column};
                }
                else if (peek_next(input, s) == ' ' || peek_next(input, s) == '\t' || peek_next(input, s) == '\n')
                {
                    // sequence entry
                    advance(input, s);
                    return token{token_type::sequence_entry, "-", start_line, start_column};
                }
                break;

            case '.':
                if (peek_next(input, s) == '.' && s.position + 2 < input.size() && input[s.position + 2] == '.')
                {
                    // document end
                    advance(input, s);
                    advance(input, s);
                    advance(input, s);
                    return token{token_type::document_end, "...", start_line, start_column};
                }
                break;

            case ':':
                advance(input, s);
                return token{token_type::mapping_key, ":", start_line, start_column};

            case '[':
                advance(input, s);
                return token{token_type::sequence_start, "[", start_line, start_column};

            case ']':
                advance(input, s);
                return token{token_type::sequence_end, "]", start_line, start_column};

            case '{':
                advance(input, s);
                return token{token_type::mapping_start, "{", start_line, start_column};

            case '}':
                advance(input, s);
                return token{token_type::mapping_end, "}", start_line, start_column};

            case ',':
                advance(input, s);
                return token{token_type::string_literal, ",", start_line, start_column};

            case '"':
                return parse_quoted_string(input, s, '"');

            case '\'':
                return parse_quoted_string(input, s, '\'');

            case '&':
                return parse_anchor(input, s);

            case '*':
                return parse_alias(input, s);

            case '!':
                return parse_tag(input, s);

            case '|':
                advance(input, s);
                return token{token_type::literal_string, "|", start_line, start_column};

            case '>':
                advance(input, s);
                return token{token_type::folded_string, ">", start_line, start_column};

            default:
                if (is_digit(c) || c == '-' || c == '+')
                {
                    return parse_number(input, s);
                }
                else if (is_alpha(c) || c == '_')
                {
                    return parse_identifier(input, s);
                }
                break;
            }

            return yaml::ct::error_code::unexpected_token;
        }

        static constexpr auto parse_quoted_string(std::string_view input, state &s, char quote) noexcept -> std::variant<token, yaml::ct::error_code>
        {
            std::size_t start_line = s.line;
            std::size_t start_column = s.column;
            std::size_t start_pos = s.position;

            advance(input, s); // skip opening quote

            while (!at_end(input, s) && peek(input, s) != quote)
            {
                if (peek(input, s) == '\\')
                {
                    advance(input, s); // skip backslash
                    if (!at_end(input, s))
                        advance(input, s); // skip escaped character
                }
                else
                {
                    advance(input, s);
                }
            }

            if (at_end(input, s))
            {
                return yaml::ct::error_code::unterminated_string;
            }

            advance(input, s); // skip closing quote

            std::string_view value = input.substr(start_pos, s.position - start_pos);
            return token{token_type::quoted_string, value, start_line, start_column};
        }

        static constexpr auto parse_number(std::string_view input, state &s) noexcept -> std::variant<token, yaml::ct::error_code>
        {
            std::size_t start_line = s.line;
            std::size_t start_column = s.column;
            std::size_t start_pos = s.position;

            // handle sign
            if (peek(input, s) == '-' || peek(input, s) == '+')
            {
                advance(input, s);
            }

            // parse digits
            while (!at_end(input, s) && is_digit(peek(input, s)))
            {
                advance(input, s);
            }

            bool is_float = false;

            // handle decimal point
            if (!at_end(input, s) && peek(input, s) == '.')
            {
                is_float = true;
                advance(input, s);
                while (!at_end(input, s) && is_digit(peek(input, s)))
                {
                    advance(input, s);
                }
            }

            // handle scientific notation
            if (!at_end(input, s) && (peek(input, s) == 'e' || peek(input, s) == 'E'))
            {
                is_float = true;
                advance(input, s);
                if (!at_end(input, s) && (peek(input, s) == '+' || peek(input, s) == '-'))
                {
                    advance(input, s);
                }
                while (!at_end(input, s) && is_digit(peek(input, s)))
                {
                    advance(input, s);
                }
            }

            std::string_view value = input.substr(start_pos, s.position - start_pos);
            token_type type = is_float ? token_type::float_literal : token_type::integer_literal;
            return token{type, value, start_line, start_column};
        }

        static constexpr auto parse_identifier(std::string_view input, state &s) noexcept -> std::variant<token, yaml::ct::error_code>
        {
            std::size_t start_line = s.line;
            std::size_t start_column = s.column;
            std::size_t start_pos = s.position;

            while (!at_end(input, s) && (is_alnum(peek(input, s)) || peek(input, s) == '_' || peek(input, s) == '-'))
            {
                advance(input, s);
            }

            std::string_view value = input.substr(start_pos, s.position - start_pos);

            // check for special keywords
            if (value == "true" || value == "false")
            {
                return token{token_type::boolean_literal, value, start_line, start_column};
            }
            else if (value == "null" || value == "~")
            {
                return token{token_type::null_literal, value, start_line, start_column};
            }

            return token{token_type::string_literal, value, start_line, start_column};
        }

        static constexpr auto parse_anchor(std::string_view input, state &s) noexcept -> std::variant<token, yaml::ct::error_code>
        {
            std::size_t start_line = s.line;
            std::size_t start_column = s.column;
            std::size_t start_pos = s.position;

            advance(input, s); // skip &

            while (!at_end(input, s) && (is_alnum(peek(input, s)) || peek(input, s) == '_' || peek(input, s) == '-'))
            {
                advance(input, s);
            }

            std::string_view value = input.substr(start_pos, s.position - start_pos);
            return token{token_type::anchor, value, start_line, start_column};
        }

        static constexpr auto parse_alias(std::string_view input, state &s) noexcept -> std::variant<token, yaml::ct::error_code>
        {
            std::size_t start_line = s.line;
            std::size_t start_column = s.column;
            std::size_t start_pos = s.position;

            advance(input, s); // skip *

            while (!at_end(input, s) && (is_alnum(peek(input, s)) || peek(input, s) == '_' || peek(input, s) == '-'))
            {
                advance(input, s);
            }

            std::string_view value = input.substr(start_pos, s.position - start_pos);
            return token{token_type::alias, value, start_line, start_column};
        }

        static constexpr auto parse_tag(std::string_view input, state &s) noexcept -> std::variant<token, yaml::ct::error_code>
        {
            std::size_t start_line = s.line;
            std::size_t start_column = s.column;
            std::size_t start_pos = s.position;

            advance(input, s); // skip first !
            if (!at_end(input, s) && peek(input, s) == '!')
            {
                advance(input, s); // skip second !
            }

            while (!at_end(input, s) && peek(input, s) != ' ' && peek(input, s) != '\t' && peek(input, s) != '\n')
            {
                advance(input, s);
            }

            std::string_view value = input.substr(start_pos, s.position - start_pos);
            return token{token_type::tag, value, start_line, start_column};
        }
    };

} // namespace yaml::ct::detail