# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

# add the shared library with version number
add_library(shared STATIC)

# add the shared library sources
file(GLOB_RECURSE SOURCE_FILES "*.cpp" EXCLUDE unittest)
file(GLOB_RECURSE HEADERS_FILES "*.hpp" EXCLUDE unittest)
target_sources(shared PRIVATE ${SOURCE_FILES} ${HEADERS_FILES})

add_subdirectory(${OB_3RDPARTY_DIR}/spdlog spdlog)
target_link_libraries(shared PUBLIC spdlog::spdlog)
target_include_directories(shared PUBLIC ${OB_PUBLIC_HEADERS_DIR} ${CMAKE_CURRENT_LIST_DIR})

add_subdirectory(${OB_3RDPARTY_DIR}/tinyxml2 tinyxml2)
target_link_libraries(shared PUBLIC tinyxml2::tinyxml2)

add_subdirectory(environment)
target_link_libraries(shared PUBLIC ob::env_config_rc)

add_library(ob::shared ALIAS shared)
ob_source_group(ob::shared)
set_target_properties(shared PROPERTIES FOLDER "modules")

