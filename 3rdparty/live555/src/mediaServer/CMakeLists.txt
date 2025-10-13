cmake_minimum_required(VERSION 3.10)

aux_source_directory(./ MEDIA_SERVER_SRCS)
add_executable(mediaServer ${MEDIA_SERVER_SRCS})

target_link_libraries(mediaServer PRIVATE live555)
target_include_directories(mediaServer
    PRIVATE
    ${LIVE555_INCLUDE_DIRS}
)
# if (WIN32)
#     target_link_libraries(mediaServer PRIVATE Ws2_32)
# endif ()