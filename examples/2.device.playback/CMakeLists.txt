# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)
project(ob_device_playback)

add_executable(ob_device_playback device_playback.cpp)

set_property(TARGET ob_device_playback PROPERTY CXX_STANDARD 11)
target_link_libraries(ob_device_playback ob::OrbbecSDK ob::examples::utils )

set_target_properties (ob_device_playback PROPERTIES FOLDER "examples")

install(TARGETS ob_device_playback RUNTIME DESTINATION bin)

