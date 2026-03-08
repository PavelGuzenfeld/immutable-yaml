#pragma once

// embed.hpp — Helpers for embedding data files at compile time
//
// Usage with CMake:
//   include(DataEmbed)
//   data_embed(my_target config.yaml settings.json config.toml)
//
//   #include "config.yaml.hpp"
//   constexpr auto& cfg = data::embedded::config;

#include <immutable_data/yaml.hpp>
#include <immutable_data/json.hpp>
#include <immutable_data/toml.hpp>
#include <immutable_data/xml.hpp>
