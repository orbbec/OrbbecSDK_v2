cmake_minimum_required(VERSION 3.10)

add_library(dylib INTERFACE)
target_include_directories(dylib INTERFACE ${CMAKE_CURRENT_LIST_DIR}/src/include)
if(UNIX)
    target_link_libraries(dylib INTERFACE dl)
endif()

add_library(dylib::dylib ALIAS dylib)
