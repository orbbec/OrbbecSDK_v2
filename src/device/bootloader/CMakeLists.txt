# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)


target_sources(
    ${OB_TARGET_DEVICE}
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/BootDeviceInfo.hpp
        ${CMAKE_CURRENT_LIST_DIR}/BootDeviceInfo.cpp
        ${CMAKE_CURRENT_LIST_DIR}/BootDevice.hpp
        ${CMAKE_CURRENT_LIST_DIR}/BootDevice.cpp
)
