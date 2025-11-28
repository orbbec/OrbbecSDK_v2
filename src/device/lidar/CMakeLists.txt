# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

if(OB_BUILD_NET_PAL)
    file(GLOB_RECURSE SOURCES 
        "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/*.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/*.c"
        "${CMAKE_CURRENT_LIST_DIR}/*.h"
    )

    target_sources(${OB_TARGET_DEVICE} PRIVATE ${SOURCES})
endif()
