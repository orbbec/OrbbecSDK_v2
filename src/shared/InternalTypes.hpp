// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <stdint.h>
#include <stdbool.h>

#pragma pack(push, 1)

/**
 *@brief device version information
 */
typedef struct {
    char    firmwareVersion[16];   ///< Such as: 1.2.18
    char    hardwareVersion[16];   ///< Such as: 1.0.18
    char    sdkVersion[16];        ///< The lowest supported SDK version number, SDK version number: 2.3.2 (major.minor.revision)
    char    depthChip[16];         ///< Such as：mx6000, mx6600
    char    systemChip[16];        ///< Such as：ar9201
    char    serialNumber[16];      ///< serial number
    int32_t deviceType;            ///< 1:Monocular 2:binocular 3:tof. enumeration value
    char    deviceName[16];        ///< device name，such as: astra+
    char    subSystemVersion[16];  ///< For example，such as：Femto’s MCU firmware version 1.0.23
    char    reserved[32];          ///< Reserved
} OBVersionInfo;

/**
 *@brief device time
 */
typedef struct {
    uint64_t time;  ///< sdk->dev: timing time; dev->sdk: current time of device;
    uint64_t rtt;   ///< sdk->dev: command round-trip time, the device sets the time to time+rtt/2 after receiving it; dev->sdk: reserved; unit: ms
} OBDeviceTime, ob_device_time;

/**
 *@brief Post-process parameters after depth align to color
 *
 */
typedef struct {
    float   depthScale;   // Depth frame scaling ratio
    int16_t alignLeft;    // Depth aligned to left after Color
    int16_t alignTop;     // Depth aligned to the top after Color
    int16_t alignRight;   // Depth aligned to right after Color
    int16_t alignBottom;  // Depth aligned to the bottom after Color
} OBD2CPostProcessParam, ob_d2c_post_process_param;

typedef enum {
    ALIGN_UNSUPPORTED    = 0,
    ALIGN_D2C_HW         = 1,
    ALIGN_D2C_SW         = 2,
    ALIGN_D2C_HW_SW_BOTH = 3,
} OBAlignSupportedType,
    ob_align_supported_type;

/**
 *@brief Supported depth align color profile
 *
 */
typedef struct {
    uint16_t              colorWidth;
    uint16_t              colorHeight;
    uint16_t              depthWidth;
    uint16_t              depthHeight;
    uint8_t               alignType;
    uint8_t               paramIndex;
    OBD2CPostProcessParam postProcessParam;
} OBD2CProfile, ob_d2c_supported_profile_info;

typedef struct {
    float   colorScale;
    int16_t alignLeft;
    int16_t alignTop;
    int16_t alignRight;
    int16_t alignBottom;
} OBD2CPreProcessParam, ob_d2c_pre_process_param;

typedef struct {
    uint32_t             reserved;
    OBD2CPreProcessParam preProcessParam;
} OBD2CColorPreProcessProfile;

typedef struct {
    uint32_t depthMode;    ///< Monocular/Binocular
    float    baseline;     ///< baseline distance
    float    z0;           ///< Calibration distance
    float    focalPix;     ///< Focal length used for depth calculation or focal length f after rectify
    float    unit;         ///< Unit x1 mm, such as: unit=0.25, means 0.25*1mm=0.25mm
    float    dispOffset;   ///< Parallax offset, real parallax = chip output parallax + disp_offset
    int32_t  invalidDisp;  ///< Invalid parallax, usually 0; when the chip min_disp is not equal to 0 or -128, the invalid parallax is no longer equal to 0
} OBDepthCalibrationParam;

typedef enum {
    OB_DISP_PACK_ORIGINAL     = 0,  // MX6000 Parallax
    OB_DISP_PACK_OPENNI       = 1,  // OpenNI disparity
    OB_DISP_PACK_ORIGINAL_NEW = 2,  // MX6600 Parallax
    OB_DISP_PACK_GEMINI2XL    = 3,  // Gemini2XL parallax
} OBDisparityPackMode;

/**
 *@brief Structure for distortion parameters
 */
typedef struct {
    float k1;  ///< Radial distortion factor 1
    float k2;  ///< Radial distortion factor 2
    float k3;  ///< Radial distortion factor 3
    float k4;  ///< Radial distortion factor 4
    float k5;  ///< Radial distortion factor 5
    float k6;  ///< Radial distortion factor 6
    float p1;  ///< Tangential distortion factor 1
    float p2;  ///< Tangential distortion factor 2
} OBCameraDistortion_Internal;

