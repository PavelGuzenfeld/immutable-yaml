#include <cassert>
#include <iostream>
#include <yaml/yaml.hpp>

using namespace yaml::ct;
using namespace yaml::ct::detail;

// --- Compile-time validation tests ---

// Valid YAML
static_assert(is_valid(R"(key: value)"));
static_assert(is_valid(R"(a: 1)"));
static_assert(is_valid(R"([1, 2, 3])"));
static_assert(is_valid(R"({key: value})"));
static_assert(is_valid(R"(flag: true)"));
static_assert(is_valid(R"(nothing: null)"));
static_assert(is_valid(R"(pi: 3.14)"));
static_assert(is_valid(R"("quoted key": "quoted value")"));

// Invalid YAML
static_assert(!is_valid(R"({key: value, key: duplicate})"), "duplicate keys");
static_assert(!is_valid(R"([1, 2, 3,])"), "trailing comma in sequence");
static_assert(!is_valid(R"(key: "unterminated string)"), "unterminated string");
static_assert(!is_valid(R"(: invalid key)"), "missing key");

// --- Compile-time parsing tests ---

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

// Error handling via parse()
constexpr auto bad_result = parse(R"({a: 1, a: 2})");
static_assert(std::holds_alternative<error_code>(bad_result));
static_assert(std::get<error_code>(bad_result) == error_code::duplicate_key);

int main()
{
    std::cout << "constexpr tests:\n";

    {
        auto name = simple.find(simple.root_, "name");
        assert(name.has_value());
        assert(name->as_string() == "test");
        std::cout << "  simple key-value: OK\n";
    }

    {
        auto val = int_doc.find(int_doc.root_, "count");
        assert(val->as_int() == 42);
        std::cout << "  integer value: OK\n";
    }

    {
        auto val = bool_doc.find(bool_doc.root_, "active");
        assert(val->as_bool() == true);
        std::cout << "  boolean value: OK\n";
    }

    {
        assert(seq_doc.root_.is_sequence());
        assert(seq_doc.size(seq_doc.root_) == 3);
        assert(seq_doc.at(seq_doc.root_, 0).as_int() == 10);
        assert(seq_doc.at(seq_doc.root_, 1).as_int() == 20);
        assert(seq_doc.at(seq_doc.root_, 2).as_int() == 30);
        std::cout << "  flow sequence: OK\n";
    }

    {
        assert(map_doc.root_.is_mapping());
        assert(map_doc.size(map_doc.root_) == 3);
        auto x = map_doc.find(map_doc.root_, "x"); assert(x->as_int() == 1);
        auto y = map_doc.find(map_doc.root_, "y"); assert(y->as_int() == 2);
        auto z = map_doc.find(map_doc.root_, "z"); assert(z->as_int() == 3);
        std::cout << "  flow mapping: OK\n";
    }

    {
        auto outer = nested.find(nested.root_, "outer");
        auto val = nested.find(*outer, "inner");
        assert(val->as_int() == 99);
        std::cout << "  nested mapping: OK\n";
    }

    {
        assert(multi.size(multi.root_) == 3);
        auto a = multi.find(multi.root_, "a"); assert(a->as_int() == 1);
        auto b = multi.find(multi.root_, "b"); assert(b->as_int() == 2);
        auto c = multi.find(multi.root_, "c"); assert(c->as_int() == 3);
        std::cout << "  multiple keys: OK\n";
    }

    {
        auto items_val = block_seq.find(block_seq.root_, "items");
        assert(items_val.has_value());
        assert(items_val->is_sequence());
        assert(block_seq.size(*items_val) == 3);
        assert(block_seq.at(*items_val, 0).as_string() == "one");
        assert(block_seq.at(*items_val, 1).as_string() == "two");
        assert(block_seq.at(*items_val, 2).as_string() == "three");
        std::cout << "  block sequence: OK\n";
    }

    {
        auto err = std::get<error_code>(bad_result);
        assert(err == error_code::duplicate_key);
        std::cout << "  error handling (duplicate key): OK\n";
    }

    std::cout << "all constexpr tests passed\n";
    return 0;
}
