#pragma once

#include <immutable_data/detail/json_lexer.hpp>
#include <immutable_data/detail/types.hpp>
#include <variant>

namespace data::json::detail
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

        struct depth_guard
        {
            std::size_t &depth_;
            constexpr explicit depth_guard(std::size_t &d) noexcept : depth_{d} { ++depth_; }
            constexpr ~depth_guard() noexcept { --depth_; }
        };

        constexpr auto parse_value() noexcept -> std::variant<value, data::parse_error>
        {
            if (depth_ >= MAX_PARSE_DEPTH)
                return make_error(data::error_code::max_depth_exceeded);
            depth_guard guard{depth_};

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
            case token_type::quoted_string:
                return parse_string_value();
            case token_type::sequence_start:
                return parse_array();
            case token_type::mapping_start:
                return parse_object();
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
            if (tok.type_ != token_type::quoted_string)
                return make_error(data::error_code::unexpected_token);
            advance();
            std::string_view raw = tok.value_;
            if (raw.size() < 2)
                return string_type{};

            // Strip surrounding quotes
            raw = raw.substr(1, raw.size() - 2);

            // Fast path: no backslashes means no escapes
            bool has_escape = false;
            for (auto c : raw)
                if (c == '\\') { has_escape = true; break; }
            if (!has_escape)
                return string_type{raw};

            // Process escape sequences
            string_type result{};
            for (std::size_t i = 0; i < raw.size(); ++i)
            {
                if (raw[i] != '\\')
                {
                    result.push_back(raw[i]);
                    continue;
                }
                if (++i >= raw.size())
                    break;
                switch (raw[i])
                {
                case '"':  result.push_back('"');  break;
                case '\\': result.push_back('\\'); break;
                case '/':  result.push_back('/');  break;
                case 'b':  result.push_back('\b'); break;
                case 'f':  result.push_back('\f'); break;
                case 'n':  result.push_back('\n'); break;
                case 'r':  result.push_back('\r'); break;
                case 't':  result.push_back('\t'); break;
                case 'u':
                {
                    if (i + 4 >= raw.size())
                        break;
                    std::uint32_t cp = 0;
                    for (int k = 0; k < 4; ++k)
                        cp = (cp << 4) | hex_value(raw[i + 1 + k]);
                    i += 4;

                    // Handle surrogate pairs
                    if (cp >= 0xD800 && cp <= 0xDBFF &&
                        i + 2 < raw.size() && raw[i + 1] == '\\' && raw[i + 2] == 'u')
                    {
                        if (i + 6 < raw.size())
                        {
                            std::uint32_t lo = 0;
                            for (int k = 0; k < 4; ++k)
                                lo = (lo << 4) | hex_value(raw[i + 3 + k]);
                            if (lo >= 0xDC00 && lo <= 0xDFFF)
                            {
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                                i += 6;
                            }
                        }
                    }

                    // Encode as UTF-8
                    if (cp < 0x80)
                    {
                        result.push_back(static_cast<char>(cp));
                    }
                    else if (cp < 0x800)
                    {
                        result.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                        result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    }
                    else if (cp < 0x10000)
                    {
                        result.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                        result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                        result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    }
                    else
                    {
                        result.push_back(static_cast<char>(0xF0 | (cp >> 18)));
                        result.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                        result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                        result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    }
                    break;
                }
                default:
                    result.push_back(raw[i]);
                    break;
                }
            }
            return result;
        }

        constexpr auto parse_string_value() noexcept -> std::variant<value, data::parse_error>
        {
            auto result = parse_string_raw();
            if (std::holds_alternative<data::parse_error>(result))
                return std::get<data::parse_error>(result);
            return value::make_string(std::get<string_type>(result));
        }

        constexpr auto parse_array() noexcept -> std::variant<value, data::parse_error>
        {
            advance(); // skip [
            std::array<value, DATA_CT_MAX_ITEMS> temp{};
            std::size_t count = 0;

            if (current_token().type_ == token_type::sequence_end)
            {
                advance();
                return value::make_sequence(doc_.pool_size_, 0);
            }

            while (true)
            {
                auto value_result = parse_value();
                if (std::holds_alternative<data::parse_error>(value_result))
                    return std::get<data::parse_error>(value_result);
                if (count >= DATA_CT_MAX_ITEMS) return make_error(data::error_code::invalid_syntax);
                temp[count++] = std::get<value>(value_result);

                if (current_token().type_ == token_type::comma)
                {
                    advance();
                    if (current_token().type_ == token_type::sequence_end)
                        return make_error(data::error_code::trailing_comma);
                    continue;
                }
                break;
            }

            if (current_token().type_ != token_type::sequence_end)
                return make_error(data::error_code::unexpected_token);
            advance();

            if (!doc_.can_alloc(count))
                return make_error(data::error_code::pool_overflow);
            auto start = doc_.pool_size_;
            for (std::size_t i = 0; i < count; ++i)
            {
                doc_.pool_[doc_.pool_size_].key = string_type{};
                doc_.pool_[doc_.pool_size_].val_ = temp[i];
                doc_.pool_size_++;
            }
            return value::make_sequence(start, count);
        }

        constexpr auto parse_object() noexcept -> std::variant<value, data::parse_error>
        {
            advance(); // skip {
            std::array<pool_entry, DATA_CT_MAX_ITEMS> temp{};
            std::size_t count = 0;

            if (current_token().type_ == token_type::mapping_end)
            {
                advance();
                return value::make_mapping(doc_.pool_size_, 0);
            }

            while (true)
            {
                // key must be a quoted string
                auto key_result = parse_string_raw();
                if (std::holds_alternative<data::parse_error>(key_result))
                    return std::get<data::parse_error>(key_result);
                auto key = std::get<string_type>(key_result);

                // check for duplicate keys
                for (std::size_t j = 0; j < count; ++j)
                    if (temp[j].key.view() == key.view())
                        return make_error(data::error_code::duplicate_key);

                // expect colon
                if (current_token().type_ != token_type::mapping_key)
                    return make_error(data::error_code::unexpected_token);
                advance();

                // parse value
                auto value_result = parse_value();
                if (std::holds_alternative<data::parse_error>(value_result))
                    return std::get<data::parse_error>(value_result);
                if (count >= DATA_CT_MAX_ITEMS) return make_error(data::error_code::invalid_syntax);
                temp[count++] = pool_entry{std::move(key), std::get<value>(value_result)};

                if (current_token().type_ == token_type::comma)
                {
                    advance();
                    if (current_token().type_ == token_type::mapping_end)
                        return make_error(data::error_code::trailing_comma);
                    continue;
                }
                break;
            }

            if (current_token().type_ != token_type::mapping_end)
                return make_error(data::error_code::unexpected_token);
            advance();

            if (!doc_.can_alloc(count))
                return make_error(data::error_code::pool_overflow);
            auto start = doc_.pool_size_;
            for (std::size_t i = 0; i < count; ++i)
                doc_.pool_[doc_.pool_size_++] = temp[i];
            return value::make_mapping(start, count);
        }

        const token_array<MaxTokens> &tokens_;
        document &doc_;
        std::size_t position_{0};
        std::size_t depth_{0};
    };

} // namespace data::json::detail
