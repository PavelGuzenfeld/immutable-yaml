# Fuzz targets — build with: cmake -B build-fuzz -DBUILD_FUZZING=ON -DCMAKE_CXX_COMPILER=clang++
# Run with: ./build-fuzz/fuzz/fuzz_yaml corpus/yaml/ -max_total_time=60

if(NOT BUILD_FUZZING)
    return()
endif()

set(FUZZ_TARGETS yaml json toml xml)

foreach(fmt IN LISTS FUZZ_TARGETS)
    add_executable(fuzz_${fmt} fuzz_${fmt}.cpp)
    target_link_libraries(fuzz_${fmt} PRIVATE ${PROJECT_NAME})
    target_compile_options(fuzz_${fmt} PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(fuzz_${fmt} PRIVATE -fsanitize=fuzzer,address,undefined)
endforeach()
