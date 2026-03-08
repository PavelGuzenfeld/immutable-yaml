#include <immutable_data/json.hpp>
#include <cstdint>
#include <cstddef>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size == 0 || size > 4096)
        return 0;

    constexpr size_t MAX = 4096;
    char buf[MAX + 1]{};
    for (size_t i = 0; i < size && i < MAX; ++i)
        buf[i] = static_cast<char>(data[i]);
    buf[size] = '\0';

    std::string_view input{buf, size};

    data::detail::document doc{};
    data::json::detail::lexer<1024> lex{};
    auto tokens_result = lex.tokenize(input);
    if (std::holds_alternative<data::parse_error>(tokens_result))
        return 0;

    auto const &tokens = std::get<data::detail::token_array<1024>>(tokens_result);
    data::json::detail::parser<1024> parser{tokens, doc};
    auto result = parser.parse_document();
    (void)result;
    return 0;
}
