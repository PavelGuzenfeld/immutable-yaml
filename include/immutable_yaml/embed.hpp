#pragma once

// embed.hpp — Helpers for embedding YAML files via C++23 #embed
//
// Usage with #embed (GCC 15+, Clang 19+):
//
//   static constexpr char raw[] = {
//       #embed "config.yaml"
//       , '\0'
//   };
//   constexpr auto config = yaml::ct::from_embedded(raw);
//
// Usage with CMake (any C++23 compiler):
//
//   # CMakeLists.txt:
//   include(YamlEmbed)
//   yaml_embed(my_target config.yaml)
//
//   // C++:
//   #include "config.yaml.hpp"
//   constexpr auto& cfg = yaml::embedded::config;

#include <immutable_yaml/immutable_yaml.hpp>

namespace yaml::ct
{

    template <std::size_t N>
    constexpr auto from_embedded(const char (&data)[N]) noexcept -> result<document>
    {
        return parse(data);
    }

    template <std::size_t N>
    constexpr auto from_embedded_or_throw(const char (&data)[N]) -> document
    {
        return parse_or_throw(data);
    }

} // namespace yaml::ct
