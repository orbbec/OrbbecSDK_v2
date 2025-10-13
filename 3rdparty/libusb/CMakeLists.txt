cmake_minimum_required(VERSION 3.10)

add_subdirectory(src)

# remove warning as error
get_target_property(compile_options libusb_static COMPILE_OPTIONS)
list(REMOVE_ITEM compile_options "-Werror" "/WX" "-Wpedantic")
set_target_properties(libusb_static PROPERTIES COMPILE_OPTIONS "${compile_options}")

add_library(libusb::libusb ALIAS libusb_static)
ob_source_group(libusb::libusb)
set_target_properties(libusb_static PROPERTIES FOLDER "dependencies")
