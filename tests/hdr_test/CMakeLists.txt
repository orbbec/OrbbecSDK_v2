# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

add_executable(hdr_test hdr_test.cpp)
target_include_directories(hdr_test PRIVATE ${OB_PROJECT_ROOT_DIR}/src/filter/public_filters/)
target_link_libraries(hdr_test PRIVATE ob::OrbbecSDK ob::shared)
set_target_properties(hdr_test PROPERTIES FOLDER "tests")

