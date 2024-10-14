# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

if(OB_BUILD_WIN32)
    target_sources(${OB_TARGET_PAL} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/WmfUvcDevicePort.cpp")
    target_sources(${OB_TARGET_PAL} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/WmfUvcDevicePort.hpp")
    target_sources(${OB_TARGET_PAL} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/WinHelpers.hpp")
    target_sources(${OB_TARGET_PAL} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/WinHelpers.cpp")

else()
    target_sources(${OB_TARGET_PAL} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/UvcTypes.hpp")
    target_sources(${OB_TARGET_PAL} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/ObLibuvcDevicePort.cpp")
    target_sources(${OB_TARGET_PAL} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/ObLibuvcDevicePort.hpp")

    if(OB_BUILD_LINUX)
        target_sources(${OB_TARGET_PAL} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/ObV4lUvcDevicePort.hpp")
        target_sources(${OB_TARGET_PAL} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/ObV4lUvcDevicePort.cpp")
        if(OB_BUILD_GMSL_PAL)
            target_sources(${OB_TARGET_PAL} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/ObV4lGmslDevicePort.hpp")
            target_sources(${OB_TARGET_PAL} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/ObV4lGmslDevicePort.cpp")
            target_sources(${OB_TARGET_PAL} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/ObV4lGmslHostProtocolTypes.hpp")
        endif()
    endif()
endif()

