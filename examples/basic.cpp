// Example: inline constexpr YAML parsing
//
// Parses YAML at compile time — no files, no runtime overhead.
// Errors in the YAML source become compile-time errors.

#include <cassert>
#include <immutable_yaml/immutable_yaml.hpp>

// Parse at compile time — this YAML is baked into the binary
constexpr auto doc = yaml::ct::parse_or_throw(R"(
server:
  host: "0.0.0.0"
  port: 8080
  workers: 4
debug: false
)");

// Compile-time validation
static_assert(doc.root_.is_mapping());
static_assert(yaml::ct::is_valid(R"(key: value)"));
static_assert(!yaml::ct::is_valid(R"({a: 1, a: duplicate})"));

int main()
{
    auto server = doc.find(doc.root_, "server");
    assert(server.has_value());

    assert(doc.find(*server, "host")->as_string() == "0.0.0.0");
    assert(doc.find(*server, "port")->as_int() == 8080);
    assert(doc.find(*server, "workers")->as_int() == 4);
    assert(doc.find(doc.root_, "debug")->as_bool() == false);

    return 0;
}
