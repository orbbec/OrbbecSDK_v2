# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)
project(ob_benchmark)

file(GLOB_RECURSE SOURCE_FILES *.cpp)
file(GLOB_RECURSE HEADER_FILES *.hpp)

add_executable(ob_benchmark ${SOURCE_FILES} ${HEADER_FILES})

set_property(TARGET ob_benchmark PROPERTY CXX_STANDARD 11)

find_package(Threads REQUIRED)
target_link_libraries(ob_benchmark ob::OrbbecSDK Threads::Threads)

target_include_directories(ob_benchmark PUBLIC ${CMAKE_CURRENT_LIST_DIR})

install(TARGETS ob_benchmark RUNTIME DESTINATION bin)