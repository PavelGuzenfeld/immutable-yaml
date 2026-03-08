#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <immutable_data/json.hpp>

using namespace data::json;
using namespace data::detail;

// --- Compile-time validation ---

static_assert(is_valid(R"({"key": "value"})"));
static_assert(is_valid(R"([1, 2, 3])"));
static_assert(is_valid(R"(42)"));
static_assert(is_valid(R"("hello")"));
static_assert(is_valid(R"(true)"));
static_assert(is_valid(R"(false)"));
static_assert(is_valid(R"(null)"));
static_assert(is_valid(R"({"a": 1, "b": 2})"));
static_assert(is_valid(R"({"nested": {"deep": true}})"));
static_assert(is_valid(R"([])"));
static_assert(is_valid(R"({})"));

static_assert(!is_valid(R"({"a": 1, "a": 2})"), "duplicate keys");
static_assert(!is_valid(R"([1, 2, 3,])"), "trailing comma in array");
static_assert(!is_valid(R"({"a": 1,})"), "trailing comma in object");
static_assert(!is_valid(R"("unterminated)"), "unterminated string");

// --- Compile-time parsed documents ---

constexpr auto simple = parse_or_throw(R"({"name": "test", "count": 42})");
static_assert(simple.root_.is_mapping());

constexpr auto arr = parse_or_throw(R"([10, 20, 30])");
static_assert(arr.root_.is_sequence());

constexpr auto nested = parse_or_throw(R"({"outer": {"inner": 99}})");

constexpr auto types = parse_or_throw(R"({
    "str": "hello",
    "int": 42,
    "neg": -17,
    "flt": 3.14,
    "yes": true,
    "no": false,
    "nil": null
})");

constexpr auto bad = parse(R"({"a": 1, "a": 2})");
static_assert(std::holds_alternative<data::parse_error>(bad));
static_assert(std::get<data::parse_error>(bad).code == data::error_code::duplicate_key);

// --- Runtime tests ---

TEST_CASE("json: simple object")
{
    auto name = simple.find(simple.root_, "name");
    REQUIRE(name);
    CHECK(name->as_string() == "test");
    CHECK(simple.find(simple.root_, "count")->as_int() == 42);
}

TEST_CASE("json: array")
{
    CHECK(arr.root_.is_sequence());
    CHECK(arr.size(arr.root_) == 3);
    CHECK(arr.at(arr.root_, 0).as_int() == 10);
    CHECK(arr.at(arr.root_, 1).as_int() == 20);
    CHECK(arr.at(arr.root_, 2).as_int() == 30);
}

TEST_CASE("json: nested object")
{
    auto outer = nested.find(nested.root_, "outer");
    REQUIRE(outer);
    CHECK(nested.find(*outer, "inner")->as_int() == 99);
}

TEST_CASE("json: all types")
{
    CHECK(types.find(types.root_, "str")->as_string() == "hello");
    CHECK(types.find(types.root_, "int")->as_int() == 42);
    CHECK(types.find(types.root_, "neg")->as_int() == -17);
    CHECK(types.find(types.root_, "flt")->as_float() == doctest::Approx(3.14).epsilon(0.01));
    CHECK(types.find(types.root_, "yes")->as_bool() == true);
    CHECK(types.find(types.root_, "no")->as_bool() == false);
    CHECK(types.find(types.root_, "nil")->is_null());
}

TEST_CASE("json: empty containers")
{
    constexpr auto empty_arr = parse_or_throw(R"([])");
    CHECK(empty_arr.root_.is_sequence());
    CHECK(empty_arr.size(empty_arr.root_) == 0);

    constexpr auto empty_obj = parse_or_throw(R"({})");
    CHECK(empty_obj.root_.is_mapping());
    CHECK(empty_obj.size(empty_obj.root_) == 0);
}

TEST_CASE("json: nested arrays")
{
    constexpr auto doc = parse_or_throw(R"({"matrix": [[1, 2], [3, 4]]})");
    auto matrix = doc.find(doc.root_, "matrix");
    REQUIRE(matrix);
    CHECK(matrix->is_sequence());
    CHECK(doc.size(*matrix) == 2);

    auto row0 = doc.at(*matrix, 0);
    CHECK(row0.is_sequence());

    auto row1 = doc.at(*matrix, 1);
    CHECK(row1.is_sequence());
}

TEST_CASE("json: iterate array values")
{
    std::int64_t sum = 0;
    std::size_t count = 0;
    for (auto const& val : arr.values(arr.root_))
    {
        sum += val.as_int();
        ++count;
    }
    CHECK(count == 3);
    CHECK(sum == 60);
}

TEST_CASE("json: iterate object entries")
{
    std::size_t count = 0;
    for (auto [key, val] : simple.entries(simple.root_))
    {
        CHECK(!key.empty());
        ++count;
    }
    CHECK(count == 2);
}

