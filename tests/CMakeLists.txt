# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.13)

set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

if(MSVC)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()

add_subdirectory(${PROJECT_SOURCE_DIR}/3rdparty/googletest
                 ${CMAKE_BINARY_DIR}/3rdparty/googletest)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")

    target_compile_options(gtest PRIVATE
        -Wno-error
    )

    target_compile_options(gtest_main PRIVATE
        -Wno-error
    )

elseif(MSVC)

    target_compile_options(gtest PRIVATE
        /WX-
    )

    target_compile_options(gtest_main PRIVATE
        /WX-
    )

endif()

file(GLOB subdirectories RELATIVE ${CMAKE_CURRENT_LIST_DIR} "*")

foreach(subdir ${subdirectories})
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${subdir}
       AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${subdir}/CMakeLists.txt)
        add_subdirectory(${subdir})
    endif()
endforeach()