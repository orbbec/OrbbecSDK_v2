# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)


file (GLOB _cpp_files "${CMAKE_CURRENT_LIST_DIR}/*.cpp")
file (GLOB _head_files "${CMAKE_CURRENT_LIST_DIR}/*.hpp")


foreach(_file IN ITEMS ${_cpp_files})
        target_sources(${OB_TARGET_PAL} PRIVATE "${_file}")
endforeach()

foreach(_file IN ITEMS ${_head_files})
        target_sources(${OB_TARGET_PAL} PRIVATE "${_file}")
endforeach()


# if (LIBUSB_1_LIBRARIES AND LIBUSB_1_INCLUDE_DIRS)
#   # in cache already
#   set(LIBUSB_FOUND TRUE)
#   set(LIBUSB_1_FOUND TRUE)

# else (LIBUSB_1_LIBRARIES AND LIBUSB_1_INCLUDE_DIRS)
#   find_path(LIBUSB_1_INCLUDE_DIR
#             NAMES libusb-1.0/libusb.h
# 	    PATHS /usr/include /usr/local/include /opt/local/include /sw/include
# 	    PATH_SUFFIXES libusb-1.0)

#   find_library(LIBUSB_1_LIBRARY
#                NAMES usb-1.0
# 	       PATHS /usr/lib /usr/local/lib /opt/local/lib /sw/lib)

#   set(LIBUSB_1_INCLUDE_DIRS ${LIBUSB_1_INCLUDE_DIR}/libusb-1.0/)
#   set(LIBUSB_1_LIBRARIES ${LIBUSB_1_LIBRARY})

#   include(FindPackageHandleStandardArgs)
#   find_package_handle_standard_args(LIBUSB_1 DEFAULT_MSG LIBUSB_1_LIBRARY LIBUSB_1_INCLUDE_DIR)

#   # show the LIBUSB_1_INCLUDE_DIRS and LIBUSB_1_LIBRARIES variables only in the advanced view
#   mark_as_advanced(LIBUSB_1_INCLUDE_DIRS LIBUSB_1_LIBRARIES)
# endif (LIBUSB_1_LIBRARIES AND LIBUSB_1_INCLUDE_DIRS)

# include_directories(${LIBUSB_1_INCLUDE_DIRS})

