cmake_minimum_required(VERSION 3.5)
project(rosbag LANGUAGES CXX C)

include(${CMAKE_CURRENT_LIST_DIR}/config.cmake)

add_definitions(-DNO_OPENSSL -DXXH_NAMESPACE=OB_)
if (WIN32)
    add_definitions(-DNO_GETIFADDRS)
endif()

FILE(GLOB_RECURSE AllSources
        ${LZ4_DIR}/lz4.h
        ${LZ4_DIR}/lz4.c
        ${ROSBAG_DIR}/*.h
        ${ROSBAG_DIR}/*.cpp
        ${ROSBAG_DIR}/*.c
)

add_library(rosbag STATIC
    ${AllSources}
    ${SOURCE_FILES_ROSBAG}
)

# if(ANDROID_NDK_TOOLCHAIN_INCLUDED AND CMAKE_ANDROID_NDK_VERSION VERSION_LESS 24)
#     aux_source_directory(android_ifaddrs_addition ANDROID_ADDITION_SRCS)
#     target_sources(live555 PRIVATE "${ANDROID_ADDITION_SRCS}")
#     set(LIVE555_INCLUDE_DIRS
#         ${LIVE555_INCLUDE_DIRS}
#         ${CMAKE_CURRENT_SOURCE_DIR}/android_ifaddrs_addition
#     )
# endif()

target_include_directories(${PROJECT_NAME} PUBLIC
        ${ROSBAG_HEADER_DIRS}
        ${LZ4_INCLUDE_PATH}
)

if (WIN32)
    target_link_libraries(rosbag PRIVATE Ws2_32)
endif ()

# for test:
# add_subdirectory(mediaServer)
# add_subdirectory(testProgs)