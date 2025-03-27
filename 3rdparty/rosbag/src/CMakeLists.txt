cmake_minimum_required(VERSION 3.5)
project(rosbag LANGUAGES CXX C)

include(${CMAKE_CURRENT_LIST_DIR}/config.cmake)

add_definitions(-DNO_OPENSSL)
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

target_include_directories(${PROJECT_NAME} PUBLIC
        ${ROSBAG_HEADER_DIRS}
        ${LZ4_INCLUDE_PATH}
)

if (WIN32)
    target_link_libraries(rosbag PRIVATE Ws2_32)
endif ()
