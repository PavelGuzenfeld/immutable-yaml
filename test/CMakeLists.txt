find_package(fmt REQUIRED)
find_package(cmake-library REQUIRED)

# Test executable
add_executable(${PROJECT_NAME}_test test_main.cpp)
target_link_libraries(${PROJECT_NAME}_test PRIVATE ${PROJECT_NAME} fmt)
set_warnings_and_errors(${PROJECT_NAME}_test)

add_executable(${PROJECT_NAME}_test_negative test_negative.cpp)
target_link_libraries(${PROJECT_NAME}_test_negative PRIVATE ${PROJECT_NAME} fmt)
set_warnings_and_errors(${PROJECT_NAME}_test_negative)

# Export the target
install(
    TARGETS 
    ${PROJECT_NAME}_test_negative 
    ${PROJECT_NAME}_test
    EXPORT ${PROJECT_NAME}Targets
    DESTINATION bin
)