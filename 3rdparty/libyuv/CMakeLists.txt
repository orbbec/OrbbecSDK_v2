cmake_minimum_required(VERSION 3.10)

add_subdirectory(src)

get_target_property(compile_options yuv COMPILE_OPTIONS)
list(REMOVE_ITEM compile_options "-Werror" "/WX")
set_target_properties(yuv PROPERTIES COMPILE_OPTIONS "${compile_options}")

target_include_directories(yuv PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src/include)

add_library(libyuv::libyuv ALIAS yuv)
ob_source_group(libyuv::libyuv)
set_target_properties(yuv PROPERTIES FOLDER "dependencies")
