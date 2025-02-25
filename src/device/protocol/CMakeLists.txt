# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

target_sources(${OB_TARGET_DEVICE} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/Protocol.cpp ${CMAKE_CURRENT_LIST_DIR}/Protocol.hpp)


if(OB_BUILD_NET_PAL)
    target_sources(
        ${OB_TARGET_DEVICE} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/LiDARProtocol.cpp
        ${CMAKE_CURRENT_LIST_DIR}/LiDARProtocol.hpp
    )
endif()
