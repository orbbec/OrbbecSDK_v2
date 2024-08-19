cmake_minimum_required(VERSION 3.5)

file(GLOB subdirectories RELATIVE ${CMAKE_CURRENT_LIST_DIR} "*")

# dependecies
find_package(OpenCV QUIET)
if(NOT ${OpenCV_FOUND})
    message(WARNING "OpenCV not found, some examples may not be built. Please install OpenCV or set OpenCV_DIR to the directory of your OpenCV installation.")
endif()

# utils
add_subdirectory(utils)

# c examples
add_subdirectory(c_examples)

# cpp examples
add_subdirectory(0.basic.enumerate)
add_subdirectory(1.stream.imu)
add_subdirectory(2.device.control)
add_subdirectory(3.advanced.point_cloud)
add_subdirectory(3.advanced.preset)

# cpp examples with OpenCV Requirements
if(${OpenCV_FOUND})
    add_subdirectory(0.basic.quick_start)
    add_subdirectory(1.stream.depth)
    add_subdirectory(1.stream.color)
    add_subdirectory(1.stream.infrared)
    add_subdirectory(1.stream.callback)
    add_subdirectory(1.stream.multi_streams)
    add_subdirectory(2.device.hot_plugin)
    # add_subdirectory(2.device.net_device)
    add_subdirectory(3.advanced.common_usages)
    add_subdirectory(3.advanced.sync_align)
    add_subdirectory(3.advanced.post_processing)
    add_subdirectory(3.advanced.coordinate_transform)
    add_subdirectory(3.advanced.hdr)
    add_subdirectory(3.advanced.multi_devices)
endif()

# InstallRequiredSystemLibraries
include(InstallRequiredSystemLibraries)