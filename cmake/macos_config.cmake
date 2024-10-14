# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

message(STATUS "Setting Apple configurations")

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -Wl,-pedantic -g
# -Wno-missing-field-initializers -fpermissive -fvisibility=hidden")

# string(REPLACE "-O3" "-O0" CMAKE_CXX_FLAGS_RELEASE
# "${CMAKE_CXX_FLAGS_RELEASE}")
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
execute_process(
  COMMAND uname -r
  OUTPUT_VARIABLE UNAME_RESULT
  OUTPUT_STRIP_TRAILING_WHITESPACE)
message(-- " Kernel version: " ${UNAME_RESULT})
string(REGEX MATCH "[0-9]+.[0-9]+" LINUX_KERNEL_VERSION ${UNAME_RESULT})
message(STATUS "linux version ${LINUX_KERNEL_VERSION}")

add_definitions(-DOS_MACOS)

if(NOT IOS)
  if(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15)
    add_definitions(-DOS_MACOS_X64)
    set(OB_CURRENT_OS "macos_x64")
  elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)
    add_definitions(-DOS_MACOS_ARM64)
    set(OB_CURRENT_OS "macos_arm64")
  else()
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
      set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15)
      add_definitions(-DOS_MACOS_X64)
      set(OB_CURRENT_OS "macos_x64")
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
      set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)
      add_definitions(-DOS_MACOS_ARM64)
      set(OB_CURRENT_OS "macos_arm64")
    endif()
  endif()
else()
  add_definitions(-DOS_IOS)
  set(OB_CURRENT_OS "ios")
endif()

execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpmachine
                OUTPUT_VARIABLE MACHINE)

set(OB_BUILD_MACOS ON)
set(CMAKE_INSTALL_NAME_DIR "@loader_path")

