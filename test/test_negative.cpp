#include <immutable_yaml/immutable_yaml.hpp>

int main()
{
    static_assert(!yaml::ct::is_valid(R"({key: value, key: duplicate})"),
                  "duplicate keys should be invalid");

    static_assert(!yaml::ct::is_valid(R"([1, 2, 3,])"),
                  "trailing comma should be invalid");

    static_assert(!yaml::ct::is_valid(R"(key: "unterminated string)"),
                  "unterminated string should be invalid");

    static_assert(!yaml::ct::is_valid(R"(: invalid key)"),
                  "missing key should be invalid");

    return 0;
}
