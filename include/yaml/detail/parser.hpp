#pragma once

// here comes the actual parsing logic that will make your brain hurt
// but at least it's correct, unlike whatever you were going to write

#include "lexer.hpp"
#include "types.hpp"
#include <cctype>
#include <variant>

namespace yaml::ct::detail
{

    template <std::size_t MaxTokens = 1024>
    class parser
    {
    public:
        constexpr explicit parser(const token_array<MaxTokens> &tokens) noexcept
            : tokens_{tokens} {}

        constexpr auto parse_document() noexcept -> std::variant<document, yaml::ct::error_code>
        {
            // handle optional document start
            if (current_token().type_ == token_type::document_start)
            {
                advance();
            }

            auto value_result = parse_value();
            if (std::holds_alternative<yaml::ct::error_code>(value_result))
            {
                return std::get<yaml::ct::error_code>(value_result);
            }

            return document{std::get<yaml_value>(value_result)};
        }

    private:
        constexpr auto current_token() const noexcept -> const token &
        {
            return tokens_[position_];
        }

        constexpr auto advance() noexcept -> void
        {
            if (position_ < MaxTokens - 1)
            {
                ++position_;
            }
        }

        constexpr auto parse_value() noexcept -> std::variant<yaml_value, yaml::ct::error_code>
        {
            const auto &tok = current_token();

            switch (tok.type_)
            {
            case token_type::null_literal:
                advance();
                return null_t{};

            case token_type::boolean_literal:
                advance();
                return tok.value_ == "true";

            case token_type::integer_literal:
                return parse_integer();

            case token_type::float_literal:
                return parse_float();

            case token_type::string_literal:
            case token_type::quoted_string:
                return parse_string();

            case token_type::sequence_start:
                return parse_flow_sequence();

            case token_type::mapping_start:
                return parse_flow_mapping();

            case token_type::sequence_entry:
                return parse_block_sequence();

            default:
                // try to parse as block mapping
                return parse_block_mapping();
            }
        }

        constexpr auto parse_integer() noexcept -> std::variant<yaml_value, yaml::ct::error_code>
        {
            const auto &tok = current_token();
            advance();

            // simple integer parsing - in real life you'd want proper overflow checking
            integer result = 0;
            bool negative = false;
            std::size_t i = 0;

            if (!tok.value_.empty() && tok.value_[0] == '-')
            {
                negative = true;
                i = 1;
            }
            else if (!tok.value_.empty() && tok.value_[0] == '+')
            {
                i = 1;
            }

            for (; i < tok.value_.size(); ++i)
            {
                if (is_digit(tok.value_[i]))
                {
                    result = result * 10 + (tok.value_[i] - '0');
                }
            }

            return negative ? -result : result;
        }

        constexpr auto parse_float() noexcept -> std::variant<yaml_value, yaml::ct::error_code>
        {
            const auto &tok = current_token();
            advance();

            // simplified float parsing - real implementation would be more robust
            floating result = 0.0;
            floating decimal_place = 0.1;
            bool negative = false;
            bool in_decimal = false;
            std::size_t i = 0;

            if (!tok.value_.empty() && tok.value_[0] == '-')
            {
                negative = true;
                i = 1;
            }
            else if (!tok.value_.empty() && tok.value_[0] == '+')
            {
                i = 1;
            }

            for (; i < tok.value_.size(); ++i)
            {
                char c = tok.value_[i];
                if (is_digit(c))
                {
                    if (in_decimal)
                    {
                        result += (c - '0') * decimal_place;
                        decimal_place *= 0.1;
                    }
                    else
                    {
                        result = result * 10.0 + (c - '0');
                    }
                }
                else if (c == '.')
                {
                    in_decimal = true;
                }
                else if (c == 'e' || c == 'E')
                {
                    // skip scientific notation for now
                    break;
                }
            }

            return negative ? -result : result;
        }

        constexpr auto parse_string() noexcept -> std::variant<yaml_value, yaml::ct::error_code>
        {
            const auto &tok = current_token();
            advance();

            if (tok.type_ == token_type::quoted_string)
            {
                // remove quotes
                std::string_view content = tok.value_;
                if (content.size() >= 2)
                {
                    content = content.substr(1, content.size() - 2);
                }
                return string_storage<256>{content};
            }
            else
            {
                return string_storage<256>{tok.value_};
            }
        }

