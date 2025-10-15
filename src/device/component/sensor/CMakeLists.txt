# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

target_sources(${OB_TARGET_DEVICE} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/SensorBase.cpp ${CMAKE_CURRENT_LIST_DIR}/SensorBase.hpp)

add_subdirectory(imu)
add_subdirectory(lidar)
add_subdirectory(video)
