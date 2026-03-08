#pragma once

#include <immutable_data/detail/types.hpp>
#include <immutable_data/detail/utils.hpp>
#include <array>
#include <string_view>
#include <variant>

namespace data::xml::detail
{

    using namespace data::detail;

    class parser
    {
    public:
        constexpr explicit parser(std::string_view input, document &doc) noexcept
            : input_{input}, doc_{doc} {}

        constexpr auto parse_document() noexcept -> std::variant<document, data::parse_error>
        {
            skip_whitespace();
            skip_prolog();

            auto result = parse_element();
            if (std::holds_alternative<data::parse_error>(result))
                return std::get<data::parse_error>(result);

            doc_.root_ = std::get<pool_entry>(result).val_;
            return doc_;
        }

    private:
        constexpr bool at_end() const noexcept { return pos_ >= input_.size(); }
        constexpr char peek() const noexcept { return at_end() ? '\0' : input_[pos_]; }
        constexpr char peek_at(std::size_t off) const noexcept
        {
            return (pos_ + off >= input_.size()) ? '\0' : input_[pos_ + off];
        }

        constexpr char advance() noexcept
        {
            if (at_end()) return '\0';
            char c = input_[pos_++];
            if (c == '\n') { ++line_; col_ = 1; } else ++col_;
            return c;
        }

        constexpr auto make_error(data::error_code ec) const noexcept -> data::parse_error
        {
            return {ec, line_, col_};
        }

