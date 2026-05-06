# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

# add the filter library with the version as the project
add_library(filter STATIC)
set_target_properties(filter PROPERTIES VERSION ${PROJECT_VERSION})

# add the source files
file(GLOB_RECURSE SOURCE_FILES "*.cpp" EXCLUDE unittest)
file(GLOB_RECURSE HEADERS_FILES "*.hpp" EXCLUDE unittest)
target_sources(filter PRIVATE ${SOURCE_FILES} ${HEADERS_FILES})

# depedecencies
target_link_libraries(filter PUBLIC ob::shared ob::core ob::device)
target_include_directories(filter PUBLIC ${OB_PUBLIC_HEADERS_DIR} ${CMAKE_CURRENT_LIST_DIR})

add_subdirectory(${OB_3RDPARTY_DIR}/dylib dylib)
target_link_libraries(filter PUBLIC dylib::dylib)

add_subdirectory(${OB_3RDPARTY_DIR}/libjpeg libjpeg)
target_link_libraries(filter PUBLIC libjpeg::libjpeg)

add_subdirectory(${OB_3RDPARTY_DIR}/libyuv libyuv)
target_link_libraries(filter PUBLIC libyuv::libyuv)

add_library(ob::filter ALIAS filter)
ob_source_group(ob::filter)

set_target_properties(filter PROPERTIES FOLDER "modules")

# Enable SSE4.1 on x86-64 for GCC/Clang so the SIMD undistortion path is compiled in.
# MSVC x64 has SSE4.1 available by default without an extra flag.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        target_compile_options(filter PRIVATE -msse4.1)
    endif()
endif()

if(OB_INSTALL_FILTER_DEV_HEADERS)
    install(FILES ${CMAKE_CURRENT_LIST_DIR}/privatefilters/PrivFilterTypes.h DESTINATION include/libobsensor/h/)
endif()

