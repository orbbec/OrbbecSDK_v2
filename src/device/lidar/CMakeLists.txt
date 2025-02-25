# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

if(OB_BUILD_NET_PAL)
    target_sources(
        ${OB_TARGET_DEVICE} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/LiDARDevice.cpp
                                    ${CMAKE_CURRENT_LIST_DIR}/LiDARDevice.hpp
                                    ${CMAKE_CURRENT_LIST_DIR}/LiDARDeviceInfo.cpp
                                    ${CMAKE_CURRENT_LIST_DIR}/LiDARDeviceInfo.hpp
    )
endif()
