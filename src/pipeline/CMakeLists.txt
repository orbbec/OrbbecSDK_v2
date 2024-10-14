# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

# add the pipeline library with version number
add_library(pipeline STATIC)
set_target_properties(pipeline PROPERTIES VERSION ${PROJECT_VERSION})

# add the pipeline library sources
file(GLOB_RECURSE SOURCE_FILES "*.cpp" EXCLUDE unittest)
file(GLOB_RECURSE HEADERS_FILES "*.hpp" EXCLUDE unittest)
target_sources(pipeline PRIVATE ${SOURCE_FILES} ${HEADERS_FILES})

target_link_libraries(pipeline PUBLIC ob::device)
target_include_directories(pipeline PUBLIC  ${OB_PUBLIC_HEADERS_DIR} ${CMAKE_CURRENT_LIST_DIR})

add_library(ob::pipeline ALIAS pipeline)
ob_source_group(ob::pipeline)
set_target_properties(pipeline PROPERTIES FOLDER "modules")

