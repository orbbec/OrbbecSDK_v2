# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

add_executable(undistortion_filter_test undistortion_filter_test.cpp)
target_include_directories(undistortion_filter_test PRIVATE ${OB_PROJECT_ROOT_DIR}/src/filter/publicfilters)
target_link_libraries(undistortion_filter_test PRIVATE ob::filter ob::shared ${CMAKE_DL_LIBS})
set_target_properties(undistortion_filter_test PROPERTIES FOLDER "tests")
