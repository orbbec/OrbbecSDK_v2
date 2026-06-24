# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

if(DEFINED OPEN_SDK_LIB_NAME)
    set(OB_SDK_LIB_NAME "${OPEN_SDK_LIB_NAME}")
    message(STATUS "Using SDK library name: ${OB_SDK_LIB_NAME}")
else()
    set(OB_SDK_LIB_NAME "OrbbecSDK")
    message(STATUS "Using default SDK library name: ${OB_SDK_LIB_NAME}")
endif()

project(${OB_SDK_LIB_NAME} VERSION 2.9.1 LANGUAGES CXX C)

# Check OB_SDK_LIB_NAME and set OB_BUILD_OEM accordingly
if(${OB_SDK_LIB_NAME} STREQUAL "OrbbecSDK")
    set(OB_BUILD_OEM FALSE)
else()
    set(OB_BUILD_OEM TRUE)
    # Turn off OB_BUILD_DOCS for OEM building
    message(STATUS "Turn off OB_BUILD_DOCS for OEM building")
    set(OB_BUILD_DOCS OFF CACHE BOOL "Build api document and install doc" FORCE)
endif()

# Set CXX standard to C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set C standard to C99
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# check if current project is a sub project of other project
if(OB_BUILD_MAIN_PROJECT)
    set(OB_IS_MAIN_PROJECT TRUE)
elseif(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
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

# configure platform-specific settings (contains platform-specific compiler flags)
if(ANDROID_NDK_TOOLCHAIN_INCLUDED)
    include(cmake/android_config.cmake)
elseif(WIN32)
    include(cmake/windows_config.cmake)
elseif(APPLE)
    include(cmake/macos_config.cmake)
elseif(UNIX OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
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
        file(READ ${CMAKE_CURRENT_SOURCE_DIR}/examples/publish_files/CMakeLists.txt ORIGINAL_CONTENT)
        string(REPLACE "OrbbecSDK" "${OB_SDK_LIB_NAME}" MODIFIED_CONTENT "${ORIGINAL_CONTENT}")
        string(REPLACE "set(CMAKE_CXX_STANDARD 11)"
            "set(CMAKE_CXX_STANDARD 11)\nset(OB_SDK_LIB_NAME \"${OB_SDK_LIB_NAME}\")"
            MODIFIED_CONTENT "${MODIFIED_CONTENT}")
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/CMakeLists_modified.txt "${MODIFIED_CONTENT}")
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/CMakeLists_modified.txt DESTINATION examples RENAME CMakeLists.txt)

        if(UNIX)
            if(APPLE)
                install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/examples/publish_files/build_examples_macos.sh DESTINATION . RENAME build_examples.sh)
            else()
                install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/examples/publish_files/build_examples.sh DESTINATION .)
            endif()
        endif()
    endif()
endif()

# Bundle license notices into the installed package so binary distributions keep
# the SDK's own MIT license and the original license text of every bundled
# third-party library (libusb, live555, libjpeg, libyuv, spdlog, ...). This runs
# only for standalone SDK packaging; when the SDK is embedded into another project
# as source, the host project is responsible for collecting these notices.
if(OB_IS_MAIN_PROJECT AND OB_INSTALL_LICENSES)
    if(EXISTS ${OB_PROJECT_ROOT_DIR}/LICENSE.txt)
        install(FILES ${OB_PROJECT_ROOT_DIR}/LICENSE.txt DESTINATION .)
    endif()
    if(EXISTS ${OB_3RDPARTY_DIR}/readme.md)
        install(FILES ${OB_3RDPARTY_DIR}/readme.md DESTINATION licenses RENAME THIRD_PARTY_LICENSES.md)
    endif()
    # Collect each library's actual license file (one level per library), avoiding the
    # empty directories and stray files that install(DIRECTORY ... FILES_MATCHING) leaves behind.
    file(GLOB _ob_3rdparty_libs RELATIVE ${OB_3RDPARTY_DIR} ${OB_3RDPARTY_DIR}/*)
    foreach(_lib ${_ob_3rdparty_libs})
        if(IS_DIRECTORY ${OB_3RDPARTY_DIR}/${_lib})
            file(GLOB_RECURSE _lib_lics
                 ${OB_3RDPARTY_DIR}/${_lib}/LICENSE
                 ${OB_3RDPARTY_DIR}/${_lib}/LICENSE.txt
                 ${OB_3RDPARTY_DIR}/${_lib}/LICENSE.md
                 ${OB_3RDPARTY_DIR}/${_lib}/COPYING
                 ${OB_3RDPARTY_DIR}/${_lib}/COPYING.LESSER)
            foreach(_lic ${_lib_lics})
                install(FILES ${_lic} DESTINATION licenses/${_lib})
            endforeach()
        endif()
    endforeach()
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

