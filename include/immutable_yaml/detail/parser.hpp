#pragma once

#include <immutable_yaml/detail/lexer.hpp>
#include <immutable_yaml/detail/types.hpp>
#include <variant>

namespace yaml::ct::detail
{

    template <std::size_t MaxTokens = 1024>
    class parser
    {
    public:
        constexpr explicit parser(const token_array<MaxTokens> &tokens, document &doc) noexcept
            : tokens_{tokens}, doc_{doc} {}

        constexpr auto parse_document() noexcept -> std::variant<document, yaml::ct::error_code>
        {
            if (current_token().type_ == token_type::document_start)
                advance();

            if (current_token().type_ == token_type::mapping_key ||
                current_token().type_ == token_type::sequence_end ||
                current_token().type_ == token_type::mapping_end)
            {
                return yaml::ct::error_code::unexpected_token;
            }

            auto value_result = parse_value();
            if (std::holds_alternative<yaml::ct::error_code>(value_result))
                return std::get<yaml::ct::error_code>(value_result);

            doc_.root_ = std::get<yaml_value>(value_result);
            return doc_;
        }

    private:
        constexpr auto current_token() const noexcept -> const token &
        {
            return tokens_[position_];
        }

        constexpr auto advance() noexcept -> void
        {
            if (position_ < MaxTokens - 1)
                ++position_;
        }

        constexpr auto parse_value() noexcept -> std::variant<yaml_value, yaml::ct::error_code>
        {
            const auto &tok = current_token();

            switch (tok.type_)
            {
            case token_type::null_literal:
                advance();
                return yaml_value::make_null();

            case token_type::boolean_literal:
                advance();
                return yaml_value::make_bool(tok.value_ == "true");

            case token_type::integer_literal:
                return parse_integer();

            case token_type::float_literal:
                return parse_float();

            case token_type::string_literal:
            case token_type::quoted_string:
                // peek ahead: if next token is colon, this is a block mapping key
                if (position_ + 1 < MaxTokens &&
                    tokens_[position_ + 1].type_ == token_type::mapping_key)
                {
                    return parse_block_mapping();
                }
                return parse_string_value();

            case token_type::sequence_start:
                return parse_flow_sequence();

            case token_type::mapping_start:
                return parse_flow_mapping();

            case token_type::sequence_entry:
                return parse_block_sequence();

            case token_type::mapping_key:
                return yaml::ct::error_code::unexpected_token;

            default:
                return yaml::ct::error_code::unexpected_token;
            }
        }

        constexpr auto parse_integer() noexcept -> std::variant<yaml_value, yaml::ct::error_code>
        {
            const auto &tok = current_token();
            advance();

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
                    result = result * 10 + (tok.value_[i] - '0');
            }

            return yaml_value::make_int(negative ? -result : result);
        }

