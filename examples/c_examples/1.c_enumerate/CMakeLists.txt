# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

project(ob_enumerate_c)
add_executable(${PROJECT_NAME} enumerate.c)

target_link_libraries(${PROJECT_NAME} PRIVATE ob::OrbbecSDK ob::examples::utils)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "examples_c")
if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

