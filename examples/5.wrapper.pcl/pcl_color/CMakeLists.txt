# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W3)
endif()

project(ob_pcl_color)

find_package(PCL REQUIRED)

if (PCL_FOUND)
    include_directories(${PCL_INCLUDE_DIRS})
    add_definitions(${PCL_DEFINITIONS})
    link_directories(${PCL_LIBRARY_DIRS})
endif()
list(APPEND DEPENDENCIES ${PCL_LIBRARIES})

add_executable(ob_pcl_color pcl_color.cpp)

set_property(TARGET ob_pcl_color PROPERTY CXX_STANDARD 11)
target_link_libraries(ob_pcl_color ob::OrbbecSDK ob::examples::utils ${DEPENDENCIES})

set_target_properties(ob_pcl_color PROPERTIES FOLDER "examples")

install(TARGETS ob_pcl RUNTIME DESTINATION bin)