cmake_minimum_required(VERSION 3.10)

add_subdirectory(src)

# Remove warning as error for live555
get_target_property(compile_options live555 COMPILE_OPTIONS)
list(REMOVE_ITEM compile_options "-Werror" "/WX")
set_target_properties(live555 PROPERTIES COMPILE_OPTIONS "${compile_options}")

add_library(live555::live555 ALIAS live555)
ob_source_group(live555::live555)
set_target_properties(live555 PROPERTIES FOLDER "dependencies")
