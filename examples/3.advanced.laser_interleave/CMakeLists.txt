# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)
project(ob_laser_interleave)

add_executable(ob_laser_interleave laser_interleave.cpp)

set_property(TARGET ob_laser_interleave PROPERTY CXX_STANDARD 11)
target_link_libraries(ob_laser_interleave ob::OrbbecSDK ob::examples::utils)
set_target_properties(ob_laser_interleave PROPERTIES FOLDER "examples")

install(TARGETS ob_laser_interleave RUNTIME DESTINATION bin)

