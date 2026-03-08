#include <immutable_data/xml.hpp>
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
    data::xml::detail::parser parser{input, doc};
    auto result = parser.parse_document();
    (void)result;
    return 0;
}
