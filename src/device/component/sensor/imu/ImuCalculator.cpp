// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "ImuCalculator.hpp"
#include "libobsensor/h/ObTypes.h"

namespace libobsensor {

float ImuCalculatorICM42668P::calculateAccelGravity(int16_t accelValue, uint8_t accelFSR) {
    float sensitivity = 0.f;

    switch(accelFSR) {
    case OB_ACCEL_FS_2g: /*!< 2g*/
        sensitivity = ACCEL_MAX / 2.0f;
        break;
    case OB_ACCEL_FS_4g: /*!< 4g*/
        sensitivity = ACCEL_MAX / 4.0f;
        break;
    case OB_ACCEL_FS_8g: /*!< 8g*/
        sensitivity = ACCEL_MAX / 8.0f;
        break;
    case OB_ACCEL_FS_16g: /*!< 16g*/
        sensitivity = ACCEL_MAX / 16.0f;
        break;
    case OB_ACCEL_FS_3g:
        sensitivity = ACCEL_MAX / 3.0f;
        break;
    case OB_ACCEL_FS_6g:
        sensitivity = ACCEL_MAX / 6.0f;
        break;
    case OB_ACCEL_FS_12g:
        sensitivity = ACCEL_MAX / 12.0f;
        break;
    case OB_ACCEL_FS_24g:
        sensitivity = ACCEL_MAX / 24.0f;
        break;
    }

    return (accelValue / sensitivity);
}

float ImuCalculatorICM42668P::calculateGyroDPS(int16_t gyroValue, uint8_t gyroFSR) {
    float sensitivity = 0.f;

    switch(gyroFSR) {
    case OB_GYRO_FS_16dps: /*!< 16dps*/
        sensitivity = GYRO_MAX / 16.0f;
        break;
    case OB_GYRO_FS_31dps: /*!< 31dps*/
        sensitivity = GYRO_MAX / 31.0f;
        break;
    case OB_GYRO_FS_62dps: /*!< 62dps*/
        sensitivity = GYRO_MAX / 62.0f;
        break;
    case OB_GYRO_FS_125dps: /*!< 125dps*/
        sensitivity = GYRO_MAX / 125.0f;
        break;
    case OB_GYRO_FS_250dps: /*!< 250dps*/
        sensitivity = GYRO_MAX / 250.0f;
        break;
    case OB_GYRO_FS_500dps: /*!< 500dps*/
        sensitivity = GYRO_MAX / 500.0f;
        break;
    case OB_GYRO_FS_1000dps: /*!< 1000dps*/
        sensitivity = GYRO_MAX / 1000.0f;
        break;
    case OB_GYRO_FS_2000dps: /*!< 2000dps*/
        sensitivity = GYRO_MAX / 2000.0f;
        break;
    case OB_GYRO_FS_400dps:
        sensitivity = GYRO_MAX / 400.0f;
        break;
    case OB_GYRO_FS_800dps:
        sensitivity = GYRO_MAX / 800.0f;
        break;
    }

    return (gyroValue / sensitivity);
}

float ImuCalculatorICM42668P::calculateRegisterTemperature(int16_t tempValue) {
    float ret = static_cast<float>(tempValue / 132.48 + 25);
    return ret;
}

#define GRAVITY_EARTH (9.80665f)
float ImuCalculatorBMI088::calculateAccelGravity(int16_t accelValue, uint8_t accelFSR) {
    float sensitivity = 0.0f;

    switch(accelFSR) {
    case OB_ACCEL_FS_2g: /*!< 2g*/
        sensitivity = 2.0f;
        break;
    case OB_ACCEL_FS_4g: /*!< 4g*/
        sensitivity = 4.0f;
        break;
    case OB_ACCEL_FS_8g: /*!< 8g*/
        sensitivity = 8.0f;
        break;
    case OB_ACCEL_FS_16g: /*!< 16g*/
        sensitivity = 16.0f;
        break;
    case OB_ACCEL_FS_3g:
        sensitivity = 3.0f;
        break;
    case OB_ACCEL_FS_6g:
        sensitivity = 6.0f;
        break;
    case OB_ACCEL_FS_12g:
        sensitivity = 12.0f;
        break;
    case OB_ACCEL_FS_24g:
        sensitivity = 24.0f;
        break;
    }

    float halfScale = (1 << 16) / 2.0f;

    return (GRAVITY_EARTH * accelValue * sensitivity) / halfScale;
}

#define BMI08X_GYRO_RANGE_2000_DPS UINT8_C(0x00)
float ImuCalculatorBMI088::calculateGyroDPS(int16_t gyroValue, uint8_t gyroFSR) {
    float sensitivity = 0.f;

    switch(gyroFSR) {
    case OB_GYRO_FS_16dps:
        sensitivity = 16.0f;
        break;
    case OB_GYRO_FS_31dps:
        sensitivity = 31.0f;
        break;
    case OB_GYRO_FS_62dps:
        sensitivity = 62.0f;
        break;
    case OB_GYRO_FS_125dps:
        sensitivity = 125.0f;
        break;
    case OB_GYRO_FS_250dps:
        sensitivity = 250.0f;
        break;
    case OB_GYRO_FS_500dps:
        sensitivity = 500.0f;
        break;
    case OB_GYRO_FS_1000dps:
        sensitivity = 1000.0f;
        break;
    case OB_GYRO_FS_2000dps:
        sensitivity = 2000.0f;
        break;
    case OB_GYRO_FS_400dps:
        sensitivity = 400.0f;
        break;
    case OB_GYRO_FS_800dps:
        sensitivity = 800.0f;
        break;
    }

    float deg2rad   = 0.017453293f;
    float halfScale = (1 << 16) / 2.0f;

    return sensitivity / (halfScale + BMI08X_GYRO_RANGE_2000_DPS) * gyroValue * deg2rad;
}

float ImuCalculatorBMI088::calculateRegisterTemperature(int16_t tempValue) {
    return static_cast<float>(tempValue) / 100.0f;
}
}  // namespace libobsensor