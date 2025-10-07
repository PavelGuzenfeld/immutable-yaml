#pragma once

#include <memory> // Added for recursive_wrapper
#include <new>    // Added for placement new
#include <optional>
#include <string_view>
#include <utility>
#include <variant>
#include <yaml/detail/string_storage.hpp> // for string_storage
#include <yaml/detail/utils.hpp>          // for is_alpha, is_digit, etc.

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

    template <std::size_t MaxTokens = 1024>
    using token_array = std::array<token, MaxTokens>;

    // yaml value types that actually make sense
    struct null_t
    {
        constexpr bool operator==(const null_t &) const = default;
        constexpr auto operator<=>(const null_t &) const = default;
    };
    using boolean = bool;
    using integer = std::int64_t;
    using floating = double;

    // Forward declare yaml_container for use in yaml_value
    struct yaml_container;

    // The main value type - forward declared yaml_container works fine in variant
    using yaml_value = std::variant
        null_t,
          boolean,
          integer,
          floating,
          string_storage<256>,
          yaml_container > ;

    // NOW define sequence_impl and mapping_impl with complete yaml_value
    // these fuckers need to come BEFORE yaml_container implementation
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
            -> yaml_value const &
        {
            return items_[idx]; // bounds checking is for debug builds
        }

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
            -> pair_type const &
        {
            return pairs_[idx];
        }

    private:
        std::array<pair_type, 64> pairs_{};
        std::size_t size_{0};
    };

    // NOW we can properly implement yaml_container with complete types
    // this is how you do type erasure without being a complete moron
    struct yaml_container
    {
        enum class type
        {
            none,
            sequence,
            mapping
        };
        type kind_{type::none};

        // Use aligned storage to store either type - this is actually clever unlike your previous attempt
        alignas(alignof(sequence_impl) > alignof(mapping_impl) ? alignof(sequence_impl) : alignof(mapping_impl)) char storage_[sizeof(sequence_impl) > sizeof(mapping_impl) ? sizeof(sequence_impl) : sizeof(mapping_impl)];

        constexpr yaml_container() noexcept = default;

        constexpr yaml_container(sequence_impl &&s) noexcept : kind_{type::sequence}
        {
            new (storage_) sequence_impl(std::move(s));
        }

        constexpr yaml_container(mapping_impl &&m) noexcept : kind_{type::mapping}
        {
            new (storage_) mapping_impl(std::move(m));
        }

        constexpr yaml_container(yaml_container const &other) noexcept : kind_{other.kind_}
        {
            if (kind_ == type::sequence)
            {
                new (storage_) sequence_impl(*reinterpret_cast<sequence_impl const *>(other.storage_));
            }
            else if (kind_ == type::mapping)
            {
                new (storage_) mapping_impl(*reinterpret_cast<mapping_impl const *>(other.storage_));
            }
        }

        constexpr yaml_container(yaml_container &&other) noexcept : kind_{other.kind_}
        {
            if (kind_ == type::sequence)
            {
                new (storage_) sequence_impl(std::move(*reinterpret_cast<sequence_impl *>(other.storage_)));
            }
            else if (kind_ == type::mapping)
            {
                new (storage_) mapping_impl(std::move(*reinterpret_cast<mapping_impl *>(other.storage_)));
            }
        }

        constexpr ~yaml_container() noexcept
        {
            if (kind_ == type::sequence)
            {
                reinterpret_cast<sequence_impl *>(storage_)->~sequence_impl();
            }
            else if (kind_ == type::mapping)
            {
                reinterpret_cast<mapping_impl *>(storage_)->~mapping_impl();
            }
        }

        constexpr auto operator=(yaml_container const &other) noexcept -> yaml_container &
        {
            if (this != &other)
            {
                this->~yaml_container();
                new (this) yaml_container(other);
            }
            return *this;
        }

        constexpr auto operator=(yaml_container &&other) noexcept -> yaml_container &
        {
            if (this != &other)
            {
                this->~yaml_container();
                new (this) yaml_container(std::move(other));
            }
            return *this;
        }

        [[nodiscard]] constexpr auto as_sequence() noexcept -> sequence_impl &
        {
            return *reinterpret_cast<sequence_impl *>(storage_);
        }

        [[nodiscard]] constexpr auto as_sequence() const noexcept -> sequence_impl const &
        {
            return *reinterpret_cast<sequence_impl const *>(storage_);
        }

        [[nodiscard]] constexpr auto as_mapping() noexcept -> mapping_impl &
        {
            return *reinterpret_cast<mapping_impl *>(storage_);
        }

        [[nodiscard]] constexpr auto as_mapping() const noexcept -> mapping_impl const &
        {
            return *reinterpret_cast<mapping_impl const *>(storage_);
        }

        [[nodiscard]] constexpr auto is_sequence() const noexcept -> bool { return kind_ == type::sequence; }
        [[nodiscard]] constexpr auto is_mapping() const noexcept -> bool { return kind_ == type::mapping; }
    };

    // Template aliases for backward compatibility - because you probably have other broken code
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

        constexpr document() noexcept = default;
        constexpr explicit document(value_type root) noexcept
            : root_{std::move(root)} {}
    };

} // namespace yaml::ct::detail