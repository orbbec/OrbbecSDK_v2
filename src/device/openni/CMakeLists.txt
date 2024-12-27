# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

target_sources(
    ${OB_TARGET_DEVICE}
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/OpenNIDeviceInfo.cpp
    ${CMAKE_CURRENT_LIST_DIR}/OpenNIDeviceInfo.hpp
    ${CMAKE_CURRENT_LIST_DIR}/OpenNIDeviceBase.cpp
    ${CMAKE_CURRENT_LIST_DIR}/OpenNIDeviceBase.hpp
    ${CMAKE_CURRENT_LIST_DIR}/OpenNIAlgParamManager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/OpenNIAlgParamManager.hpp
	${CMAKE_CURRENT_LIST_DIR}/OpenNIPropertyAccessors.cpp
    ${CMAKE_CURRENT_LIST_DIR}/OpenNIPropertyAccessors.hpp
    ${CMAKE_CURRENT_LIST_DIR}/MaxProDevice.cpp
	${CMAKE_CURRENT_LIST_DIR}/MaxProDevice.hpp
)

