cmake_minimum_required(VERSION 3.10)

add_subdirectory(src)
ob_source_group(spdlog::spdlog)
set_target_properties(spdlog PROPERTIES FOLDER "dependencies")

get_target_property(compile_options spdlog COMPILE_OPTIONS)
list(REMOVE_ITEM compile_options "-Werror" "/WX")
set_target_properties(spdlog PROPERTIES COMPILE_OPTIONS "${compile_options}")
