# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)
project(ob_device_record)

add_executable(ob_device_record device_record.cpp)

set_property(TARGET ob_device_record PROPERTY CXX_STANDARD 11)
target_link_libraries(ob_device_record ob::OrbbecSDK ob::examples::utils )

set_target_properties (ob_device_record PROPERTIES FOLDER "examples")

install(TARGETS ob_device_record RUNTIME DESTINATION bin)

