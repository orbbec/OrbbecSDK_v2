# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

macro(ob_print_summary)
    message(STATUS "")
    message(STATUS "=================================================================================================")
    message(STATUS "Build Infomation:")
    message(STATUS "    Rove Package Version: ${PROJECT_VERSION}")
    message(STATUS "    CMake ${CMAKE_VERSION} successfully configured using ${CMAKE_GENERATOR} generator")
    message(STATUS "    Installation target path: ${CMAKE_INSTALL_PREFIX}")
    message(STATUS "    Building Dynamic Libraries")
    message(STATUS "    Building Type: ${CMAKE_BUILD_TYPE}")
    message(STATUS "        - Debug Library Postfix: ${CMAKE_DEBUG_POSTFIX}")
    message(STATUS "        - Release Library Postfix: ${CMAKE_RELEASE_POSTFIX}")
    message(STATUS "        - MinSizeRel Library Postfix: ${CMAKE_MINSIZEREL_POSTFIX}")
    message(STATUS "        - RelWithDebInfo Library Postfix: ${CMAKE_RELWITHDEBINFO_POSTFIX}")
    message(STATUS "C Complier Flags:")
    message(STATUS "    C_FLAGS:  =${CMAKE_C_FLAGS}")
    message(STATUS "    C_FLAGS_DEBUG:=${CMAKE_C_FLAGS_DEBUG}")
    message(STATUS "    C_FLAGS_RELEASE:=${CMAKE_C_FLAGS_RELEASE}")
    message(STATUS "    C_FLAGS_MINSIZEREL:=${CMAKE_C_FLAGS_MINSIZEREL}")
    message(STATUS "    C_FLAGS_RELWITHDEBINFO:=${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    message(STATUS "C++ Complier Flags:")
    message(STATUS "    CXX_FLAGS:=${CMAKE_CXX_FLAGS}")
    message(STATUS "    CXX_FLAGS_DEBUG:=${CMAKE_CXX_FLAGS_DEBUG}")
    message(STATUS "    CXX_FLAGS_RELEASE:=${CMAKE_CXX_FLAGS_RELEASE}")
    message(STATUS "    CMAKE_EXE_LINKER_FLAGS_RELEASE:=${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
    message(STATUS "    CXX_FLAGS_MINSIZEREL:=${CMAKE_CXX_FLAGS_MINSIZEREL}")
    message(STATUS "    CXX_FLAGS_RELWITHDEBINFO:=${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
endmacro()

macro(generate_useful_info BRANCH_NAME COMMIT_HASH CURRENT_TIME PLATFORM PROGRAMMING_LANGUAGE)
    set(GIT_NAME git)
    find_program(GIT_COMMAND ${GIT_NAME})

    if(EXISTS ${GIT_COMMAND})
        execute_process(
            COMMAND ${GIT_COMMAND} --version
            WORKING_DIRECTORY ${ORBBEC_SDK_ROOT_DIR}
            OUTPUT_VARIABLE GIT_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        message("Found ${GIT_NAME}: ${GIT_COMMAND} (${GIT_VERSION})")
        execute_process(
            COMMAND ${GIT_COMMAND} rev-parse --short HEAD
            WORKING_DIRECTORY ${ORBBEC_SDK_ROOT_DIR}
            OUTPUT_VARIABLE GIT_COMMIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        execute_process(
            COMMAND ${GIT_COMMAND} rev-parse --abbrev-ref HEAD
            WORKING_DIRECTORY ${ORBBEC_SDK_ROOT_DIR}
            OUTPUT_VARIABLE GIT_BRANCH
            OUTPUT_STRIP_TRAILING_WHITESPACE)
    else()
        message(FATAL_ERROR "Command ${GIT_NAME} cannot be found, so could not obtain GIT_BRANCH and GIT_COMMIT_HASH informations.")
    endif()

    string(TIMESTAMP TIME "%Y%m%d")
    set(${BRANCH_NAME} "alpha")

    if(${GIT_BRANCH} MATCHES "beta" OR ${GIT_BRANCH} MATCHES "Beta")
        set(${BRANCH_NAME} "beta")
    elseif(${GIT_BRANCH} MATCHES "master" OR ${GIT_BRANCH} MATCHES "Master")
        set(${BRANCH_NAME} "release")
    endif()

    set(${COMMIT_HASH} ${GIT_COMMIT_HASH})
    set(${CURRENT_TIME} ${TIME})

    set(${PLATFORM} ${OB_CURRENT_OS})
    set(${PROGRAMMING_LANGUAGE} "C_C++")

    if(${OB_CURRENT_OS} STREQUAL "linux")
        set(${PLATFORM} "${OB_CURRENT_OS}_x64")
    endif()
endmacro()

macro(ob_source_group target)
    get_target_property(LBS_FILES ${target} SOURCES)

    list(APPEND LBS_HEADERS ${LBS_FILES})
    list(APPEND LBS_SOURCES ${LBS_FILES})
    list(FILTER LBS_HEADERS INCLUDE REGEX ".\\.h$|.\\.hpp$|.\\.def$|.\\.cuh$")
    list(FILTER LBS_SOURCES INCLUDE REGEX ".\\.c$|.\\.cpp$|.\\.cc$|.\\.cu$")

    foreach(_file IN ITEMS ${LBS_HEADERS})
        string(REPLACE ${CMAKE_CURRENT_SOURCE_DIR}/ "" _relative_file ${_file})
        get_filename_component(_file_path "${_relative_file}" PATH)
        string(REPLACE "/" "\\" _file_path_msvc "${_file_path}")
        source_group("${_file_path_msvc}" FILES "${_relative_file}")
    endforeach()

    foreach(_file IN ITEMS ${LBS_SOURCES})
        string(REPLACE ${CMAKE_CURRENT_SOURCE_DIR}/ "" _relative_file ${_file})
        get_filename_component(_file_path "${_relative_file}" PATH)
        message(STATUS "file path: ${_file_path}:${_relative_file}")
        string(REPLACE "/" "\\" _file_path_msvc "${_file_path}")
        source_group("${_file_path_msvc}" FILES "${_relative_file}")
    endforeach()
endmacro()

