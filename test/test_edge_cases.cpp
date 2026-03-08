#include <cassert>

#include "edge_minimal.yaml.hpp"
#include "edge_nested.yaml.hpp"
#include "edge_many_items.yaml.hpp"
#include "edge_long_strings.yaml.hpp"
#include "edge_types.yaml.hpp"
#include "edge_sequences.yaml.hpp"
#include "edge_comments.yaml.hpp"

void test_minimal()
{
    constexpr auto& doc = yaml::embedded::edge_minimal;
    static_assert(doc.root_.is_mapping());

    assert(doc.find(doc.root_, "key")->as_string() == "value");
    assert(doc.size(doc.root_) == 1);
}

void test_nested()
{
    constexpr auto& doc = yaml::embedded::edge_nested;

    auto l1 = doc.find(doc.root_, "level1");
    assert(l1.has_value());

    auto l2 = doc.find(*l1, "level2");
    assert(l2.has_value());

    auto l3 = doc.find(*l2, "level3");
    assert(l3.has_value());

    auto l4 = doc.find(*l3, "level4");
    assert(l4.has_value());

    assert(doc.find(*l4, "value")->as_string() == "deep");
    assert(doc.find(*l3, "sibling")->as_int() == 42);

    auto other = doc.find(*l1, "other");
    assert(doc.find(*other, "flag")->as_bool() == true);
    assert(doc.find(*other, "count")->as_int() == 0);
}

void test_many_items()
{
    constexpr auto& doc = yaml::embedded::edge_many_items;

    assert(doc.size(doc.root_) == 16);
    assert(doc.find(doc.root_, "item_01")->as_string() == "alpha");
    assert(doc.find(doc.root_, "item_08")->as_string() == "hotel");
    assert(doc.find(doc.root_, "item_16")->as_string() == "papa");
}

void test_long_strings()
{
    constexpr auto& doc = yaml::embedded::edge_long_strings;

    assert(doc.find(doc.root_, "short")->as_string() == "hi");
    assert(doc.find(doc.root_, "medium")->as_string() == "the quick brown fox jumps over the lazy dog");

    auto long_val = doc.find(doc.root_, "long_value")->as_string();
    assert(long_val.size() == 95);
    assert(long_val.starts_with("abcdef"));
    assert(long_val.ends_with("6789"));

    assert(doc.find(doc.root_, "path")->as_string() == "/usr/local/share/some/deeply/nested/directory/structure/config.yaml");
}

void test_types()
{
    constexpr auto& doc = yaml::embedded::edge_types;

    assert(doc.find(doc.root_, "string_val")->as_string() == "hello");
    assert(doc.find(doc.root_, "integer_val")->as_int() == 42);
    assert(doc.find(doc.root_, "negative_int")->as_int() == -17);
    assert(doc.find(doc.root_, "zero")->as_int() == 0);
    assert(doc.find(doc.root_, "large_int")->as_int() == 999999);

    auto f = doc.find(doc.root_, "float_val")->as_float();
    assert(f > 3.13 && f < 3.15);

    assert(doc.find(doc.root_, "bool_true")->as_bool() == true);
    assert(doc.find(doc.root_, "bool_false")->as_bool() == false);
    assert(doc.find(doc.root_, "null_val")->is_null());
}

void test_sequences()
{
    constexpr auto& doc = yaml::embedded::edge_sequences;

    auto colors = doc.find(doc.root_, "colors");
    assert(colors.has_value());
    assert(colors->is_sequence());
    assert(doc.size(*colors) == 3);
    assert(doc.at(*colors, 0).as_string() == "red");
    assert(doc.at(*colors, 1).as_string() == "green");
    assert(doc.at(*colors, 2).as_string() == "blue");

    auto numbers = doc.find(doc.root_, "numbers");
    assert(doc.size(*numbers) == 5);
    assert(doc.at(*numbers, 0).as_int() == 1);
    assert(doc.at(*numbers, 4).as_int() == 5);

    auto mixed = doc.find(doc.root_, "mixed");
    assert(doc.size(*mixed) == 4);
    assert(doc.at(*mixed, 0).is_string());
    assert(doc.at(*mixed, 1).is_int());
    assert(doc.at(*mixed, 2).is_bool());
    assert(doc.at(*mixed, 3).is_null());
}

void test_comments()
{
    constexpr auto& doc = yaml::embedded::edge_comments;
    static_assert(doc.root_.is_mapping());

    auto db = doc.find(doc.root_, "database");
    assert(db.has_value());
    assert(db->is_mapping());
    assert(doc.find(*db, "host")->as_string() == "localhost");
    assert(doc.find(*db, "port")->as_int() == 5432);

    auto cache = doc.find(doc.root_, "cache");
    assert(cache.has_value());
    assert(doc.find(*cache, "ttl")->as_int() == 3600);
}

void test_iterate_sequence()
{
    constexpr auto& doc = yaml::embedded::edge_sequences;

    // iterate colors sequence with values()
    auto colors = doc.find(doc.root_, "colors");
    std::size_t count = 0;
    for (auto const& val : doc.values(*colors))
    {
        assert(val.is_string());
        ++count;
    }
    assert(count == 3);

    // iterate numbers and sum
    auto numbers = doc.find(doc.root_, "numbers");
    std::int64_t sum = 0;
    for (auto const& val : doc.values(*numbers))
        sum += val.as_int();
    assert(sum == 15); // 1+2+3+4+5
}

void test_iterate_mapping()
{
    constexpr auto& doc = yaml::embedded::edge_types;

    // iterate all top-level entries
    std::size_t count = 0;
    for (auto [key, val] : doc.entries(doc.root_))
    {
        assert(!key.empty());
        (void)val;
        ++count;
    }
    assert(count == doc.size(doc.root_));
}

void test_iterate_tree()
{
    constexpr auto& doc = yaml::embedded::edge_nested;

    // walk the tree: root -> level1 -> level2 -> level3 entries
    auto l1 = doc.find(doc.root_, "level1");
    auto l2 = doc.find(*l1, "level2");
    auto l3 = doc.find(*l2, "level3");

    std::size_t count = 0;
    for (auto [key, val] : doc.entries(*l3))
    {
        if (key == "level4") assert(val.is_mapping());
        else if (key == "sibling") assert(val.as_int() == 42);
        ++count;
    }
    assert(count == 2);

    // walk into level4
    auto l4 = doc.find(*l3, "level4");
    for (auto [key, val] : doc.entries(*l4))
    {
        assert(key == "value");
        assert(val.as_string() == "deep");
    }
}

int main()
{
    test_minimal();
    test_nested();
    test_many_items();
    test_long_strings();
    test_types();
    test_sequences();
    test_comments();
    test_iterate_sequence();
    test_iterate_mapping();
    test_iterate_tree();

    return 0;
}
