# immutable-yaml

a compile-time yaml parser that actually works and doesn't make you want to quit programming.

## features

- **compile-time parsing**: all yaml parsing happens at compile time
- **zero runtime overhead**: parsed data is available as constexpr values
- **full yaml syntax support**: scalars, sequences, mappings, flow/block styles
- **comprehensive validation**: catches yaml syntax errors at compile time
- **minimal dependencies**: header-only, requires only c++23 standard library
- **extensive testing**: includes negative test cases and edge cases

## requirements

- c++23 compatible compiler (gcc 13+, clang 16+, msvc 19.35+)
- cmake 3.21+

## usage

```cpp
#include <yaml/yaml.hpp>

// compile-time validation
static_assert(yaml::ct::is_valid(R"(
    name: "john doe"
    age: 30
    active: true
)"));

// compile-time parsing
constexpr auto config = yaml::ct::parse_or_throw(R"(
    database:
        host: "localhost"
        port: 5432
        ssl: true
    cache:
        ttl: 3600
        size: 1000
)");

// access parsed data at compile time
constexpr auto db_host = std::get<yaml::ct::detail::mapping<64>>(config.root_)
    .find("database")->find("host");