# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)
project(ob_timestamp_tracker)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp *.cpp)
file(GLOB_RECURSE HEADER_FILES src/*.hpp)

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 11)

find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} 
    ob::${OB_SDK_LIB_NAME} 
    Threads::Threads
    jsoncpp::jsoncpp
)

# Include SDK shared utils
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
)

if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES 
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    )
endif()

if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INSTALL_RPATH "@loader_path;@loader_path/../lib"
    )
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
