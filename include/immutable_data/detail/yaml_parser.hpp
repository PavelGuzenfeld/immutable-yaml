#pragma once

#include <immutable_data/detail/yaml_lexer.hpp>
#include <immutable_data/detail/types.hpp>
#include <variant>

namespace data::yaml::detail
{

    using namespace data::detail;

    template <std::size_t MaxTokens = 1024>
    class parser
    {
    public:
        constexpr explicit parser(const token_array<MaxTokens> &tokens, document &doc) noexcept
            : tokens_{tokens}, doc_{doc} {}

        constexpr auto parse_document() noexcept -> std::variant<document, data::parse_error>
        {
            if (current_token().type_ == token_type::document_start)
                advance();

            if (current_token().type_ == token_type::mapping_key ||
                current_token().type_ == token_type::sequence_end ||
                current_token().type_ == token_type::mapping_end)
                return make_error(data::error_code::unexpected_token);

            auto value_result = parse_value();
            if (std::holds_alternative<data::parse_error>(value_result))
                return std::get<data::parse_error>(value_result);

            doc_.root_ = std::get<value>(value_result);
            return doc_;
        }

    private:
        constexpr auto current_token() const noexcept -> const token & { return tokens_[position_]; }
        constexpr auto advance() noexcept -> void { if (position_ < MaxTokens - 1) ++position_; }
        constexpr auto make_error(data::error_code ec) const noexcept -> data::parse_error
        {
            auto const &tok = current_token();
            return {ec, tok.line_, tok.column_};
        }

        constexpr auto parse_value() noexcept -> std::variant<value, data::parse_error>
        {
            const auto &tok = current_token();
            switch (tok.type_)
            {
            case token_type::null_literal:
                advance();
                return value::make_null();
            case token_type::boolean_literal:
                advance();
                return value::make_bool(tok.value_ == "true");
            case token_type::integer_literal:
                return parse_integer();
            case token_type::float_literal:
                return parse_float();
            case token_type::string_literal:
            case token_type::quoted_string:
                if (position_ + 1 < MaxTokens &&
                    tokens_[position_ + 1].type_ == token_type::mapping_key)
                    return parse_block_mapping();
                return parse_string_value();
            case token_type::literal_string:
                return parse_block_scalar(true);
            case token_type::folded_string:
                return parse_block_scalar(false);
            case token_type::sequence_start:
                return parse_flow_sequence();
            case token_type::mapping_start:
                return parse_flow_mapping();
            case token_type::sequence_entry:
                return parse_block_sequence();
            default:
                return make_error(data::error_code::unexpected_token);
            }
        }

        constexpr auto parse_integer() noexcept -> std::variant<value, data::parse_error>
        {
            const auto &tok = current_token();
            advance();
            integer result = 0;
            bool negative = false;
            std::size_t i = 0;
            if (!tok.value_.empty() && tok.value_[0] == '-') { negative = true; i = 1; }
            else if (!tok.value_.empty() && tok.value_[0] == '+') { i = 1; }
            for (; i < tok.value_.size(); ++i)
                if (is_digit(tok.value_[i]))
                    result = result * 10 + (tok.value_[i] - '0');
            return value::make_int(negative ? -result : result);
        }

        constexpr auto parse_float() noexcept -> std::variant<value, data::parse_error>
        {
            const auto &tok = current_token();
            advance();
            floating result = 0.0;
            floating decimal_place = 0.1;
            bool negative = false;
            bool in_decimal = false;
            std::size_t i = 0;
            if (!tok.value_.empty() && tok.value_[0] == '-') { negative = true; i = 1; }
            else if (!tok.value_.empty() && tok.value_[0] == '+') { i = 1; }
            for (; i < tok.value_.size(); ++i)
            {
                char c = tok.value_[i];
                if (is_digit(c))
                {
                    if (in_decimal) { result += (c - '0') * decimal_place; decimal_place *= 0.1; }
                    else { result = result * 10.0 + (c - '0'); }
                }
                else if (c == '.') { in_decimal = true; }
                else if (c == 'e' || c == 'E') { break; }
            }
            return value::make_float(negative ? -result : result);
        }

        constexpr auto parse_string_raw() noexcept -> std::variant<string_type, data::parse_error>
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

