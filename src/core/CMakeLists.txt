# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

# add the core library with version number
add_library(core STATIC)
set_target_properties(core PROPERTIES VERSION ${PROJECT_VERSION})

# add the core library sources
file(GLOB_RECURSE SOURCE_FILES "*.cpp" EXCLUDE unittest)
file(GLOB_RECURSE HEADERS_FILES "*.hpp" EXCLUDE unittest)
target_sources(core PRIVATE ${SOURCE_FILES} ${HEADERS_FILES})

target_link_libraries(core PUBLIC ob::shared)
target_include_directories(core PUBLIC  ${OB_PUBLIC_HEADERS_DIR} ${CMAKE_CURRENT_LIST_DIR})

add_library(ob::core ALIAS core)
ob_source_group(ob::core)
set_target_properties(core PROPERTIES FOLDER "modules")

