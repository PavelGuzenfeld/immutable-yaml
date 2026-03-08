# DataEmbed.cmake — Embed YAML/JSON files as compile-time parsed C++ constants
#
# Usage:
#   find_package(immutable-data-embedder REQUIRED)
#   include(DataEmbed)
#   data_embed(my_target config.yaml settings.json)
#
# This generates headers you can include:
#   #include "config.yaml.hpp"
#   constexpr auto& cfg = data::embedded::config;
#
# Format is auto-detected from file extension (.yaml/.yml → YAML, .json → JSON).

# Round up to the next power of 2 (minimum 16)
function(_data_round_up_pow2 INPUT OUTPUT)
    set(_val ${INPUT})
    set(_pow 16)
    while(_pow LESS _val)
        math(EXPR _pow "${_pow} * 2")
    endwhile()
    set(${OUTPUT} ${_pow} PARENT_SCOPE)
endfunction()

# Analyze a data file and compute capacity requirements
function(_data_analyze_file DATA_FILE OUT_TOKENS OUT_ITEMS OUT_STRING OUT_NODES)
    file(READ "${DATA_FILE}" CONTENT)

    # Count lines → approximate token count
    string(REGEX MATCHALL "\n" NEWLINES "${CONTENT}")
    list(LENGTH NEWLINES LINE_COUNT)
    math(EXPR LINE_COUNT "${LINE_COUNT} + 1")
    math(EXPR TOKEN_EST "${LINE_COUNT} * 4 + 8")

    # Count mapping entries (colons not inside quotes)
    string(REGEX MATCHALL "[a-zA-Z0-9_\"]+:" COLONS "${CONTENT}")
    list(LENGTH COLONS COLON_COUNT)

    # Count sequence entries (dash at line start or array elements)
    string(REGEX MATCHALL "\n[ ]*- " DASHES "${CONTENT}")
    list(LENGTH DASHES DASH_COUNT)
    string(REGEX MATCH "^[ ]*- " LEADING_DASH "${CONTENT}")
    if(LEADING_DASH)
        math(EXPR DASH_COUNT "${DASH_COUNT} + 1")
    endif()

    # Total nodes
    math(EXPR TOTAL_NODES "${COLON_COUNT} + ${DASH_COUNT} + 4")

    # Max items per level
    if(COLON_COUNT GREATER DASH_COUNT)
        set(ITEMS_EST ${COLON_COUNT})
    else()
        set(ITEMS_EST ${DASH_COUNT})
    endif()
    math(EXPR ITEMS_EST "${ITEMS_EST} + 4")

    # Max string size = longest line + margin
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
    _data_round_up_pow2(${TOKEN_EST} TOKEN_POW2)
    _data_round_up_pow2(${ITEMS_EST} ITEMS_POW2)
    _data_round_up_pow2(${STRING_EST} STRING_POW2)
    _data_round_up_pow2(${TOTAL_NODES} NODES_POW2)

    set(${OUT_TOKENS} ${TOKEN_POW2} PARENT_SCOPE)
    set(${OUT_ITEMS} ${ITEMS_POW2} PARENT_SCOPE)
    set(${OUT_STRING} ${STRING_POW2} PARENT_SCOPE)
    set(${OUT_NODES} ${NODES_POW2} PARENT_SCOPE)
endfunction()

function(data_embed TARGET)
    set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/data_generated")
    file(MAKE_DIRECTORY "${OUTPUT_DIR}")

    # Track max sizes across all files for this target
    set(MAX_TOKENS 16)
    set(MAX_ITEMS 16)
    set(MAX_STRING 16)
    set(MAX_NODES 16)

    foreach(DATA_FILE ${ARGN})
        get_filename_component(FILE_ABSOLUTE "${DATA_FILE}" ABSOLUTE)
        get_filename_component(FILE_NAME "${DATA_FILE}" NAME)
        get_filename_component(FILE_NAME_WE "${DATA_FILE}" NAME_WE)
        get_filename_component(FILE_EXT "${DATA_FILE}" LAST_EXT)

        # Sanitize filename to valid C++ identifier
        string(REGEX REPLACE "[^a-zA-Z0-9_]" "_" CPP_IDENT "${FILE_NAME_WE}")

        # Detect format from extension
        string(TOLOWER "${FILE_EXT}" FILE_EXT_LOWER)
        if(FILE_EXT_LOWER STREQUAL ".json")
            set(DATA_FORMAT "json")
        elseif(FILE_EXT_LOWER STREQUAL ".toml")
            set(DATA_FORMAT "toml")
        else()
            set(DATA_FORMAT "yaml")
        endif()

        set(OUTPUT_FILE "${OUTPUT_DIR}/${FILE_NAME}.hpp")

        # Analyze this file
        _data_analyze_file("${FILE_ABSOLUTE}" FILE_TOKENS FILE_ITEMS FILE_STRING FILE_NODES)

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
                -DDATA_INPUT=${FILE_ABSOLUTE}
                -DDATA_OUTPUT=${OUTPUT_FILE}
                -DCPP_IDENT=${CPP_IDENT}
                -DDATA_NAME=${FILE_NAME}
                -DDATA_FORMAT=${DATA_FORMAT}
                -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/data_embed_generate.cmake"
            DEPENDS "${FILE_ABSOLUTE}"
                    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/data_embed_generate.cmake"
            COMMENT "Embedding ${DATA_FORMAT}: ${FILE_NAME}"
            VERBATIM
        )

        target_sources(${TARGET} PRIVATE "${OUTPUT_FILE}")
    endforeach()

    message(STATUS "data_embed(${TARGET}): TOKENS=${MAX_TOKENS} ITEMS=${MAX_ITEMS} STRING=${MAX_STRING} NODES=${MAX_NODES}")

    target_compile_definitions(${TARGET} PRIVATE
        DATA_CT_MAX_TOKENS=${MAX_TOKENS}
        DATA_CT_MAX_ITEMS=${MAX_ITEMS}
        DATA_CT_MAX_STRING_SIZE=${MAX_STRING}
        DATA_CT_MAX_NODES=${MAX_NODES}
    )
    target_include_directories(${TARGET} PRIVATE "${OUTPUT_DIR}")
endfunction()
