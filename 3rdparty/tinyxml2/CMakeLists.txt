cmake_minimum_required(VERSION 3.10)

add_library(tinyxml2 STATIC src/tinyxml2.cpp)
target_include_directories(tinyxml2 INTERFACE ${CMAKE_CURRENT_LIST_DIR}/src)

add_library(tinyxml2::tinyxml2 ALIAS tinyxml2)
ob_source_group(tinyxml2::tinyxml2)

set_target_properties(tinyxml2 PROPERTIES FOLDER "dependencies")

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
    check_cxx_compiler_flag("-Wno-unsafe-buffer-usage" HAS_NO_UNSAFE_BUFFER_USAGE)
    if(HAS_NO_UNSAFE_BUFFER_USAGE)
        target_compile_options(tinyxml2 PRIVATE -Wno-unsafe-buffer-usage)
    endif()
endif()
