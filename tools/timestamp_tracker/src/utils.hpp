// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <libobsensor/ObSensor.hpp>
#include <memory>
#include <string>

namespace libobsensor {
namespace tools {

std::string           toLower(const std::string &s);
bool                  isImuSensorName(const std::string &name);
bool                  isWildcardToken(const std::string &value);
OBSensorType          sensorNameToType(const std::string &name);
OBFormat              formatStringToOBFormat(const std::string &fmt);
OBAccelFullScaleRange accelFullScaleRangeFromString(const std::string &range);
OBGyroFullScaleRange  gyroFullScaleRangeFromString(const std::string &range);
bool                  deviceHasSensor(std::shared_ptr<ob::Device> device, OBSensorType type);
bool                  isEscPressed();
uint64_t              getWallTimesUs();
uint64_t              getSteadyTimeUs();

}  // namespace tools
}  // namespace libobsensor
