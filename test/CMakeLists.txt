include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
)
FetchContent_MakeAvailable(doctest)

include(${CMAKE_SOURCE_DIR}/cmake/DataEmbed.cmake)

# --- YAML parse API tests ---
add_executable(${PROJECT_NAME}_test_yaml test_yaml_parse.cpp)
target_link_libraries(${PROJECT_NAME}_test_yaml PRIVATE ${PROJECT_NAME} doctest)
add_test(NAME yaml_parse COMMAND ${PROJECT_NAME}_test_yaml)

# --- JSON parse API tests ---
add_executable(${PROJECT_NAME}_test_json test_json_parse.cpp)
target_link_libraries(${PROJECT_NAME}_test_json PRIVATE ${PROJECT_NAME} doctest)
add_test(NAME json_parse COMMAND ${PROJECT_NAME}_test_json)

# --- Embed integration tests (YAML + JSON) ---
add_executable(${PROJECT_NAME}_test_embed test_embed.cpp)
target_link_libraries(${PROJECT_NAME}_test_embed PRIVATE ${PROJECT_NAME} doctest)
data_embed(${PROJECT_NAME}_test_embed
    ${CMAKE_CURRENT_SOURCE_DIR}/sample_config.yaml
    ${CMAKE_CURRENT_SOURCE_DIR}/edge_minimal.yaml
    ${CMAKE_CURRENT_SOURCE_DIR}/edge_nested.yaml
    ${CMAKE_CURRENT_SOURCE_DIR}/edge_many_items.yaml
    ${CMAKE_CURRENT_SOURCE_DIR}/edge_long_strings.yaml
    ${CMAKE_CURRENT_SOURCE_DIR}/edge_types.yaml
    ${CMAKE_CURRENT_SOURCE_DIR}/edge_sequences.yaml
    ${CMAKE_CURRENT_SOURCE_DIR}/edge_comments.yaml
    ${CMAKE_CURRENT_SOURCE_DIR}/settings.json
)
add_test(NAME data_embed COMMAND ${PROJECT_NAME}_test_embed)
