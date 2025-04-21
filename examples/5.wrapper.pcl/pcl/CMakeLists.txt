# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W3)
endif()

project(ob_pcl)

find_package(PCL REQUIRED)

if (PCL_FOUND)
    include_directories(${PCL_INCLUDE_DIRS})
    add_definitions(${PCL_DEFINITIONS})
    link_directories(${PCL_LIBRARY_DIRS})
endif()
list(APPEND DEPENDENCIES ${PCL_LIBRARIES})

add_executable(${PROJECT_NAME} pcl.cpp)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 11)
target_link_libraries(${PROJECT_NAME} ob::OrbbecSDK ob::examples::utils)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "examples")
if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)