# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

target_sources(
    ${OB_TARGET_DEVICE}
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/G305DeviceInfo.cpp
    ${CMAKE_CURRENT_LIST_DIR}/G305DeviceInfo.hpp
    ${CMAKE_CURRENT_LIST_DIR}/G305Device.hpp
    ${CMAKE_CURRENT_LIST_DIR}/G305Device.cpp
    ${CMAKE_CURRENT_LIST_DIR}/G305DepthWorkModeManager.hpp
    ${CMAKE_CURRENT_LIST_DIR}/G305DepthWorkModeManager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/G305PresetManager.hpp
    ${CMAKE_CURRENT_LIST_DIR}/G305PresetManager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/G305FrameInterleaveManager.hpp
    ${CMAKE_CURRENT_LIST_DIR}/G305FrameInterleaveManager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/G305FrameMetadataParserContainer.hpp
    ${CMAKE_CURRENT_LIST_DIR}/G305MetadataParser.hpp
    ${CMAKE_CURRENT_LIST_DIR}/G305MetadataTypes.hpp
    ${CMAKE_CURRENT_LIST_DIR}/G305FrameTimestampCalculator.hpp
    ${CMAKE_CURRENT_LIST_DIR}/G305FrameTimestampCalculator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/G305SensorStreamStrategy.hpp
    ${CMAKE_CURRENT_LIST_DIR}/G305SensorStreamStrategy.cpp
    ${CMAKE_CURRENT_LIST_DIR}/G305PropertyAccessors.hpp
    ${CMAKE_CURRENT_LIST_DIR}/G305PropertyAccessors.cpp
    ${CMAKE_CURRENT_LIST_DIR}/G305AlgParamManager.hpp
    ${CMAKE_CURRENT_LIST_DIR}/G305AlgParamManager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/G305MetadataModifier.hpp
    ${CMAKE_CURRENT_LIST_DIR}/G305MetadataModifier.cpp
)

