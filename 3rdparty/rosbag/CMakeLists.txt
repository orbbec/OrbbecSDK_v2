cmake_minimum_required(VERSION 3.5)

add_subdirectory(src)

get_target_property(compile_options rosbag COMPILE_OPTIONS)
list(REMOVE_ITEM compile_options "-Werror" "/WX")
set_target_properties(rosbag PROPERTIES COMPILE_OPTIONS "${compile_options}")

add_library(rosbag::rosbag ALIAS rosbag)
ob_source_group(rosbag::rosbag)
set_target_properties(rosbag PROPERTIES FOLDER "dependencies")
