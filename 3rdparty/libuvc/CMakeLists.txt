cmake_minimum_required(VERSION 3.10)

add_subdirectory(src)

# remove warning as error
get_target_property(compile_options uvc_static COMPILE_OPTIONS)
list(REMOVE_ITEM compile_options "-Werror" "/WX" "-Wpedantic")
set_target_properties(uvc_static PROPERTIES COMPILE_OPTIONS "${compile_options}")

add_library(libuvc::libuvc ALIAS uvc_static)
ob_source_group(libuvc::libuvc)
set_target_properties(uvc_static PROPERTIES FOLDER "dependencies")