        constexpr auto parse_string_value() noexcept -> std::variant<value, data::parse_error>
        {
            auto result = parse_string_raw();
            if (std::holds_alternative<data::parse_error>(result))
                return std::get<data::parse_error>(result);
            return value::make_string(std::get<string_type>(result));
        }

        constexpr auto parse_block_scalar(bool literal) noexcept -> std::variant<value, data::parse_error>
        {
            const auto &tok = current_token();
            advance();

            std::string_view raw = tok.value_;
            if (raw.empty())
                return value::make_string(string_type{});

            // Find block indentation from first non-blank line
            std::size_t block_indent = 0;
            {
                std::size_t i = 0;
                while (i < raw.size())
                {
                    std::size_t indent = 0;
                    while (i < raw.size() && raw[i] == ' ')
                        { ++indent; ++i; }
                    if (i >= raw.size() || raw[i] == '\n')
                    {
                        if (i < raw.size()) ++i;
                        continue;
                    }
                    block_indent = indent;
                    break;
                }
            }

            string_type result{};
            std::size_t i = 0;

            if (literal)
            {
                // Literal (|): preserve newlines, strip common indent
                bool first_line = true;
                while (i < raw.size())
                {
                    // Strip indentation
                    std::size_t indent = 0;
                    while (i < raw.size() && raw[i] == ' ' && indent < block_indent)
                        { ++indent; ++i; }

                    if (i >= raw.size()) break;

                    // Blank line
                    if (raw[i] == '\n')
                    {
                        result.push_back('\n');
                        ++i;
                        continue;
                    }

                    if (!first_line)
                        result.push_back('\n');
                    first_line = false;

                    // Copy line content
                    while (i < raw.size() && raw[i] != '\n')
                        result.push_back(raw[i++]);
                    if (i < raw.size()) ++i; // skip \n
                }
            }
            else
            {
                // Folded (>): adjacent non-blank lines joined with space, blank lines become \n
                bool first_content = true;
                bool prev_was_blank = false;
                while (i < raw.size())
                {
                    // Strip indentation
                    std::size_t indent = 0;
                    while (i < raw.size() && raw[i] == ' ' && indent < block_indent)
                        { ++indent; ++i; }

                    if (i >= raw.size()) break;

                    // Blank line
                    if (raw[i] == '\n')
                    {
                        if (!first_content)
                            result.push_back('\n');
                        prev_was_blank = true;
                        ++i;
                        continue;
                    }

                    // Non-blank line — space-join with previous unless after blank
                    if (!first_content && !prev_was_blank)
                        result.push_back(' ');
                    first_content = false;
                    prev_was_blank = false;

                    // Copy line content
                    while (i < raw.size() && raw[i] != '\n')
                        result.push_back(raw[i++]);
                    if (i < raw.size()) ++i; // skip \n
                }
            }

            return value::make_string(result);
        }

        constexpr auto parse_flow_sequence() noexcept -> std::variant<value, data::parse_error>
        {
            advance(); // skip [
            std::array<value, DATA_CT_MAX_ITEMS> temp{};
            std::size_t count = 0;
            bool expect_value = true;

            while (current_token().type_ != token_type::sequence_end &&
                   current_token().type_ != token_type::eof)
            {
                if (current_token().type_ == token_type::comma)
                {
                    if (expect_value) return make_error(data::error_code::unexpected_token);
                    advance();
                    expect_value = true;
                    continue;
                }
                if (!expect_value) return make_error(data::error_code::unexpected_token);

                auto value_result = parse_value();
                if (std::holds_alternative<data::parse_error>(value_result))
                    return std::get<data::parse_error>(value_result);
                if (count >= DATA_CT_MAX_ITEMS) return make_error(data::error_code::invalid_syntax);
                temp[count++] = std::get<value>(value_result);
                expect_value = false;
            }

            if (expect_value && count > 0) return make_error(data::error_code::unexpected_token);
            if (current_token().type_ == token_type::sequence_end) advance();

            auto start = doc_.pool_size_;
            for (std::size_t i = 0; i < count; ++i)
            {
                doc_.pool_[doc_.pool_size_].key = string_type{};
                doc_.pool_[doc_.pool_size_].val_ = temp[i];
                doc_.pool_size_++;
            }
            return value::make_sequence(start, count);
        }

