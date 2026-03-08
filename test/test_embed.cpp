#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

// YAML embedded files
#include "sample_config.yaml.hpp"
#include "edge_minimal.yaml.hpp"
#include "edge_nested.yaml.hpp"
#include "edge_many_items.yaml.hpp"
#include "edge_long_strings.yaml.hpp"
#include "edge_types.yaml.hpp"
#include "edge_sequences.yaml.hpp"
#include "edge_comments.yaml.hpp"

// JSON embedded file
#include "settings.json.hpp"

// TOML embedded file
#include "app_settings.toml.hpp"

// --- YAML embed tests ---

TEST_CASE("yaml embed: sample_config")
{
    constexpr auto& doc = data::embedded::sample_config;
    static_assert(doc.root_.is_mapping());

    auto db = doc.find(doc.root_, "database");
    REQUIRE(db);
    CHECK(db->is_mapping());
    CHECK(doc.find(*db, "host")->as_string() == "localhost");
    CHECK(doc.find(*db, "port")->as_int() == 5432);
    CHECK(doc.find(*db, "ssl")->as_bool() == true);

    auto cache = doc.find(doc.root_, "cache");
    REQUIRE(cache);
    CHECK(doc.find(*cache, "ttl")->as_int() == 3600);
}

TEST_CASE("yaml embed: minimal")
{
    constexpr auto& doc = data::embedded::edge_minimal;
    static_assert(doc.root_.is_mapping());
    CHECK(doc.find(doc.root_, "key")->as_string() == "value");
    CHECK(doc.size(doc.root_) == 1);
}

TEST_CASE("yaml embed: nested")
{
    constexpr auto& doc = data::embedded::edge_nested;

    auto l1 = doc.find(doc.root_, "level1");
    REQUIRE(l1);
    auto l2 = doc.find(*l1, "level2");
    REQUIRE(l2);
    auto l3 = doc.find(*l2, "level3");
    REQUIRE(l3);
    auto l4 = doc.find(*l3, "level4");
    REQUIRE(l4);

    CHECK(doc.find(*l4, "value")->as_string() == "deep");
    CHECK(doc.find(*l3, "sibling")->as_int() == 42);

    auto other = doc.find(*l1, "other");
    REQUIRE(other);
    CHECK(doc.find(*other, "flag")->as_bool() == true);
    CHECK(doc.find(*other, "count")->as_int() == 0);
}

TEST_CASE("yaml embed: many items")
{
    constexpr auto& doc = data::embedded::edge_many_items;
    CHECK(doc.size(doc.root_) == 16);
    CHECK(doc.find(doc.root_, "item_01")->as_string() == "alpha");
    CHECK(doc.find(doc.root_, "item_08")->as_string() == "hotel");
    CHECK(doc.find(doc.root_, "item_16")->as_string() == "papa");
}

TEST_CASE("yaml embed: long strings")
{
    constexpr auto& doc = data::embedded::edge_long_strings;
    CHECK(doc.find(doc.root_, "short")->as_string() == "hi");
    CHECK(doc.find(doc.root_, "medium")->as_string() == "the quick brown fox jumps over the lazy dog");

    auto long_node = doc.find(doc.root_, "long_value");
    REQUIRE(long_node);
    CHECK(long_node->as_string().size() == 99);
    CHECK(long_node->as_string().starts_with("abcdef"));
    CHECK(long_node->as_string().ends_with("6789"));

    CHECK(doc.find(doc.root_, "path")->as_string() == "/usr/local/share/some/deeply/nested/directory/structure/config.yaml");
}

TEST_CASE("yaml embed: types")
{
    constexpr auto& doc = data::embedded::edge_types;
    CHECK(doc.find(doc.root_, "string_val")->as_string() == "hello");
    CHECK(doc.find(doc.root_, "integer_val")->as_int() == 42);
    CHECK(doc.find(doc.root_, "negative_int")->as_int() == -17);
    CHECK(doc.find(doc.root_, "zero")->as_int() == 0);
    CHECK(doc.find(doc.root_, "large_int")->as_int() == 999999);
    CHECK(doc.find(doc.root_, "float_val")->as_float() == doctest::Approx(3.14).epsilon(0.01));
    CHECK(doc.find(doc.root_, "bool_true")->as_bool() == true);
    CHECK(doc.find(doc.root_, "bool_false")->as_bool() == false);
    CHECK(doc.find(doc.root_, "null_val")->is_null());
}