        constexpr void skip_whitespace() noexcept
        {
            while (!at_end() && (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r'))
                advance();
        }

        constexpr void skip_prolog() noexcept
        {
            // Skip XML declaration <?...?>
            while (!at_end() && peek() == '<' && peek_at(1) == '?')
            {
                advance(); advance();
                while (!at_end() && !(peek() == '?' && peek_at(1) == '>'))
                    advance();
                if (!at_end()) { advance(); advance(); }
                skip_whitespace();
            }
            // Skip DOCTYPE <!DOCTYPE ...>
            if (!at_end() && peek() == '<' && peek_at(1) == '!' && peek_at(2) == 'D')
            {
                while (!at_end() && peek() != '>')
                    advance();
                if (!at_end()) advance();
                skip_whitespace();
            }
            skip_comments();
        }

        constexpr void skip_comments() noexcept
        {
            while (!at_end() && peek() == '<' && peek_at(1) == '!' &&
                   peek_at(2) == '-' && peek_at(3) == '-')
            {
                for (int i = 0; i < 4; ++i) advance();
                while (!at_end() && !(peek() == '-' && peek_at(1) == '-' && peek_at(2) == '>'))
                    advance();
                if (!at_end()) { advance(); advance(); advance(); }
                skip_whitespace();
            }
        }

        constexpr auto read_name() noexcept -> string_type
        {
            string_type name{};
            while (!at_end() && (is_alnum(peek()) || peek() == '_' || peek() == '-' ||
                                 peek() == '.' || peek() == ':'))
                name.push_back(advance());
            return name;
        }

        constexpr auto parse_attr_value() noexcept -> std::variant<string_type, data::parse_error>
        {
            skip_whitespace();
            if (at_end() || peek() != '=')
                return make_error(data::error_code::unexpected_token);
            advance(); // skip =
            skip_whitespace();

            char quote = peek();
            if (quote != '"' && quote != '\'')
                return make_error(data::error_code::unexpected_token);
            advance(); // skip opening quote

            string_type val{};
            while (!at_end() && peek() != quote)
                val.push_back(advance());

            if (at_end())
                return make_error(data::error_code::unterminated_string);
            advance(); // skip closing quote
            return val;
        }

        static constexpr auto trim_view(std::string_view sv) noexcept -> std::string_view
        {
            while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' ||
                                   sv.front() == '\n' || sv.front() == '\r'))
                sv.remove_prefix(1);
            while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' ||
                                   sv.back() == '\n' || sv.back() == '\r'))
                sv.remove_suffix(1);
            return sv;
        }

        constexpr auto detect_scalar(std::string_view sv) noexcept -> value
        {
            if (sv.empty()) return value::make_string(string_type{});
            if (sv == "true") return value::make_bool(true);
            if (sv == "false") return value::make_bool(false);
            if (sv == "null") return value::make_null();

            // Try number
            bool valid = true;
            bool has_dot = false;
            bool negative = false;
            std::size_t start = 0;

            if (sv[0] == '-') { negative = true; start = 1; }
            else if (sv[0] == '+') { start = 1; }

            if (start >= sv.size() || !is_digit(sv[start]))
                valid = false;

            if (valid)
            {
                for (std::size_t i = start; i < sv.size(); ++i)
                {
                    if (sv[i] == '.')
                    {
                        if (has_dot) { valid = false; break; } // multiple dots = not a number
                        has_dot = true;
                        continue;
                    }
                    if (!is_digit(sv[i])) { valid = false; break; }
                }
            }

            if (valid && has_dot)
            {
                floating result = 0.0;
                floating decimal_place = 0.1;
                bool in_decimal = false;
                for (std::size_t i = start; i < sv.size(); ++i)
                {
                    if (is_digit(sv[i]))
                    {
                        if (in_decimal) { result += (sv[i] - '0') * decimal_place; decimal_place *= 0.1; }
                        else result = result * 10.0 + (sv[i] - '0');
                    }
                    else if (sv[i] == '.') in_decimal = true;
                }
                return value::make_float(negative ? -result : result);
            }

            if (valid)
            {
                integer result = 0;
                for (std::size_t i = start; i < sv.size(); ++i)
                    result = result * 10 + (sv[i] - '0');
                return value::make_int(negative ? -result : result);
            }

            return value::make_string(string_type{sv});
        }

        constexpr auto parse_element() noexcept -> std::variant<pool_entry, data::parse_error>
        {
            skip_whitespace();
            skip_comments();

            if (at_end() || peek() != '<')
                return make_error(data::error_code::unexpected_token);
            advance(); // skip <

            auto tag_name = read_name();
            if (tag_name.view().empty())
                return make_error(data::error_code::unexpected_token);

            // Parse attributes
            std::array<pool_entry, DATA_CT_MAX_ITEMS> attrs{};
            std::size_t attr_count = 0;

            skip_whitespace();
            while (!at_end() && peek() != '>' && peek() != '/')
            {
                auto attr_name = read_name();
                if (attr_name.view().empty()) break;

                auto attr_val_result = parse_attr_value();
                if (std::holds_alternative<data::parse_error>(attr_val_result))
                    return std::get<data::parse_error>(attr_val_result);
                auto attr_val = std::get<string_type>(attr_val_result);

                if (attr_count >= DATA_CT_MAX_ITEMS)
                    return make_error(data::error_code::invalid_syntax);
                attrs[attr_count++] = pool_entry{attr_name, detect_scalar(attr_val.view())};
                skip_whitespace();
            }

            // Self-closing tag?
            if (peek() == '/')
            {
                advance(); // skip /
                if (at_end() || peek() != '>')
                    return make_error(data::error_code::unexpected_token);
                advance(); // skip >

                if (attr_count == 0)
                    return pool_entry{tag_name, value::make_null()};

                auto start = doc_.pool_size_;
                for (std::size_t i = 0; i < attr_count; ++i)
                    doc_.pool_[doc_.pool_size_++] = attrs[i];
                return pool_entry{tag_name, value::make_mapping(start, attr_count)};
            }

            if (at_end() || peek() != '>')
                return make_error(data::error_code::unexpected_token);
            advance(); // skip >

            // Parse content: text, child elements, comments
            std::array<pool_entry, DATA_CT_MAX_ITEMS> children{};
            std::size_t child_count = attr_count;
            // Start with attributes
            for (std::size_t i = 0; i < attr_count; ++i)
                children[i] = attrs[i];

            string_type text_buf{};

            while (!at_end())
            {
                if (peek() == '<')
                {
                    // Closing tag
                    if (peek_at(1) == '/')
                        break;

                    // Comment
                    if (peek_at(1) == '!' && peek_at(2) == '-' && peek_at(3) == '-')
                    {
                        skip_comments();
                        continue;
                    }

                    // CDATA
                    if (peek_at(1) == '!' && peek_at(2) == '[')
                    {
                        // Skip <![CDATA[
                        for (int i = 0; i < 9; ++i) advance();
                        while (!at_end() && !(peek() == ']' && peek_at(1) == ']' && peek_at(2) == '>'))
                            text_buf.push_back(advance());
                        if (!at_end()) { advance(); advance(); advance(); } // skip ]]>
                        continue;
                    }

                    // Child element
                    auto child_result = parse_element();
                    if (std::holds_alternative<data::parse_error>(child_result))
                        return child_result;

                    auto &child = std::get<pool_entry>(child_result);

                    // Duplicate key check
                    for (std::size_t j = 0; j < child_count; ++j)
                        if (children[j].key.view() == child.key.view())
                            return make_error(data::error_code::duplicate_key);

                    if (child_count >= DATA_CT_MAX_ITEMS)
                        return make_error(data::error_code::invalid_syntax);
                    children[child_count++] = child;
                }
                else
                {
                    // Text content
                    while (!at_end() && peek() != '<')
                        text_buf.push_back(advance());
                }
            }

            // Parse closing tag </name>
            if (at_end() || peek() != '<')
                return make_error(data::error_code::unexpected_token);
            advance(); // <
            if (at_end() || peek() != '/')
                return make_error(data::error_code::unexpected_token);
            advance(); // /

            auto close_name = read_name();
            if (close_name.view() != tag_name.view())
                return make_error(data::error_code::unexpected_token);

            skip_whitespace();
            if (at_end() || peek() != '>')
                return make_error(data::error_code::unexpected_token);
            advance(); // >

            // Determine result type
            if (child_count > attr_count)
            {
                // Has child elements (possibly with attributes) → mapping
                auto start = doc_.pool_size_;
                for (std::size_t i = 0; i < child_count; ++i)
                    doc_.pool_[doc_.pool_size_++] = children[i];
                return pool_entry{tag_name, value::make_mapping(start, child_count)};
            }

            auto trimmed = trim_view(text_buf.view());

            if (attr_count > 0)
            {
                // Attributes + text → mapping with attrs, text is ignored if empty
                if (!trimmed.empty())
                {
                    // Store text under "_text" key
                    if (child_count >= DATA_CT_MAX_ITEMS)
                        return make_error(data::error_code::invalid_syntax);
                    children[child_count++] = pool_entry{string_type{"_text"}, detect_scalar(trimmed)};
                }
                auto start = doc_.pool_size_;
                for (std::size_t i = 0; i < child_count; ++i)
                    doc_.pool_[doc_.pool_size_++] = children[i];
                return pool_entry{tag_name, value::make_mapping(start, child_count)};
            }

            // Text only → auto-detect scalar
            return pool_entry{tag_name, detect_scalar(trimmed)};
        }

        std::string_view input_;
        document &doc_;
        std::size_t pos_{0};
        std::size_t line_{1};
        std::size_t col_{1};
    };

} // namespace data::xml::detail