        constexpr auto parse_flow_sequence() noexcept -> std::variant<yaml_value, yaml::ct::error_code>
        {
            advance(); // skip [

            sequence_impl seq{};

            while (current_token().type_ != token_type::sequence_end &&
                   current_token().type_ != token_type::eof)
            {

                auto value_result = parse_value();
                if (std::holds_alternative<yaml::ct::error_code>(value_result))
                {
                    return std::get<yaml::ct::error_code>(value_result);
                }

                if (!seq.push_back(std::get<yaml_value>(value_result)))
                {
                    return yaml::ct::error_code::invalid_syntax; // sequence full
                }

                // handle comma separation (simplified)
                if (current_token().type_ == token_type::string_literal &&
                    current_token().value_ == ",")
                {
                    advance();
                }
            }

            if (current_token().type_ == token_type::sequence_end)
            {
                advance(); // skip ]
            }

            return seq;
        }

        constexpr auto parse_flow_mapping() noexcept -> std::variant<yaml_value, yaml::ct::error_code>
        {
            advance(); // skip {

            mapping_impl map{};

            while (current_token().type_ != token_type::mapping_end &&
                   current_token().type_ != token_type::eof)
            {

                // parse key
                auto key_result = parse_string();
                if (std::holds_alternative<yaml::ct::error_code>(key_result))
                {
                    return std::get<yaml::ct::error_code>(key_result);
                }

                auto key = std::get<string_storage<256>>(std::get<yaml_value>(key_result));

                // expect colon
                if (current_token().type_ != token_type::mapping_key)
                {
                    return yaml::ct::error_code::unexpected_token;
                }
                advance();

                // parse value
                auto value_result = parse_value();
                if (std::holds_alternative<yaml::ct::error_code>(value_result))
                {
                    return std::get<yaml::ct::error_code>(value_result);
                }

                if (!map.insert(std::move(key), std::get<yaml_value>(value_result)))
                {
                    return yaml::ct::error_code::duplicate_key;
                }

                // handle comma separation (simplified)
                if (current_token().type_ == token_type::string_literal &&
                    current_token().value_ == ",")
                {
                    advance();
                }
            }

            if (current_token().type_ == token_type::mapping_end)
            {
                advance(); // skip }
            }

            return map;
        }

        constexpr auto parse_block_sequence() noexcept -> std::variant<yaml_value, yaml::ct::error_code>
        {
            sequence_impl seq{};

            while (current_token().type_ == token_type::sequence_entry)
            {
                advance(); // skip -

                auto value_result = parse_value();
                if (std::holds_alternative<yaml::ct::error_code>(value_result))
                {
                    return std::get<yaml::ct::error_code>(value_result);
                }

                if (!seq.push_back(std::get<yaml_value>(value_result)))
                {
                    return yaml::ct::error_code::invalid_syntax; // sequence full
                }
            }

            return seq;
        }

        constexpr auto parse_block_mapping() noexcept -> std::variant<yaml_value, yaml::ct::error_code>
        {
            mapping_impl map{};

            while (current_token().type_ == token_type::string_literal ||
                   current_token().type_ == token_type::quoted_string)
            {

                // parse key
                auto key_result = parse_string();
                if (std::holds_alternative<yaml::ct::error_code>(key_result))
                {
                    return std::get<yaml::ct::error_code>(key_result);
                }

                auto key = std::get<string_storage<256>>(std::get<yaml_value>(key_result));

                // expect colon
                if (current_token().type_ != token_type::mapping_key)
                {
                    return yaml::ct::error_code::unexpected_token;
                }
                advance();

                // parse value
                auto value_result = parse_value();
                if (std::holds_alternative<yaml::ct::error_code>(value_result))
                {
                    return std::get<yaml::ct::error_code>(value_result);
                }

                if (!map.insert(std::move(key), std::get<yaml_value>(value_result)))
                {
                    return yaml::ct::error_code::duplicate_key;
                }
            }

            return map;
        }

        const token_array<MaxTokens> &tokens_;
        std::size_t position_{0};
    };

} // namespace yaml::ct::detail