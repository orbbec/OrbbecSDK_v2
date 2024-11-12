// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <stdint.h>

namespace libobsensor {

class IImuCalculator {
public:
    virtual ~IImuCalculator() = default;

    virtual float calculateAccelGravity(int16_t accelValue, uint8_t accelFSR) = 0;
    virtual float calculateGyroDPS(int16_t gyroValue, uint8_t gyroFSR)        = 0;
    virtual float calculateRegisterTemperature(int16_t tempValue)             = 0;
};

class ImuCalculatorICM42668P : public IImuCalculator {
public:
    ImuCalculatorICM42668P()           = default;
    ~ImuCalculatorICM42668P() override = default;

    float calculateAccelGravity(int16_t accelValue, uint8_t accelFSR) override;
    float calculateGyroDPS(int16_t gyroValue, uint8_t gyroFSR) override;
    float calculateRegisterTemperature(int16_t tempValue) override;

private:
    const float GYRO_MAX  = 32800 / 0.017453293f;
    const float ACCEL_MAX = 32768 / 9.80f;
};

class ImuCalculatorBMI088 : public IImuCalculator {
public:
    ImuCalculatorBMI088()           = default;
    ~ImuCalculatorBMI088() override = default;

    float calculateAccelGravity(int16_t accelValue, uint8_t accelFSR) override;
    float calculateGyroDPS(int16_t gyroValue, uint8_t gyroFSR) override;
    float calculateRegisterTemperature(int16_t tempValue) override;
};
}  // namespace libobsensor