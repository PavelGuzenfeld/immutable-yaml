include(${CMAKE_SOURCE_DIR}/cmake/YamlEmbed.cmake)

# Legacy tests (require fmt + cmake-library)
find_package(fmt QUIET)
find_package(cmake-library QUIET)

if(fmt_FOUND AND cmake-library_FOUND)
    add_executable(${PROJECT_NAME}_test test_main.cpp)
    target_link_libraries(${PROJECT_NAME}_test PRIVATE ${PROJECT_NAME} fmt)
    set_warnings_and_errors(${PROJECT_NAME}_test)
    add_test(NAME yaml_parse_test COMMAND ${PROJECT_NAME}_test)

    add_executable(${PROJECT_NAME}_test_negative test_negative.cpp)
    target_link_libraries(${PROJECT_NAME}_test_negative PRIVATE ${PROJECT_NAME} fmt)
    set_warnings_and_errors(${PROJECT_NAME}_test_negative)
    add_test(NAME yaml_negative_test COMMAND ${PROJECT_NAME}_test_negative)

    install(
        TARGETS ${PROJECT_NAME}_test_negative ${PROJECT_NAME}_test
        EXPORT ${PROJECT_NAME}Targets
        DESTINATION bin
    )
else()
    message(STATUS "Skipping legacy tests (fmt or cmake-library not found)")
endif()

# --- Embed integration tests (self-contained, no external deps) ---

# Basic embed test
add_executable(${PROJECT_NAME}_test_embed test_embed.cpp)
target_link_libraries(${PROJECT_NAME}_test_embed PRIVATE ${PROJECT_NAME})
yaml_embed(${PROJECT_NAME}_test_embed
    ${CMAKE_CURRENT_SOURCE_DIR}/sample_config.yaml
)
add_test(NAME yaml_embed_basic COMMAND ${PROJECT_NAME}_test_embed)

# Edge case tests
add_executable(${PROJECT_NAME}_test_edge_cases test_edge_cases.cpp)
target_link_libraries(${PROJECT_NAME}_test_edge_cases PRIVATE ${PROJECT_NAME})
yaml_embed(${PROJECT_NAME}_test_edge_cases
    ${CMAKE_CURRENT_SOURCE_DIR}/edge_minimal.yaml
    ${CMAKE_CURRENT_SOURCE_DIR}/edge_nested.yaml
    ${CMAKE_CURRENT_SOURCE_DIR}/edge_many_items.yaml
    ${CMAKE_CURRENT_SOURCE_DIR}/edge_long_strings.yaml
    ${CMAKE_CURRENT_SOURCE_DIR}/edge_types.yaml
    ${CMAKE_CURRENT_SOURCE_DIR}/edge_sequences.yaml
)
add_test(NAME yaml_embed_edge_cases COMMAND ${PROJECT_NAME}_test_edge_cases)

# Standalone constexpr tests (no yaml_embed, validates parse API directly)
add_executable(${PROJECT_NAME}_test_constexpr test_constexpr.cpp)
target_link_libraries(${PROJECT_NAME}_test_constexpr PRIVATE ${PROJECT_NAME})
add_test(NAME yaml_constexpr COMMAND ${PROJECT_NAME}_test_constexpr)

install(
    TARGETS
    ${PROJECT_NAME}_test_embed
    ${PROJECT_NAME}_test_edge_cases
    ${PROJECT_NAME}_test_constexpr
    EXPORT ${PROJECT_NAME}Targets
    DESTINATION bin
)
