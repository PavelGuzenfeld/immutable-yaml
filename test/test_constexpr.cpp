#include <cassert>
#include <immutable_yaml/immutable_yaml.hpp>

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

// Inline comments parsed at compile time
constexpr auto commented = parse_or_throw(R"(
# full-line comment
key: value  # inline comment
num: 42
)");
static_assert(commented.root_.is_mapping());

// Error handling via parse()
constexpr auto bad_result = parse(R"({a: 1, a: 2})");
static_assert(std::holds_alternative<error_code>(bad_result));
static_assert(std::get<error_code>(bad_result) == error_code::duplicate_key);

int main()
{
    // simple key-value
    auto name = simple.find(simple.root_, "name");
    assert(name.has_value());
    assert(name->as_string() == "test");

    // integer value
    auto count_val = int_doc.find(int_doc.root_, "count");
    assert(count_val->as_int() == 42);

    // boolean value
    auto active_val = bool_doc.find(bool_doc.root_, "active");
    assert(active_val->as_bool() == true);

    // flow sequence
    assert(seq_doc.root_.is_sequence());
    assert(seq_doc.size(seq_doc.root_) == 3);
    assert(seq_doc.at(seq_doc.root_, 0).as_int() == 10);
    assert(seq_doc.at(seq_doc.root_, 1).as_int() == 20);
    assert(seq_doc.at(seq_doc.root_, 2).as_int() == 30);

    // flow mapping
    assert(map_doc.root_.is_mapping());
    assert(map_doc.size(map_doc.root_) == 3);
    assert(map_doc.find(map_doc.root_, "x")->as_int() == 1);
    assert(map_doc.find(map_doc.root_, "y")->as_int() == 2);
    assert(map_doc.find(map_doc.root_, "z")->as_int() == 3);

    // nested mapping
    auto outer = nested.find(nested.root_, "outer");
    assert(nested.find(*outer, "inner")->as_int() == 99);

    // multiple keys
    assert(multi.size(multi.root_) == 3);
    assert(multi.find(multi.root_, "a")->as_int() == 1);
    assert(multi.find(multi.root_, "b")->as_int() == 2);
    assert(multi.find(multi.root_, "c")->as_int() == 3);

    // block sequence
    auto items_val = block_seq.find(block_seq.root_, "items");
    assert(items_val.has_value());
    assert(items_val->is_sequence());
    assert(block_seq.size(*items_val) == 3);
    assert(block_seq.at(*items_val, 0).as_string() == "one");
    assert(block_seq.at(*items_val, 1).as_string() == "two");
    assert(block_seq.at(*items_val, 2).as_string() == "three");

    // inline comments
    assert(commented.find(commented.root_, "key")->as_string() == "value");
    assert(commented.find(commented.root_, "num")->as_int() == 42);

    // error handling
    assert(std::get<error_code>(bad_result) == error_code::duplicate_key);

    // --- Iteration tests ---

    // iterate sequence values
    {
        std::int64_t sum = 0;
        std::size_t count = 0;
        for (auto const &val : seq_doc.values(seq_doc.root_))
        {
            sum += val.as_int();
            ++count;
        }
        assert(count == 3);
        assert(sum == 60);
    }

    // iterate mapping values
    {
        std::int64_t sum = 0;
        for (auto const &val : map_doc.values(map_doc.root_))
            sum += val.as_int();
        assert(sum == 6);
    }

    // iterate mapping entries (structured bindings)
    {
        std::size_t count = 0;
        for (auto [key, val] : multi.entries(multi.root_))
        {
            if (key == "a") assert(val.as_int() == 1);
            else if (key == "b") assert(val.as_int() == 2);
            else if (key == "c") assert(val.as_int() == 3);
            ++count;
        }
        assert(count == 3);
    }

    // iterate nested block sequence
    {
        std::size_t count = 0;
        for (auto const &val : block_seq.values(*items_val))
        {
            assert(val.is_string());
            ++count;
        }
        assert(count == 3);
    }

    // values() on scalar returns empty range
    {
        auto view = simple.values(*name);
        assert(view.size() == 0);
        std::size_t count = 0;
        for ([[maybe_unused]] auto const &v : view)
            ++count;
        assert(count == 0);
    }

    // entries() on sequence returns empty range
    assert(seq_doc.entries(seq_doc.root_).size() == 0);

    return 0;
}
