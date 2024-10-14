# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

add_executable(total_test total_test.cpp)
target_link_libraries(total_test PRIVATE ob::OrbbecSDK)
set_target_properties(total_test PROPERTIES FOLDER "tests")

