include(${CMAKE_SOURCE_DIR}/cmake/YamlEmbed.cmake)

# Example 1: inline constexpr parsing
add_executable(example_basic basic.cpp)
target_link_libraries(example_basic PRIVATE ${PROJECT_NAME})
add_test(NAME example_basic COMMAND example_basic)

# Example 2: embedded YAML file
add_executable(example_embed embed.cpp)
target_link_libraries(example_embed PRIVATE ${PROJECT_NAME})
yaml_embed(example_embed
    ${CMAKE_CURRENT_SOURCE_DIR}/app_config.yaml
)
add_test(NAME example_embed COMMAND example_embed)

# Example 3: iteration over sequences, mappings, and trees
add_executable(example_iterate iterate.cpp)
target_link_libraries(example_iterate PRIVATE ${PROJECT_NAME})
add_test(NAME example_iterate COMMAND example_iterate)
