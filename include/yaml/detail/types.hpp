#pragma once

#include <array>
#include <memory> // for std::construct_at, std::destroy_at
#include <optional>
#include <string_view>
#include <utility>
#include <yaml/detail/string_storage.hpp>
#include <yaml/detail/utils.hpp>

// Configurable capacity limits
#ifndef YAML_CT_MAX_STRING_SIZE
#define YAML_CT_MAX_STRING_SIZE 256
#endif

#ifndef YAML_CT_MAX_ITEMS
#define YAML_CT_MAX_ITEMS 64
#endif

#ifndef YAML_CT_MAX_NODES
#define YAML_CT_MAX_NODES (YAML_CT_MAX_ITEMS * 4)
#endif

namespace yaml::ct
{
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
        eof,
        newline,
        space,
        tab,
        document_start,
        document_end,
        sequence_entry,
        mapping_key,
        mapping_value,
        sequence_start,
        sequence_end,
        mapping_start,
        mapping_end,
        string_literal,
        integer_literal,
        float_literal,
        boolean_literal,
        null_literal,
        anchor,
        alias,
        tag,
        quoted_string,
        literal_string,
        folded_string,
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

    // scalar type aliases
    struct null_t
    {
        constexpr bool operator==(null_t const &) const = default;
        constexpr auto operator<=>(null_t const &) const = default;
    };
    using boolean = bool;
    using integer = std::int64_t;
    using floating = double;
    using string_type = string_storage<YAML_CT_MAX_STRING_SIZE>;

    // container reference — index range into document's node pool
    // no default member initializers: keeps trivial default ctor for constexpr union activation
    struct container_ref
    {
        std::size_t start;
        std::size_t count;
    };

    // yaml_value — flat tagged union, no recursive types
    struct yaml_value
    {
        enum class kind : std::uint8_t
        {
            null,
            boolean,
            integer,
            floating,
            string,
            sequence,
            mapping
        };

        kind kind_{kind::null};

        union data_t
        {
            constexpr data_t() noexcept : dummy_{} {}
            constexpr ~data_t() noexcept {}
            char dummy_;
            bool bool_;
            std::int64_t int_;
            double float_;
            string_type str_;
            container_ref children_;
        } data_{};

        constexpr yaml_value() noexcept = default;

        constexpr yaml_value(yaml_value const &o) noexcept : kind_{o.kind_}
        {
            switch (kind_)
            {
            case kind::string:
                std::construct_at(&data_.str_, o.data_.str_);
                break;
            case kind::boolean:
                data_.bool_ = o.data_.bool_;
                break;
            case kind::integer:
                data_.int_ = o.data_.int_;
                break;
            case kind::floating:
                data_.float_ = o.data_.float_;
                break;
            case kind::sequence:
            case kind::mapping:
                data_.children_ = o.data_.children_;
                break;
            default:
                break;
            }
        }

        constexpr yaml_value(yaml_value &&o) noexcept : kind_{o.kind_}
        {
            switch (kind_)
            {
            case kind::string:
                std::construct_at(&data_.str_, std::move(o.data_.str_));
                break;
            case kind::boolean:
                data_.bool_ = o.data_.bool_;
                break;
            case kind::integer:
                data_.int_ = o.data_.int_;
                break;
            case kind::floating:
                data_.float_ = o.data_.float_;
                break;
            case kind::sequence:
            case kind::mapping:
                data_.children_ = o.data_.children_;
                break;
            default:
                break;
            }
        }

        constexpr ~yaml_value() noexcept
        {
            if (kind_ == kind::string)
                std::destroy_at(&data_.str_);
        }

        constexpr auto operator=(yaml_value const &o) noexcept -> yaml_value &
        {
            if (this != &o)
            {
                std::destroy_at(this);
                std::construct_at(this, o);
            }
            return *this;
        }

        constexpr auto operator=(yaml_value &&o) noexcept -> yaml_value &
        {
            if (this != &o)
            {
                std::destroy_at(this);
                std::construct_at(this, std::move(o));
            }
            return *this;
        }

        // factory methods
        static constexpr auto make_null() noexcept -> yaml_value { return {}; }

        static constexpr auto make_bool(bool b) noexcept -> yaml_value
        {
            yaml_value v;
            v.kind_ = kind::boolean;
            v.data_.bool_ = b;
            return v;
        }

