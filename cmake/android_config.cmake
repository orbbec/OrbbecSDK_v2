# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

message(STATUS "Setting Android configurations")
message(STATUS "Prepare ObSensor SDK for Android OS (${ANDROID_NDK_ABI_NAME})")

unset(WIN32)
unset(UNIX)
set(OB_BUILD_EXAMPLES OFF)
set(OB_BUILD_TESTS OFF)
set(ANDROID_STL "c++_static")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}   -fPIC -pedantic -g -D_DEFAULT_SOURCE -fpermissive")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -pedantic -g -Wno-missing-field-initializers")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-switch -Wno-multichar  -Wno-narrowing -fpermissive")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fPIE -pie")

set(OB_BUILD_ANDROID ON)
set(OB_CURRENT_OS "android")
add_definitions(-DBUILD_ANDROID)

