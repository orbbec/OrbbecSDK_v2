# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

if(OB_BUILD_WIN32)
    add_subdirectory(win)
endif()

if(OB_BUILD_LINUX)
	add_subdirectory(linux)
endif()

if(OB_BUILD_MACOS)
	add_subdirectory(mac)
endif()

if(OB_BUILD_ANDROID)
	add_subdirectory(android)
endif()
