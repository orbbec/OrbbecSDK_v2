// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

/**
 * @file Property.h
 * @brief Control command property list maintenance
 */

#pragma once

#include "ObTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enumeration value describing all attribute control commands of the device
 */
typedef enum {
    /**
     * @brief LDP switch
     */
    OB_PROP_LDP_BOOL = 2,

    /**
     * @brief Laser switch
     */
    OB_PROP_LASER_BOOL = 3,

    /**
     * @brief laser pulse width
     */
    OB_PROP_LASER_PULSE_WIDTH_INT = 4,

    /**
     * @brief Laser current (uint: mA)
     */
    OB_PROP_LASER_CURRENT_FLOAT = 5,

    /**
     * @brief IR flood switch
     */
    OB_PROP_FLOOD_BOOL = 6,

    /**
     * @brief IR flood level
     */
    OB_PROP_FLOOD_LEVEL_INT = 7,

    /**
     * @brief Enable/disable temperature compensation
     *
     */
    OB_PROP_TEMPERATURE_COMPENSATION_BOOL = 8,

    /**
     * @brief Depth mirror
     */
    OB_PROP_DEPTH_MIRROR_BOOL = 14,

    /**
     * @brief Depth flip
     */
    OB_PROP_DEPTH_FLIP_BOOL = 15,

    /**
     * @brief Depth Postfilter
     */
    OB_PROP_DEPTH_POSTFILTER_BOOL = 16,

    /**
     * @brief Depth Holefilter
     */
    OB_PROP_DEPTH_HOLEFILTER_BOOL = 17,

    /**
     * @brief IR mirror
     */
    OB_PROP_IR_MIRROR_BOOL = 18,

    /**
     * @brief IR flip
     */
    OB_PROP_IR_FLIP_BOOL = 19,

    /**
     * @brief Minimum depth threshold
     */
    OB_PROP_MIN_DEPTH_INT = 22,

    /**
     * @brief Maximum depth threshold
     */
    OB_PROP_MAX_DEPTH_INT = 23,

    /**
     * @brief Software filter switch
     */
    OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_BOOL = 24,

    /**
     * @brief LDP status
     */
    OB_PROP_LDP_STATUS_BOOL = 32,

    /**
     * @brief maxdiff for depth noise removal filter
     */
    OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_DIFF_INT = 40,

    /**
     * @brief maxSpeckleSize for depth noise removal filter
     */
    OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_SPECKLE_SIZE_INT = 41,

    /**
     * @brief Hardware d2c is on
     */
    OB_PROP_DEPTH_ALIGN_HARDWARE_BOOL = 42,

    /**
     * @brief Timestamp adjustment
     */
    OB_PROP_TIMESTAMP_OFFSET_INT = 43,

    /**
     * @brief Hardware distortion switch Rectify
     */
    OB_PROP_HARDWARE_DISTORTION_SWITCH_BOOL = 61,

    /**
     * @brief Fan mode switch
     */
    OB_PROP_FAN_WORK_MODE_INT = 62,

    /**
     * @brief Multi-resolution D2C mode
     */
    OB_PROP_DEPTH_ALIGN_HARDWARE_MODE_INT = 63,

    /**
     * @brief Anti_collusion activation status
     */
    OB_PROP_ANTI_COLLUSION_ACTIVATION_STATUS_BOOL = 64,

    /**
     * @brief the depth precision level, which may change the depth frame data unit, needs to be confirmed through the ValueScale interface of
     * DepthFrame
     */
    OB_PROP_DEPTH_PRECISION_LEVEL_INT = 75,

    /**
     * @brief tof filter range configuration
     */
    OB_PROP_TOF_FILTER_RANGE_INT = 76,

    /**
     * @brief laser mode, the firmware terminal currently only return 1: IR Drive, 2: Torch
     */
    OB_PROP_LASER_MODE_INT = 79,

    /**
     * @brief brt2r-rectify function switch (brt2r is a special module on mx6600), 0: Disable, 1: Rectify Enable
     */
    OB_PROP_RECTIFY2_BOOL = 80,

    /**
     * @brief Color mirror
     */
    OB_PROP_COLOR_MIRROR_BOOL = 81,

    /**
     * @brief Color flip
     */
    OB_PROP_COLOR_FLIP_BOOL = 82,

    /**
     * @brief Indicator switch, 0: Disable, 1: Enable
     */
    OB_PROP_INDICATOR_LIGHT_BOOL = 83,

    /**
     * @brief Disparity to depth switch, false: switch to software disparity convert to depth, true: switch to hardware disparity convert to depth
     */
    OB_PROP_DISPARITY_TO_DEPTH_BOOL = 85,

    /**
     * @brief BRT function switch (anti-background interference), 0: Disable, 1: Enable
     */
    OB_PROP_BRT_BOOL = 86,

    /**
     * @brief Watchdog function switch, 0: Disable, 1: Enable
     */
    OB_PROP_WATCHDOG_BOOL = 87,

    /**
     * @brief External signal trigger restart function switch, 0: Disable, 1: Enable
     */
    OB_PROP_EXTERNAL_SIGNAL_RESET_BOOL = 88,

    /**
     * @brief Heartbeat monitoring function switch, 0: Disable, 1: Enable
     */
    OB_PROP_HEARTBEAT_BOOL = 89,

    /**
     * @brief Depth cropping mode device: OB_DEPTH_CROPPING_MODE
     */
    OB_PROP_DEPTH_CROPPING_MODE_INT = 90,

    /**
     * @brief D2C preprocessing switch (such as RGB cropping), 0: off, 1: on
     */
    OB_PROP_D2C_PREPROCESS_BOOL = 91,

    /**
     * @brief Enable/disable GPM function
     */
    OB_PROP_GPM_BOOL = 93,

    /**
     * @brief Custom RGB cropping switch, 0 is off, 1 is on custom cropping, and the ROI cropping area is issued
     */
    OB_PROP_RGB_CUSTOM_CROP_BOOL = 94,

    /**
     * @brief Device operating mode (power consumption)
     */
    OB_PROP_DEVICE_WORK_MODE_INT = 95,

    /**
     * @brief Device communication type, 0: USB; 1: Ethernet(RTSP)
     */
    OB_PROP_DEVICE_COMMUNICATION_TYPE_INT = 97,

    /**
     * @brief Switch infrared imaging mode, 0: active IR mode, 1: passive IR mode
     */
    OB_PROP_SWITCH_IR_MODE_INT = 98,

    /**
     * @brief Laser power level
     */
    OB_PROP_LASER_POWER_LEVEL_CONTROL_INT = 99,

    /**
     * @brief LDP's measure distance, unit: mm
     */
    OB_PROP_LDP_MEASURE_DISTANCE_INT = 100,

    /**
     * @brief Reset device time to zero
     */
    OB_PROP_TIMER_RESET_SIGNAL_BOOL = 104,

    /**
     * @brief Enable send reset device time signal to other device. true: enable, false: disable
     */
    OB_PROP_TIMER_RESET_TRIGGER_OUT_ENABLE_BOOL = 105,

    /**
     * @brief Delay to reset device time, unit: us
     */
    OB_PROP_TIMER_RESET_DELAY_US_INT = 106,

    /**
     * @brief Signal to capture image
     */
    OB_PROP_CAPTURE_IMAGE_SIGNAL_BOOL = 107,

    /**
     * @brief Right IR sensor mirror state
     */
    OB_PROP_IR_RIGHT_MIRROR_BOOL = 112,

    /**
     * @brief Number frame to capture once a 'OB_PROP_CAPTURE_IMAGE_SIGNAL_BOOL' effect. range: [1, 255]
     */
    OB_PROP_CAPTURE_IMAGE_FRAME_NUMBER_INT = 113,

    /**
     * @brief Right IR sensor flip state. true: flip image, false: origin, default: false
     */
    OB_PROP_IR_RIGHT_FLIP_BOOL = 114,

    /**
     * @brief Color sensor rotation, angle{0, 90, 180, 270}
     */
    OB_PROP_COLOR_ROTATE_INT = 115,

    /**
     * @brief IR/Left-IR sensor rotation, angle{0, 90, 180, 270}
     */
    OB_PROP_IR_ROTATE_INT = 116,

    /**
     * @brief Right IR sensor rotation, angle{0, 90, 180, 270}
     */
    OB_PROP_IR_RIGHT_ROTATE_INT = 117,

    /**
     * @brief Depth sensor rotation, angle{0, 90, 180, 270}
     */
    OB_PROP_DEPTH_ROTATE_INT = 118,

    /**
     * @brief Get hardware laser power actual level which real state of laser element. OB_PROP_LASER_POWER_LEVEL_CONTROL_INT99 will effect this command
     * which it setting and changed the hardware laser energy level.
     */
    OB_PROP_LASER_POWER_ACTUAL_LEVEL_INT = 119,

    /**
     * @brief USB's power state, enum type: OBUSBPowerState
     */
    OB_PROP_USB_POWER_STATE_INT = 121,

    /**
     * @brief DC's power state, enum type: OBDCPowerState
     */
    OB_PROP_DC_POWER_STATE_INT = 122,

    /**
     * @brief Device development mode switch, optional modes can refer to the definition in @ref OBDeviceDevelopmentMode,the default mode is
     * @ref OB_USER_MODE
     * @attention The device takes effect after rebooting when switching modes.
     */
    OB_PROP_DEVICE_DEVELOPMENT_MODE_INT = 129,

    /**
     * @brief Multi-DeviceSync synchronized signal trigger out is enable state. true: enable, false: disable
     */
    OB_PROP_SYNC_SIGNAL_TRIGGER_OUT_BOOL = 130,

    /**
     * @brief Restore factory settings and factory parameters
     * @attention This command can only be written, and the parameter value must be true. The command takes effect after restarting the device.
     */
    OB_PROP_RESTORE_FACTORY_SETTINGS_BOOL = 131,

    /**
     * @brief Enter recovery mode (flashing mode) when boot the device
     * @attention The device will take effect after rebooting with the enable option. After entering recovery mode, you can upgrade the device system. Upgrading
     * the system may cause system damage, please use it with caution.
     */
    OB_PROP_BOOT_INTO_RECOVERY_MODE_BOOL = 132,

    /**
     * @brief Query whether the current device is running in recovery mode (read-only)
     */
    OB_PROP_DEVICE_IN_RECOVERY_MODE_BOOL = 133,

    /**
     * @brief Capture interval mode, 0:time interval, 1:number interval
     */
    OB_PROP_CAPTURE_INTERVAL_MODE_INT = 134,

    /**
     * @brief Capture time interval
     */
    OB_PROP_CAPTURE_IMAGE_TIME_INTERVAL_INT = 135,

    /**
     * @brief Capture number interval
     */
    OB_PROP_CAPTURE_IMAGE_NUMBER_INTERVAL_INT = 136,

    /*
     * @brief Timer reset function enable
     */
    OB_PROP_TIMER_RESET_ENABLE_BOOL = 140,

    /**
     * @brief Enable or disable the device to retry USB2.0 re-identification when the device is connected to a USB2.0 port.
     * @brief This feature ensures that the device is not mistakenly identified as a USB 2.0 device when connected to a USB 3.0 port.
     */
    OB_PROP_DEVICE_USB2_REPEAT_IDENTIFY_BOOL = 141,

    /**
     * @brief Reboot device delay mode. Delay time unit: ms, range: [0, 8000).
     */
    OB_PROP_DEVICE_REBOOT_DELAY_INT = 142,

    /**
     * @brief Query the status of laser overcurrent protection (read-only)
     */
    OB_PROP_LASER_OVERCURRENT_PROTECTION_STATUS_BOOL = 148,

    /**
     * @brief Query the status of laser pulse width protection (read-only)
     */
    OB_PROP_LASER_PULSE_WIDTH_PROTECTION_STATUS_BOOL = 149,

    /**
     * @brief Laser always on, true: always on, false: off, laser will be turned off when out of exposure time
     */
    OB_PROP_LASER_ALWAYS_ON_BOOL = 174,

    /**
     * @brief Laser on/off alternate mode, 0: off, 1: on-off alternate, 2: off-on alternate
     * @attention When turn on this mode, the laser will turn on and turn off alternately each frame.
     */
    OB_PROP_LASER_ON_OFF_PATTERN_INT = 175,

    /**
     * @brief Depth unit flexible adjustment\
     * @brief This property allows continuous adjustment of the depth unit, unlike @ref OB_PROP_DEPTH_PRECISION_LEVEL_INT must be set to some fixed value.
     */
    OB_PROP_DEPTH_UNIT_FLEXIBLE_ADJUSTMENT_FLOAT = 176,

    /**
     * @brief Laser control, 0: off, 1: on, 2: auto
     *
     */
    OB_PROP_LASER_CONTROL_INT = 182,

    /**
     * @brief IR brightness
     */
    OB_PROP_IR_BRIGHTNESS_INT = 184,

    /**
     * @brief Slave/secondary device synchronization status (read-only)
     */
    OB_PROP_SLAVE_DEVICE_SYNC_STATUS_BOOL = 188,

    /**
     * @brief Color AE max exposure
     */
    OB_PROP_COLOR_AE_MAX_EXPOSURE_INT = 189,

    /**
     * @brief Max exposure time of IR auto exposure
     */
    OB_PROP_IR_AE_MAX_EXPOSURE_INT = 190,

    /**
     * @brief Disparity search range mode, 1: 128, 2: 256
     */
    OB_PROP_DISP_SEARCH_RANGE_MODE_INT = 191,

    /**
     * @brief Laser high temperature protection
     */
    OB_PROP_LASER_HIGH_TEMPERATURE_PROTECT_BOOL = 193,

    /**
     * @brief low exposure laser control
     *
     * @brief Currently using for DabaiA device,if the exposure value is lower than a certain threshold, the laser is turned off;
     * if it exceeds another threshold, the laser is turned on again.
     */
    OB_PROP_LOW_EXPOSURE_LASER_CONTROL_BOOL = 194,

    /**
     * @brief check pps sync in signal
     */
    OB_PROP_CHECK_PPS_SYNC_IN_SIGNAL_BOOL = 195,

    /**
     * @brief Disparity search range offset, range: [0, 127]
     */
    OB_PROP_DISP_SEARCH_OFFSET_INT = 196,

    /**
     * @brief Repower device (cut off power and power on again)
     *
     * @brief Currently using for GMSL device, cut off power and power on again by GMSL host driver.
     */
    OB_PROP_DEVICE_REPOWER_BOOL = 202,

    /**
     * @brief frame interleave config index
     */
    OB_PROP_FRAME_INTERLEAVE_CONFIG_INDEX_INT = 204,

    /**
     * @brief frame interleave enable (true:enable,false:disable)
     */
    OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL = 205,
    /**
     * @brief laser pattern sync with delay(us)
     */
    OB_PROP_FRAME_INTERLEAVE_LASER_PATTERN_SYNC_DELAY_INT = 206,
    /**
     * @brief Get the health check result from device,range is [0.0f,1.5f]
     */
    OB_PROP_ON_CHIP_CALIBRATION_HEALTH_CHECK_FLOAT = 209,

    /**
     * @brief Enable or disable on-chip calibration
     */
    OB_PROP_ON_CHIP_CALIBRATION_ENABLE_BOOL = 210,

    /**
     * @brief hardware noise remove filter switch
     */
    OB_PROP_HW_NOISE_REMOVE_FILTER_ENABLE_BOOL = 211,
    /**
     * @brief hardware noise remove filter threshold ,range [0.0 - 1.0]
     */
    OB_PROP_HW_NOISE_REMOVE_FILTER_THRESHOLD_FLOAT = 212,
    /**
     * @brief soft trigger auto capture enable, use in OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING mode
     */
    OB_DEVICE_AUTO_CAPTURE_ENABLE_BOOL = 216,
    /**
     * @brief soft trigger auto capture interval time, use in OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING mode
     */
    OB_DEVICE_AUTO_CAPTURE_INTERVAL_TIME_INT = 217,

    /**
     * @brief PTP time synchronization enable
     */
    OB_DEVICE_PTP_CLOCK_SYNC_ENABLE_BOOL = 223,

    /**
     * @brief Depth with confidence stream enable
     */
    OB_PROP_DEPTH_WITH_CONFIDENCE_STREAM_ENABLE_BOOL = 224,

    /**
     * @brief Enable or disable confidence stream filter
     */
    OB_PROP_CONFIDENCE_STREAM_FILTER_BOOL = 226,

    /**
     * @brief Confidence stream filter threshold, range [0, 255]
     */
    OB_PROP_CONFIDENCE_STREAM_FILTER_THRESHOLD_INT = 227,

    /**
     * @brief Confidence stream mirror enable
     */
    OB_PROP_CONFIDENCE_MIRROR_BOOL = 229,

    /**
     * @brief Confidence stream flip enable
     */
    OB_PROP_CONFIDENCE_FLIP_BOOL   = 230,

    /**
     * @brief Confidence stream rotate angle{0, 90, 180, 270}
     */
    OB_PROP_CONFIDENCE_ROTATE_INT  = 231,

    /**
     * @brief Baseline calibration parameters
     */
    OB_STRUCT_BASELINE_CALIBRATION_PARAM = 1002,

    /**
     * @brief Device temperature information
     */
    OB_STRUCT_DEVICE_TEMPERATURE = 1003,

    /**
     * @brief TOF exposure threshold range
     */
    OB_STRUCT_TOF_EXPOSURE_THRESHOLD_CONTROL = 1024,

    /**
     * @brief get/set serial number
     */
    OB_STRUCT_DEVICE_SERIAL_NUMBER = 1035,

    /**
     * @brief get/set device time
     */
    OB_STRUCT_DEVICE_TIME = 1037,

    /**
     * @brief Multi-device synchronization mode and parameter configuration
     */
    OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG = 1038,

    /**
     * @brief RGB cropping ROI
     */
    OB_STRUCT_RGB_CROP_ROI = 1040,

    /**
     * @brief Device IP address configuration
     */
    OB_STRUCT_DEVICE_IP_ADDR_CONFIG = 1041,

    /**
     * @brief The current camera depth mode
     */
    OB_STRUCT_CURRENT_DEPTH_ALG_MODE = 1043,

    /**
     * @brief A list of depth accuracy levels, returning an array of uin16_t, corresponding to the enumeration
     */
    OB_STRUCT_DEPTH_PRECISION_SUPPORT_LIST = 1045,

    /**
     * @brief Device network static ip config record
     * @brief Using for get last static ip config, witch is record in device flash when user set static ip config
     *
     * @attention read only
     */
    OB_STRUCT_DEVICE_STATIC_IP_CONFIG_RECORD = 1053,

    /**
     * @brief Using to configure the depth sensor's HDR mode
     * @brief The Value type is @ref OBHdrConfig
     *
     * @attention After enable HDR mode, the depth sensor auto exposure will be disabled.
     */
    OB_STRUCT_DEPTH_HDR_CONFIG = 1059,

    /**
     * @brief Color Sensor AE ROI configuration
     * @brief The Value type is @ref OBRegionOfInterest
     */
    OB_STRUCT_COLOR_AE_ROI = 1060,

    /**
     * @brief Depth Sensor AE ROI configuration
     * @brief The Value type is @ref OBRegionOfInterest
     * @brief Since the ir sensor is the same physical sensor as the depth sensor, this property will also effect the ir sensor.
     */
    OB_STRUCT_DEPTH_AE_ROI = 1061,

    /**
     * @brief ASIC serial number
     */
    OB_STRUCT_ASIC_SERIAL_NUMBER = 1063,

    /**
     * @brief Disparity offset interleaving
     */
    OB_STRUCT_DISP_OFFSET_CONFIG = 1064,

    /**
     * @brief Preset resolution ratio configuration
     */
    OB_STRUCT_PRESET_RESOLUTION_CONFIG = 1069,

    /**
     * @brief Color camera auto exposure
     */
    OB_PROP_COLOR_AUTO_EXPOSURE_BOOL = 2000,

    /**
     * @brief Color camera exposure adjustment
     */
    OB_PROP_COLOR_EXPOSURE_INT = 2001,

    /**
     * @brief Color camera gain adjustment
     */
    OB_PROP_COLOR_GAIN_INT = 2002,

    /**
     * @brief Color camera automatic white balance
     */
    OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL = 2003,

    /**
     * @brief Color camera white balance adjustment
     */
    OB_PROP_COLOR_WHITE_BALANCE_INT = 2004,

    /**
     * @brief Color camera brightness adjustment
     */
    OB_PROP_COLOR_BRIGHTNESS_INT = 2005,

    /**
     * @brief Color camera sharpness adjustment
     */
    OB_PROP_COLOR_SHARPNESS_INT = 2006,

    /**
     * @brief Color camera shutter adjustment
     */
    OB_PROP_COLOR_SHUTTER_INT = 2007,

    /**
     * @brief Color camera saturation adjustment
     */
    OB_PROP_COLOR_SATURATION_INT = 2008,

    /**
     * @brief Color camera contrast adjustment
     */
    OB_PROP_COLOR_CONTRAST_INT = 2009,

    /**
     * @brief Color camera gamma adjustment
     */
    OB_PROP_COLOR_GAMMA_INT = 2010,

    /**
     * @brief Color camera image rotation
     */
    OB_PROP_COLOR_ROLL_INT = 2011,

    /**
     * @brief Color camera auto exposure priority
     */
    OB_PROP_COLOR_AUTO_EXPOSURE_PRIORITY_INT = 2012,

    /**
     * @brief Color camera brightness compensation
     */
    OB_PROP_COLOR_BACKLIGHT_COMPENSATION_INT = 2013,

    /**
     * @brief Color camera color tint
     */
    OB_PROP_COLOR_HUE_INT = 2014,

    /**
     * @brief Color Camera Power Line Frequency
     */
    OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT = 2015,

    /**
     * @brief Automatic exposure of depth camera (infrared camera will be set synchronously under some models of devices)
     */
    OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL = 2016,

    /**
     * @brief Depth camera exposure adjustment (infrared cameras will be set synchronously under some models of devices)
     */
    OB_PROP_DEPTH_EXPOSURE_INT = 2017,

    /**
     * @brief Depth camera gain adjustment (infrared cameras will be set synchronously under some models of devices)
     */
    OB_PROP_DEPTH_GAIN_INT = 2018,

    /**
     * @brief Infrared camera auto exposure (depth camera will be set synchronously under some models of devices)
     */
    OB_PROP_IR_AUTO_EXPOSURE_BOOL = 2025,

    /**
     * @brief Infrared camera exposure adjustment (some models of devices will set the depth camera synchronously)
     */
    OB_PROP_IR_EXPOSURE_INT = 2026,

    /**
     * @brief Infrared camera gain adjustment (the depth camera will be set synchronously under some models of devices)
     */
    OB_PROP_IR_GAIN_INT = 2027,

    /**
     * @brief Select Infrared camera data source channel. If not support throw exception. 0 : IR stream from IR Left sensor; 1 : IR stream from IR Right sensor;
     */
    OB_PROP_IR_CHANNEL_DATA_SOURCE_INT = 2028,

    /**
     * @brief Depth effect dedistortion, true: on, false: off. mutually exclusive with D2C function, RM_Filter disable When hardware or software D2C is enabled.
     */
    OB_PROP_DEPTH_RM_FILTER_BOOL = 2029,

    /**
     * @brief Color camera maximal gain
     */
    OB_PROP_COLOR_MAXIMAL_GAIN_INT = 2030,

    /**
     * @brief Color camera shutter gain
     */
    OB_PROP_COLOR_MAXIMAL_SHUTTER_INT = 2031,

    /**
     * @brief The enable/disable switch for IR short exposure function, supported only by a few devices.
     */
    OB_PROP_IR_SHORT_EXPOSURE_BOOL = 2032,

    /**
     * @brief Color camera HDR
     */
    OB_PROP_COLOR_HDR_BOOL = 2034,

    /**
     * @brief IR long exposure mode switch read and write.
     */
    OB_PROP_IR_LONG_EXPOSURE_BOOL = 2035,

    /**
     * @brief Setting and getting the USB device frame skipping mode status, true: frame skipping mode, false: non-frame skipping mode.
     */
    OB_PROP_SKIP_FRAME_BOOL = 2036,

    /**
     * @brief Depth HDR merge, true: on, false: off.
     */
    OB_PROP_HDR_MERGE_BOOL = 2037,

    /**
     * @brief Color camera FOCUS
     */
    OB_PROP_COLOR_FOCUS_INT = 2038,
    /**
     * @brief ir rectify status,true: ir rectify, false: no rectify
     */
    OB_PROP_IR_RECTIFY_BOOL = 2040,

    /**
     * @brief Depth camera priority
     *
     */
    OB_PROP_DEPTH_AUTO_EXPOSURE_PRIORITY_INT = 2052,

    /**
     * @brief Software disparity to depth
     */
    OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL = 3004,

    /**
     * @brief Depth data unpacking function switch (each open stream will be turned on by default, support RLE/Y10/Y11/Y12/Y14 format)
     */
    OB_PROP_SDK_DEPTH_FRAME_UNPACK_BOOL = 3007,

    /**
     * @brief IR data unpacking function switch (each current will be turned on by default, support RLE/Y10/Y11/Y12/Y14 format)
     */
    OB_PROP_SDK_IR_FRAME_UNPACK_BOOL = 3008,

    /**
     * @brief Accel data conversion function switch (on by default)
     */
    OB_PROP_SDK_ACCEL_FRAME_TRANSFORMED_BOOL = 3009,

    /**
     * @brief Gyro data conversion function switch (on by default)
     */
    OB_PROP_SDK_GYRO_FRAME_TRANSFORMED_BOOL = 3010,

    /**
     * @brief Left IR frame data unpacking function switch (each current will be turned on by default, support RLE/Y10/Y11/Y12/Y14 format)
     */
    OB_PROP_SDK_IR_LEFT_FRAME_UNPACK_BOOL = 3011,

    /**
     * @brief Right IR frame data unpacking function switch (each current will be turned on by default, support RLE/Y10/Y11/Y12/Y14 format)
     */
    OB_PROP_SDK_IR_RIGHT_FRAME_UNPACK_BOOL = 3012,

    /**
     * @brief Read the current network bandwidth type of the network device, whether it is Gigabit Ethernet or Fast Ethernet, such as G335LE.
     */
    OB_PROP_NETWORK_BANDWIDTH_TYPE_INT = 3027,

    /**
     * @brief Switch device performance mode, currently available in Adaptive Mode and High Performance Mode, such as G335LE.
     */
    OB_PROP_DEVICE_PERFORMANCE_MODE_INT = 3028,

    /**
     * @brief Calibration JSON file read from device (Femto Mega, read only)
     */
    OB_RAW_DATA_CAMERA_CALIB_JSON_FILE = 4029,

    /**
     * @brief Confidence degree
     */
    OB_PROP_DEBUG_ESGM_CONFIDENCE_FLOAT = 5013,
} OBPropertyID,
    ob_property_id;

