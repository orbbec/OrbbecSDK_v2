# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

# minimum required cmake version: 3.5 support vs2019
cmake_minimum_required(VERSION 3.5)

target_sources(
    ${OB_TARGET_DEVICE} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/DeviceManager.cpp ${CMAKE_CURRENT_LIST_DIR}/DeviceManager.hpp
                                ${CMAKE_CURRENT_LIST_DIR}/DeviceEnumInfoBase.hpp
)

if(OB_BUILD_USB_PAL)
    target_sources(
        ${OB_TARGET_DEVICE} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/UsbDeviceEnumerator.cpp
                                    ${CMAKE_CURRENT_LIST_DIR}/UsbDeviceEnumerator.hpp
    )
endif()

if(OB_BUILD_NET_PAL)
    target_sources(
        ${OB_TARGET_DEVICE} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/NetDeviceEnumerator.cpp
                                    ${CMAKE_CURRENT_LIST_DIR}/NetDeviceEnumerator.hpp
    )
endif()

