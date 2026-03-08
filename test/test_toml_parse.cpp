#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <immutable_data/toml.hpp>

using namespace data::toml;
using namespace data::detail;

// --- Compile-time validation ---

static_assert(is_valid(R"(key = "value")"));
static_assert(is_valid(R"(count = 42)"));
static_assert(is_valid(R"(flag = true)"));
static_assert(is_valid(R"(pi = 3.14)"));
static_assert(is_valid(R"(
[server]
host = "localhost"
port = 8080
)"));

static_assert(!is_valid(R"(key = "value"
key = "duplicate")"), "duplicate keys");

// --- Compile-time parsed documents ---

constexpr auto simple = parse_or_throw(R"(
name = "test"
count = 42
)");
static_assert(simple.root_.is_mapping());

constexpr auto types_doc = parse_or_throw(R"(
str = "hello"
int_val = 42
neg = -17
flt = 3.14
yes = true
no = false
)");

constexpr auto table_doc = parse_or_throw(R"(
title = "My App"

[database]
host = "localhost"
port = 5432
ssl = true

[cache]
ttl = 3600
max_size = 1024
)");

constexpr auto bad = parse(R"(
name = "first"
name = "second"
)");
static_assert(std::holds_alternative<data::parse_error>(bad));
static_assert(std::get<data::parse_error>(bad).code == data::error_code::duplicate_key);

// --- Runtime tests ---

TEST_CASE("toml: simple key-value pairs")
{
    auto name = simple.find(simple.root_, "name");
    REQUIRE(name);
    CHECK(name->as_string() == "test");
    CHECK(simple.find(simple.root_, "count")->as_int() == 42);
}

TEST_CASE("toml: all scalar types")
{
    CHECK(types_doc.find(types_doc.root_, "str")->as_string() == "hello");
    CHECK(types_doc.find(types_doc.root_, "int_val")->as_int() == 42);
    CHECK(types_doc.find(types_doc.root_, "neg")->as_int() == -17);
    CHECK(types_doc.find(types_doc.root_, "flt")->as_float() == doctest::Approx(3.14).epsilon(0.01));
    CHECK(types_doc.find(types_doc.root_, "yes")->as_bool() == true);
    CHECK(types_doc.find(types_doc.root_, "no")->as_bool() == false);
}

TEST_CASE("toml: table headers")
{
    CHECK(table_doc.find(table_doc.root_, "title")->as_string() == "My App");

    auto db = table_doc.find(table_doc.root_, "database");
    REQUIRE(db);
    CHECK(db->is_mapping());
    CHECK(table_doc.find(*db, "host")->as_string() == "localhost");
    CHECK(table_doc.find(*db, "port")->as_int() == 5432);
    CHECK(table_doc.find(*db, "ssl")->as_bool() == true);

    auto cache = table_doc.find(table_doc.root_, "cache");
    REQUIRE(cache);
    CHECK(table_doc.find(*cache, "ttl")->as_int() == 3600);
    CHECK(table_doc.find(*cache, "max_size")->as_int() == 1024);
}

TEST_CASE("toml: arrays")
{
    constexpr auto doc = parse_or_throw(R"(
ports = [8080, 8081, 8082]
tags = ["web", "api", "prod"]
)");
    auto ports = doc.find(doc.root_, "ports");
    REQUIRE(ports);
    CHECK(ports->is_sequence());
    CHECK(doc.size(*ports) == 3);
    CHECK(doc.at(*ports, 0).as_int() == 8080);
    CHECK(doc.at(*ports, 1).as_int() == 8081);
    CHECK(doc.at(*ports, 2).as_int() == 8082);

    auto tags = doc.find(doc.root_, "tags");
    REQUIRE(tags);
    CHECK(doc.size(*tags) == 3);
    CHECK(doc.at(*tags, 0).as_string() == "web");
    CHECK(doc.at(*tags, 2).as_string() == "prod");
}

TEST_CASE("toml: empty array")
{
    constexpr auto doc = parse_or_throw(R"(empty = [])");
    auto empty = doc.find(doc.root_, "empty");
    REQUIRE(empty);
    CHECK(empty->is_sequence());
    CHECK(doc.size(*empty) == 0);
}

TEST_CASE("toml: array with trailing comma")
{
    constexpr auto doc = parse_or_throw(R"(items = [1, 2, 3,])");
    auto items = doc.find(doc.root_, "items");
    REQUIRE(items);
    CHECK(doc.size(*items) == 3);
}

TEST_CASE("toml: inline tables")
{
    constexpr auto doc = parse_or_throw(R"(point = {x = 1, y = 2})");
    auto point = doc.find(doc.root_, "point");
    REQUIRE(point);
    CHECK(point->is_mapping());
    CHECK(doc.find(*point, "x")->as_int() == 1);
    CHECK(doc.find(*point, "y")->as_int() == 2);
}

TEST_CASE("toml: empty inline table")
{
    constexpr auto doc = parse_or_throw(R"(empty = {})");
    auto empty = doc.find(doc.root_, "empty");
    REQUIRE(empty);
    CHECK(empty->is_mapping());
    CHECK(doc.size(*empty) == 0);
}

TEST_CASE("toml: quoted keys")
{
    constexpr auto doc = parse_or_throw(R"("quoted key" = "value")");
    CHECK(doc.find(doc.root_, "quoted key")->as_string() == "value");
}

TEST_CASE("toml: basic string escapes")
{
    constexpr auto doc = parse_or_throw(R"(msg = "hello\nworld")");
    CHECK(doc.find(doc.root_, "msg")->as_string() == "hello\nworld");
}

TEST_CASE("toml: literal strings (no escape processing)")
{
    constexpr auto doc = parse_or_throw(R"(path = 'C:\Users\foo')");
    CHECK(doc.find(doc.root_, "path")->as_string() == "C:\\Users\\foo");
}

TEST_CASE("toml: positive integers")
{
    constexpr auto doc = parse_or_throw(R"(val = +42)");
    CHECK(doc.find(doc.root_, "val")->as_int() == 42);
}

TEST_CASE("toml: integer with underscores")
{
    constexpr auto doc = parse_or_throw(R"(big = 1_000_000)");
    CHECK(doc.find(doc.root_, "big")->as_int() == 1000000);
}

TEST_CASE("toml: iterate entries")
{
    std::size_t count = 0;
    for (auto [key, val] : simple.entries(simple.root_))
    {
        CHECK(!key.empty());
        ++count;
    }
    CHECK(count == 2);
}

TEST_CASE("toml: iterate array values")
{
    constexpr auto doc = parse_or_throw(R"(nums = [10, 20, 30])");
    auto nums = doc.find(doc.root_, "nums");
    REQUIRE(nums);
    std::int64_t sum = 0;
    for (auto const& val : doc.values(*nums))
        sum += val.as_int();
    CHECK(sum == 60);
}

TEST_CASE("toml: error - duplicate key")
{
    auto err = std::get<data::parse_error>(bad);
    CHECK(err.code == data::error_code::duplicate_key);
    CHECK(err.line > 0);
    CHECK(err.column > 0);
    CHECK(err.message() == "duplicate key");
}

TEST_CASE("toml: error - unterminated string")
{
    constexpr auto r = parse(R"(key = "unterminated)");
    CHECK(std::holds_alternative<data::parse_error>(r));
    auto err = std::get<data::parse_error>(r);
    CHECK(err.code == data::error_code::unterminated_string);
}

TEST_CASE("toml: comments are ignored")
{
    constexpr auto doc = parse_or_throw(R"(
# This is a comment
key = "value" # inline comment
count = 5
)");
    CHECK(doc.find(doc.root_, "key")->as_string() == "value");
    CHECK(doc.find(doc.root_, "count")->as_int() == 5);
}
