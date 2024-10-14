# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

file(GLOB_RECURSE SOURCE_FILES "*.cpp" EXCLUDE unittest)
file(GLOB_RECURSE HEADERS_FILES "*.hpp" EXCLUDE unittest)
target_sources( ${OB_TARGET_DEVICE} PRIVATE ${SOURCE_FILES} ${HEADERS_FILES})

target_include_directories(${OB_TARGET_DEVICE} PUBLIC ${CMAKE_CURRENT_LIST_DIR})
