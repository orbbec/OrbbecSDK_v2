# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)


file (GLOB _cpp_files "${CMAKE_CURRENT_LIST_DIR}/*.cpp")
file (GLOB _head_files "${CMAKE_CURRENT_LIST_DIR}/*.hpp")


foreach(_file IN ITEMS ${_cpp_files})
        target_sources(${OB_TARGET_PAL} PRIVATE "${_file}")
endforeach()

foreach(_file IN ITEMS ${_head_files})
        target_sources(${OB_TARGET_PAL} PRIVATE "${_file}")
endforeach()



