# because apparently you need me to teach you cmake too
# this is how you write a build system that doesn't make people cry

cmake_minimum_required(VERSION 3.21)

project(immutable-yaml
    VERSION 1.0.0
    DESCRIPTION "compile-time yaml parser that doesn't suck"
    LANGUAGES CXX
)

# c++23 because we're not living in the stone age
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# compiler flags that actually matter
if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
else()
    add_compile_options(
        -Wall -Wextra -Werror -Wpedantic
        -Wconversion -Wsign-conversion
        -Wcast-align -Wcast-qual
        -Wdisabled-optimization
        -Wformat=2 -Winit-self
        -Wlogical-op -Wmissing-include-dirs
        -Wnoexcept -Wold-style-cast
        -Woverloaded-virtual -Wredundant-decls
        -Wshadow -Wsign-promo
        -Wstrict-null-sentinel -Wundef
        -Wno-unused -Wno-variadic-macros
        -Wno-parentheses -fdiagnostics-show-option
    )
endif()

# header-only library interface
add_library(${PROJECT_NAME} INTERFACE)
add_library(yaml::ct ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} 
    INTERFACE 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_23)

# tests that actually test things
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()

# install configuration for when people actually want to use this
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}_targets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT ${PROJECT_NAME}_targets
    FILE ${PROJECT_NAME}-targets.cmake
    NAMESPACE yaml::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)