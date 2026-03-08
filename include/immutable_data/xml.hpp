#pragma once

#include <immutable_data/detail/xml_parser.hpp>
#include <immutable_data/detail/types.hpp>

#include <string_view>
#include <variant>

namespace data::xml
{

    using data::detail::document;
    using data::parse_error;

    template <typename T>
    using result = std::variant<T, parse_error>;

    template <std::size_t N>
    constexpr auto parse(const char (&str)[N]) noexcept -> result<document>
    {
        if constexpr (N <= 1)
            return parse_error{error_code::invalid_syntax, 0, 0};

        std::string_view view{str, N - 1};
        data::detail::document doc{};
        detail::parser p{view, doc};
        return p.parse_document();
    }

    template <std::size_t N>
    constexpr auto parse_or_throw(const char (&str)[N]) -> document
    {
        auto r = parse(str);
        if (std::holds_alternative<document>(r))
            return std::get<document>(r);
        throw "XML parse error";
    }

    template <std::size_t N>
    constexpr auto is_valid(const char (&str)[N]) noexcept -> bool
    {
        auto r = parse(str);
        return std::holds_alternative<document>(r);
    }

} // namespace data::xml
