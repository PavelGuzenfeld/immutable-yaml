#pragma once

#include <array>
#include <memory> // for std::construct_at, std::destroy_at
#include <string_view>
#include <utility>
#include <immutable_data/detail/string_storage.hpp>
#include <immutable_data/detail/utils.hpp>

// Configurable capacity limits
#ifndef DATA_CT_MAX_STRING_SIZE
#define DATA_CT_MAX_STRING_SIZE 256
#endif

#ifndef DATA_CT_MAX_ITEMS
#define DATA_CT_MAX_ITEMS 64
#endif

#ifndef DATA_CT_MAX_NODES
#define DATA_CT_MAX_NODES (DATA_CT_MAX_ITEMS * 4)
#endif

namespace data
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
        unsupported_feature,
        trailing_comma,
    };
}

namespace data::detail
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
        comma,
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
    using string_type = string_storage<DATA_CT_MAX_STRING_SIZE>;

    // container reference — index range into document's node pool
    struct container_ref
    {
        std::size_t start;
        std::size_t count;
    };

    // value — flat tagged union for any hierarchical data (YAML, JSON, etc.)
    struct value
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

        constexpr value() noexcept = default;

        constexpr value(value const &o) noexcept : kind_{o.kind_}
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

        constexpr value(value &&o) noexcept : kind_{o.kind_}
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

        constexpr ~value() noexcept
        {
            if (kind_ == kind::string)
                std::destroy_at(&data_.str_);
        }

        constexpr auto operator=(value const &o) noexcept -> value &
        {
            if (this != &o)
            {
                std::destroy_at(this);
                std::construct_at(this, o);
            }
            return *this;
        }

        constexpr auto operator=(value &&o) noexcept -> value &
        {
            if (this != &o)
            {
                std::destroy_at(this);
                std::construct_at(this, std::move(o));
            }
            return *this;
        }

        // factory methods
        static constexpr auto make_null() noexcept -> value { return {}; }

        static constexpr auto make_bool(bool b) noexcept -> value
        {
            value v;
            v.kind_ = kind::boolean;
            v.data_.bool_ = b;
            return v;
        }

        static constexpr auto make_int(std::int64_t i) noexcept -> value
        {
            value v;
            v.kind_ = kind::integer;
            v.data_.int_ = i;
            return v;
        }

        static constexpr auto make_float(double f) noexcept -> value
        {
            value v;
            v.kind_ = kind::floating;
            v.data_.float_ = f;
            return v;
        }

        static constexpr auto make_string(string_type s) noexcept -> value
        {
            value v;
            v.kind_ = kind::string;
            std::construct_at(&v.data_.str_, std::move(s));
            return v;
        }

        static constexpr auto make_sequence(std::size_t start, std::size_t count) noexcept -> value
        {
            value v;
            v.kind_ = kind::sequence;
            v.data_.children_ = {start, count};
            return v;
        }

        static constexpr auto make_mapping(std::size_t start, std::size_t count) noexcept -> value
        {
            value v;
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
        value val_{};
    };

    // view for iterating sequence/mapping values
    struct value_view
    {
        pool_entry const *begin_;
        pool_entry const *end_;

        struct iterator
        {
            pool_entry const *ptr_;

            constexpr auto operator*() const noexcept -> data::detail::value const & { return ptr_->val_; }
            constexpr auto operator++() noexcept -> iterator & { ++ptr_; return *this; }
            constexpr auto operator!=(iterator const &o) const noexcept -> bool { return ptr_ != o.ptr_; }
            constexpr auto operator==(iterator const &o) const noexcept -> bool { return ptr_ == o.ptr_; }
        };

        [[nodiscard]] constexpr auto begin() const noexcept -> iterator { return {begin_}; }
        [[nodiscard]] constexpr auto end() const noexcept -> iterator { return {end_}; }
        [[nodiscard]] constexpr auto size() const noexcept -> std::size_t { return end_ - begin_; }
    };

    // key-value pair for mapping iteration
    struct entry_view_item
    {
        std::string_view key;
        data::detail::value const &value;
    };

    // view for iterating mapping key-value pairs
    struct entry_view
    {
        pool_entry const *begin_;
        pool_entry const *end_;

        struct iterator
        {
            pool_entry const *ptr_;

            constexpr auto operator*() const noexcept -> entry_view_item
            {
                return {ptr_->key.view(), ptr_->val_};
            }
            constexpr auto operator++() noexcept -> iterator & { ++ptr_; return *this; }
            constexpr auto operator!=(iterator const &o) const noexcept -> bool { return ptr_ != o.ptr_; }
            constexpr auto operator==(iterator const &o) const noexcept -> bool { return ptr_ == o.ptr_; }
        };

        [[nodiscard]] constexpr auto begin() const noexcept -> iterator { return {begin_}; }
        [[nodiscard]] constexpr auto end() const noexcept -> iterator { return {end_}; }
        [[nodiscard]] constexpr auto size() const noexcept -> std::size_t { return end_ - begin_; }
    };

    // document — holds the root value and a flat pool of all container children
    struct document
    {
        value root_{};
        std::array<pool_entry, DATA_CT_MAX_NODES> pool_{};
        std::size_t pool_size_{0};

        constexpr document() noexcept = default;

        constexpr auto alloc(std::size_t count) noexcept -> std::size_t
        {
            auto start = pool_size_;
            pool_size_ += count;
            return start;
        }

        [[nodiscard]] constexpr auto find(value const &v, std::string_view key) const noexcept
            -> value const *
        {
            if (v.kind_ != value::kind::mapping)
                return nullptr;
            for (std::size_t i = 0; i < v.data_.children_.count; ++i)
            {
                auto &entry = pool_[v.data_.children_.start + i];
                if (entry.key.view() == key)
                    return &entry.val_;
            }
            return nullptr;
        }

        [[nodiscard]] constexpr auto at(value const &v, std::size_t idx) const noexcept
            -> value const &
        {
            return pool_[v.data_.children_.start + idx].val_;
        }

        [[nodiscard]] constexpr auto size(value const &v) const noexcept -> std::size_t
        {
            if (v.kind_ == value::kind::sequence || v.kind_ == value::kind::mapping)
                return v.data_.children_.count;
            return 0;
        }

        [[nodiscard]] constexpr auto key_at(value const &v, std::size_t idx) const noexcept
            -> std::string_view
        {
            return pool_[v.data_.children_.start + idx].key.view();
        }

        [[nodiscard]] constexpr auto values(value const &v) const noexcept -> value_view
        {
            if (v.kind_ != value::kind::sequence && v.kind_ != value::kind::mapping)
                return {pool_.data(), pool_.data()};
            auto *base = pool_.data() + v.data_.children_.start;
            return {base, base + v.data_.children_.count};
        }

        [[nodiscard]] constexpr auto entries(value const &v) const noexcept -> entry_view
        {
            if (v.kind_ != value::kind::mapping)
                return {pool_.data(), pool_.data()};
            auto *base = pool_.data() + v.data_.children_.start;
            return {base, base + v.data_.children_.count};
        }
    };

} // namespace data::detail
