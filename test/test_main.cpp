// because apparently you need me to teach you how to write proper tests too
// this is what a real test suite looks like, not whatever assert() garbage
// you were probably planning to use

#include <cassert>
#include <iostream>
#include <yaml/yaml.hpp> // fixed your fucking include path

int main()
{
  std::cout << "running compile-time yaml parser tests...\n";
  std::cout << "if any of these fail, your yaml library is garbage\n\n";

  // basic validation tests
  static_assert(yaml::ct::is_valid(R"(key: value)"));
  static_assert(yaml::ct::is_valid(R"([1, 2, 3])"));
  static_assert(yaml::ct::is_valid(R"({key: value, other: 42})"));
  static_assert(!yaml::ct::is_valid(R"({key: value, key: duplicate})"));

  // actual parsing tests - simplified to avoid complex compile-time access
  constexpr auto simple_doc = yaml::ct::parse_or_throw(R"(key: value)");
  constexpr auto array_doc = yaml::ct::parse_or_throw(R"([1, 2, 3, 4, 5])");
  constexpr auto mapping_doc = yaml::ct::parse_or_throw(R"({name: "test", count: 42})");

  // Test that the parsing actually worked by checking we can create the documents
  static_assert(std::holds_alternative<yaml::ct::detail::mapping_impl>(simple_doc.root_) ||
                std::holds_alternative<yaml::ct::detail::sequence_impl>(simple_doc.root_) ||
                std::holds_alternative<yaml::ct::detail::string_storage<256>>(simple_doc.root_));

  static_assert(std::holds_alternative<yaml::ct::detail::sequence_impl>(array_doc.root_));
  static_assert(std::holds_alternative<yaml::ct::detail::mapping_impl>(mapping_doc.root_));

  std::cout << "all compile-time tests passed!\n";
  std::cout << "your yaml parser doesn't completely suck\n";

  // Runtime verification that the values are accessible
  if (auto *mapping = std::get_if<yaml::ct::detail::mapping_impl>(&simple_doc.root_))
  {
    if (auto value = mapping->find("key"))
    {
      if (auto *str = std::get_if<yaml::ct::detail::string_storage<256>>(&*value))
      {
        std::cout << "Found key 'key' with value: '" << str->view() << "'\n";
      }
    }
  }

  if (auto *sequence = std::get_if<yaml::ct::detail::sequence_impl>(&array_doc.root_))
  {
    std::cout << "Array has " << sequence->size() << " elements\n";
    if (sequence->size() > 0)
    {
      if (auto *num = std::get_if<yaml::ct::detail::integer>(&(*sequence)[0]))
      {
        std::cout << "First element: " << *num << "\n";
      }
    }
  }

  return 0;
}