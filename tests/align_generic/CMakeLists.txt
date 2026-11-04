# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.13)

add_executable(align_generic_test
    align_generic_test.cpp
    ${PROJECT_SOURCE_DIR}/src/filter/publicfilters/AlignImplGeneric.cpp)
target_include_directories(align_generic_test PRIVATE ${PROJECT_SOURCE_DIR}/src/filter/publicfilters)
target_link_libraries(align_generic_test PRIVATE ob::shared gtest_main)
# Exercise the scalar implementation even on hosts with SIMD enabled globally.
if(MSVC)
    target_compile_options(align_generic_test PRIVATE /U__SSSE3__ /U__ARM_NEON__ /U__NEON__)
else()
    target_compile_options(align_generic_test PRIVATE -U__SSSE3__ -U__ARM_NEON__ -U__NEON__)
endif()
include(GoogleTest)
gtest_discover_tests(align_generic_test PROPERTIES LABELS "unit")
set_target_properties(align_generic_test PROPERTIES FOLDER "tests")
