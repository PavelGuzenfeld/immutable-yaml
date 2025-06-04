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
        constexpr explicit lexer(std::string_view yaml) noexcept
            : input_{yaml} {}

        constexpr auto tokenize() noexcept -> std::variant<token_array<MaxTokens>, yaml::ct::error_code>
        {
            token_array<MaxTokens> tokens{};
            std::size_t token_count = 0;

            while (!at_end() && token_count < MaxTokens)
            {
                skip_whitespace_and_comments();

                if (at_end())
                    break;

                auto token_result = next_token();
                if (std::holds_alternative<yaml::ct::error_code>(token_result))
                {
                    return std::get<yaml::ct::error_code>(token_result);
                }

                tokens[token_count++] = std::get<token>(token_result);
            }

            // add eof token because we're civilized
            if (token_count < MaxTokens)
            {
                tokens[token_count] = token{token_type::eof, {}, line_, column_};
            }

            return tokens;
        }

    private:
        constexpr auto at_end() const noexcept -> bool
        {
            return position_ >= input_.size();
        }

        constexpr auto peek() const noexcept -> char
        {
            return at_end() ? '\0' : input_[position_];
        }

        constexpr auto peek_next() const noexcept -> char
        {
            return (position_ + 1 >= input_.size()) ? '\0' : input_[position_ + 1];
        }

        constexpr auto advance() noexcept -> char
        {
            if (at_end())
                return '\0';

            char c = input_[position_++];
            if (c == '\n')
            {
                ++line_;
                column_ = 1;
            }
            else
            {
                ++column_;
            }
            return c;
        }

        constexpr auto skip_whitespace_and_comments() noexcept -> void
        {
            while (!at_end())
            {
                char c = peek();

                if (c == ' ' || c == '\t' || c == '\n')
                {
                    advance();
                }
                else if (c == '#')
                {
                    // skip comment to end of line
                    while (!at_end() && peek() != '\n')
                    {
                        advance();
                    }
                }
                else
                {
                    break;
                }
            }
        }

        constexpr auto next_token() noexcept -> std::variant<token, yaml::ct::error_code>
        {
            char c = peek();
            std::size_t start_line = line_;
            std::size_t start_column = column_;

            switch (c)
            {
            case '\n':
                advance();
                return token{token_type::newline, "\n", start_line, start_column};

            case '-':
                if (peek_next() == '-' && position_ + 2 < input_.size() && input_[position_ + 2] == '-')
                {
                    // document start
                    advance();
                    advance();
                    advance();
                    return token{token_type::document_start, "---", start_line, start_column};
                }
                else if (peek_next() == ' ' || peek_next() == '\t' || peek_next() == '\n')
                {
                    // sequence entry
                    advance();
                    return token{token_type::sequence_entry, "-", start_line, start_column};
                }
                break;

            case '.':
                if (peek_next() == '.' && position_ + 2 < input_.size() && input_[position_ + 2] == '.')
                {
                    // document end
                    advance();
                    advance();
                    advance();
                    return token{token_type::document_end, "...", start_line, start_column};
                }
                break;

            case ':':
                advance();
                return token{token_type::mapping_key, ":", start_line, start_column};

            case '[':
                advance();
                return token{token_type::sequence_start, "[", start_line, start_column};

            case ']':
                advance();
                return token{token_type::sequence_end, "]", start_line, start_column};

            case '{':
                advance();
                return token{token_type::mapping_start, "{", start_line, start_column};

            case '}':
                advance();
                return token{token_type::mapping_end, "}", start_line, start_column};

            case '"':
                return parse_quoted_string('"');

            case '\'':
                return parse_quoted_string('\'');

            case '&':
                return parse_anchor();

            case '*':
                return parse_alias();

            case '!':
                return parse_tag();

            case '|':
                advance();
                return token{token_type::literal_string, "|", start_line, start_column};

            case '>':
                advance();
                return token{token_type::folded_string, ">", start_line, start_column};

            default:
                if (std::isdigit(c) || c == '-' || c == '+')
                {
                    return parse_number();
                }
                else if (std::isalpha(c) || c == '_')
                {
                    return parse_identifier();
                }
                break;
            }

            return yaml::ct::error_code::unexpected_token;
        }

        constexpr auto parse_quoted_string(char quote) noexcept -> std::variant<token, yaml::ct::error_code>
        {
            std::size_t start_line = line_;
            std::size_t start_column = column_;
            std::size_t start_pos = position_;

            advance(); // skip opening quote

            while (!at_end() && peek() != quote)
            {
                if (peek() == '\\')
                {
                    advance(); // skip backslash
                    if (!at_end())
                        advance(); // skip escaped character
                }
                else
                {
                    advance();
                }
            }

            if (at_end())
            {
                return yaml::ct::error_code::unterminated_string;
            }

            advance(); // skip closing quote

            std::string_view value = input_.substr(start_pos, position_ - start_pos);
            return token{token_type::quoted_string, value, start_line, start_column};
        }

        constexpr auto parse_number() noexcept -> std::variant<token, yaml::ct::error_code>
        {
            std::size_t start_line = line_;
            std::size_t start_column = column_;
            std::size_t start_pos = position_;

            // handle sign
            if (peek() == '-' || peek() == '+')
            {
                advance();
            }

            // parse digits
            while (!at_end() && std::isdigit(peek()))
            {
                advance();
            }

            bool is_float = false;

            // handle decimal point
            if (!at_end() && peek() == '.')
            {
                is_float = true;
                advance();
                while (!at_end() && std::isdigit(peek()))
                {
                    advance();
                }
            }

            // handle scientific notation
            if (!at_end() && (peek() == 'e' || peek() == 'E'))
            {
                is_float = true;
                advance();
                if (!at_end() && (peek() == '+' || peek() == '-'))
                {
                    advance();
                }
                while (!at_end() && std::isdigit(peek()))
                {
                    advance();
                }
            }

            std::string_view value = input_.substr(start_pos, position_ - start_pos);
            token_type type = is_float ? token_type::float_literal : token_type::integer_literal;
            return token{type, value, start_line, start_column};
        }

        constexpr auto parse_identifier() noexcept -> std::variant<token, yaml::ct::error_code>
        {
            std::size_t start_line = line_;
            std::size_t start_column = column_;
            std::size_t start_pos = position_;

            while (!at_end() && (std::isalnum(peek()) || peek() == '_' || peek() == '-'))
            {
                advance();
            }

            std::string_view value = input_.substr(start_pos, position_ - start_pos);

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

        constexpr auto parse_anchor() noexcept -> std::variant<token, yaml::ct::error_code>
        {
            std::size_t start_line = line_;
            std::size_t start_column = column_;
            std::size_t start_pos = position_;

            advance(); // skip &

            while (!at_end() && (std::isalnum(peek()) || peek() == '_' || peek() == '-'))
            {
                advance();
            }

            std::string_view value = input_.substr(start_pos, position_ - start_pos);
            return token{token_type::anchor, value, start_line, start_column};
        }

        constexpr auto parse_alias() noexcept -> std::variant<token, yaml::ct::error_code>
        {
            std::size_t start_line = line_;
            std::size_t start_column = column_;
            std::size_t start_pos = position_;

            advance(); // skip *

            while (!at_end() && (std::isalnum(peek()) || peek() == '_' || peek() == '-'))
            {
                advance();
            }

            std::string_view value = input_.substr(start_pos, position_ - start_pos);
            return token{token_type::alias, value, start_line, start_column};
        }

        constexpr auto parse_tag() noexcept -> std::variant<token, yaml::ct::error_code>
        {
            std::size_t start_line = line_;
            std::size_t start_column = column_;
            std::size_t start_pos = position_;

            advance(); // skip first !
            if (!at_end() && peek() == '!')
            {
                advance(); // skip second !
            }

            while (!at_end() && !std::isspace(peek()))
            {
                advance();
            }

            std::string_view value = input_.substr(start_pos, position_ - start_pos);
            return token{token_type::tag, value, start_line, start_column};
        }

        std::string_view input_;
        std::size_t position_{0};
        std::size_t line_{1};
        std::size_t column_{1};
    };

} // namespace yaml::ct::detail