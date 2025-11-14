# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

set(OB_INTERFACE_DIR "${CMAKE_CURRENT_LIST_DIR}/interface")

if(OB_ENABLE_BOLT_OPENGL_COMPAT)
    if(NOT ANDROID AND NOT APPLE)
        find_package(OpenGL REQUIRED)
        if(OPENGL_FOUND)
            if(OPENGL_INCLUDE_DIR)
                include_directories(${OPENGL_INCLUDE_DIR})
                message(STATUS "OpenGL found: include directory: ${OPENGL_INCLUDE_DIR}")
            endif()
            message(STATUS "OpenGL found: libraries: ${OPENGL_gl_LIBRARY}")
            add_definitions(-DOB_BOLT_OPENGL_COMPAT)
        else()
            message(WARNING "Femto Bolt OpenGL compatibility disabled: OpenGL not found")
        endif()
    else()
        message(STATUS "Femto Bolt is not supported on Android/Apple platforms, skipping OpenGL check")
    endif()
endif()

add_definitions(-DOB_SDK_LIBRARY_NAME=\"${OB_SDK_LIB_NAME}\")

# modules
add_subdirectory(shared) # utils, logger, etc.
add_subdirectory(core) # core library
add_subdirectory(filter)
add_subdirectory(platform)
add_subdirectory(device)
add_subdirectory(pipeline)
add_subdirectory(media) # record playback, etc.

# config version info
if(MSVC)
    set(ORBBEC_VERSIONINFO_RC "${CMAKE_BINARY_DIR}/VersionInfo.rc")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/VersionInfo.rc.in "${ORBBEC_VERSIONINFO_RC}")
endif()

# only support shared library for now
add_library(${OB_SDK_LIB_NAME} SHARED "" ${ORBBEC_VERSIONINFO_RC})
include(GenerateExportHeader)
generate_export_header(${OB_SDK_LIB_NAME} BASE_NAME OB EXPORT_FILE_NAME ${OB_PUBLIC_HEADERS_DIR}/libobsensor/h/Export.h)

file(GLOB_RECURSE SOURCE_FILES ${CMAKE_CURRENT_LIST_DIR}/impl/*.cpp ${CMAKE_CURRENT_LIST_DIR}/context/*.cpp)
file(GLOB_RECURSE HEADER_FILES ${CMAKE_CURRENT_LIST_DIR}/impl/*.hpp ${CMAKE_CURRENT_LIST_DIR}/context/*.hpp)


target_sources(${OB_SDK_LIB_NAME} PRIVATE ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(${OB_SDK_LIB_NAME} PRIVATE ob::shared ob::core ob::filter ob::platform ob::device ob::pipeline ob::media)
target_include_directories(${OB_SDK_LIB_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_include_directories(
    ${OB_SDK_LIB_NAME} PUBLIC "$<BUILD_INTERFACE:${OB_PUBLIC_HEADERS_DIR}>" "$<INSTALL_INTERFACE:include>"
)

if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(${OB_SDK_LIB_NAME} PRIVATE Threads::Threads)
endif()

# Link OpenGL libraries if found
if(OPENGL_gl_LIBRARY)
    target_link_libraries(${OB_SDK_LIB_NAME} PRIVATE ${OPENGL_gl_LIBRARY})
endif()

add_library(ob::${OB_SDK_LIB_NAME} ALIAS ${OB_SDK_LIB_NAME})
ob_source_group(ob::${OB_SDK_LIB_NAME})


if(OB_IS_MAIN_PROJECT)
    install(
        TARGETS ${OB_SDK_LIB_NAME}
        EXPORT ${OB_SDK_LIB_NAME}Config
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

    install(EXPORT ${OB_SDK_LIB_NAME}Config NAMESPACE ob:: DESTINATION lib)

    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/${OB_SDK_LIB_NAME}Version.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMajorVersion
    )
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/${OB_SDK_LIB_NAME}Version.cmake"
        DESTINATION lib
    )
    if(MSVC)
        install(FILES $<TARGET_PDB_FILE:${OB_SDK_LIB_NAME}> DESTINATION bin OPTIONAL)
    endif()
endif()

if(OB_BUILD_SOVERSION)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 2
    )
endif()