        static constexpr auto make_int(std::int64_t i) noexcept -> yaml_value
        {
            yaml_value v;
            v.kind_ = kind::integer;
            v.data_.int_ = i;
            return v;
        }

        static constexpr auto make_float(double f) noexcept -> yaml_value
        {
            yaml_value v;
            v.kind_ = kind::floating;
            v.data_.float_ = f;
            return v;
        }

        static constexpr auto make_string(string_type s) noexcept -> yaml_value
        {
            yaml_value v;
            v.kind_ = kind::string;
            std::construct_at(&v.data_.str_, std::move(s));
            return v;
        }

        static constexpr auto make_sequence(std::size_t start, std::size_t count) noexcept -> yaml_value
        {
            yaml_value v;
            v.kind_ = kind::sequence;
            v.data_.children_ = {start, count};
            return v;
        }

        static constexpr auto make_mapping(std::size_t start, std::size_t count) noexcept -> yaml_value
        {
            yaml_value v;
            v.kind_ = kind::mapping;
            v.data_.children_ = {start, count};
            return v;
        }

        // type checks
        [[nodiscard]] constexpr auto is_null() const noexcept -> bool { return kind_ == kind::null; }
        [[nodiscard]] constexpr auto is_bool() const noexcept -> bool { return kind_ == kind::boolean; }
        [[nodiscard]] constexpr auto is_int() const noexcept -> bool { return kind_ == kind::integer; }
        [[nodiscard]] constexpr auto is_float() const noexcept -> bool { return kind_ == kind::floating; }
        [[nodiscard]] constexpr auto is_string() const noexcept -> bool { return kind_ == kind::string; }
        [[nodiscard]] constexpr auto is_sequence() const noexcept -> bool { return kind_ == kind::sequence; }
        [[nodiscard]] constexpr auto is_mapping() const noexcept -> bool { return kind_ == kind::mapping; }

        // scalar accessors
        [[nodiscard]] constexpr auto as_bool() const noexcept -> bool { return data_.bool_; }
        [[nodiscard]] constexpr auto as_int() const noexcept -> std::int64_t { return data_.int_; }
        [[nodiscard]] constexpr auto as_float() const noexcept -> double { return data_.float_; }
        [[nodiscard]] constexpr auto as_string() const noexcept -> std::string_view { return data_.str_.view(); }
    };

    // pool entry — a value with an optional key (for mapping entries)
    struct pool_entry
    {
        string_type key{};
        yaml_value value{};
    };

    // document — holds the root value and a flat pool of all container children
    struct document
    {
        yaml_value root_{};
        std::array<pool_entry, YAML_CT_MAX_NODES> pool_{};
        std::size_t pool_size_{0};

        constexpr document() noexcept = default;

        // allocate contiguous entries in the pool, return start index
        constexpr auto alloc(std::size_t count) noexcept -> std::size_t
        {
            auto start = pool_size_;
            pool_size_ += count;
            return start;
        }

        // find a key in a mapping value
        [[nodiscard]] constexpr auto find(yaml_value const &v, std::string_view key) const noexcept
            -> std::optional<yaml_value>
        {
            if (v.kind_ != yaml_value::kind::mapping)
                return std::nullopt;
            for (std::size_t i = 0; i < v.data_.children_.count; ++i)
            {
                auto &entry = pool_[v.data_.children_.start + i];
                if (entry.key.view() == key)
                    return entry.value;
            }
            return std::nullopt;
        }

        // get sequence element by index
        [[nodiscard]] constexpr auto at(yaml_value const &v, std::size_t idx) const noexcept
            -> yaml_value const &
        {
            return pool_[v.data_.children_.start + idx].value;
        }

        // get container size (sequence or mapping)
        [[nodiscard]] constexpr auto size(yaml_value const &v) const noexcept -> std::size_t
        {
            if (v.kind_ == yaml_value::kind::sequence || v.kind_ == yaml_value::kind::mapping)
                return v.data_.children_.count;
            return 0;
        }

        // get mapping key at index
        [[nodiscard]] constexpr auto key_at(yaml_value const &v, std::size_t idx) const noexcept
            -> std::string_view
        {
            return pool_[v.data_.children_.start + idx].key.view();
        }
    };

} // namespace yaml::ct::detail
