# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)
project(ob_lidar_stream_c C)

add_executable(${PROJECT_NAME} lidar_stream.c)

target_link_libraries(${PROJECT_NAME} ob::${OB_SDK_LIB_NAME} ob::examples::utils)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "lidar_examples")
if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

