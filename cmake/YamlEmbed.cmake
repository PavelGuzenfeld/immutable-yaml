# YamlEmbed.cmake — Embed YAML files as compile-time parsed C++ constants
#
# Usage:
#   find_package(immutable-yaml REQUIRED)
#   include(YamlEmbed)
#   yaml_embed(my_target config.yaml settings.yaml)
#
# This generates headers you can include:
#   #include "config.yaml.hpp"
#   constexpr auto& cfg = yaml::embedded::config;
#
# Allocation sizes (max tokens, max items, max string size) are computed
# automatically from the YAML files — no manual tuning required.

# Round up to the next power of 2 (minimum 16)
function(_yaml_round_up_pow2 INPUT OUTPUT)
    set(_val ${INPUT})
    set(_pow 16)
    while(_pow LESS _val)
        math(EXPR _pow "${_pow} * 2")
    endwhile()
    set(${OUTPUT} ${_pow} PARENT_SCOPE)
endfunction()

# Analyze a YAML file and compute capacity requirements
function(_yaml_analyze_file YAML_FILE OUT_TOKENS OUT_ITEMS OUT_STRING OUT_NODES)
    file(READ "${YAML_FILE}" CONTENT)

    # Count lines → approximate token count (each line ≈ 3-4 tokens)
    string(REGEX MATCHALL "\n" NEWLINES "${CONTENT}")
    list(LENGTH NEWLINES LINE_COUNT)
    math(EXPR LINE_COUNT "${LINE_COUNT} + 1")
    math(EXPR TOKEN_EST "${LINE_COUNT} * 4 + 8")

    # Count mapping entries (colons not inside quotes)
    string(REGEX MATCHALL "[a-zA-Z0-9_]+:" COLONS "${CONTENT}")
    list(LENGTH COLONS COLON_COUNT)

    # Count sequence entries (dash at line start after optional whitespace)
    string(REGEX MATCHALL "\n[ ]*- " DASHES "${CONTENT}")
    list(LENGTH DASHES DASH_COUNT)

    # Also check if file starts with a sequence entry
    string(REGEX MATCH "^[ ]*- " LEADING_DASH "${CONTENT}")
    if(LEADING_DASH)
        math(EXPR DASH_COUNT "${DASH_COUNT} + 1")
    endif()

    # Total nodes = all entries across all levels (for pool sizing)
    math(EXPR TOTAL_NODES "${COLON_COUNT} + ${DASH_COUNT} + 4")

    # Max items per level = conservative estimate
    if(COLON_COUNT GREATER DASH_COUNT)
        set(ITEMS_EST ${COLON_COUNT})
    else()
        set(ITEMS_EST ${DASH_COUNT})
    endif()
    math(EXPR ITEMS_EST "${ITEMS_EST} + 4")

    # Max string size = longest scalar value + margin
    set(MAX_LINE_LEN 0)
    string(REGEX REPLACE "\n" ";" LINES "${CONTENT}")
    foreach(LINE ${LINES})
        string(LENGTH "${LINE}" LEN)
        if(LEN GREATER MAX_LINE_LEN)
            set(MAX_LINE_LEN ${LEN})
        endif()
    endforeach()
    math(EXPR STRING_EST "${MAX_LINE_LEN} + 16")

    # Round up to powers of 2
    _yaml_round_up_pow2(${TOKEN_EST} TOKEN_POW2)
    _yaml_round_up_pow2(${ITEMS_EST} ITEMS_POW2)
    _yaml_round_up_pow2(${STRING_EST} STRING_POW2)
    _yaml_round_up_pow2(${TOTAL_NODES} NODES_POW2)

    set(${OUT_TOKENS} ${TOKEN_POW2} PARENT_SCOPE)
    set(${OUT_ITEMS} ${ITEMS_POW2} PARENT_SCOPE)
    set(${OUT_STRING} ${STRING_POW2} PARENT_SCOPE)
    set(${OUT_NODES} ${NODES_POW2} PARENT_SCOPE)
endfunction()

function(yaml_embed TARGET)
    set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/yaml_generated")
    file(MAKE_DIRECTORY "${OUTPUT_DIR}")

    # Track max sizes across all YAML files for this target
    set(MAX_TOKENS 16)
    set(MAX_ITEMS 16)
    set(MAX_STRING 16)
    set(MAX_NODES 16)

    foreach(YAML_FILE ${ARGN})
        get_filename_component(YAML_ABSOLUTE "${YAML_FILE}" ABSOLUTE)
        get_filename_component(YAML_NAME "${YAML_FILE}" NAME)
        get_filename_component(YAML_NAME_WE "${YAML_FILE}" NAME_WE)

        # Sanitize filename to valid C++ identifier
        string(REGEX REPLACE "[^a-zA-Z0-9_]" "_" CPP_IDENT "${YAML_NAME_WE}")

        set(OUTPUT_FILE "${OUTPUT_DIR}/${YAML_NAME}.hpp")

        # Analyze this YAML file
        _yaml_analyze_file("${YAML_ABSOLUTE}" FILE_TOKENS FILE_ITEMS FILE_STRING FILE_NODES)

        # Update maximums
        if(FILE_TOKENS GREATER MAX_TOKENS)
            set(MAX_TOKENS ${FILE_TOKENS})
        endif()
        if(FILE_ITEMS GREATER MAX_ITEMS)
            set(MAX_ITEMS ${FILE_ITEMS})
        endif()
        if(FILE_STRING GREATER MAX_STRING)
            set(MAX_STRING ${FILE_STRING})
        endif()
        if(FILE_NODES GREATER MAX_NODES)
            set(MAX_NODES ${FILE_NODES})
        endif()

        add_custom_command(
            OUTPUT "${OUTPUT_FILE}"
            COMMAND "${CMAKE_COMMAND}"
                -DYAML_INPUT=${YAML_ABSOLUTE}
                -DYAML_OUTPUT=${OUTPUT_FILE}
                -DCPP_IDENT=${CPP_IDENT}
                -DYAML_NAME=${YAML_NAME}
                -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/yaml_embed_generate.cmake"
            DEPENDS "${YAML_ABSOLUTE}"
                    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/yaml_embed_generate.cmake"
            COMMENT "Embedding YAML: ${YAML_NAME}"
            VERBATIM
        )

        target_sources(${TARGET} PRIVATE "${OUTPUT_FILE}")
    endforeach()

    message(STATUS "yaml_embed(${TARGET}): TOKENS=${MAX_TOKENS} ITEMS=${MAX_ITEMS} STRING=${MAX_STRING} NODES=${MAX_NODES}")

    target_compile_definitions(${TARGET} PRIVATE
        YAML_CT_MAX_TOKENS=${MAX_TOKENS}
        YAML_CT_MAX_ITEMS=${MAX_ITEMS}
        YAML_CT_MAX_STRING_SIZE=${MAX_STRING}
        YAML_CT_MAX_NODES=${MAX_NODES}
    )
    target_include_directories(${TARGET} PRIVATE "${OUTPUT_DIR}")
endfunction()
