# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.13)

# cJSON library for JSON config parsing
add_library(cjson STATIC cJSON.c)
target_include_directories(cjson PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(cjson PROPERTIES FOLDER "tests")

# INTERFACE library: header-only, consumed by all new test targets
add_library(ob_test_common INTERFACE)

target_link_libraries(ob_test_common INTERFACE
    GTest::gtest
    ob::OrbbecSDK
    cjson
)

target_include_directories(ob_test_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/..   # tests/ — so #include "common/test_fixtures.hpp" resolves
)

# New test targets do not inherit SDK's strict /WX / -Werror
if(MSVC)
    target_compile_options(ob_test_common INTERFACE
        /W3
        /EHa         # catch SEH exceptions via C++ catch(...) / _set_se_translator
        /wd4189       # uninitialized local variable (common in conditional skip)
        /wd4505       # unreferenced local function
    )
    target_compile_options(ob_test_common INTERFACE /WX-)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ob_test_common INTERFACE
        -Wall -Wextra
        -Wno-unused-variable
        -Wno-unused-parameter
        -Wno-unreachable-code
    )
    target_compile_options(ob_test_common INTERFACE -Wno-error)
endif()

target_compile_features(ob_test_common INTERFACE cxx_std_11)

set_target_properties(ob_test_common PROPERTIES FOLDER "tests")
