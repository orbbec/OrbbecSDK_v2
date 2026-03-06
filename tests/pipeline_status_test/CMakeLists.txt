# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

add_executable(pipeline_status_test pipeline_status_test.cpp)
target_link_libraries(pipeline_status_test PRIVATE ob::OrbbecSDK)
set_target_properties(pipeline_status_test PROPERTIES FOLDER "tests")
