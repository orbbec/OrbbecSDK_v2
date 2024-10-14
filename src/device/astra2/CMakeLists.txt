# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)


target_sources(
    ${OB_TARGET_DEVICE}
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/Astra2DeviceInfo.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Astra2DeviceInfo.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Astra2Device.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Astra2Device.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Astra2PropertyAccessors.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Astra2PropertyAccessors.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Astra2FrameTimestampCalculator.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Astra2FrameTimestampCalculator.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Astra2AlgParamManager.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Astra2AlgParamManager.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Astra2DepthWorkModeManager.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Astra2DepthWorkModeManager.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Astra2StreamProfileFilter.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Astra2StreamProfileFilter.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Astra2DeviceSyncConfigurator.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Astra2DeviceSyncConfigurator.cpp
)
