// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "libobsensor/h/ObTypes.h"
#include <iostream>

namespace libobsensor {
namespace utils {
float    getBytesPerPixel(OBFormat format);
uint32_t calcDefaultStrideBytes(OBFormat format, uint32_t width);
uint32_t calcVideoFrameMaxDataSize(OBFormat format, uint32_t width, uint32_t height);

OBFrameType  mapStreamTypeToFrameType(OBStreamType type);
OBStreamType mapFrameTypeToStreamType(OBFrameType type);
OBStreamType mapSensorTypeToStreamType(OBSensorType type);
OBSensorType mapStreamTypeToSensorType(OBStreamType type);

template <typename T> uint32_t fourCc2Int(const T a, const T b, const T c, const T d) {
    static_assert((std::is_integral<T>::value), "fourcc supports integral built-in types only");
    return ((static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(c) << 8) | (static_cast<uint32_t>(d) << 0));
}
OBFormat uvcFourccToOBFormat(uint32_t fourcc);
uint32_t obFormatToUvcFourcc(OBFormat format);

float mapIMUSampleRateToValue(OBIMUSampleRate rate);

const std::string &obFormatToStr(OBFormat type);
const std::string &obFrameToStr(OBFrameType type);
const std::string &obStreamToStr(OBStreamType type);
const std::string &obSensorToStr(OBSensorType type);
const std::string &obImuRateToStr(OBIMUSampleRate type);
const std::string &GyroFullScaleRangeToStr(OBGyroFullScaleRange range);
const std::string &AccelFullScaleRangeToStr(OBAccelFullScaleRange params);
const std::string &MetaDataToStr(OBFrameMetadataType params);

OBFormat              strToOBFormat(const std::string str);
OBFrameType           strToOBFrame(const std::string str);
OBStreamType          strToOBStream(const std::string str);
OBSensorType          strToOBSensor(const std::string str);
OBIMUSampleRate       strToObImuRate(const std::string str);
OBGyroFullScaleRange  strToGyroFullScaleRange(const std::string str);
OBAccelFullScaleRange strToAccelFullScaleRange(const std::string str);

float                 depthPrecisionLevelToUnit(OBDepthPrecisionLevel precision);
OBDepthPrecisionLevel depthUnitToPrecisionLevel(float unit);

}  // namespace utils
}  // namespace libobsensor

std::ostream &operator<<(std::ostream &os, const OBFormat &type);
std::ostream &operator<<(std::ostream &os, const OBFrameType &type);
std::ostream &operator<<(std::ostream &os, const OBStreamType &type);
std::ostream &operator<<(std::ostream &os, const OBSensorType &type);
std::ostream &operator<<(std::ostream &os, const OBIMUSampleRate &type);  // also for accel
std::ostream &operator<<(std::ostream &os, const OBGyroFullScaleRange &type);
std::ostream &operator<<(std::ostream &os, const OBAccelFullScaleRange &type);
std::ostream &operator<<(std::ostream &os, const OBCameraParam &params);
std::ostream &operator<<(std::ostream &os, const OBCalibrationParam &type);
std::ostream &operator<<(std::ostream &os, const OBPoint3f &type);
std::ostream &operator<<(std::ostream &os, const OBExtrinsic &type);
std::ostream &operator<<(std::ostream &os, const OBPoint2f &type);
std::ostream &operator<<(std::ostream &os, const OBCameraIntrinsic &type);
std::ostream &operator<<(std::ostream &os, const OBCameraDistortion &type);


