# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

message(STATUS "Setting Android configurations")
message(STATUS "Prepare ObSensor SDK for Android OS (${ANDROID_NDK_ABI_NAME})")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

unset(WIN32)
unset(UNIX)
set(OB_BUILD_EXAMPLES OFF)
set(OB_BUILD_TESTS OFF)
set(ANDROID_STL "c++_static")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}   -fPIC -pedantic -D_DEFAULT_SOURCE -fpermissive")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -pedantic -Wno-missing-field-initializers")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-switch -Wno-multichar  -Wno-narrowing -fpermissive")

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -DNDEBUG -fPIC")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -DNDEBUG -fPIC")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s -fPIE -pie")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -s")

set(OB_BUILD_ANDROID ON)
set(OB_BUILD_TOOLS OFF)
set(OB_BUILD_SOVERSION OFF)
set(OB_BUILD_GMSL_PAL OFF)
set(OB_CURRENT_OS "android")
add_definitions(-DBUILD_ANDROID)
add_definitions(-D__NEON__)