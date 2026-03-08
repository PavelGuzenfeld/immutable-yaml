#include <cassert>
#include <iostream>

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

    auto val = doc.find(doc.root_, "key");
    assert(val.has_value());
    assert(val->as_string() == "value");
    assert(doc.size(doc.root_) == 1);

    std::cout << "  minimal: OK\n";
}

void test_nested()
{
    constexpr auto& doc = yaml::embedded::edge_nested;

    auto l1_val = doc.find(doc.root_, "level1");
    assert(l1_val.has_value());

    auto l2_val = doc.find(*l1_val, "level2");
    assert(l2_val.has_value());

    auto l3_val = doc.find(*l2_val, "level3");
    assert(l3_val.has_value());

    auto l4_val = doc.find(*l3_val, "level4");
    assert(l4_val.has_value());

    auto deep = doc.find(*l4_val, "value");
    assert(deep->as_string() == "deep");

    auto sib = doc.find(*l3_val, "sibling");
    assert(sib->as_int() == 42);

    auto other_val = doc.find(*l1_val, "other");
    assert(other_val.has_value());

    auto flag = doc.find(*other_val, "flag");
    assert(flag->as_bool() == true);

    auto count = doc.find(*other_val, "count");
    assert(count->as_int() == 0);

    std::cout << "  nested (4 levels deep): OK\n";
}

void test_many_items()
{
    constexpr auto& doc = yaml::embedded::edge_many_items;

    assert(doc.size(doc.root_) == 16);

    auto v1 = doc.find(doc.root_, "item_01");
    assert(v1->as_string() == "alpha");
    auto v8 = doc.find(doc.root_, "item_08");
    assert(v8->as_string() == "hotel");
    auto v16 = doc.find(doc.root_, "item_16");
    assert(v16->as_string() == "papa");

    std::cout << "  many items (16 entries): OK\n";
}

void test_long_strings()
{
    constexpr auto& doc = yaml::embedded::edge_long_strings;

    auto s = doc.find(doc.root_, "short");
    assert(s->as_string() == "hi");

    auto m = doc.find(doc.root_, "medium");
    assert(m->as_string() == "the quick brown fox jumps over the lazy dog");

    auto l = doc.find(doc.root_, "long_value");
    auto long_val = l->as_string();
    assert(long_val.size() == 95);
    assert(long_val.starts_with("abcdef"));
    assert(long_val.ends_with("6789"));

    auto p = doc.find(doc.root_, "path");
    assert(p->as_string() == "/usr/local/share/some/deeply/nested/directory/structure/config.yaml");

    std::cout << "  long strings: OK\n";
}

void test_types()
{
    constexpr auto& doc = yaml::embedded::edge_types;

    // String
    auto sv = doc.find(doc.root_, "string_val");
    assert(sv->as_string() == "hello");

    // Integers
    auto iv = doc.find(doc.root_, "integer_val");
    assert(iv->as_int() == 42);

    auto nv = doc.find(doc.root_, "negative_int");
    assert(nv->as_int() == -17);

    auto zv = doc.find(doc.root_, "zero");
    assert(zv->as_int() == 0);

    auto lv = doc.find(doc.root_, "large_int");
    assert(lv->as_int() == 999999);

    // Float
    auto fv = doc.find(doc.root_, "float_val");
    assert(fv.has_value());
    auto f = fv->as_float();
    assert(f > 3.13 && f < 3.15);

    // Booleans
    auto bt = doc.find(doc.root_, "bool_true");
    assert(bt->as_bool() == true);
    auto bf = doc.find(doc.root_, "bool_false");
    assert(bf->as_bool() == false);

    // Null
    auto nv2 = doc.find(doc.root_, "null_val");
    assert(nv2.has_value());
    assert(nv2->is_null());

    std::cout << "  all value types: OK\n";
}

void test_sequences()
{
    constexpr auto& doc = yaml::embedded::edge_sequences;

    // String sequence
    auto colors_val = doc.find(doc.root_, "colors");
    assert(colors_val.has_value());
    assert(colors_val->is_sequence());
    assert(doc.size(*colors_val) == 3);
    assert(doc.at(*colors_val, 0).as_string() == "red");
    assert(doc.at(*colors_val, 1).as_string() == "green");
    assert(doc.at(*colors_val, 2).as_string() == "blue");

    // Integer sequence
    auto numbers_val = doc.find(doc.root_, "numbers");
    assert(numbers_val.has_value());
    assert(doc.size(*numbers_val) == 5);
    assert(doc.at(*numbers_val, 0).as_int() == 1);
    assert(doc.at(*numbers_val, 4).as_int() == 5);

    // Mixed-type sequence
    auto mixed_val = doc.find(doc.root_, "mixed");
    assert(mixed_val.has_value());
    assert(doc.size(*mixed_val) == 4);
    assert(doc.at(*mixed_val, 0).is_string());
    assert(doc.at(*mixed_val, 1).is_int());
    assert(doc.at(*mixed_val, 2).is_bool());
    assert(doc.at(*mixed_val, 3).is_null());

    std::cout << "  sequences (typed + mixed): OK\n";
}

void test_comments()
{
    constexpr auto& doc = yaml::embedded::edge_comments;
    static_assert(doc.root_.is_mapping());

    // Comments should be stripped — only data remains
    auto db = doc.find(doc.root_, "database");
    assert(db.has_value());
    assert(db->is_mapping());

    auto host = doc.find(*db, "host");
    assert(host->as_string() == "localhost");

    auto port = doc.find(*db, "port");
    assert(port->as_int() == 5432);

    auto cache = doc.find(doc.root_, "cache");
    assert(cache.has_value());

    auto ttl = doc.find(*cache, "ttl");
    assert(ttl->as_int() == 3600);

    std::cout << "  comments stripped: OK\n";
}

int main()
{
    std::cout << "yaml_embed edge case tests:\n";

    test_minimal();
    test_nested();
    test_many_items();
    test_long_strings();
    test_types();
    test_sequences();
    test_comments();

    std::cout << "all edge case tests passed\n";
    return 0;
}
