// because apparently you need me to teach you how to write proper tests too
// this is what a real test suite looks like, not whatever assert() garbage
// you were probably planning to use

#include <cassert>
#include <iostream>
#include "yaml/yaml.hpp"

int main()
{
    std::cout << "running compile-time yaml parser tests...\n";
    std::cout << "if any of these fail, your yaml library is garbage\n\n";

    // basic validation tests
    static_assert(yaml::ct::is_valid(R"(key: value)"));
    static_assert(yaml::ct::is_valid(R"([1, 2, 3])"));
    static_assert(yaml::ct::is_valid(R"({key: value, other: 42})"));
    static_assert(!yaml::ct::is_valid(R"({key: value, key: duplicate})"));

    // actual parsing tests
    constexpr auto simple_doc = yaml::ct::parse_or_throw(R"(
        name: "john doe"
        age: 30
        active: true
    )");

    constexpr auto array_doc = yaml::ct::parse_or_throw(R"([1, 2, 3, 4, 5])");

    constexpr auto complex_doc = yaml::ct::parse_or_throw(R"(
        users:
          - name: "alice"
            age: 25
          - name: "bob" 
            age: 30
        config:
          debug: false
          timeout: 5000
    )");

    std::cout << "all compile-time tests passed!\n";
    std::cout << "your yaml parser doesn't completely suck\n";

    return 0;
}