typedef struct {
    OBCameraIntrinsic           depthIntrinsic;   ///< Depth camera internal parameters
    OBCameraIntrinsic           rgbIntrinsic;     ///< Color camera internal parameters
    OBCameraDistortion_Internal depthDistortion;  ///< Depth camera distortion parameters

    OBCameraDistortion_Internal rgbDistortion;  ///< Distortion parameters for color camera
    OBD2CTransform              transform;      ///< Rotation/transformation matrix
} OBCameraParam_Internal_V0;

typedef struct {
    uint8_t  checksum[16];  ///< The camera depth mode corresponds to the hash binary array
    char     name[32];      ///< name
    uint32_t optionCode;    // OBDepthModeOptionCode
} OBDepthWorkMode_Internal;

typedef enum {
    NORMAL                             = 0,           // Normal mode, no special processing required
    MX6600_RIGHT_IR_FROM_DEPTH_CHANNEL = 2,           // Gemini2 calibration mode, right IR data goes through the depth channel
    RIGHT_IR_NO_FROM_DEPTH_CHANNEL     = 4,           // Gemini2XL, right IR goes to the right IR channel
    INVALID                            = 0xffffffff,  // Invalid value
} OBDepthModeOptionCode;

// Orbbec Magnetometer model
typedef struct {
    double referenceTemp;    ///< Reference temperature
    double tempSlope[9];     ///< Temperature slope (linear thermal drift coefficient)
    double misalignment[9];  ///< Misalignment matrix
    double softIron[9];      ///< Soft iron effect matrix
    double scale[3];         ///< Scale vector
    double hardIron[3];      ///< Hard iron bias
} OBMagnetometerIntrinsic;

// Single IMU parameters
typedef struct {
    char                    name[12];                   /// ＜ imu name
    uint16_t                version;                    ///< IMU calibration library version number
    uint16_t                imuModel;                   ///< IMU model
    double                  body_to_gyroscope[9];       ///< Rotation from body coordinate system to gyroscope coordinate system
    double                  acc_to_gyro_factor[9];      ///< Influence factor of accelerometer measurements on gyroscope measurements
    OBAccelIntrinsic        acc;                        ///< Accelerometer model
    OBGyroIntrinsic         gyro;                       ///< Gyroscope model
    OBMagnetometerIntrinsic mag;                        ///< Magnetometer model
    double                  timeshift_cam_to_imu;       ///< Time offset between camera and IMU
    double                  imu_to_cam_extrinsics[16];  ///< Extrinsic parameters from IMU to Cam(Depth)
} OBSingleIMUParams;

// IMU Calibration Parameters
typedef struct {
    uint8_t           validNum;            ///< Number of valid IMUs
    OBSingleIMUParams singleIMUParams[3];  ///< Array of single IMU parameter models
} OBIMUCalibrateParams;

/**
 *@brief List of resolutions supported by the device in the current camera depth mode
 *
 */
typedef struct {
    OBSensorType sensorType;  ///< sensor type
    OBFormat     format;      ///< Image format
    uint32_t     width;       ///< image width
    uint32_t     height;      ///< Image height
    uint32_t     maxFps;      ///< Maximum supported frame rate
} OBEffectiveStreamProfile, ob_effective_stream_profile;

typedef struct {
    uint16_t sensorType;  // enum value of OBSensorType
    union Profile {
        struct Video {  // This structure is used for Color, IR, and Depth
            uint32_t width;
            uint32_t height;
            uint32_t fps;
            uint32_t formatFourcc;  // Example: {'Y', 'U', 'Y', 'V'} // fourcc is a common concept in UVC
        } video;
        struct Accel {                // This structure is used for Accel
            uint16_t fullScaleRange;  // enum value of OBAccelFullScaleRange
            uint16_t sampleRate;      // enum value of OBAccelSampleRate
        } accel;
        struct Gyro {                 // This structure is used for Gyro
            uint16_t fullScaleRange;  // enum value of OBGyroFullScaleRange
            uint16_t sampleRate;      // enum value of OBGyroSampleRate
        } gyro;
    } profile;
} OBInternalStreamProfile;

#pragma pack(pop)
