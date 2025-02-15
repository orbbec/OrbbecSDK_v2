# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

set(OB_INTERFACE_DIR "${CMAKE_CURRENT_LIST_DIR}/interface")

# modules
add_subdirectory(shared) # utils, logger, etc.
add_subdirectory(core) # core library
add_subdirectory(filter)
add_subdirectory(platform)
add_subdirectory(device)
add_subdirectory(pipeline)

# config version info
if(MSVC)
    set(ORBBEC_VERSIONINFO_RC "${CMAKE_BINARY_DIR}/VersionInfo.rc")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/VersionInfo.rc.in "${ORBBEC_VERSIONINFO_RC}")
endif()

# only support shared library for now
add_library(OrbbecSDK SHARED "" ${ORBBEC_VERSIONINFO_RC})
include(GenerateExportHeader)
generate_export_header(OrbbecSDK BASE_NAME OB EXPORT_FILE_NAME ${OB_PUBLIC_HEADERS_DIR}/libobsensor/h/Export.h)

file(GLOB_RECURSE SOURCE_FILES ${CMAKE_CURRENT_LIST_DIR}/impl/*.cpp ${CMAKE_CURRENT_LIST_DIR}/context/*.cpp)
file(GLOB_RECURSE HEADER_FILES ${CMAKE_CURRENT_LIST_DIR}/impl/*.hpp ${CMAKE_CURRENT_LIST_DIR}/context/*.hpp)

target_sources(OrbbecSDK PRIVATE ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(OrbbecSDK PRIVATE ob::shared ob::core ob::filter ob::platform ob::device ob::pipeline)
target_include_directories(OrbbecSDK PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_include_directories(
    OrbbecSDK PUBLIC "$<BUILD_INTERFACE:${OB_PUBLIC_HEADERS_DIR}>" "$<INSTALL_INTERFACE:include>"
)

if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(OrbbecSDK PRIVATE Threads::Threads)
endif()

find_package(OpenGL REQUIRED)

if(OPENGL_FOUND)
    include_directories(${OPENGL_INCLUDE_DIR})
    message("OpenGL include directory: ${OPENGL_INCLUDE_DIR}")
    target_link_libraries(OrbbecSDK PRIVATE ${OPENGL_gl_LIBRARY})
    add_definitions(-DOPENGL_FOUND)
else()
    message("OpenGL not found")
endif()

add_library(ob::OrbbecSDK ALIAS OrbbecSDK)
ob_source_group(ob::OrbbecSDK)

if(OB_IS_MAIN_PROJECT)
    install(
        TARGETS OrbbecSDK
        EXPORT OrbbecSDKConfig
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )

    install(
        DIRECTORY ${OB_PUBLIC_HEADERS_DIR}
        DESTINATION include
        FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hpp"
    )

    install(EXPORT OrbbecSDKConfig NAMESPACE ob:: DESTINATION lib)
endif()

if(OB_BUILD_SOVERSION)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 2
    )
endif()