TEST_CASE("yaml embed: sequences")
{
    constexpr auto& doc = data::embedded::edge_sequences;

    auto colors = doc.find(doc.root_, "colors");
    REQUIRE(colors);
    CHECK(colors->is_sequence());
    CHECK(doc.size(*colors) == 3);
    CHECK(doc.at(*colors, 0).as_string() == "red");
    CHECK(doc.at(*colors, 1).as_string() == "green");
    CHECK(doc.at(*colors, 2).as_string() == "blue");

    auto numbers = doc.find(doc.root_, "numbers");
    REQUIRE(numbers);
    CHECK(doc.size(*numbers) == 5);
    CHECK(doc.at(*numbers, 0).as_int() == 1);
    CHECK(doc.at(*numbers, 4).as_int() == 5);
}

TEST_CASE("yaml embed: comments")
{
    constexpr auto& doc = data::embedded::edge_comments;
    static_assert(doc.root_.is_mapping());
    auto db = doc.find(doc.root_, "database");
    REQUIRE(db);
    CHECK(doc.find(*db, "host")->as_string() == "localhost");
    CHECK(doc.find(*db, "port")->as_int() == 5432);
}

TEST_CASE("yaml embed: iterate values")
{
    constexpr auto& doc = data::embedded::edge_sequences;
    auto colors = doc.find(doc.root_, "colors");
    REQUIRE(colors);
    std::size_t count = 0;
    for (auto const& val : doc.values(*colors))
    {
        CHECK(val.is_string());
        ++count;
    }
    CHECK(count == 3);
}

TEST_CASE("yaml embed: iterate entries")
{
    constexpr auto& doc = data::embedded::edge_types;
    std::size_t count = 0;
    for (auto [key, val] : doc.entries(doc.root_))
    {
        CHECK(!key.empty());
        (void)val;
        ++count;
    }
    CHECK(count == doc.size(doc.root_));
}

// --- JSON embed tests ---

TEST_CASE("json embed: sample_config")
{
    constexpr auto& doc = data::embedded::settings;
    static_assert(doc.root_.is_mapping());

    auto db = doc.find(doc.root_, "database");
    REQUIRE(db);
    CHECK(db->is_mapping());
    CHECK(doc.find(*db, "host")->as_string() == "localhost");
    CHECK(doc.find(*db, "port")->as_int() == 5432);
    CHECK(doc.find(*db, "ssl")->as_bool() == true);

    auto cache = doc.find(doc.root_, "cache");
    REQUIRE(cache);
    CHECK(doc.find(*cache, "ttl")->as_int() == 3600);
    CHECK(doc.find(*cache, "max_size")->as_int() == 1024);

    auto features = doc.find(doc.root_, "features");
    REQUIRE(features);
    CHECK(features->is_sequence());
    CHECK(doc.size(*features) == 3);
    CHECK(doc.at(*features, 0).as_string() == "auth");
    CHECK(doc.at(*features, 1).as_string() == "logging");
    CHECK(doc.at(*features, 2).as_string() == "metrics");
}

TEST_CASE("json embed: iterate entries")
{
    constexpr auto& doc = data::embedded::settings;
    std::size_t count = 0;
    for (auto [key, val] : doc.entries(doc.root_))
    {
        CHECK(!key.empty());
        ++count;
    }
    CHECK(count == 3);
}

// --- TOML embed tests ---

TEST_CASE("toml embed: app_settings")
{
    constexpr auto& doc = data::embedded::app_settings;
    static_assert(doc.root_.is_mapping());

    CHECK(doc.find(doc.root_, "title")->as_string() == "App Settings");

    auto db = doc.find(doc.root_, "database");
    REQUIRE(db);
    CHECK(db->is_mapping());
    CHECK(doc.find(*db, "host")->as_string() == "localhost");
    CHECK(doc.find(*db, "port")->as_int() == 5432);
    CHECK(doc.find(*db, "ssl")->as_bool() == true);

    auto cache = doc.find(doc.root_, "cache");
    REQUIRE(cache);
    CHECK(doc.find(*cache, "ttl")->as_int() == 3600);
    CHECK(doc.find(*cache, "max_size")->as_int() == 1024);

    auto features = doc.find(doc.root_, "features");
    REQUIRE(features);
    CHECK(doc.find(*features, "auth")->as_bool() == true);
    CHECK(doc.find(*features, "logging")->as_bool() == true);
    CHECK(doc.find(*features, "metrics")->as_bool() == false);
}

TEST_CASE("toml embed: iterate entries")
{
    constexpr auto& doc = data::embedded::app_settings;
    std::size_t count = 0;
    for (auto [key, val] : doc.entries(doc.root_))
    {
        CHECK(!key.empty());
        ++count;
    }
    CHECK(count == 4);
}
