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
    if(MSVC OR CMAKE_GENERATOR STREQUAL "Xcode")
        message(STATUS "Using multi-config generator: ${CMAKE_GENERATOR}")
        foreach(OUTPUTCONFIG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
            string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG_UPPER)
            set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
            set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
            set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
        endforeach()
    endif()

    # set install prefix to binary directory if not specified
    if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        set(CMAKE_INSTALL_PREFIX
            ${CMAKE_BINARY_DIR}/install
            CACHE PATH "default install path" FORCE)
    endif()
    if(OB_ENABLE_CLANG_TIDY)
        find_program(CLANG_TIDY_EXE NAMES "clang-tidy" QUIET)
        if(CLANG_TIDY_EXE)
            set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
        endif()
    endif()
endif()

if(MSVC)
    set(OB_RESOURCES_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    set(OB_EXTENSIONS_INSTALL_RPEFIX "bin")
else()
    set(OB_RESOURCES_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    set(OB_EXTENSIONS_INSTALL_RPEFIX "lib")    
endif()