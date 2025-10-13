cmake_minimum_required(VERSION 3.10)
project(Live555 LANGUAGES CXX C)

add_definitions(-DNO_OPENSSL)
if (WIN32)
    add_definitions(-DNO_GETIFADDRS)
endif()

aux_source_directory(groupsock GROUPSOCK_SRCS)
aux_source_directory(liveMedia LIVEMEDIA_SRCS)
aux_source_directory(UsageEnvironment USAGE_ENVIRONMENT_SRCS)
aux_source_directory(BasicUsageEnvironment BASIC_USAGE_ENVIRONMENT_SRCS)

set(LIVE555_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/groupsock/include
    ${CMAKE_CURRENT_SOURCE_DIR}/liveMedia/include
    ${CMAKE_CURRENT_SOURCE_DIR}/UsageEnvironment/include
    ${CMAKE_CURRENT_SOURCE_DIR}/BasicUsageEnvironment/include
)

# message("LIVE555_INCLUDE_DIRS=${LIVE555_INCLUDE_DIRS}")

add_library(live555 STATIC
    ${GROUPSOCK_SRCS}
    ${LIVEMEDIA_SRCS}
    ${USAGE_ENVIRONMENT_SRCS}
    ${BASIC_USAGE_ENVIRONMENT_SRCS}
    ${ANDROID_ADDITION_SRCS}
)

if(ANDROID_NDK_TOOLCHAIN_INCLUDED AND CMAKE_ANDROID_NDK_VERSION VERSION_LESS 24)
    aux_source_directory(android_ifaddrs_addition ANDROID_ADDITION_SRCS)
    target_sources(live555 PRIVATE "${ANDROID_ADDITION_SRCS}")
    set(LIVE555_INCLUDE_DIRS
        ${LIVE555_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/android_ifaddrs_addition
    )
endif()

target_include_directories(live555
    PUBLIC
    ${LIVE555_INCLUDE_DIRS}
)

if (WIN32)
    target_link_libraries(live555 PRIVATE Ws2_32)
endif ()

# for test:
# add_subdirectory(mediaServer)
# add_subdirectory(testProgs)