        constexpr auto parse_float() noexcept -> std::variant<yaml_value, yaml::ct::error_code>
        {
            const auto &tok = current_token();
            advance();

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
                    break;
                }
            }

            return yaml_value::make_float(negative ? -result : result);
        }

        // parse a string token and return its string_type (for use as key or value)
        constexpr auto parse_string_raw() noexcept -> std::variant<string_type, yaml::ct::error_code>
        {
            const auto &tok = current_token();
            advance();

            if (tok.type_ == token_type::quoted_string)
            {
                std::string_view content = tok.value_;
                if (content.size() >= 2)
                    content = content.substr(1, content.size() - 2);
                return string_type{content};
            }
            return string_type{tok.value_};
        }

        // parse a string token and return as yaml_value
        constexpr auto parse_string_value() noexcept -> std::variant<yaml_value, yaml::ct::error_code>
        {
            auto result = parse_string_raw();
            if (std::holds_alternative<yaml::ct::error_code>(result))
                return std::get<yaml::ct::error_code>(result);
            return yaml_value::make_string(std::get<string_type>(result));
        }

        constexpr auto parse_flow_sequence() noexcept -> std::variant<yaml_value, yaml::ct::error_code>
        {
            advance(); // skip [

            // temp buffer for this level's entries
            std::array<yaml_value, YAML_CT_MAX_ITEMS> temp{};
            std::size_t count = 0;
            bool expect_value = true;

            while (current_token().type_ != token_type::sequence_end &&
                   current_token().type_ != token_type::eof)
            {
                if (current_token().type_ == token_type::string_literal &&
                    current_token().value_ == ",")
                {
                    if (expect_value)
                        return yaml::ct::error_code::unexpected_token;
                    advance();
                    expect_value = true;
                    continue;
                }

                if (!expect_value)
                    return yaml::ct::error_code::unexpected_token;

                auto value_result = parse_value();
                if (std::holds_alternative<yaml::ct::error_code>(value_result))
                    return std::get<yaml::ct::error_code>(value_result);

                if (count >= YAML_CT_MAX_ITEMS)
                    return yaml::ct::error_code::invalid_syntax;

                temp[count++] = std::get<yaml_value>(value_result);
                expect_value = false;
            }

            if (expect_value && count > 0)
                return yaml::ct::error_code::unexpected_token;

            if (current_token().type_ == token_type::sequence_end)
                advance();

            // flush to pool
            auto start = doc_.pool_size_;
            for (std::size_t i = 0; i < count; ++i)
            {
                doc_.pool_[doc_.pool_size_].key = string_type{};
                doc_.pool_[doc_.pool_size_].value = temp[i];
                doc_.pool_size_++;
            }

            return yaml_value::make_sequence(start, count);
        }

        constexpr auto parse_flow_mapping() noexcept -> std::variant<yaml_value, yaml::ct::error_code>
        {
            advance(); // skip {

            std::array<pool_entry, YAML_CT_MAX_ITEMS> temp{};
            std::size_t count = 0;
            bool expect_key = true;

            while (current_token().type_ != token_type::mapping_end &&
                   current_token().type_ != token_type::eof)
            {
                if (current_token().type_ == token_type::string_literal &&
                    current_token().value_ == ",")
                {
                    if (expect_key)
                        return yaml::ct::error_code::unexpected_token;
                    advance();
                    expect_key = true;
                    continue;
                }

                if (!expect_key)
                    return yaml::ct::error_code::unexpected_token;

                // parse key
                auto key_result = parse_string_raw();
                if (std::holds_alternative<yaml::ct::error_code>(key_result))
                    return std::get<yaml::ct::error_code>(key_result);

                auto key = std::get<string_type>(key_result);

                // check for duplicate keys
                for (std::size_t j = 0; j < count; ++j)
                {
                    if (temp[j].key.view() == key.view())
                        return yaml::ct::error_code::duplicate_key;
                }

                // expect colon
                if (current_token().type_ != token_type::mapping_key)
                    return yaml::ct::error_code::unexpected_token;
                advance();

                // parse value
                auto value_result = parse_value();
                if (std::holds_alternative<yaml::ct::error_code>(value_result))
                    return std::get<yaml::ct::error_code>(value_result);

                if (count >= YAML_CT_MAX_ITEMS)
                    return yaml::ct::error_code::invalid_syntax;

                temp[count++] = pool_entry{std::move(key), std::get<yaml_value>(value_result)};
                expect_key = false;
            }

            if (expect_key && count > 0)
                return yaml::ct::error_code::unexpected_token;

            if (current_token().type_ == token_type::mapping_end)
                advance();

            // flush to pool
            auto start = doc_.pool_size_;
            for (std::size_t i = 0; i < count; ++i)
            {
                doc_.pool_[doc_.pool_size_++] = temp[i];
            }

            return yaml_value::make_mapping(start, count);
        }

        constexpr auto parse_block_sequence() noexcept -> std::variant<yaml_value, yaml::ct::error_code>
        {
            std::array<yaml_value, YAML_CT_MAX_ITEMS> temp{};
            std::size_t count = 0;

            while (current_token().type_ == token_type::sequence_entry)
            {
                advance(); // skip -

                auto value_result = parse_value();
                if (std::holds_alternative<yaml::ct::error_code>(value_result))
                    return std::get<yaml::ct::error_code>(value_result);

                if (count >= YAML_CT_MAX_ITEMS)
                    return yaml::ct::error_code::invalid_syntax;

                temp[count++] = std::get<yaml_value>(value_result);
            }

            // flush to pool
            auto start = doc_.pool_size_;
            for (std::size_t i = 0; i < count; ++i)
            {
                doc_.pool_[doc_.pool_size_].key = string_type{};
                doc_.pool_[doc_.pool_size_].value = temp[i];
                doc_.pool_size_++;
            }

            return yaml_value::make_sequence(start, count);
        }

        constexpr auto parse_block_mapping() noexcept -> std::variant<yaml_value, yaml::ct::error_code>
        {
            std::array<pool_entry, YAML_CT_MAX_ITEMS> temp{};
            std::size_t count = 0;

            while (current_token().type_ == token_type::string_literal ||
                   current_token().type_ == token_type::quoted_string)
            {
                // parse key
                auto key_result = parse_string_raw();
                if (std::holds_alternative<yaml::ct::error_code>(key_result))
                    return std::get<yaml::ct::error_code>(key_result);

                auto key = std::get<string_type>(key_result);

                // check for duplicate keys
                for (std::size_t j = 0; j < count; ++j)
                {
                    if (temp[j].key.view() == key.view())
                        return yaml::ct::error_code::duplicate_key;
                }

                // expect colon
                if (current_token().type_ != token_type::mapping_key)
                    return yaml::ct::error_code::unexpected_token;
                advance();

                // parse value
                auto value_result = parse_value();
                if (std::holds_alternative<yaml::ct::error_code>(value_result))
                    return std::get<yaml::ct::error_code>(value_result);

                if (count >= YAML_CT_MAX_ITEMS)
                    return yaml::ct::error_code::invalid_syntax;

                temp[count++] = pool_entry{std::move(key), std::get<yaml_value>(value_result)};
            }

            // flush to pool
            auto start = doc_.pool_size_;
            for (std::size_t i = 0; i < count; ++i)
            {
                doc_.pool_[doc_.pool_size_++] = temp[i];
            }

            return yaml_value::make_mapping(start, count);
        }

        const token_array<MaxTokens> &tokens_;
        document &doc_;
        std::size_t position_{0};
    };

} // namespace yaml::ct::detail