        constexpr auto parse_flow_mapping() noexcept -> std::variant<value, data::parse_error>
        {
            advance(); // skip {
            std::array<pool_entry, DATA_CT_MAX_ITEMS> temp{};
            std::size_t count = 0;
            bool expect_key = true;

            while (current_token().type_ != token_type::mapping_end &&
                   current_token().type_ != token_type::eof)
            {
                if (current_token().type_ == token_type::comma)
                {
                    if (expect_key) return make_error(data::error_code::unexpected_token);
                    advance();
                    expect_key = true;
                    continue;
                }
                if (!expect_key) return make_error(data::error_code::unexpected_token);

                auto key_result = parse_string_raw();
                if (std::holds_alternative<data::parse_error>(key_result))
                    return std::get<data::parse_error>(key_result);
                auto key = std::get<string_type>(key_result);

                for (std::size_t j = 0; j < count; ++j)
                    if (temp[j].key.view() == key.view())
                        return make_error(data::error_code::duplicate_key);

                if (current_token().type_ != token_type::mapping_key)
                    return make_error(data::error_code::unexpected_token);
                advance();

                auto value_result = parse_value();
                if (std::holds_alternative<data::parse_error>(value_result))
                    return std::get<data::parse_error>(value_result);
                if (count >= DATA_CT_MAX_ITEMS) return make_error(data::error_code::invalid_syntax);
                temp[count++] = pool_entry{std::move(key), std::get<value>(value_result)};
                expect_key = false;
            }

            if (expect_key && count > 0) return make_error(data::error_code::unexpected_token);
            if (current_token().type_ == token_type::mapping_end) advance();

            auto start = doc_.pool_size_;
            for (std::size_t i = 0; i < count; ++i)
                doc_.pool_[doc_.pool_size_++] = temp[i];
            return value::make_mapping(start, count);
        }

        constexpr auto parse_block_sequence() noexcept -> std::variant<value, data::parse_error>
        {
            std::array<value, DATA_CT_MAX_ITEMS> temp{};
            std::size_t count = 0;
            auto expected_col = current_token().column_;

            while (current_token().type_ == token_type::sequence_entry &&
                   current_token().column_ == expected_col)
            {
                advance(); // skip -
                auto value_result = parse_value();
                if (std::holds_alternative<data::parse_error>(value_result))
                    return std::get<data::parse_error>(value_result);
                if (count >= DATA_CT_MAX_ITEMS) return make_error(data::error_code::invalid_syntax);
                temp[count++] = std::get<value>(value_result);
            }

            auto start = doc_.pool_size_;
            for (std::size_t i = 0; i < count; ++i)
            {
                doc_.pool_[doc_.pool_size_].key = string_type{};
                doc_.pool_[doc_.pool_size_].val_ = temp[i];
                doc_.pool_size_++;
            }
            return value::make_sequence(start, count);
        }

        constexpr auto parse_block_mapping() noexcept -> std::variant<value, data::parse_error>
        {
            std::array<pool_entry, DATA_CT_MAX_ITEMS> temp{};
            std::size_t count = 0;
            auto expected_col = current_token().column_;

            while ((current_token().type_ == token_type::string_literal ||
                    current_token().type_ == token_type::quoted_string) &&
                   current_token().column_ == expected_col)
            {
                auto key_result = parse_string_raw();
                if (std::holds_alternative<data::parse_error>(key_result))
                    return std::get<data::parse_error>(key_result);
                auto key = std::get<string_type>(key_result);

                for (std::size_t j = 0; j < count; ++j)
                    if (temp[j].key.view() == key.view())
                        return make_error(data::error_code::duplicate_key);

                if (current_token().type_ != token_type::mapping_key)
                    return make_error(data::error_code::unexpected_token);
                advance();

                auto value_result = parse_value();
                if (std::holds_alternative<data::parse_error>(value_result))
                    return std::get<data::parse_error>(value_result);
                if (count >= DATA_CT_MAX_ITEMS) return make_error(data::error_code::invalid_syntax);
                temp[count++] = pool_entry{std::move(key), std::get<value>(value_result)};
            }

            auto start = doc_.pool_size_;
            for (std::size_t i = 0; i < count; ++i)
                doc_.pool_[doc_.pool_size_++] = temp[i];
            return value::make_mapping(start, count);
        }

        const token_array<MaxTokens> &tokens_;
        document &doc_;
        std::size_t position_{0};
    };

} // namespace data::yaml::detail
