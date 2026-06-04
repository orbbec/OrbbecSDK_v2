# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS "*.cpp" EXCLUDE unittest)
file(GLOB_RECURSE HEADERS_FILES CONFIGURE_DEPENDS "*.hpp" EXCLUDE unittest)
target_sources( ${OB_TARGET_DEVICE} PRIVATE ${SOURCE_FILES} ${HEADERS_FILES})

target_include_directories(${OB_TARGET_DEVICE} PUBLIC ${CMAKE_CURRENT_LIST_DIR})
