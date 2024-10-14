# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

set(OB_TARGET_DEVICE device)
add_library(${OB_TARGET_DEVICE} STATIC)
set_target_properties(${OB_TARGET_DEVICE} PROPERTIES VERSION ${PROJECT_VERSION})

file(GLOB SOURCE_FILES ${CMAKE_CURRENT_LIST_DIR}/*.cpp)
file(GLOB HEADER_FILES ${CMAKE_CURRENT_LIST_DIR}/*.hpp)
target_sources(${OB_TARGET_DEVICE} PRIVATE ${SOURCE_FILES} ${HEADER_FILES})

target_link_libraries(${OB_TARGET_DEVICE} PUBLIC ob::shared ob::core ob::platform ob::filter)
target_include_directories(${OB_TARGET_DEVICE} PUBLIC ${OB_PUBLIC_HEADERS_DIR} ${CMAKE_CURRENT_LIST_DIR})

# common:
add_subdirectory(context)
add_subdirectory(devicemanager)
add_subdirectory(protocol)
add_subdirectory(component)

# devices support:
add_subdirectory(gemini330) # Gemini 330 series
add_subdirectory(gemini2) # Gemini 2 series
add_subdirectory(astra2) # Astra 2 series
add_subdirectory(femtobolt) # FemtoBolt
add_subdirectory(femtomega) # FemtoMega
add_subdirectory(bootloader) # Bootloader device

# dependecies:
add_subdirectory(${OB_3RDPARTY_DIR}/jsoncpp jsoncpp)
target_link_libraries(${OB_TARGET_DEVICE} PUBLIC jsoncpp::jsoncpp)

add_library(ob::device ALIAS ${OB_TARGET_DEVICE})
ob_source_group(ob::device)
set_target_properties(${OB_TARGET_DEVICE} PROPERTIES FOLDER "modules")

