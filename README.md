# immutable-yaml

[![CI](https://github.com/PavelGuzenfeld/immutable-yaml/actions/workflows/ci.yml/badge.svg)](https://github.com/PavelGuzenfeld/immutable-yaml/actions/workflows/ci.yml)

Compile-time YAML parser for C++23. Parse YAML at compile time, embed configs as `constexpr` data with zero runtime overhead.

## Features

- **Compile-time parsing** — all YAML parsing happens at compile time via `constexpr`
- **Zero runtime overhead** — parsed data lives in static storage, no heap allocations
- **CMake integration** — `yaml_embed()` auto-generates headers from YAML files with optimal sizing
- **Compile-time validation** — catches syntax errors, duplicate keys, and type issues before your code runs
- **Comment stripping** — full-line comments are stripped at build time; inline comments are handled by the lexer
- **Header-only** — single include, no dependencies beyond C++23 standard library

## Requirements

- C++23 compiler (tested: GCC 15+)
- CMake 3.22+

## Installation

### CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    immutable-yaml
    GIT_REPOSITORY https://github.com/PavelGuzenfeld/immutable-yaml.git
    GIT_TAG v0.0.1
)
FetchContent_MakeAvailable(immutable-yaml)

target_link_libraries(my_app PRIVATE immutable-yaml)
```

### System install

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --install build --prefix /usr/local
```

## Quick Start

### Option 1: CMake yaml_embed (recommended)

Embed YAML files as compile-time constants. Allocation sizes are computed automatically from the YAML content.

```cmake
find_package(immutable-yaml REQUIRED)
include(YamlEmbed)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE immutable-yaml)
yaml_embed(my_app config.yaml)
```

```cpp
#include "config.yaml.hpp"

int main() {
    constexpr auto& doc = yaml::embedded::config;

    auto host = doc.find(doc.root_, "host");
    if (host) {
        // host->as_string() returns std::string_view
    }
}
```

### Option 2: Inline constexpr

```cpp
#include <immutable_yaml/immutable_yaml.hpp>

constexpr auto config = yaml::ct::parse_or_throw(R"(
database:
  host: "localhost"
  port: 5432
  ssl: true
)");

// Compile-time validation
static_assert(yaml::ct::is_valid(R"(key: value)"));
static_assert(!yaml::ct::is_valid(R"({a: 1, a: duplicate})"));
```

## API

The parsed document uses a flat node pool architecture. Scalars are accessed directly on `yaml_value`; containers are navigated through the `document`.

```cpp
#include <immutable_yaml/immutable_yaml.hpp>

constexpr auto doc = yaml::ct::parse_or_throw(R"(
name: "test"
count: 42
pi: 3.14
active: true
items:
  - "one"
  - "two"
)");

// Type checks
doc.root_.is_mapping();   // true

// Mapping lookup — returns std::optional<yaml_value>
auto name = doc.find(doc.root_, "name");
name->as_string();        // "test" (std::string_view)

auto count = doc.find(doc.root_, "count");
count->as_int();          // 42 (int64_t)

auto pi = doc.find(doc.root_, "pi");
pi->as_float();           // 3.14 (double)

auto active = doc.find(doc.root_, "active");
active->as_bool();        // true

// Sequence access
auto items = doc.find(doc.root_, "items");
doc.size(*items);         // 2
doc.at(*items, 0).as_string();  // "one"

// Type checks on any value
value.is_null();
value.is_bool();
value.is_int();
value.is_float();
value.is_string();
value.is_sequence();
value.is_mapping();

// Error handling without exceptions
constexpr auto result = yaml::ct::parse(R"({a: 1, a: 2})");
// result is std::variant<document, error_code>
// error_code::duplicate_key in this case
```

## Capacity Limits

Defaults work for most configs. Override via CMake definitions or `#define` before including:

| Macro | Default | Description |
|-------|---------|-------------|
| `YAML_CT_MAX_STRING_SIZE` | 256 | Max bytes per string value |
| `YAML_CT_MAX_ITEMS` | 64 | Max entries per sequence/mapping level |
| `YAML_CT_MAX_NODES` | 256 | Total node pool size across all nesting levels |
| `YAML_CT_MAX_TOKENS` | 1024 | Max lexer tokens |

When using `yaml_embed()`, these are computed automatically from the YAML file content.

## Building & Testing

```bash
docker build -t yaml-test .
docker run --rm yaml-test
```

Or directly:

```bash
cmake -B build -DCMAKE_CXX_STANDARD=23 -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## License

MIT
