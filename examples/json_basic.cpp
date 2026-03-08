// Example: inline constexpr JSON parsing
//
// Same API as YAML — just include json.hpp instead of yaml.hpp.

#include <cassert>
#include <cstdio>
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

int main()
{
    auto server = doc.find(doc.root_, "server");
    assert(server);

    assert(doc.find(*server, "host")->as_string() == "0.0.0.0");
    assert(doc.find(*server, "port")->as_int() == 8080);
    assert(doc.find(doc.root_, "debug")->as_bool() == false);

    auto tags = doc.find(doc.root_, "tags");
    assert(tags && tags->is_sequence());
    std::printf("tags:");
    for (auto const& val : doc.values(*tags))
        std::printf(" %.*s", static_cast<int>(val.as_string().size()), val.as_string().data());
    std::printf("\n");

    return 0;
}
