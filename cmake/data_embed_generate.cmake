# data_embed_generate.cmake — called at build time by data_embed()
# Inputs: DATA_INPUT, DATA_OUTPUT, CPP_IDENT, DATA_NAME, DATA_FORMAT

file(READ "${DATA_INPUT}" DATA_CONTENT)

# For YAML/TOML files, strip full-line comments
if(DATA_FORMAT STREQUAL "yaml" OR DATA_FORMAT STREQUAL "toml")
    string(REGEX REPLACE "\n[ \t]*#[^\n]*" "" DATA_CONTENT "${DATA_CONTENT}")
    string(REGEX REPLACE "^[ \t]*#[^\n]*\n?" "" DATA_CONTENT "${DATA_CONTENT}")
endif()

# Collapse multiple blank lines into one
string(REGEX REPLACE "\n\n+" "\n" DATA_CONTENT "${DATA_CONTENT}")
# Trim leading/trailing whitespace
string(STRIP "${DATA_CONTENT}" DATA_CONTENT)

# Select the right parser based on format
if(DATA_FORMAT STREQUAL "json")
    set(INCLUDE_HEADER "immutable_data/json.hpp")
    set(PARSE_FUNC "data::json::parse_or_throw")
elseif(DATA_FORMAT STREQUAL "toml")
    set(INCLUDE_HEADER "immutable_data/toml.hpp")
    set(PARSE_FUNC "data::toml::parse_or_throw")
elseif(DATA_FORMAT STREQUAL "xml")
    set(INCLUDE_HEADER "immutable_data/xml.hpp")
    set(PARSE_FUNC "data::xml::parse_or_throw")
else()
    set(INCLUDE_HEADER "immutable_data/yaml.hpp")
    set(PARSE_FUNC "data::yaml::parse_or_throw")
endif()

file(WRITE "${DATA_OUTPUT}"
"#pragma once
// Auto-generated from ${DATA_NAME} — do not edit
// Modify the source file and rebuild

#include <${INCLUDE_HEADER}>

namespace data::embedded {

inline constexpr auto ${CPP_IDENT} = ${PARSE_FUNC}(
R\"__data__(${DATA_CONTENT})__data__\");

} // namespace data::embedded
")
