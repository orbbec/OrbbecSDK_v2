# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

project(OrbbecSDK VERSION 2.0.16 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# check if current project is a sub project of other project
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(OB_IS_MAIN_PROJECT TRUE)
else()
    set(OB_IS_MAIN_PROJECT FALSE)
endif()

# load project options
include(cmake/options.cmake)

# load macros
include(cmake/macro.cmake)

# set compiler flags
include(cmake/compiler_flags.cmake)

# configure platform-specific settings（contains platform-specific compiler flags）
if(ANDROID_NDK_TOOLCHAIN_INCLUDED)
    include(cmake/android_config.cmake)
elseif(WIN32)
    include(cmake/windows_config.cmake)
elseif(APPLE)
    include(cmake/macos_config.cmake)
elseif(UNIX)
    include(cmake/linux_config.cmake)
endif()

# configure global settings: install rules, output directories, global variables, clang-tidy, etc.
include(cmake/global_config.cmake)

# configure global directories variables
set(OB_PUBLIC_HEADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/include/)
set(OB_3RDPARTY_DIR ${CMAKE_CURRENT_LIST_DIR}/3rdparty)
set(OB_PROJECT_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

add_subdirectory(extensions)
add_subdirectory(src)

if(OB_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(OB_INSTALL_EXAMPLES_SOURCE AND OB_IS_MAIN_PROJECT)
    install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/examples/ DESTINATION examples/src PATTERN "*publish_files*" EXCLUDE )
    if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/examples/publish_files)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/examples/publish_files/CMakeLists.txt DESTINATION examples)
        if(UNIX)
            install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/examples/publish_files/build_examples.sh DESTINATION ./)
        endif()
    endif()
endif()

if(OB_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(OB_BUILD_DOCS)
    add_subdirectory(docs)
endif()

if(OB_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

add_subdirectory(scripts)

ob_print_summary()

