# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

add_subdirectory(pal)
add_subdirectory(enumerator)
add_subdirectory(uvc)
add_subdirectory(hid)
add_subdirectory(vendor)

target_sources(${OB_TARGET_PAL} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/UsbPortGroup.hpp ${CMAKE_CURRENT_LIST_DIR}/UsbPortGroup.cpp)

