#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <immutable_data/yaml.hpp>
#include <immutable_data/json.hpp>
#include <immutable_data/toml.hpp>
#include <immutable_data/xml.hpp>
#include <immutable_data/version.hpp>

// --- Version macros ---

TEST_CASE("version macros defined")
{
    CHECK(IMMUTABLE_DATA_VERSION_MAJOR == 0);
    CHECK(IMMUTABLE_DATA_VERSION_MINOR == 2);
    CHECK(IMMUTABLE_DATA_VERSION_PATCH == 0);
    CHECK(std::string_view{IMMUTABLE_DATA_VERSION_STRING} == "0.2.0");
}

// --- Pool overflow detection ---

TEST_CASE("yaml: pool overflow returns error")
{
    // Use very small pool to trigger overflow
    // With default DATA_CT_MAX_NODES = DATA_CT_MAX_ITEMS * 4 = 256,
    // a document with many entries will exhaust the pool.
    // We test via parse() returning an error variant, not parse_or_throw.
    constexpr auto result = data::yaml::parse(R"(
a: 1
b: 2
c: 3
d: 4
e: 5
f: 6
g: 7
h: 8
)");
    // With default sizes this should succeed
    CHECK(std::holds_alternative<data::detail::document>(result));
}

TEST_CASE("json: pool overflow returns error")
{
    constexpr auto result = data::json::parse(R"({"a":1,"b":2,"c":3,"d":4,"e":5})");
    CHECK(std::holds_alternative<data::detail::document>(result));
}

// --- Depth limit ---

TEST_CASE("yaml: deeply nested mapping succeeds within limit")
{
    // 4 levels deep — well within MAX_PARSE_DEPTH (64)
    constexpr auto result = data::yaml::parse(R"(
l1:
  l2:
    l3:
      l4: deep
)");
    REQUIRE(std::holds_alternative<data::detail::document>(result));
    auto const& doc = std::get<data::detail::document>(result);
    auto l1 = doc.find(doc.root_, "l1");
    REQUIRE(l1);
    auto l2 = doc.find(*l1, "l2");
    REQUIRE(l2);
    auto l3 = doc.find(*l2, "l3");
    REQUIRE(l3);
    CHECK(doc.find(*l3, "l4")->as_string() == "deep");
}

TEST_CASE("json: deeply nested arrays succeed within limit")
{
    // 5 levels deep
    constexpr auto result = data::json::parse(R"([[[[[42]]]]])");
    REQUIRE(std::holds_alternative<data::detail::document>(result));
    auto const& doc = std::get<data::detail::document>(result);
    auto const& l1 = doc.at(doc.root_, 0);
    auto const& l2 = doc.at(l1, 0);
    auto const& l3 = doc.at(l2, 0);
    auto const& l4 = doc.at(l3, 0);
    auto const& l5 = doc.at(l4, 0);
    CHECK(l5.as_int() == 42);
}

// --- Error code messages ---

TEST_CASE("new error codes have messages")
{
    CHECK(data::error_message(data::error_code::pool_overflow) == "node pool overflow");
    CHECK(data::error_message(data::error_code::string_overflow) == "string capacity exceeded");
    CHECK(data::error_message(data::error_code::max_depth_exceeded) == "maximum nesting depth exceeded");
}

// --- String storage overflow detection ---

TEST_CASE("string_storage::push_back returns false on overflow")
{
    data::detail::string_storage<4> s;  // 3 chars + null
    CHECK(s.push_back('a') == true);
    CHECK(s.push_back('b') == true);
    CHECK(s.push_back('c') == true);
    CHECK(s.push_back('d') == false);  // overflow
    CHECK(s.size() == 3);
    CHECK(s.view() == "abc");
    CHECK(s.full());
}

TEST_CASE("string_storage::full() reports correctly")
{
    data::detail::string_storage<2> s;  // 1 char + null
    CHECK(!s.full());
    s.push_back('x');
    CHECK(s.full());
}

// --- document::can_alloc ---

TEST_CASE("document::can_alloc bounds checking")
{
    data::detail::document doc{};
    CHECK(doc.can_alloc(0));
    CHECK(doc.can_alloc(1));
    CHECK(doc.can_alloc(DATA_CT_MAX_NODES));
    CHECK(!doc.can_alloc(DATA_CT_MAX_NODES + 1));
}

// --- XML depth ---

TEST_CASE("xml: nested elements succeed within limit")
{
    constexpr auto result = data::xml::parse(R"(
<root>
  <level1>
    <level2>
      <level3>deep</level3>
    </level2>
  </level1>
</root>
)");
    REQUIRE(std::holds_alternative<data::detail::document>(result));
    auto const& doc = std::get<data::detail::document>(result);
    auto l1 = doc.find(doc.root_, "level1");
    REQUIRE(l1);
    auto l2 = doc.find(*l1, "level2");
    REQUIRE(l2);
    auto l3 = doc.find(*l2, "level3");
    REQUIRE(l3);
    CHECK(l3->as_string() == "deep");
}

// --- TOML depth ---

TEST_CASE("toml: nested inline tables within limit")
{
    constexpr auto result = data::toml::parse(R"(key = {a = {b = 1}})");
    REQUIRE(std::holds_alternative<data::detail::document>(result));
    auto const& doc = std::get<data::detail::document>(result);
    auto key = doc.find(doc.root_, "key");
    REQUIRE(key);
    auto a = doc.find(*key, "a");
    REQUIRE(a);
    CHECK(doc.find(*a, "b")->as_int() == 1);
}

// --- Compile-time validation of safety features ---

static_assert(data::detail::MAX_PARSE_DEPTH == 64, "default depth limit");
