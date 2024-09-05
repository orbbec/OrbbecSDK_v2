cmake_minimum_required(VERSION 3.5)

target_sources(${OB_TARGET_PAL} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/HidDevicePort.cpp")
target_sources(${OB_TARGET_PAL} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/HidDevicePort.hpp")

if(OB_BUILD_GMSL_PAL AND OB_BUILD_LINUX)
    target_sources(${OB_TARGET_PAL} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/HidDevicePortGmsl.hpp")
    target_sources(${OB_TARGET_PAL} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/HidDevicePortGmsl.cpp")
endif()
