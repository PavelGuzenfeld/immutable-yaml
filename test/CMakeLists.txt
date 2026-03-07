find_package(fmt REQUIRED)
find_package(cmake-library REQUIRED)

# Test executable
add_executable(${PROJECT_NAME}_test test_main.cpp)
target_link_libraries(${PROJECT_NAME}_test PRIVATE ${PROJECT_NAME} fmt)
set_warnings_and_errors(${PROJECT_NAME}_test)

add_executable(${PROJECT_NAME}_test_negative test_negative.cpp)
target_link_libraries(${PROJECT_NAME}_test_negative PRIVATE ${PROJECT_NAME} fmt)
set_warnings_and_errors(${PROJECT_NAME}_test_negative)

# Embed test — uses yaml_embed() to compile YAML files into C++
include(${CMAKE_SOURCE_DIR}/cmake/YamlEmbed.cmake)
add_executable(${PROJECT_NAME}_test_embed test_embed.cpp)
target_link_libraries(${PROJECT_NAME}_test_embed PRIVATE ${PROJECT_NAME})
yaml_embed(${PROJECT_NAME}_test_embed
    ${CMAKE_CURRENT_SOURCE_DIR}/sample_config.yaml
)

# Export the target
install(
    TARGETS
    ${PROJECT_NAME}_test_negative
    ${PROJECT_NAME}_test
    ${PROJECT_NAME}_test_embed
    EXPORT ${PROJECT_NAME}Targets
    DESTINATION bin
)