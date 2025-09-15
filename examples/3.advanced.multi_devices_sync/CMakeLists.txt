# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)
project(ob_multi_devices_sync)

file(GLOB_RECURSE SOURCE_FILES *.cpp)
file(GLOB_RECURSE HEADER_FILES *.hpp)

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES} utils/cJSON.c)

#add_executable(${PROJECT_NAME} ob_multi_devices_sync.cpp PipelineHolder.cpp utils/cJSON.c)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 11)

find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} ob::OrbbecSDK ob::examples::utils Threads::Threads)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "examples")
if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()

install(FILES ${CMAKE_CURRENT_LIST_DIR}/MultiDeviceSyncConfig.json DESTINATION bin)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
