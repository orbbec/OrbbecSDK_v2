# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

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
	${CMAKE_CURRENT_LIST_DIR}/OpenNIFrameTimestampCalculator.hpp
	${CMAKE_CURRENT_LIST_DIR}/OpenNIFrameTimestampCalculator.cpp
	${CMAKE_CURRENT_LIST_DIR}/OpenNIDisparitySensor.cpp
	${CMAKE_CURRENT_LIST_DIR}/OpenNIDisparitySensor.hpp
	${CMAKE_CURRENT_LIST_DIR}/DaBaiDevice.cpp
	${CMAKE_CURRENT_LIST_DIR}/DaBaiDevice.hpp
    ${CMAKE_CURRENT_LIST_DIR}/MaxDevice.cpp
	${CMAKE_CURRENT_LIST_DIR}/MaxDevice.hpp
	${CMAKE_CURRENT_LIST_DIR}/DW2Device.cpp
	${CMAKE_CURRENT_LIST_DIR}/DW2Device.hpp
	${CMAKE_CURRENT_LIST_DIR}/OpenNIStreamProfileFilter.cpp
	${CMAKE_CURRENT_LIST_DIR}/OpenNIStreamProfileFilter.hpp
	${CMAKE_CURRENT_LIST_DIR}/MaxDisparitySensor.cpp
	${CMAKE_CURRENT_LIST_DIR}/MaxDisparitySensor.hpp
	${CMAKE_CURRENT_LIST_DIR}/DW2DisparitySensor.cpp
	${CMAKE_CURRENT_LIST_DIR}/DW2DisparitySensor.hpp
	${CMAKE_CURRENT_LIST_DIR}/AstraMiniDevice.cpp
	${CMAKE_CURRENT_LIST_DIR}/AstraMiniDevice.hpp
	${CMAKE_CURRENT_LIST_DIR}/OpenNIDeviceSyncConfigurator.cpp
	${CMAKE_CURRENT_LIST_DIR}/OpenNIDeviceSyncConfigurator.hpp
	${CMAKE_CURRENT_LIST_DIR}/AstraMiniSensorStreamStrategy.cpp
	${CMAKE_CURRENT_LIST_DIR}/AstraMiniSensorStreamStrategy.hpp
)

