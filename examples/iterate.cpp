// Example: iterate over YAML structures
//
// Shows how to walk sequences, mappings, and nested trees
// using values(), entries(), and structured bindings.

#include <cassert>
#include <cstdio>
#include <immutable_yaml/immutable_yaml.hpp>

constexpr auto doc = yaml::ct::parse_or_throw(R"(
services:
  web:
    port: 80
    proto: "http"
  api:
    port: 443
    proto: "https"
  mqtt:
    port: 1883
    proto: "mqtt"
ports:
  - 80
  - 443
  - 1883
)");

int main()
{
    // Iterate mapping entries with structured bindings
    auto services = doc.find(doc.root_, "services");
    std::printf("services:\n");
    for (auto [name, svc] : doc.entries(*services))
    {
        auto port = doc.find(svc, "port")->as_int();
        auto proto = doc.find(svc, "proto");
        std::printf("  %.*s: %.*s://localhost:%lld\n",
            static_cast<int>(name.size()), name.data(),
            static_cast<int>(proto->as_string().size()), proto->as_string().data(),
            static_cast<long long>(port));
    }

    // Iterate sequence values and sum
    auto ports = doc.find(doc.root_, "ports");
    std::int64_t sum = 0;
    for (auto const& val : doc.values(*ports))
        sum += val.as_int();
    std::printf("port sum: %lld\n", static_cast<long long>(sum));
    assert(sum == 80 + 443 + 1883);

    return 0;
}
