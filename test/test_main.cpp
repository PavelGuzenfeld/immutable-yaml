#include <cassert>
#include <immutable_yaml/immutable_yaml.hpp>

int main()
{
    static_assert(yaml::ct::is_valid(R"(key: value)"));
    static_assert(yaml::ct::is_valid(R"([1, 2, 3])"));
    static_assert(yaml::ct::is_valid(R"({key: value, other: 42})"));
    static_assert(!yaml::ct::is_valid(R"({key: value, key: duplicate})"));

    constexpr auto simple_doc = yaml::ct::parse_or_throw(R"(key: value)");
    static_assert(simple_doc.root_.is_mapping());

    constexpr auto array_doc = yaml::ct::parse_or_throw(R"([1, 2, 3, 4, 5])");
    static_assert(array_doc.root_.is_sequence());

    constexpr auto mapping_doc = yaml::ct::parse_or_throw(R"({name: "test", count: 42})");
    static_assert(mapping_doc.root_.is_mapping());

    assert(simple_doc.find(simple_doc.root_, "key")->as_string() == "value");
    assert(array_doc.size(array_doc.root_) == 5);
    assert(array_doc.at(array_doc.root_, 0).as_int() == 1);
    assert(mapping_doc.find(mapping_doc.root_, "name")->as_string() == "test");
    assert(mapping_doc.find(mapping_doc.root_, "count")->as_int() == 42);

    return 0;
}
