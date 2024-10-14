# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

if(OB_IS_MAIN_PROJECT)
    # output directories
    string(TOLOWER ${CMAKE_BUILD_TYPE} OB_BUILD_TYPE)
    set(OB_OUTPUT_DIRECTORY_ROOT ${CMAKE_BINARY_DIR}/${OB_CURRENT_OS})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OB_OUTPUT_DIRECTORY_ROOT}/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OB_OUTPUT_DIRECTORY_ROOT}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OB_OUTPUT_DIRECTORY_ROOT}/bin)
    if(MSVC)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/${OB_CURRENT_OS}/bin")
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/${OB_CURRENT_OS}/bin")
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/${OB_CURRENT_OS}/lib")
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/${OB_CURRENT_OS}/lib")
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/${OB_CURRENT_OS}/lib")
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/${OB_CURRENT_OS}/lib")
    endif()

    # set install prefix to binary directory if not specified
    if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        set(CMAKE_INSTALL_PREFIX
            ${CMAKE_BINARY_DIR}/install
            CACHE PATH "default install path" FORCE)
    endif()

    find_program(CLANG_TIDY_EXE NAMES "clang-tidy" QUIET)
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    endif()
endif()
