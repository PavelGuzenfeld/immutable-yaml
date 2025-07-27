#pragma once

// jesus fucking christ, the amount of type safety i'm about to implement
// will make your head spin. this is what happens when you actually
// understand c++ instead of just cargo-culting from tutorials

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>

namespace yaml::ct
{
    // forward declare error_code because apparently you don't understand dependencies
    enum class [[nodiscard]] error_code : std::uint8_t
    {
        none = 0,
        invalid_syntax,
        unexpected_token,
        invalid_indentation,
        unterminated_string,
        invalid_escape_sequence,
        duplicate_key,
        invalid_document_start,
        invalid_document_end,
        cyclic_reference,
        unsupported_feature
    };
}

namespace yaml::ct::detail
{

    // because you probably think yaml only has strings and numbers
    enum class [[nodiscard]] token_type : std::uint8_t
    {
        // basic tokens
        eof,
        newline,
        space,
        tab,

        // structural tokens
        document_start, // ---
        document_end,   // ...
        sequence_entry, // -
        mapping_key,    // :
        mapping_value,  // value after :

        // bracketed containers
        sequence_start, // [
        sequence_end,   // ]
        mapping_start,  // {
        mapping_end,    // }

        // scalars
        string_literal,
        integer_literal,
        float_literal,
        boolean_literal,
        null_literal,

        // yaml specific
        anchor, // &anchor
        alias,  // *alias
        tag,    // !!type

        // strings
        quoted_string,  // "string" or 'string'
        literal_string, // |
        folded_string,  // >

        // errors
        invalid
    };

    // token structure that doesn't make me want to quit programming
    struct token
    {
        token_type type_{token_type::invalid};
        std::string_view value_{};
        std::size_t line_{0};
        std::size_t column_{0};

        constexpr token() = default;
        constexpr token(token_type type, std::string_view value,
                        std::size_t line, std::size_t column) noexcept
            : type_{type}, value_{value}, line_{line}, column_{column} {}
    };

    // because dynamic allocation at compile time is still a pipe dream
    template <std::size_t MaxTokens = 1024>
    using token_array = std::array<token, MaxTokens>;

    // yaml value types that actually make sense
    struct null_t
    {
        constexpr bool operator==(const null_t &) const = default;
        constexpr auto operator<=>(const null_t &) const = default;
    };
    using boolean = bool;
    using integer = std::int64_t; // because int is for children
    using floating = double;      // precision matters, unlike your last project

    // compile-time string storage because std::string is runtime garbage
    template <std::size_t MaxSize>
    class string_storage
    {
    public:
        constexpr string_storage() = default;

        constexpr explicit string_storage(std::string_view str) noexcept
        {
            // bounds checking because we're not writing c
            const auto copy_size = std::min(str.size(), MaxSize - 1);
            for (std::size_t i = 0; i < copy_size; ++i)
            {
                data_[i] = str[i];
            }
            size_ = copy_size;
            data_[size_] = '\0';
        }

        [[nodiscard]] constexpr auto view() const noexcept -> std::string_view
        {
            return {data_.data(), size_};
        }

        [[nodiscard]] constexpr auto size() const noexcept -> std::size_t
        {
            return size_;
        }

        [[nodiscard]] constexpr auto operator<=>(const string_storage &) const = default;

    private:
        std::array<char, MaxSize> data_{};
        std::size_t size_{0};
    };

    // Helper constexpr functions for character classification
    constexpr bool is_alpha(char c) noexcept
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    constexpr bool is_digit(char c) noexcept
    {
        return c >= '0' && c <= '9';
    }

    constexpr bool is_alnum(char c) noexcept
    {
        return is_alpha(c) || is_digit(c);
    }

    // Forward declarations to break circular dependency
    struct sequence_impl;
    struct mapping_impl;

    // The main value type - fixed to avoid circular template dependency
    using yaml_value = std::variant<
        null_t,
        boolean,
        integer,
        floating,
        string_storage<256>,
        sequence_impl,
        mapping_impl>;

    // sequence implementation that doesn't make me cry
    struct sequence_impl
    {
        constexpr sequence_impl() = default;

        constexpr auto push_back(yaml_value val) noexcept -> bool
        {
            if (size_ >= 64)
                return false; // overflow protection
            items_[size_++] = std::move(val);
            return true;
        }

        [[nodiscard]] constexpr auto size() const noexcept -> std::size_t
        {
            return size_;
        }

        [[nodiscard]] constexpr auto operator[](std::size_t idx) const noexcept
            -> const yaml_value &
        {
            return items_[idx]; // bounds checking is for debug builds
        }

        // Comparison operators removed to avoid circular dependency issues with variants

    private:
        std::array<yaml_value, 64> items_{};
        std::size_t size_{0};
    };

    // mapping implementation because apparently maps are hard
    struct mapping_impl
    {
        using key_type = string_storage<256>;
        using pair_type = std::pair<key_type, yaml_value>;

        constexpr mapping_impl() = default;

        constexpr auto insert(key_type key, yaml_value val) noexcept -> bool
        {
            // check for duplicates because yaml doesn't allow them
            for (std::size_t i = 0; i < size_; ++i)
            {
                if (pairs_[i].first == key)
                {
                    return false; // duplicate key
                }
            }

            if (size_ >= 64)
                return false; // overflow protection
            pairs_[size_++] = {std::move(key), std::move(val)};
            return true;
        }

        [[nodiscard]] constexpr auto find(std::string_view key) const noexcept
            -> std::optional<yaml_value>
        {
            for (std::size_t i = 0; i < size_; ++i)
            {
                if (pairs_[i].first.view() == key)
                {
                    return pairs_[i].second;
                }
            }
            return std::nullopt;
        }

        [[nodiscard]] constexpr auto size() const noexcept -> std::size_t
        {
            return size_;
        }

        [[nodiscard]] constexpr auto operator[](std::size_t idx) const noexcept
            -> const pair_type &
        {
            return pairs_[idx];
        }

        // Comparison operators removed to avoid circular dependency issues with variants

    private:
        std::array<pair_type, 64> pairs_{};
        std::size_t size_{0};
    };

    // Template aliases for backward compatibility
    template <std::size_t MaxItems = 64>
    using sequence = sequence_impl;

    template <std::size_t MaxPairs = 64>
    using mapping = mapping_impl;

    template <std::size_t MaxStringSize = 256, std::size_t MaxItems = 64>
    using value = yaml_value;

    // document structure because yaml can have multiple documents
    struct document
    {
        using value_type = yaml_value;
        value_type root_{};

        constexpr document() = default;
        constexpr explicit document(value_type root) noexcept
            : root_{std::move(root)} {}

        // Comparison operators removed to avoid circular dependency issues with variants
    };

} // namespace yaml::ct::detail