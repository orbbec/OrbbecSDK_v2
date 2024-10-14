# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

add_executable(align_test align_test.cpp ${OB_PROJECT_ROOT_DIR}/src/filter/publicfilters/AlignImpl.hpp ../../src/filter/publicfilters/AlignImpl.cpp)
target_include_directories(align_test PRIVATE ${OB_PROJECT_ROOT_DIR}/src/filter/publicfilters/)
target_link_libraries(align_test PRIVATE ob::OrbbecSDK ob::shared)
set_target_properties(align_test PROPERTIES FOLDER "tests")

