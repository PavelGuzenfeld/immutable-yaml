# test configuration because testing is important
# unlike your last project

add_executable(test_main test_main.cpp)
target_link_libraries(test_main PRIVATE yaml::ct)

add_executable(test_negative test_negative.cpp)  
target_link_libraries(test_negative PRIVATE yaml::ct)

add_test(NAME basic_tests COMMAND test_main)
add_test(NAME negative_tests COMMAND test_negative)

# compile-time tests - these will fail at compile time if yaml is invalid
add_executable(test_compile_time_validation test_main.cpp)
target_link_libraries(test_compile_time_validation PRIVATE yaml::ct)
target_compile_definitions(test_compile_time_validation PRIVATE 
    COMPILE_TIME_VALIDATION_TEST=1
)