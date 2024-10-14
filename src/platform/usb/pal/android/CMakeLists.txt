# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

file(GLOB _src_files
    AndroidUsbPal.cpp
    AndroidUsbPal.hpp
)

if(BUILD_USB_PAL)
    file(GLOB _usb_src_files
        AndroidUsbDeviceManager.cpp
        AndroidUsbDeviceManager.hpp
    )
    list(APPEND _src_files ${_usb_src_files})
endif()

foreach(_file IN ITEMS ${_src_files})
    target_sources(${OB_TARGET_PAL} PRIVATE "${_file}")
endforeach()


