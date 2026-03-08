// Example: inline constexpr XML parsing
//
// Same API as YAML/JSON/TOML — just include xml.hpp.

#include <cassert>
#include <cstdio>
#include <immutable_data/xml.hpp>

constexpr auto doc = data::xml::parse_or_throw(R"(
<server>
  <host>0.0.0.0</host>
  <port>8080</port>
  <workers>4</workers>
  <debug>false</debug>
</server>
)");

static_assert(doc.root_.is_mapping());
static_assert(data::xml::is_valid(R"(<root><key>value</key></root>)"));
static_assert(!data::xml::is_valid(R"(<open></close>)"));

int main()
{
    assert(doc.find(doc.root_, "host")->as_string() == "0.0.0.0");
    assert(doc.find(doc.root_, "port")->as_int() == 8080);
    assert(doc.find(doc.root_, "workers")->as_int() == 4);
    assert(doc.find(doc.root_, "debug")->as_bool() == false);

    std::printf("server config:\n");
    for (auto [key, val] : doc.entries(doc.root_))
        std::printf("  %.*s\n", static_cast<int>(key.size()), key.data());

    return 0;
}