TEST_CASE("json: error - duplicate key")
{
    auto err = std::get<data::parse_error>(bad);
    CHECK(err.code == data::error_code::duplicate_key);
    CHECK(err.line > 0);
    CHECK(err.column > 0);
    CHECK(err.message() == "duplicate key");
}

TEST_CASE("json: error - trailing comma")
{
    constexpr auto r = parse(R"([1, 2,])");
    CHECK(std::holds_alternative<data::parse_error>(r));
    auto err = std::get<data::parse_error>(r);
    CHECK(err.code == data::error_code::trailing_comma);
    CHECK(err.message() == "trailing comma");
}

TEST_CASE("json: error - unterminated string")
{
    constexpr auto r = parse(R"("unterminated)");
    CHECK(std::holds_alternative<data::parse_error>(r));
    auto err = std::get<data::parse_error>(r);
    CHECK(err.code == data::error_code::unterminated_string);
    CHECK(err.line == 1);
    CHECK(err.column == 1);
}

TEST_CASE("json: error_message function")
{
    CHECK(data::error_message(data::error_code::none) == "no error");
    CHECK(data::error_message(data::error_code::invalid_syntax) == "invalid syntax");
    CHECK(data::error_message(data::error_code::unexpected_token) == "unexpected token");
    CHECK(data::error_message(data::error_code::duplicate_key) == "duplicate key");
    CHECK(data::error_message(data::error_code::trailing_comma) == "trailing comma");
}

TEST_CASE("json: escape sequences")
{
    constexpr auto doc = parse_or_throw(R"({"msg": "hello\nworld", "tab": "a\tb", "quote": "say \"hi\""})");
    CHECK(doc.find(doc.root_, "msg")->as_string() == "hello\nworld");
    CHECK(doc.find(doc.root_, "tab")->as_string() == "a\tb");
    CHECK(doc.find(doc.root_, "quote")->as_string() == "say \"hi\"");
}

TEST_CASE("json: escape sequences - special chars")
{
    constexpr auto doc = parse_or_throw(R"({"bs": "a\\b", "slash": "a\/b", "cr": "a\rb", "bs_char": "a\bb", "ff": "a\fb"})");
    CHECK(doc.find(doc.root_, "bs")->as_string() == "a\\b");
    CHECK(doc.find(doc.root_, "slash")->as_string() == "a/b");
    CHECK(doc.find(doc.root_, "cr")->as_string() == "a\rb");
    CHECK(doc.find(doc.root_, "bs_char")->as_string() == "a\bb");
    CHECK(doc.find(doc.root_, "ff")->as_string() == "a\fb");
}

TEST_CASE("json: unicode escape - basic")
{
    // \u0041 = 'A'
    constexpr auto doc = parse_or_throw(R"({"ch": "\u0041"})");
    CHECK(doc.find(doc.root_, "ch")->as_string() == "A");
}

TEST_CASE("json: unicode escape - multi-byte UTF-8")
{
    // \u00E9 = 'é' (2-byte UTF-8: 0xC3 0xA9)
    constexpr auto doc = parse_or_throw(R"({"ch": "\u00E9"})");
    auto sv = doc.find(doc.root_, "ch")->as_string();
    CHECK(sv.size() == 2);
    CHECK(static_cast<unsigned char>(sv[0]) == 0xC3);
    CHECK(static_cast<unsigned char>(sv[1]) == 0xA9);
}

TEST_CASE("json: unicode escape - 3-byte UTF-8")
{
    // \u4E16 = '世' (3-byte UTF-8: 0xE4 0xB8 0x96)
    constexpr auto doc = parse_or_throw(R"({"ch": "\u4E16"})");
    auto sv = doc.find(doc.root_, "ch")->as_string();
    CHECK(sv.size() == 3);
    CHECK(static_cast<unsigned char>(sv[0]) == 0xE4);
    CHECK(static_cast<unsigned char>(sv[1]) == 0xB8);
    CHECK(static_cast<unsigned char>(sv[2]) == 0x96);
}

TEST_CASE("json: no escapes - fast path")
{
    constexpr auto doc = parse_or_throw(R"({"plain": "no escapes here"})");
    CHECK(doc.find(doc.root_, "plain")->as_string() == "no escapes here");
}

TEST_CASE("json: scalar root values")
{
    constexpr auto num = parse_or_throw(R"(42)");
    CHECK(num.root_.is_int());
    CHECK(num.root_.as_int() == 42);

    constexpr auto str = parse_or_throw(R"("hello")");
    CHECK(str.root_.is_string());
    CHECK(str.root_.as_string() == "hello");

    constexpr auto b = parse_or_throw(R"(true)");
    CHECK(b.root_.is_bool());
    CHECK(b.root_.as_bool() == true);

    constexpr auto n = parse_or_throw(R"(null)");
    CHECK(n.root_.is_null());
}