// For backward compatibility
#define OB_PROP_TIMER_RESET_TRIGGLE_OUT_ENABLE_BOOL OB_PROP_TIMER_RESET_TRIGGER_OUT_ENABLE_BOOL
#define OB_PROP_LASER_ON_OFF_MODE_INT OB_PROP_LASER_ON_OFF_PATTERN_INT
#define OB_PROP_LASER_ENERGY_LEVEL_INT OB_PROP_LASER_POWER_LEVEL_CONTROL_INT
#define OB_PROP_LASER_HW_ENERGY_LEVEL_INT OB_PROP_LASER_POWER_ACTUAL_LEVEL_INT
#define OB_PROP_DEVICE_USB3_REPEAT_IDENTIFY_BOOL OB_PROP_DEVICE_USB2_REPEAT_IDENTIFY_BOOL
#define OB_PROP_DEPTH_SOFT_FILTER_BOOL OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_BOOL
#define OB_PROP_DEPTH_MAX_DIFF_INT OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_DIFF_INT
#define OB_PROP_DEPTH_MAX_SPECKLE_SIZE_INT OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_SPECKLE_SIZE_INT

/**
 * @brief The data type used to describe all property settings
 */
typedef enum OBPropertyType {
    OB_BOOL_PROPERTY   = 0, /**< Boolean property */
    OB_INT_PROPERTY    = 1, /**< Integer property */
    OB_FLOAT_PROPERTY  = 2, /**< Floating-point property */
    OB_STRUCT_PROPERTY = 3, /**< Struct property */
} OBPropertyType,
    ob_property_type;

/**
 * @brief Used to describe the characteristics of each property
 */
typedef struct OBPropertyItem {
    OBPropertyID     id;         /**< Property ID */
    const char      *name;       /**< Property name */
    OBPropertyType   type;       /**< Property type */
    OBPermissionType permission; /**< Property read and write permission */
} OBPropertyItem, ob_property_item;

#ifdef __cplusplus
}
#endif
