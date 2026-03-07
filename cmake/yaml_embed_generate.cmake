# yaml_embed_generate.cmake — called at build time by yaml_embed()
# Inputs: YAML_INPUT, YAML_OUTPUT, CPP_IDENT, YAML_NAME

file(READ "${YAML_INPUT}" YAML_CONTENT)

file(WRITE "${YAML_OUTPUT}"
"#pragma once
// Auto-generated from ${YAML_NAME} — do not edit
// Modify the source YAML file and rebuild

#include <yaml/yaml.hpp>

namespace yaml::embedded {

inline constexpr auto ${CPP_IDENT} = yaml::ct::parse_or_throw(
R\"__yaml__(${YAML_CONTENT})__yaml__\");

} // namespace yaml::embedded
")
