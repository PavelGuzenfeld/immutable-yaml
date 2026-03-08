#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <immutable_data/yaml.hpp>

using namespace data::yaml;
using namespace data::detail;

// --- Compile-time validation ---

static_assert(is_valid(R"(key: value)"));
static_assert(is_valid(R"(a: 1)"));
static_assert(is_valid(R"([1, 2, 3])"));
static_assert(is_valid(R"({key: value})"));
static_assert(is_valid(R"(flag: true)"));
static_assert(is_valid(R"(nothing: null)"));
static_assert(is_valid(R"(pi: 3.14)"));
static_assert(is_valid(R"("quoted key": "quoted value")"));

static_assert(!is_valid(R"({key: value, key: duplicate})"), "duplicate keys");
static_assert(!is_valid(R"([1, 2, 3,])"), "trailing comma in sequence");
static_assert(!is_valid(R"(key: "unterminated string)"), "unterminated string");
static_assert(!is_valid(R"(: invalid key)"), "missing key");

// --- Compile-time parsed documents ---

constexpr auto simple = parse_or_throw(R"(name: "test")");
static_assert(simple.root_.is_mapping());

constexpr auto int_doc = parse_or_throw(R"(count: 42)");
constexpr auto bool_doc = parse_or_throw(R"(active: true)");
constexpr auto seq_doc = parse_or_throw(R"([10, 20, 30])");
constexpr auto map_doc = parse_or_throw(R"({x: 1, y: 2, z: 3})");

constexpr auto nested = parse_or_throw(R"(
outer:
  inner: 99
)");

constexpr auto multi = parse_or_throw(R"(
a: 1
b: 2
c: 3
)");

constexpr auto block_seq = parse_or_throw(R"(
items:
  - "one"
  - "two"
  - "three"
)");

constexpr auto commented = parse_or_throw(R"(
# full-line comment
key: value  # inline comment
num: 42
)");
static_assert(commented.root_.is_mapping());

constexpr auto bad_result = parse(R"({a: 1, a: 2})");
static_assert(std::holds_alternative<data::error_code>(bad_result));
static_assert(std::get<data::error_code>(bad_result) == data::error_code::duplicate_key);

// --- Runtime tests ---

TEST_CASE("yaml: simple key-value")
{
    auto name = simple.find(simple.root_, "name");
    REQUIRE(name);
    CHECK(name->as_string() == "test");
}

TEST_CASE("yaml: integer value")
{
    REQUIRE(int_doc.find(int_doc.root_, "count"));
    CHECK(int_doc.find(int_doc.root_, "count")->as_int() == 42);
}

TEST_CASE("yaml: boolean value")
{
    CHECK(bool_doc.find(bool_doc.root_, "active")->as_bool() == true);
}

TEST_CASE("yaml: flow sequence")
{
    CHECK(seq_doc.root_.is_sequence());
    CHECK(seq_doc.size(seq_doc.root_) == 3);
    CHECK(seq_doc.at(seq_doc.root_, 0).as_int() == 10);
    CHECK(seq_doc.at(seq_doc.root_, 1).as_int() == 20);
    CHECK(seq_doc.at(seq_doc.root_, 2).as_int() == 30);
}

TEST_CASE("yaml: flow mapping")
{
    CHECK(map_doc.root_.is_mapping());
    CHECK(map_doc.size(map_doc.root_) == 3);
    CHECK(map_doc.find(map_doc.root_, "x")->as_int() == 1);
    CHECK(map_doc.find(map_doc.root_, "y")->as_int() == 2);
    CHECK(map_doc.find(map_doc.root_, "z")->as_int() == 3);
}

TEST_CASE("yaml: nested mapping")
{
    auto outer = nested.find(nested.root_, "outer");
    REQUIRE(outer);
    CHECK(nested.find(*outer, "inner")->as_int() == 99);
}

TEST_CASE("yaml: multiple keys")
{
    CHECK(multi.size(multi.root_) == 3);
    CHECK(multi.find(multi.root_, "a")->as_int() == 1);
    CHECK(multi.find(multi.root_, "b")->as_int() == 2);
    CHECK(multi.find(multi.root_, "c")->as_int() == 3);
}

TEST_CASE("yaml: block sequence")
{
    auto items = block_seq.find(block_seq.root_, "items");
    REQUIRE(items);
    CHECK(items->is_sequence());
    CHECK(block_seq.size(*items) == 3);
    CHECK(block_seq.at(*items, 0).as_string() == "one");
    CHECK(block_seq.at(*items, 1).as_string() == "two");
    CHECK(block_seq.at(*items, 2).as_string() == "three");
}

TEST_CASE("yaml: inline comments")
{
    CHECK(commented.find(commented.root_, "key")->as_string() == "value");
    CHECK(commented.find(commented.root_, "num")->as_int() == 42);
}

TEST_CASE("yaml: error handling - duplicate key")
{
    CHECK(std::get<data::error_code>(bad_result) == data::error_code::duplicate_key);
}

TEST_CASE("yaml: iterate sequence values")
{
    std::int64_t sum = 0;
    std::size_t count = 0;
    for (auto const& val : seq_doc.values(seq_doc.root_))
    {
        sum += val.as_int();
        ++count;
    }
    CHECK(count == 3);
    CHECK(sum == 60);
}

TEST_CASE("yaml: iterate mapping entries")
{
    std::size_t count = 0;
    for (auto [key, val] : multi.entries(multi.root_))
    {
        if (key == "a") CHECK(val.as_int() == 1);
        else if (key == "b") CHECK(val.as_int() == 2);
        else if (key == "c") CHECK(val.as_int() == 3);
        ++count;
    }
    CHECK(count == 3);
}

TEST_CASE("yaml: values() on scalar returns empty range")
{
    auto name = simple.find(simple.root_, "name");
    auto view = simple.values(*name);
    CHECK(view.size() == 0);
}

TEST_CASE("yaml: entries() on sequence returns empty range")
{
    CHECK(seq_doc.entries(seq_doc.root_).size() == 0);
}
