// Example: inline constexpr TOML parsing
//
// Same API as YAML/JSON — just include toml.hpp.

#include <cassert>
#include <cstdio>
#include <immutable_data/toml.hpp>

constexpr auto doc = data::toml::parse_or_throw(R"(
title = "My App"

[server]
host = "0.0.0.0"
port = 8080
workers = 4

[features]
debug = false
)");

static_assert(doc.root_.is_mapping());
static_assert(data::toml::is_valid(R"(key = "value")"));
static_assert(!data::toml::is_valid(R"(key = "value"
key = "dup")"));

int main()
{
    assert(doc.find(doc.root_, "title")->as_string() == "My App");

    auto server = doc.find(doc.root_, "server");
    assert(server);
    assert(doc.find(*server, "host")->as_string() == "0.0.0.0");
    assert(doc.find(*server, "port")->as_int() == 8080);
    assert(doc.find(*server, "workers")->as_int() == 4);

    auto features = doc.find(doc.root_, "features");
    assert(features);
    assert(doc.find(*features, "debug")->as_bool() == false);

    std::printf("title: %.*s\n",
        static_cast<int>(doc.find(doc.root_, "title")->as_string().size()),
        doc.find(doc.root_, "title")->as_string().data());

    for (auto [key, val] : doc.entries(*server))
        std::printf("  server.%.*s\n", static_cast<int>(key.size()), key.data());

    return 0;
}
