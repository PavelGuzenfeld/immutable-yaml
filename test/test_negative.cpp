// negative test cases because apparently you need to learn
// what happens when your yaml is fucking garbage

#include <iostream>
#include <yaml/yaml.hpp> // fixed your fucking include path

int main()
{
    std::cout << "testing negative cases...\n";

    // these should all fail at compile time
    static_assert(!yaml::ct::is_valid(R"({key: value, key: duplicate})"),
                  "duplicate keys should be invalid");

    static_assert(!yaml::ct::is_valid(R"([1, 2, 3,])"),
                  "trailing comma should be invalid");

    static_assert(!yaml::ct::is_valid(R"(key: "unterminated string)"),
                  "unterminated string should be invalid");

    static_assert(!yaml::ct::is_valid(R"(: invalid key)"),
                  "missing key should be invalid");

    std::cout << "all negative tests passed - your validation works!\n";
    return 0;
}