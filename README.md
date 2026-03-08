# immutable-yaml

[![CI](https://github.com/PavelGuzenfeld/immutable-yaml/actions/workflows/ci.yml/badge.svg)](https://github.com/PavelGuzenfeld/immutable-yaml/actions/workflows/ci.yml)

Compile-time YAML parser for C++23. Parse YAML at compile time, embed configs as `constexpr` data with zero runtime overhead.

## Features

- **Compile-time parsing** — all YAML parsing happens at compile time via `constexpr`
- **Zero runtime overhead** — parsed data lives in static storage, no heap allocations
- **CMake integration** — `yaml_embed()` auto-generates headers from YAML files with optimal sizing
- **Compile-time validation** — catches syntax errors, duplicate keys, and type issues before your code runs
- **Comment stripping** — full-line comments are stripped at build time; inline comments are handled by the lexer
- **Iteration** — range-based for over sequences (`values()`) and mapping entries (`entries()`)
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

## Examples

Full working examples in [`examples/`](examples/).

### Inline constexpr parsing ([basic.cpp](examples/basic.cpp))

```cpp
#include <immutable_yaml/immutable_yaml.hpp>

constexpr auto doc = yaml::ct::parse_or_throw(R"(
server:
  host: "0.0.0.0"
  port: 8080
  workers: 4
debug: false
)");

static_assert(doc.root_.is_mapping());
static_assert(yaml::ct::is_valid(R"(key: value)"));
static_assert(!yaml::ct::is_valid(R"({a: 1, a: duplicate})"));

int main() {
    auto server = doc.find(doc.root_, "server");
    // server->as_string(), as_int(), as_bool(), as_float()
    assert(doc.find(*server, "port")->as_int() == 8080);
}
```

### Embed YAML files ([embed.cpp](examples/embed.cpp))

```cmake
# CMakeLists.txt
include(YamlEmbed)
yaml_embed(my_app app_config.yaml)
```

```cpp
#include "app_config.yaml.hpp"

int main() {
    constexpr auto& cfg = yaml::embedded::app_config;

    auto db = cfg.find(cfg.root_, "database");
    auto host = cfg.find(*db, "host");   // yaml_value const*
    auto port = cfg.find(*db, "port")->as_int();
    // host->as_string(), host->as_int(), etc.
}
```

### Iterate sequences and mappings ([iterate.cpp](examples/iterate.cpp))

```cpp
constexpr auto doc = yaml::ct::parse_or_throw(R"(
services:
  web:
    port: 80
    proto: "http"
  api:
    port: 443
    proto: "https"
ports:
  - 80
  - 443
  - 1883
)");

int main() {
    // Iterate mapping entries with structured bindings
    auto services = doc.find(doc.root_, "services");
    for (auto [name, svc] : doc.entries(*services)) {
        auto port = doc.find(svc, "port")->as_int();
        auto proto = doc.find(svc, "proto")->as_string();  // safe: pointer into static storage
    }

    // Iterate sequence values
    auto ports = doc.find(doc.root_, "ports");
    for (auto const& val : doc.values(*ports)) {
        val.as_int();  // 80, 443, 1883
    }
}
```

## API Reference

```cpp
// Parse (returns std::variant<document, error_code>)
constexpr auto result = yaml::ct::parse(R"(key: value)");

// Parse or compile-time error
constexpr auto doc = yaml::ct::parse_or_throw(R"(key: value)");

// Compile-time validation
constexpr bool ok = yaml::ct::is_valid(R"(key: value)");

// Document access
doc.find(node, "key")      // -> yaml_value const* (nullptr if not found)
doc.at(node, index)        // -> yaml_value const&
doc.size(node)             // -> std::size_t
doc.key_at(node, index)    // -> std::string_view

// Iteration
doc.values(node)           // range of yaml_value (sequence or mapping)
doc.entries(node)           // range of {key, value} (mapping only)

// Scalar accessors
val.as_string()            // -> std::string_view
val.as_int()               // -> int64_t
val.as_float()             // -> double
val.as_bool()              // -> bool

// Type checks
val.is_null()   val.is_bool()     val.is_int()
val.is_float()  val.is_string()   val.is_sequence()
val.is_mapping()
```

## How Sizing Works

`yaml_embed()` analyzes each YAML file at CMake configure time and sets optimal pool sizes per-target — no manual tuning required. For inline `constexpr` usage without `yaml_embed()`, generous defaults apply. Override them only if needed via `#define` before including the header (`YAML_CT_MAX_STRING_SIZE`, `YAML_CT_MAX_ITEMS`, `YAML_CT_MAX_NODES`, `YAML_CT_MAX_TOKENS`).

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

Tests and examples are only built when `BUILD_TESTING=ON` and the project is the top-level CMake project. Consumers via `FetchContent` or `add_subdirectory` get only the header-only library target.

## License

MIT
