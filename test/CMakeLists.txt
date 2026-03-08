include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
)
FetchContent_MakeAvailable(doctest)

include(${CMAKE_SOURCE_DIR}/cmake/YamlEmbed.cmake)

# --- Constexpr / parse API tests ---
add_executable(${PROJECT_NAME}_test_parse test_parse.cpp)
target_link_libraries(${PROJECT_NAME}_test_parse PRIVATE ${PROJECT_NAME} doctest)
add_test(NAME yaml_parse COMMAND ${PROJECT_NAME}_test_parse)

# --- Embed integration tests ---
add_executable(${PROJECT_NAME}_test_embed test_embed.cpp)
target_link_libraries(${PROJECT_NAME}_test_embed PRIVATE ${PROJECT_NAME} doctest)
yaml_embed(${PROJECT_NAME}_test_embed
    ${CMAKE_CURRENT_SOURCE_DIR}/sample_config.yaml
    ${CMAKE_CURRENT_SOURCE_DIR}/edge_minimal.yaml
    ${CMAKE_CURRENT_SOURCE_DIR}/edge_nested.yaml
    ${CMAKE_CURRENT_SOURCE_DIR}/edge_many_items.yaml
    ${CMAKE_CURRENT_SOURCE_DIR}/edge_long_strings.yaml
    ${CMAKE_CURRENT_SOURCE_DIR}/edge_types.yaml
    ${CMAKE_CURRENT_SOURCE_DIR}/edge_sequences.yaml
    ${CMAKE_CURRENT_SOURCE_DIR}/edge_comments.yaml
)
add_test(NAME yaml_embed COMMAND ${PROJECT_NAME}_test_embed)
