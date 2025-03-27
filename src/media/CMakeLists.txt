# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

set(OB_TARGET_MEDIA media)

# add the media library with the version as the project
add_library(${OB_TARGET_MEDIA} STATIC)
set_target_properties(${OB_TARGET_MEDIA} PROPERTIES VERSION ${PROJECT_VERSION})

# add the source files
file(GLOB_RECURSE SOURCE_FILES "*.cpp" EXCLUDE unittest)
file(GLOB_RECURSE HEADERS_FILES "*.hpp" EXCLUDE unittest)
target_sources(${OB_TARGET_MEDIA} PRIVATE ${SOURCE_FILES} ${HEADERS_FILES})

# depedecencies
target_link_libraries(${OB_TARGET_MEDIA} PUBLIC ob::shared ob::core ob::device ob::platform)
target_include_directories(${OB_TARGET_MEDIA} PUBLIC ${OB_PUBLIC_HEADERS_DIR} ${CMAKE_CURRENT_LIST_DIR})

# dependecies:
add_subdirectory(${OB_3RDPARTY_DIR}/rosbag rosbag)
target_link_libraries(${OB_TARGET_MEDIA} PUBLIC rosbag::rosbag)

add_library(ob::media ALIAS ${OB_TARGET_MEDIA})
ob_source_group(ob::media)
set_target_properties(${OB_TARGET_MEDIA} PROPERTIES FOLDER "modules")