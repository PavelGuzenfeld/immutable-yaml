#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <immutable_data/xml.hpp>

using namespace data::xml;
using namespace data::detail;

// --- Compile-time validation ---

static_assert(is_valid(R"(<root/>)"));
static_assert(is_valid(R"(<key>value</key>)"));
static_assert(is_valid(R"(<root><child>text</child></root>)"));
static_assert(is_valid(R"(<?xml version="1.0"?><root/>)"));

static_assert(!is_valid(R"(<root>)"), "unclosed tag");
static_assert(!is_valid(R"(<root></wrong>)"), "mismatched tags");

// --- Compile-time parsed documents ---

constexpr auto simple = parse_or_throw(R"(
<config>
  <host>localhost</host>
  <port>5432</port>
  <ssl>true</ssl>
</config>
)");
static_assert(simple.root_.is_mapping());

constexpr auto typed = parse_or_throw(R"(
<types>
  <str>hello</str>
  <int_val>42</int_val>
  <neg>-17</neg>
  <flt>3.14</flt>
  <yes>true</yes>
  <no>false</no>
  <nil>null</nil>
</types>
)");

// --- Runtime tests ---

TEST_CASE("xml: simple element mapping")
{
    CHECK(simple.root_.is_mapping());
    CHECK(simple.find(simple.root_, "host")->as_string() == "localhost");
    CHECK(simple.find(simple.root_, "port")->as_int() == 5432);
    CHECK(simple.find(simple.root_, "ssl")->as_bool() == true);
}

TEST_CASE("xml: all scalar types")
{
    CHECK(typed.find(typed.root_, "str")->as_string() == "hello");
    CHECK(typed.find(typed.root_, "int_val")->as_int() == 42);
    CHECK(typed.find(typed.root_, "neg")->as_int() == -17);
    CHECK(typed.find(typed.root_, "flt")->as_float() == doctest::Approx(3.14).epsilon(0.01));
    CHECK(typed.find(typed.root_, "yes")->as_bool() == true);
    CHECK(typed.find(typed.root_, "no")->as_bool() == false);
    CHECK(typed.find(typed.root_, "nil")->is_null());
}

TEST_CASE("xml: nested elements")
{
    constexpr auto doc = parse_or_throw(R"(
<root>
  <database>
    <host>db.example.com</host>
    <port>3306</port>
  </database>
  <cache>
    <ttl>3600</ttl>
  </cache>
</root>
)");
    auto db = doc.find(doc.root_, "database");
    REQUIRE(db);
    CHECK(db->is_mapping());
    CHECK(doc.find(*db, "host")->as_string() == "db.example.com");
    CHECK(doc.find(*db, "port")->as_int() == 3306);

    auto cache = doc.find(doc.root_, "cache");
    REQUIRE(cache);
    CHECK(doc.find(*cache, "ttl")->as_int() == 3600);
}

TEST_CASE("xml: self-closing element")
{
    constexpr auto doc = parse_or_throw(R"(
<root>
  <empty/>
  <name>test</name>
</root>
)");
    CHECK(doc.find(doc.root_, "empty")->is_null());
    CHECK(doc.find(doc.root_, "name")->as_string() == "test");
}

TEST_CASE("xml: attributes on self-closing element")
{
    constexpr auto doc = parse_or_throw(R"(
<root>
  <server host="localhost" port="8080"/>
</root>
)");
    auto server = doc.find(doc.root_, "server");
    REQUIRE(server);
    CHECK(server->is_mapping());
    CHECK(doc.find(*server, "host")->as_string() == "localhost");
    CHECK(doc.find(*server, "port")->as_int() == 8080);
}

TEST_CASE("xml: attributes with children")
{
    constexpr auto doc = parse_or_throw(R"(
<root>
  <db engine="postgres">
    <host>localhost</host>
  </db>
</root>
)");
    auto db = doc.find(doc.root_, "db");
    REQUIRE(db);
    CHECK(doc.find(*db, "engine")->as_string() == "postgres");
    CHECK(doc.find(*db, "host")->as_string() == "localhost");
}

TEST_CASE("xml: XML declaration is skipped")
{
    constexpr auto doc = parse_or_throw(R"(<?xml version="1.0" encoding="UTF-8"?>
<root>
  <key>value</key>
</root>
)");
    CHECK(doc.find(doc.root_, "key")->as_string() == "value");
}

TEST_CASE("xml: comments are skipped")
{
    constexpr auto doc = parse_or_throw(R"(
<root>
  <!-- This is a comment -->
  <key>value</key>
  <!-- Another comment -->
  <num>42</num>
</root>
)");
    CHECK(doc.find(doc.root_, "key")->as_string() == "value");
    CHECK(doc.find(doc.root_, "num")->as_int() == 42);
}

TEST_CASE("xml: empty text element")
{
    constexpr auto doc = parse_or_throw(R"(<root><empty></empty></root>)");
    CHECK(doc.find(doc.root_, "empty")->as_string() == "");
}

TEST_CASE("xml: iterate entries")
{
    std::size_t count = 0;
    for (auto [key, val] : simple.entries(simple.root_))
    {
        CHECK(!key.empty());
        ++count;
    }
    CHECK(count == 3);
}

TEST_CASE("xml: error - mismatched tags")
{
    constexpr auto r = parse(R"(<open></close>)");
    CHECK(std::holds_alternative<data::parse_error>(r));
}

TEST_CASE("xml: error - duplicate child elements")
{
    constexpr auto r = parse(R"(<root><a>1</a><a>2</a></root>)");
    CHECK(std::holds_alternative<data::parse_error>(r));
    auto err = std::get<data::parse_error>(r);
    CHECK(err.code == data::error_code::duplicate_key);
}

TEST_CASE("xml: deeply nested")
{
    constexpr auto doc = parse_or_throw(R"(
<a>
  <b>
    <c>
      <d>deep</d>
    </c>
  </b>
</a>
)");
    auto b = doc.find(doc.root_, "b");
    REQUIRE(b);
    auto c = doc.find(*b, "c");
    REQUIRE(c);
    auto d = doc.find(*c, "d");
    REQUIRE(d);
    CHECK(d->as_string() == "deep");
}
