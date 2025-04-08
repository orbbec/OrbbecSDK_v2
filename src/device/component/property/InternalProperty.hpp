// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "libobsensor/h/Property.h"

namespace libobsensor {

typedef enum {
    OB_PROP_FEMTO_MEGA_HARDWARE_D2C_BOOL    = 13,  /**< FemtoMega hardware d2c switch*/
    OB_PROP_DEVICE_RESET_BOOL               = 29,  /**< Reset/reboot the device*/
    OB_PROP_STOP_DEPTH_STREAM_BOOL          = 38,  /**<Disable the depth stream*/
    OB_PROP_STOP_IR_STREAM_BOOL             = 39,  /**<Disable the IR stream*/
    OB_PROP_TOF_EXPOSURE_TIME_INT           = 47,  /**<TOF exposure time //only sdk-firmware internal use*/
    OB_PROP_TOF_GAIN_INT                    = 48,  /**<TOF gain value //only sdk-firmware internal use*/
    OB_PROP_REBOOT_DEVICE_BOOL              = 57,  /**< Reboot the device*/
    OB_PROP_STOP_COLOR_STREAM_BOOL          = 77,  /**< Disable the Color stream*/
    OB_PROP_DEVICE_COMMUNICATION_TYPE_INT   = 97,  // Device communication type, 0: USB; 1: Ethernet(RTSP)
    OB_PROP_FAN_WORK_LEVEL_INT              = 110, /**< Fan speed settings*/
    OB_PROP_DEVICE_PID_INT                  = 111, /**< Device product id*/
    OB_PROP_DEPTH_MIRROR_MODULE_STATUS_BOOL = 108, /**< Depth mirror module status*/
    OB_PROP_FAN_WORK_SPEED_INT              = 120, /**< Fan speed*/
    OB_PROP_STOP_IR_RIGHT_STREAM_BOOL       = 139, /**<Disable the right IR stream*/

    OB_STRUCT_DEVICE_UPGRADE_STATUS    = 1006, /**< Firmware upgrade status, read only*/
    OB_PROP_START_COLOR_STREAM_BOOL    = 125,  /**< Start or stop color stream */
    OB_PROP_START_DEPTH_STREAM_BOOL    = 126,  /**< Start or stop depth stream */
    OB_PROP_START_IR_STREAM_BOOL       = 127,  /**< Start or stop left ir stream */
    OB_PROP_START_IR_RIGHT_STREAM_BOOL = 201,  /**< Start or stop right ir stream */

    OB_PROP_GYRO_SWITCH_BOOL     = 2019, /**< Gyroscope switch*/
    OB_PROP_ACCEL_SWITCH_BOOL    = 2020, /**< Accelerometer switch*/
    OB_PROP_GYRO_ODR_INT         = 2021, /**< get/set current gyroscope sampling rate*/
    OB_PROP_ACCEL_ODR_INT        = 2022, /**< get/set the current sampling rate of the accelerometer*/
    OB_PROP_GYRO_FULL_SCALE_INT  = 2023, /**< get/set current gyroscope range*/
    OB_PROP_ACCEL_FULL_SCALE_INT = 2024, /**< get/set current accelerometer range*/

    OB_STRUCT_VERSION                           = 1000, /**< version information*/
    OB_STRUCT_GET_GYRO_PRESETS_ODR_LIST         = 1031, /**< Get the list of sampling rates supported by the gyroscope*/
    OB_STRUCT_GET_ACCEL_PRESETS_ODR_LIST        = 1032, /**< Get the list of sampling rates supported by the accelerometer*/
    OB_STRUCT_GET_GYRO_PRESETS_FULL_SCALE_LIST  = 1033, /**< Get the range list supported by the gyroscope*/
    OB_STRUCT_GET_ACCEL_PRESETS_FULL_SCALE_LIST = 1034, /**< Get the range list supported by the accelerometer*/

    OB_STRUCT_COLOR_STREAM_PROFILE    = 1048, /**< set stream profile to color*/
    OB_STRUCT_DEPTH_STREAM_PROFILE    = 1049, /**< set stream profile to depth*/
    OB_STRUCT_IR_STREAM_PROFILE       = 1050, /**< set stream profile to ir or left ir*/
    OB_STRUCT_IR_RIGHT_STREAM_PROFILE = 1065, /**< set stream profile to right ir*/

    OB_PROP_IMU_STREAM_PORT_INT = 3026, /**< set stream port to imu*/

    OB_RAW_DATA_D2C_ALIGN_SUPPORT_PROFILE_LIST           = 4024, /**< D2C Aligned Resolution List*/
    OB_RAW_DATA_DEPTH_CALIB_PARAM                        = 4026, /**< Depth calibration parameters*/
    OB_RAW_DATA_ALIGN_CALIB_PARAM                        = 4027,
    OB_RAW_DATA_DEPTH_ALG_MODE_LIST                      = 4034, /**< Depth algorithm mode list*/
    OB_RAW_DATA_EFFECTIVE_VIDEO_STREAM_PROFILE_LIST      = 4035, /**< Current effective video stream profile list*/
    OB_RAW_DATA_STREAM_PROFILE_LIST                      = 4033, /**< Stream configuration list*/
    OB_RAW_DATA_IMU_CALIB_PARAM                          = 4036, /**< IMU calibration parameters*/
    OB_RAW_DATA_DE_IR_RECTIFY_PARAMS                     = 4037, /**< DE-IR rectification parameters*/
    OB_RAW_DATA_DEVICE_UPGRADE                           = 4039, /**< Device firmware upgrade*/
    OB_RAW_DATA_DEVICE_EXTENSION_INFORMATION             = 4041, /**< Device extension information*/
    OB_RAW_DATA_D2C_ALIGN_COLOR_PRE_PROCESS_PROFILE_LIST = 4046, /**< D2C Alignment Resolution List*/
    OB_RAW_DATA_DE_IR_TRANSFORM_PARAMS                   = 4059, /**< DE-IR transform parameters*/
    OB_PROP_DEVICE_LOG_SEVERITY_LEVEL_INT                = 5003, /**< Device log level*/
} OBInternalPropertyID;

}  // namespace libobsensor
