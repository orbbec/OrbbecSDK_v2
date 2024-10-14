# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)


target_sources(
    ${OB_TARGET_DEVICE}
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/G2DeviceInfo.hpp
        ${CMAKE_CURRENT_LIST_DIR}/G2DeviceInfo.cpp
        ${CMAKE_CURRENT_LIST_DIR}/G2Device.hpp
        ${CMAKE_CURRENT_LIST_DIR}/G2Device.cpp
        ${CMAKE_CURRENT_LIST_DIR}/G2XLDevice.cpp
        ${CMAKE_CURRENT_LIST_DIR}/G2XLDevice.cpp
        ${CMAKE_CURRENT_LIST_DIR}/G2StreamProfileFilter.hpp
        ${CMAKE_CURRENT_LIST_DIR}/G2StreamProfileFilter.cpp
        ${CMAKE_CURRENT_LIST_DIR}/G2PropertyAccessors.hpp
        ${CMAKE_CURRENT_LIST_DIR}/G2PropertyAccessors.cpp
        ${CMAKE_CURRENT_LIST_DIR}/G2DepthWorkModeManager.hpp
        ${CMAKE_CURRENT_LIST_DIR}/G2DepthWorkModeManager.cpp
        ${CMAKE_CURRENT_LIST_DIR}/G2FrameTimestampCalculator.hpp
        ${CMAKE_CURRENT_LIST_DIR}/G2FrameTimestampCalculator.cpp
        ${CMAKE_CURRENT_LIST_DIR}/G2AlgParamManager.hpp
        ${CMAKE_CURRENT_LIST_DIR}/G2AlgParamManager.cpp
)
