# immutable-data-embedder

[![CI](https://github.com/PavelGuzenfeld/immutable-data-embedder/actions/workflows/ci.yml/badge.svg)](https://github.com/PavelGuzenfeld/immutable-data-embedder/actions/workflows/ci.yml)

Compile-time YAML and JSON parser for C++23. Parse data at compile time, embed configs as `constexpr` data with zero runtime overhead.

## Features

- **Compile-time parsing** — all YAML and JSON parsing happens at compile time via `constexpr`
- **Zero runtime overhead** — parsed data lives in static storage, no heap allocations
- **Dual format** — unified API for both YAML and JSON with shared type system
- **CMake integration** — `data_embed()` auto-generates headers from YAML/JSON files with optimal sizing
- **Compile-time validation** — catches syntax errors, duplicate keys, and type issues before your code runs
- **Iteration** — range-based for over sequences (`values()`) and mapping entries (`entries()`)
- **Header-only** — single include per format, no dependencies beyond C++23 standard library

## Requirements

- C++23 compiler (tested: GCC 15+)
- CMake 3.22+

## Installation

### CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    immutable-data-embedder
    GIT_REPOSITORY https://github.com/PavelGuzenfeld/immutable-data-embedder.git
    GIT_TAG v0.2.0
)
FetchContent_MakeAvailable(immutable-data-embedder)

target_link_libraries(my_app PRIVATE immutable-data-embedder)
```

### System install

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --install build --prefix /usr/local
```

## Examples

Full working examples in [`examples/`](examples/).

### YAML — inline constexpr parsing ([basic.cpp](examples/basic.cpp))

```cpp
#include <immutable_data/yaml.hpp>

constexpr auto doc = data::yaml::parse_or_throw(R"(
server:
  host: "0.0.0.0"
  port: 8080
  workers: 4
debug: false
)");

static_assert(doc.root_.is_mapping());
static_assert(data::yaml::is_valid(R"(key: value)"));
static_assert(!data::yaml::is_valid(R"({a: 1, a: duplicate})"));

int main() {
    auto server = doc.find(doc.root_, "server");
    assert(doc.find(*server, "port")->as_int() == 8080);
}
```

### JSON — inline constexpr parsing ([json_basic.cpp](examples/json_basic.cpp))

```cpp
#include <immutable_data/json.hpp>

constexpr auto doc = data::json::parse_or_throw(R"({
    "server": {
        "host": "0.0.0.0",
        "port": 8080,
        "workers": 4
    },
    "debug": false,
    "tags": ["web", "api", "prod"]
})");

static_assert(doc.root_.is_mapping());
static_assert(data::json::is_valid(R"({"key": "value"})"));
static_assert(!data::json::is_valid(R"({"a": 1, "a": 2})"));

int main() {
    auto server = doc.find(doc.root_, "server");
    assert(doc.find(*server, "port")->as_int() == 8080);
    assert(doc.find(doc.root_, "debug")->as_bool() == false);
}
```

### Embed data files ([embed.cpp](examples/embed.cpp))

```cmake
# CMakeLists.txt
include(DataEmbed)
data_embed(my_app app_config.yaml)   # auto-detects format from extension
data_embed(my_app settings.json)     # works with JSON too
```

```cpp
#include "app_config.yaml.hpp"
#include "settings.json.hpp"

int main() {
    constexpr auto& cfg = data::embedded::app_config;
    auto db = cfg.find(cfg.root_, "database");
    auto port = cfg.find(*db, "port")->as_int();

    constexpr auto& settings = data::embedded::settings;
    // same API for JSON-embedded data
}
```

### Iterate sequences and mappings ([iterate.cpp](examples/iterate.cpp))

```cpp
constexpr auto doc = data::yaml::parse_or_throw(R"(
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
    auto services = doc.find(doc.root_, "services");
    for (auto [name, svc] : doc.entries(*services)) {
        auto port = doc.find(svc, "port")->as_int();
        auto proto = doc.find(svc, "proto")->as_string();
    }

    auto ports = doc.find(doc.root_, "ports");
    for (auto const& val : doc.values(*ports)) {
        val.as_int();  // 80, 443, 1883
    }
}
```

## API Reference

Both `data::yaml` and `data::json` namespaces expose the same API:

```cpp
// Parse (returns std::variant<document, parse_error>)
constexpr auto result = data::yaml::parse(R"(key: value)");
constexpr auto result = data::json::parse(R"({"key": "value"})");

// Parse or compile-time error
constexpr auto doc = data::yaml::parse_or_throw(R"(key: value)");
constexpr auto doc = data::json::parse_or_throw(R"({"key": "value"})");

// Compile-time validation
constexpr bool ok = data::yaml::is_valid(R"(key: value)");
constexpr bool ok = data::json::is_valid(R"({"key": "value"})");

// Document access
doc.find(node, "key")      // -> value const* (nullptr if not found)
doc.at(node, index)        // -> value const&
doc.size(node)             // -> std::size_t
doc.key_at(node, index)    // -> std::string_view

// Iteration
doc.values(node)           // range of value (sequence or mapping)
doc.entries(node)          // range of {key, value} (mapping only)

// Scalar accessors
val.as_string()            // -> std::string_view
val.as_int()               // -> int64_t
val.as_float()             // -> double
val.as_bool()              // -> bool

// Type checks
val.is_null()   val.is_bool()     val.is_int()
val.is_float()  val.is_string()   val.is_sequence()
val.is_mapping()

// Error handling
auto result = data::json::parse(R"({"a": 1, "a": 2})");
auto err = std::get<data::parse_error>(result);
err.code       // data::error_code::duplicate_key
err.line       // 1
err.column     // 10
err.message()  // "duplicate key"
data::error_message(err.code)  // same as err.message()
```

## How Sizing Works

`data_embed()` analyzes each file at CMake configure time and sets optimal pool sizes per-target — no manual tuning required. For inline `constexpr` usage without `data_embed()`, generous defaults apply. Override them only if needed via `#define` before including the header (`DATA_CT_MAX_STRING_SIZE`, `DATA_CT_MAX_ITEMS`, `DATA_CT_MAX_NODES`, `DATA_CT_MAX_TOKENS`).

## Building & Testing

```bash
docker build -t data-test .
docker run --rm data-test
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
