// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "libobsensor/h/TypeHelper.h"

#include "ImplTypes.hpp"
#include "utils/Utils.hpp"

#ifdef __cplusplus
extern "C" {
#endif

const char *ob_format_type_to_string(OBFormat type) {
    return libobsensor::utils::obFormatToStr(type).c_str();
}

const char *ob_frame_type_to_string(OBFrameType type) {
    return libobsensor::utils::obFrameToStr(type).c_str();
}

const char *ob_format_to_string(OBFormat format) {
    return libobsensor::utils::obFormatToStr(format).c_str();
}

const char *ob_stream_type_to_string(OBStreamType type) {
    return libobsensor::utils::obStreamToStr(type).c_str();
}

const char *ob_sensor_type_to_string(OBSensorType type) {
    return libobsensor::utils::obSensorToStr(type).c_str();
}

const char *ob_imu_rate_type_to_string(OBIMUSampleRate type) {
    return libobsensor::utils::obImuRateToStr(type).c_str();
}

float ob_imu_rate_type_to_value(OBIMUSampleRate type) {
    return libobsensor::utils::mapIMUSampleRateToValue(type);
}

OBIMUSampleRate ob_imu_rate_value_to_type(float value) {
    return libobsensor::utils::mapValueToIMUSampleRate(value);
}

const char *ob_gyro_range_type_to_string(OBGyroFullScaleRange type) {
    return libobsensor::utils::GyroFullScaleRangeToStr(type).c_str();
}

const char *ob_accel_range_type_to_string(OBAccelFullScaleRange type) {
    return libobsensor::utils::AccelFullScaleRangeToStr(type).c_str();
}

const char *ob_lidar_scan_rate_type_to_string(OBLiDARScanRate type) {
    return libobsensor::utils::LiDARScanRateToStr(type).c_str();
}

const char *ob_meta_data_type_to_string(OBFrameMetadataType type) {
    return libobsensor::utils::MetaDataToStr(type).c_str();
}

OBStreamType ob_sensor_type_to_stream_type(OBSensorType type) {
    return libobsensor::utils::mapSensorTypeToStreamType(type);
}

OB_EXPORT OBSensorType ob_stream_type_to_sensor_type(OBStreamType type) {
    return libobsensor::utils::mapStreamTypeToSensorType(type);
}

OB_EXPORT OBFrameType ob_stream_type_to_frame_type(OBStreamType type) {
    return libobsensor::utils::mapStreamTypeToFrameType(type);
}

OBStreamType ob_frame_type_to_stream_type(OBFrameType type) {
    return libobsensor::utils::mapFrameTypeToStreamType(type);
}

OB_EXPORT OBSensorType ob_frame_type_to_sensor_type(OBFrameType type) {
    return libobsensor::utils::mapFrameTypeToSensorType(type);
}

#ifdef __cplusplus
}
#endif
