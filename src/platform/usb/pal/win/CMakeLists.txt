# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

file(GLOB PAL_SOURCES ${CMAKE_CURRENT_LIST_DIR}/WinUsbPal.*)

if (OB_BUILD_USB_PAL)
    list(APPEND PAL_SOURCES ${CMAKE_CURRENT_LIST_DIR}/WinUsbPal.cpp)
    list(APPEND PAL_SOURCES ${CMAKE_CURRENT_LIST_DIR}/WinUsbPal.hpp)
endif ()

target_sources(${OB_TARGET_PAL} PRIVATE ${PAL_SOURCES})
