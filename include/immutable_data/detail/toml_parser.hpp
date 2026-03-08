#pragma once

#include <immutable_data/detail/toml_lexer.hpp>
#include <immutable_data/detail/types.hpp>
#include <variant>

namespace data::toml::detail
{

    using namespace data::detail;

    template <std::size_t MaxTokens = 1024>
    class parser
    {
    public:
        constexpr explicit parser(const token_array<MaxTokens> &tokens, document &doc) noexcept
            : tokens_{tokens}, doc_{doc} {}

        // TOML document is always a root mapping.
        // We parse all key-value pairs and table headers into a flat list,
        // then the root is a mapping over all top-level entries.
        constexpr auto parse_document() noexcept -> std::variant<document, data::parse_error>
        {
            // Parse as a mapping at root level
            auto result = parse_table_body();
            if (std::holds_alternative<data::parse_error>(result))
                return std::get<data::parse_error>(result);

            doc_.root_ = std::get<value>(result);
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

        // Parse the body of a table (key-value pairs until EOF or next table header)
        constexpr auto parse_table_body() noexcept -> std::variant<value, data::parse_error>
        {
            std::array<pool_entry, DATA_CT_MAX_ITEMS> entries{};
            std::size_t count = 0;

            while (current_token().type_ != token_type::eof)
            {
                // Table header [name] — creates a sub-mapping
                if (current_token().type_ == token_type::sequence_start)
                {
                    advance(); // skip [

                    // Read table name (possibly dotted)
                    auto key_result = parse_key();
                    if (std::holds_alternative<data::parse_error>(key_result))
                        return std::get<data::parse_error>(key_result);
                    auto key = std::get<string_type>(key_result);

                    if (current_token().type_ != token_type::sequence_end)
                        return make_error(data::error_code::unexpected_token);
                    advance(); // skip ]

                    // Check for duplicate keys
                    for (std::size_t j = 0; j < count; ++j)
                        if (entries[j].key.view() == key.view())
                            return make_error(data::error_code::duplicate_key);

                    // Parse the table contents as a sub-mapping
                    auto body = parse_key_value_pairs();
                    if (std::holds_alternative<data::parse_error>(body))
                        return std::get<data::parse_error>(body);

                    if (count >= DATA_CT_MAX_ITEMS)
                        return make_error(data::error_code::invalid_syntax);
                    entries[count++] = pool_entry{std::move(key), std::get<value>(body)};
                    continue;
                }

                // Key = value pair
                if (current_token().type_ == token_type::string_literal ||
                    current_token().type_ == token_type::quoted_string)
                {
                    auto kv = parse_key_value(entries, count);
                    if (std::holds_alternative<data::parse_error>(kv))
                        return std::get<data::parse_error>(kv);
                    continue;
                }

                return make_error(data::error_code::unexpected_token);
            }

            // Write entries to pool
            if (!doc_.can_alloc(count))
                return make_error(data::error_code::pool_overflow);
            auto start = doc_.pool_size_;
            for (std::size_t i = 0; i < count; ++i)
                doc_.pool_[doc_.pool_size_++] = entries[i];
            return value::make_mapping(start, count);
        }

        // Parse key-value pairs until we hit a table header or EOF
        constexpr auto parse_key_value_pairs() noexcept -> std::variant<value, data::parse_error>
        {
            std::array<pool_entry, DATA_CT_MAX_ITEMS> entries{};
            std::size_t count = 0;

            while (current_token().type_ != token_type::eof &&
                   current_token().type_ != token_type::sequence_start)
            {
                if (current_token().type_ == token_type::string_literal ||
                    current_token().type_ == token_type::quoted_string)
                {
                    auto kv = parse_key_value(entries, count);
                    if (std::holds_alternative<data::parse_error>(kv))
                        return std::get<data::parse_error>(kv);
                    continue;
                }

                return make_error(data::error_code::unexpected_token);
            }

            if (!doc_.can_alloc(count))
                return make_error(data::error_code::pool_overflow);
            auto start = doc_.pool_size_;
            for (std::size_t i = 0; i < count; ++i)
                doc_.pool_[doc_.pool_size_++] = entries[i];
            return value::make_mapping(start, count);
        }

        // Parse a single "key = value" and add to entries
        constexpr auto parse_key_value(std::array<pool_entry, DATA_CT_MAX_ITEMS> &entries,
                                       std::size_t &count) noexcept -> std::variant<bool, data::parse_error>
        {
            auto key_result = parse_key();
            if (std::holds_alternative<data::parse_error>(key_result))
                return std::get<data::parse_error>(key_result);
            auto key = std::get<string_type>(key_result);

            if (current_token().type_ != token_type::equals)
                return make_error(data::error_code::unexpected_token);
            advance(); // skip =

            auto value_result = parse_value();
            if (std::holds_alternative<data::parse_error>(value_result))
                return std::get<data::parse_error>(value_result);

            // Check for duplicate keys
            for (std::size_t j = 0; j < count; ++j)
                if (entries[j].key.view() == key.view())
                    return make_error(data::error_code::duplicate_key);

            if (count >= DATA_CT_MAX_ITEMS)
                return make_error(data::error_code::invalid_syntax);
            entries[count++] = pool_entry{std::move(key), std::get<value>(value_result)};
            return true;
        }

        // Parse a key (bare or quoted, possibly dotted — but for now just simple keys)
        constexpr auto parse_key() noexcept -> std::variant<string_type, data::parse_error>
        {
            const auto &tok = current_token();
            if (tok.type_ == token_type::quoted_string)
            {
                advance();
                std::string_view content = tok.value_;
                if (content.size() >= 2)
                    content = content.substr(1, content.size() - 2);
                return string_type{content};
            }
            if (tok.type_ == token_type::string_literal)
            {
                advance();
                return string_type{tok.value_};
            }
            return make_error(data::error_code::unexpected_token);
        }

        constexpr auto parse_value() noexcept -> std::variant<value, data::parse_error>
        {
            if (depth_ >= MAX_PARSE_DEPTH)
                return make_error(data::error_code::max_depth_exceeded);
            depth_guard guard{depth_};

            const auto &tok = current_token();
            switch (tok.type_)
            {
            case token_type::boolean_literal:
                advance();
                return value::make_bool(tok.value_ == "true");
            case token_type::integer_literal:
                return parse_integer();
            case token_type::float_literal:
                return parse_float();
            case token_type::quoted_string:
                return parse_basic_string_value();
            case token_type::literal_string:
                return parse_literal_string_value();
            case token_type::sequence_start:
                return parse_array();
            case token_type::mapping_start:
                return parse_inline_table();
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
                // skip underscores
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
                // skip underscores
            }
            return value::make_float(negative ? -result : result);
        }

        constexpr auto parse_basic_string_value() noexcept -> std::variant<value, data::parse_error>
        {
            const auto &tok = current_token();
            advance();
            std::string_view raw = tok.value_;
            if (raw.size() < 2)
                return value::make_string(string_type{});

            raw = raw.substr(1, raw.size() - 2);

            // Check for escapes
            bool has_escape = false;
            for (auto c : raw)
                if (c == '\\') { has_escape = true; break; }
            if (!has_escape)
                return value::make_string(string_type{raw});

            // Process escape sequences (same as JSON)
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
                case 'b':  result.push_back('\b'); break;
                case 'f':  result.push_back('\f'); break;
                case 'n':  result.push_back('\n'); break;
                case 'r':  result.push_back('\r'); break;
                case 't':  result.push_back('\t'); break;
                case 'u':
                {
                    if (i + 4 >= raw.size()) break;
                    std::uint32_t cp = 0;
                    for (int k = 0; k < 4; ++k)
                        cp = (cp << 4) | hex_value(raw[i + 1 + k]);
                    i += 4;
                    if (cp < 0x80)
                        result.push_back(static_cast<char>(cp));
                    else if (cp < 0x800)
                    {
                        result.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                        result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    }
                    else
                    {
                        result.push_back(static_cast<char>(0xE0 | (cp >> 12)));
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
            return value::make_string(result);
        }

        constexpr auto parse_literal_string_value() noexcept -> std::variant<value, data::parse_error>
        {
            const auto &tok = current_token();
            advance();
            std::string_view raw = tok.value_;
            if (raw.size() < 2)
                return value::make_string(string_type{});
            // Literal strings: no escape processing, just strip quotes
            raw = raw.substr(1, raw.size() - 2);
            return value::make_string(string_type{raw});
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
                if (count >= DATA_CT_MAX_ITEMS)
                    return make_error(data::error_code::invalid_syntax);
                temp[count++] = std::get<value>(value_result);

                if (current_token().type_ == token_type::comma)
                {
                    advance();
                    if (current_token().type_ == token_type::sequence_end)
                        break; // trailing comma is allowed in TOML
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

        constexpr auto parse_inline_table() noexcept -> std::variant<value, data::parse_error>
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
                auto kv = parse_key_value(temp, count);
                if (std::holds_alternative<data::parse_error>(kv))
                    return std::get<data::parse_error>(kv);

                if (current_token().type_ == token_type::comma)
                {
                    advance();
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

} // namespace data::toml::detail
