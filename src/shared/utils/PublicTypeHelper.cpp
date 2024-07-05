#include "PublicTypeHelper.hpp"
#include "exception/ObException.hpp"

#include <map>
#include <algorithm>

namespace libobsensor {
namespace utils {

float getBytesPerPixel(OBFormat format) {
    float bytesPerPixel = 0.f;
    switch(format) {
    case OB_FORMAT_Y8:
    case OB_FORMAT_BA81:
        bytesPerPixel = 1.f;
        break;
    case OB_FORMAT_Y10:
        bytesPerPixel = 10.f / 8.f;
        break;
    case OB_FORMAT_Y11:
        bytesPerPixel = 11.f / 8.f;
        break;
    case OB_FORMAT_Y12:
    case OB_FORMAT_NV12:
    case OB_FORMAT_YV12:
        bytesPerPixel = 12.f / 8.f;
        break;
    case OB_FORMAT_Y14:
        bytesPerPixel = 14.f / 8.f;
        break;
    case OB_FORMAT_NV21:
    case OB_FORMAT_I420:
        bytesPerPixel = 1.5f;
        break;
    case OB_FORMAT_Y16:
    case OB_FORMAT_Z16:
    case OB_FORMAT_YUYV:
    case OB_FORMAT_UYVY:
    case OB_FORMAT_BYR2:
    case OB_FORMAT_RW16:
    case OB_FORMAT_DISP16:
        bytesPerPixel = 2.f;
        break;
    case OB_FORMAT_RGB:
    case OB_FORMAT_BGR:
        bytesPerPixel = 3.f;
        break;
    case OB_FORMAT_RGBA:
    case OB_FORMAT_BGRA:
        bytesPerPixel = 4.f;
        break;
    case OB_FORMAT_POINT:
        bytesPerPixel = 12.f;
        break;
    case OB_FORMAT_RGB_POINT:
        bytesPerPixel = 24.f;
        break;
    default:
        throw invalid_value_exception("Unsupported image format or invalid encoding detected. Unable to determine the byte-per-pixel value.");
        break;
    }

    return bytesPerPixel;
}

uint32_t calcDefaultStrideBytes(OBFormat format, uint32_t width) {
    float    bytesPerPixel = getBytesPerPixel(format);
    uint32_t stride        = (uint32_t)(width * bytesPerPixel + 0.5);
    return stride;
}

uint32_t calcVideoFrameMaxDataSize(OBFormat format, uint32_t width, uint32_t height) {
    uint32_t maxFrameDataSize = height * width * 3;
    switch(format) {
    case OB_FORMAT_MJPG:
    case OB_FORMAT_H264:
    case OB_FORMAT_H265:
    case OB_FORMAT_HEVC:
        maxFrameDataSize = height * width;
        break;
    case OB_FORMAT_RLE:
    case OB_FORMAT_RVL:
        maxFrameDataSize = height * width * 2;
        break;
    default:  // assume planar format
        maxFrameDataSize = height * calcDefaultStrideBytes(format, width);
        break;
    }
    return maxFrameDataSize;
}

OBFrameType mapStreamTypeToFrameType(OBStreamType type) {
    switch(type) {
    case OB_STREAM_VIDEO:
        return OB_FRAME_VIDEO;
    case OB_STREAM_IR:
        return OB_FRAME_IR;
    case OB_STREAM_COLOR:
        return OB_FRAME_COLOR;
    case OB_STREAM_DEPTH:
        return OB_FRAME_DEPTH;
    case OB_STREAM_ACCEL:
        return OB_FRAME_ACCEL;
    case OB_STREAM_GYRO:
        return OB_FRAME_GYRO;
    case OB_STREAM_IR_LEFT:
        return OB_FRAME_IR_LEFT;
    case OB_STREAM_IR_RIGHT:
        return OB_FRAME_IR_RIGHT;
    case OB_STREAM_RAW_PHASE:
        return OB_FRAME_RAW_PHASE;
    default:
        break;
    }
    return OB_FRAME_UNKNOWN;
}

OBStreamType mapFrameTypeToStreamType(OBFrameType type) {
    switch(type) {
    case OB_FRAME_VIDEO:
        return OB_STREAM_VIDEO;
    case OB_FRAME_IR:
        return OB_STREAM_IR;
    case OB_FRAME_COLOR:
        return OB_STREAM_COLOR;
    case OB_FRAME_DEPTH:
        return OB_STREAM_DEPTH;
    case OB_FRAME_ACCEL:
        return OB_STREAM_ACCEL;
    case OB_FRAME_GYRO:
        return OB_STREAM_GYRO;
    case OB_FRAME_IR_LEFT:
        return OB_STREAM_IR_LEFT;
    case OB_FRAME_IR_RIGHT:
        return OB_STREAM_IR_RIGHT;
    case OB_FRAME_RAW_PHASE:
        return OB_STREAM_RAW_PHASE;
    default:
        break;
    }
    return OB_STREAM_UNKNOWN;
}

OBStreamType mapSensorTypeToStreamType(OBSensorType type) {
    switch(type) {
    case OB_SENSOR_IR:
        return OB_STREAM_IR;
    case OB_SENSOR_COLOR:
        return OB_STREAM_COLOR;
    case OB_SENSOR_DEPTH:
        return OB_STREAM_DEPTH;
    case OB_SENSOR_ACCEL:
        return OB_STREAM_ACCEL;
    case OB_SENSOR_GYRO:
        return OB_STREAM_GYRO;
    case OB_SENSOR_IR_LEFT:
        return OB_STREAM_IR_LEFT;
    case OB_SENSOR_IR_RIGHT:
        return OB_STREAM_IR_RIGHT;
    case OB_SENSOR_RAW_PHASE:
        return OB_STREAM_RAW_PHASE;
    default:
        break;
    }
    return OB_STREAM_UNKNOWN;
}

OBSensorType mapStreamTypeToSensorType(OBStreamType type) {
    switch(type) {
    case OB_STREAM_IR:
        return OB_SENSOR_IR;
    case OB_STREAM_COLOR:
        return OB_SENSOR_COLOR;
    case OB_STREAM_DEPTH:
        return OB_SENSOR_DEPTH;
    case OB_STREAM_ACCEL:
        return OB_SENSOR_ACCEL;
    case OB_STREAM_GYRO:
        return OB_SENSOR_GYRO;
    case OB_STREAM_IR_LEFT:
        return OB_SENSOR_IR_LEFT;
    case OB_STREAM_IR_RIGHT:
        return OB_SENSOR_IR_RIGHT;
    case OB_STREAM_RAW_PHASE:
        return OB_SENSOR_RAW_PHASE;
    default:
        break;
    }
    return OB_SENSOR_UNKNOWN;
}

const std::map<uint32_t, OBFormat> fourccToOBFormat = {
    { fourCc2Int('U', 'Y', 'V', 'Y'), OB_FORMAT_UYVY }, { fourCc2Int('Y', 'U', 'Y', '2'), OB_FORMAT_YUYV }, { fourCc2Int('Y', 'U', 'Y', 'V'), OB_FORMAT_YUYV },
    { fourCc2Int('N', 'V', '1', '2'), OB_FORMAT_NV12 }, { fourCc2Int('N', 'V', '2', '1'), OB_FORMAT_NV21 }, { fourCc2Int('M', 'J', 'P', 'G'), OB_FORMAT_MJPG },
    { fourCc2Int('H', '2', '6', '4'), OB_FORMAT_H264 }, { fourCc2Int('H', '2', '6', '5'), OB_FORMAT_H265 }, { fourCc2Int('Y', '1', '2', ' '), OB_FORMAT_Y12 },
    { fourCc2Int('Y', '1', '6', ' '), OB_FORMAT_Y16 },  { fourCc2Int('G', 'R', 'A', 'Y'), OB_FORMAT_GRAY }, { fourCc2Int('Y', '1', '1', ' '), OB_FORMAT_Y11 },
    { fourCc2Int('Y', '8', ' ', ' '), OB_FORMAT_Y8 },   { fourCc2Int('Y', '1', '0', ' '), OB_FORMAT_Y10 },  { fourCc2Int('H', 'E', 'V', 'C'), OB_FORMAT_HEVC },
    { fourCc2Int('Y', '1', '4', ' '), OB_FORMAT_Y14 },  { fourCc2Int('I', '4', '2', '0'), OB_FORMAT_I420 }, { fourCc2Int('Z', '1', '6', ' '), OB_FORMAT_Z16 },
    { fourCc2Int('Y', 'V', '1', '2'), OB_FORMAT_YV12 }, { fourCc2Int('B', 'A', '8', '1'), OB_FORMAT_BA81 }, { fourCc2Int('B', 'Y', 'R', '2'), OB_FORMAT_BYR2 },
    { fourCc2Int('R', 'W', '1', '6'), OB_FORMAT_RW16 },
};

OBFormat uvcFourccToOBFormat(uint32_t fourcc) {
    auto it = fourccToOBFormat.find(fourcc);
    if(it != fourccToOBFormat.end()) {
        return it->second;
    }
    return OB_FORMAT_UNKNOWN;
}

uint32_t obFormatToUvcFourcc(OBFormat format) {
    auto it = std::find_if(fourccToOBFormat.begin(), fourccToOBFormat.end(), [format](const std::pair<uint32_t, OBFormat> &p) { return p.second == format; });
    if(it != fourccToOBFormat.end()) {
        return it->first;
    }
    return 0;
}

float mapIMUSampleRateToValue(OBIMUSampleRate rate) {
    switch(rate) {
    case OB_SAMPLE_RATE_1_5625_HZ:
        return 1.5625f;
    case OB_SAMPLE_RATE_3_125_HZ:
        return 3.125f;
    case OB_SAMPLE_RATE_6_25_HZ:
        return 6.25f;
    case OB_SAMPLE_RATE_12_5_HZ:
        return 12.5f;
    case OB_SAMPLE_RATE_25_HZ:
        return 25.f;
    case OB_SAMPLE_RATE_50_HZ:
        return 50.f;
    case OB_SAMPLE_RATE_100_HZ:
        return 100.f;
    case OB_SAMPLE_RATE_200_HZ:
        return 200.f;
    case OB_SAMPLE_RATE_500_HZ:
        return 500.f;
    case OB_SAMPLE_RATE_1_KHZ:
        return 1000.f;
    case OB_SAMPLE_RATE_2_KHZ:
        return 2000.f;
    case OB_SAMPLE_RATE_4_KHZ:
        return 4000.f;
    case OB_SAMPLE_RATE_8_KHZ:
        return 8000.f;
    case OB_SAMPLE_RATE_16_KHZ:
        return 16000.f;
    case OB_SAMPLE_RATE_32_KHZ:
        return 32000.f;
    default:
        return 0.f;
    }
}

// type to string map
const std::map<OBSensorType, std::string> Sensor_Str_Map = {
    { OB_SENSOR_IR, "IR" },           { OB_SENSOR_COLOR, "Color" },    { OB_SENSOR_DEPTH, "Depth" },      { OB_SENSOR_ACCEL, "Accel" },
    { OB_SENSOR_GYRO, "Gyro" },       { OB_SENSOR_IR_LEFT, "LeftIR" }, { OB_SENSOR_IR_RIGHT, "RightIR" }, { OB_SENSOR_RAW_PHASE, "RAW_PHASE" },
    { OB_SENSOR_UNKNOWN, "Unknown" },
};

const std::map<OBFrameType, std::string> Frame_Str_Map = { { OB_FRAME_UNKNOWN, "Unknown frame type" },
                                                           { OB_FRAME_VIDEO, "Video" },
                                                           { OB_FRAME_IR, "IR" },
                                                           { OB_FRAME_COLOR, "Color" },
                                                           { OB_FRAME_DEPTH, "Depth" },
                                                           { OB_FRAME_ACCEL, "Accel" },
                                                           { OB_FRAME_SET, "SET" },
                                                           { OB_FRAME_POINTS, "Points" },
                                                           { OB_FRAME_GYRO, "Gyro" },
                                                           { OB_FRAME_IR_LEFT, "LeftIR" },
                                                           { OB_FRAME_IR_RIGHT, "RightIR" },
                                                           { OB_FRAME_RAW_PHASE, "RawPhase" } };

const std::map<OBStreamType, std::string> Stream_Str_Map = {
    { OB_STREAM_UNKNOWN, "Unknown" },  { OB_STREAM_VIDEO, "Video" },       { OB_STREAM_IR, "IR" },     { OB_STREAM_COLOR, "color" },
    { OB_STREAM_DEPTH, "depth" },      { OB_STREAM_ACCEL, "Acceler" },     { OB_STREAM_GYRO, "Gyro" }, { OB_STREAM_IR_LEFT, "LeftIR" },
    { OB_STREAM_IR_RIGHT, "RightIR" }, { OB_STREAM_RAW_PHASE, "RawPhase" }
};

const std::map<OBIMUSampleRate, std::string> ImuRate_Str_Map = { { OB_SAMPLE_RATE_UNKNOWN, "UNKNOWN" },   { OB_SAMPLE_RATE_1_5625_HZ, "1_5625_HZ" },
                                                                 { OB_SAMPLE_RATE_3_125_HZ, "3_125_HZ" }, { OB_SAMPLE_RATE_6_25_HZ, "6_25_HZ" },
                                                                 { OB_SAMPLE_RATE_12_5_HZ, "12_5_HZ" },   { OB_SAMPLE_RATE_25_HZ, "25_HZ" },
                                                                 { OB_SAMPLE_RATE_50_HZ, "50_HZ" },       { OB_SAMPLE_RATE_100_HZ, "100_HZ" },
                                                                 { OB_SAMPLE_RATE_200_HZ, "200_HZ" },     { OB_SAMPLE_RATE_500_HZ, "500_HZ" },
                                                                 { OB_SAMPLE_RATE_1_KHZ, "1_KHZ" },       { OB_SAMPLE_RATE_2_KHZ, "2_KHZ" },
                                                                 { OB_SAMPLE_RATE_4_KHZ, "4_KHZ" },       { OB_SAMPLE_RATE_8_KHZ, "8_KHZ" },
                                                                 { OB_SAMPLE_RATE_16_KHZ, "16_KHZ" },     { OB_SAMPLE_RATE_32_KHZ, "32_KHZ" } };

const std::map<OBGyroFullScaleRange, std::string> GyroFullScaleRange_STR_MAP = { { OB_GYRO_FS_UNKNOWN, "Unknown" }, { OB_GYRO_FS_16dps, "16dps" },
                                                                                 { OB_GYRO_FS_31dps, "31dps" },     { OB_GYRO_FS_62dps, "62dps" },
                                                                                 { OB_GYRO_FS_125dps, "125dps" },   { OB_GYRO_FS_250dps, "250dps" },
                                                                                 { OB_GYRO_FS_500dps, "500dps" },   { OB_GYRO_FS_1000dps, "1000dps" },
                                                                                 { OB_GYRO_FS_2000dps, "2000dps" } };

const std::map<OBAccelFullScaleRange, std::string> AccelFullScaleRange_Str_Map = {
    { OB_ACCEL_FS_UNKNOWN, "Unknown" }, { OB_ACCEL_FS_2g, "2g" }, { OB_ACCEL_FS_4g, "4g" }, { OB_ACCEL_FS_8g, "8g" }, { OB_ACCEL_FS_16g, "16g" }
};

const std::map<OBFormat, std::string> Format_Str_Map = {
    { OB_FORMAT_YUYV, "YUYV" },       { OB_FORMAT_YUY2, "YUY2" },
    { OB_FORMAT_UYVY, "UYVY" },       { OB_FORMAT_NV12, "NV12" },
    { OB_FORMAT_NV21, "NV21" },       { OB_FORMAT_MJPG, "MJPG" },
    { OB_FORMAT_H264, "H264" },       { OB_FORMAT_H265, "H265" },
    { OB_FORMAT_Y16, "Y16" },         { OB_FORMAT_Y8, "Y8" },
    { OB_FORMAT_Y10, "Y10" },         { OB_FORMAT_Y11, "Y11" },
    { OB_FORMAT_Y12, "Y12" },         { OB_FORMAT_GRAY, "GRAY" },
    { OB_FORMAT_HEVC, "HEVC" },       { OB_FORMAT_I420, "I420" },
    { OB_FORMAT_ACCEL, "ACCEL" },     { OB_FORMAT_GYRO, "GYRO" },
    { OB_FORMAT_POINT, "POINT" },     { OB_FORMAT_RGB_POINT, "RGB_POINT" },
    { OB_FORMAT_RLE, "RLE" },         { OB_FORMAT_RGB, "RGB" },
    { OB_FORMAT_BGR, "BGR" },         { OB_FORMAT_Y14, "Y14" },
    { OB_FORMAT_BGRA, "BGRA" },       { OB_FORMAT_COMPRESSED, "COMPRESSED" },
    { OB_FORMAT_RVL, "RVL" },         { OB_FORMAT_Z16, "Z16" },
    { OB_FORMAT_YV12, "YV12" },       { OB_FORMAT_BA81, "BA81" },
    { OB_FORMAT_RGBA, "RGBA" },       { OB_FORMAT_BYR2, "BYR2" },
    { OB_FORMAT_RW16, "RW16" },       { OB_FORMAT_DISP16, "DISP16" },
    { OB_FORMAT_UNKNOWN, "UNKNOWN" },
};

std::map<OBFrameMetadataType, std::string> Metadata_Str_Map = {
    {OB_FRAME_METADATA_TYPE_TIMESTAMP, "Timestamp"},
    {OB_FRAME_METADATA_TYPE_SENSOR_TIMESTAMP, "Sensor Timestamp"},
    {OB_FRAME_METADATA_TYPE_FRAME_NUMBER, "Frame Number"},
    {OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE, "Auto Exposure"},
    {OB_FRAME_METADATA_TYPE_EXPOSURE, "Exposure"},
    {OB_FRAME_METADATA_TYPE_GAIN, "Gain"},
    {OB_FRAME_METADATA_TYPE_AUTO_WHITE_BALANCE, "Auto White Balance"},
    {OB_FRAME_METADATA_TYPE_WHITE_BALANCE, "White Balance"},
    {OB_FRAME_METADATA_TYPE_BRIGHTNESS, "Brightness"},
    {OB_FRAME_METADATA_TYPE_CONTRAST, "Contrast"},
    {OB_FRAME_METADATA_TYPE_SATURATION, "Saturation"},
    {OB_FRAME_METADATA_TYPE_SHARPNESS, "Sharpness"},
    {OB_FRAME_METADATA_TYPE_BACKLIGHT_COMPENSATION, "Backlight Compensation"},
    {OB_FRAME_METADATA_TYPE_HUE, "Hue"},
    {OB_FRAME_METADATA_TYPE_GAMMA, "Gamma"},
    {OB_FRAME_METADATA_TYPE_POWER_LINE_FREQUENCY, "Power Line Frequency"},
    {OB_FRAME_METADATA_TYPE_LOW_LIGHT_COMPENSATION, "Low Light Compensation"},
    {OB_FRAME_METADATA_TYPE_MANUAL_WHITE_BALANCE, "Manual White Balance"},
    {OB_FRAME_METADATA_TYPE_ACTUAL_FRAME_RATE, "Actual Frame Rate"},
    {OB_FRAME_METADATA_TYPE_FRAME_RATE, "Frame Rate"},
    {OB_FRAME_METADATA_TYPE_AE_ROI_LEFT, "AE ROI Left"},
    {OB_FRAME_METADATA_TYPE_AE_ROI_TOP, "AE ROI Top"},
    {OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT, "AE ROI Right"},
    {OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM, "AE ROI Bottom"},
    {OB_FRAME_METADATA_TYPE_EXPOSURE_PRIORITY, "Exposure Priority"},
    {OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_NAME, "HDR Sequence Name"},
    {OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_SIZE, "HDR Sequence Size"},
    {OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_INDEX, "HDR Sequence Index"},
    {OB_FRAME_METADATA_TYPE_LASER_POWER, "Laser Power"},
    {OB_FRAME_METADATA_TYPE_LASER_POWER_LEVEL, "Laser Power Level"},
    {OB_FRAME_METADATA_TYPE_LASER_STATUS, "Laser Status"},
    {OB_FRAME_METADATA_TYPE_GPIO_INPUT_DATA, "GPIO Input Data"}
};

// type to string
const std::string &obFormatToStr(OBFormat type) {
    auto it = Format_Str_Map.find(type);
    if(it == Format_Str_Map.end()) {
        throw invalid_value_exception("Unregistered stream type");
    }
    return it->second;
}

const std::string &obFrameToStr(OBFrameType type) {
    auto it = Frame_Str_Map.find(type);
    if(it == Frame_Str_Map.end()) {
        throw invalid_value_exception("Unregistered frame type");
    }
    return it->second;
}

const std::string &obStreamToStr(OBStreamType type) {
    auto it = Stream_Str_Map.find(type);
    if(it == Stream_Str_Map.end()) {
        throw invalid_value_exception("Unregistered stream type");
    }
    return it->second;
}

const std::string &obSensorToStr(OBSensorType type) {
    auto it = Sensor_Str_Map.find(type);
    if(it == Sensor_Str_Map.end()) {
        throw invalid_value_exception("Unregistered sensor type");
    }
    return it->second;
}

const std::string &obImuRateToStr(OBIMUSampleRate type) {
    auto it = ImuRate_Str_Map.find(type);
    if(it == ImuRate_Str_Map.end()) {
        throw invalid_value_exception("Unregistered imu rate type");
    }
    return it->second;
}

const std::string &GyroFullScaleRangeToStr(OBGyroFullScaleRange type) {
    auto it = GyroFullScaleRange_STR_MAP.find(type);
    if(it == GyroFullScaleRange_STR_MAP.end()) {
        throw invalid_value_exception("Unregistered gyro full scale range name");
    }
    return it->second;
}

const std::string &AccelFullScaleRangeToStr(OBAccelFullScaleRange type) {
    auto it = AccelFullScaleRange_Str_Map.find(type);
    if(it == AccelFullScaleRange_Str_Map.end()) {
        throw invalid_value_exception("Unregistered acc full scale range name");
    }
    return it->second;
}

const std::string &MetaDataToStr(OBFrameMetadataType type) {
    auto it = Metadata_Str_Map.find(type);
    if(it == Metadata_Str_Map.end()) {
        throw invalid_value_exception("Unregistered metadata name");
    }
    return it->second;
}

// string to type
OBFormat strToOBFormat(const std::string str) {
    for(auto it = Format_Str_Map.begin(); it != Format_Str_Map.end(); ++it) {
        if(it->second == str) {
            return it->first;
        }
    }
    throw invalid_value_exception("Unregistered format type");
}

OBFrameType strToOBFrame(const std::string str) {
    for(auto it = Frame_Str_Map.begin(); it != Frame_Str_Map.end(); ++it) {
        if(it->second == str) {
            return it->first;
        }
    }
    throw invalid_value_exception("Unregistered frame type");
}

OBStreamType strToOBStream(const std::string str) {
    for(auto it = Stream_Str_Map.begin(); it != Stream_Str_Map.end(); ++it) {
        if(it->second == str) {
            return it->first;
        }
    }
    throw invalid_value_exception("Unregistered stream type");
}

OBSensorType strToOBSensor(const std::string str) {
    for(auto it = Sensor_Str_Map.begin(); it != Sensor_Str_Map.end(); ++it) {
        if(it->second == str) {
            return it->first;
        }
    }
    throw invalid_value_exception("Unregistered sensor type");
}

OBIMUSampleRate strToObImuRate(const std::string str) {
    for(auto it = ImuRate_Str_Map.begin(); it != ImuRate_Str_Map.end(); ++it) {
        if(it->second == str) {
            return it->first;
        }
    }
    throw invalid_value_exception("Unregistered Imu rate type");
}

OBGyroFullScaleRange strToGyroFullScaleRange(const std::string str) {
    for(auto it = GyroFullScaleRange_STR_MAP.begin(); it != GyroFullScaleRange_STR_MAP.end(); ++it) {
        if(it->second == str) {
            return it->first;
        }
    }
    throw invalid_value_exception("Unregistered gyro full scale range type");
}

OBAccelFullScaleRange strToAccelFullScaleRange(const std::string str) {
    for(auto it = AccelFullScaleRange_Str_Map.begin(); it != AccelFullScaleRange_Str_Map.end(); ++it) {
        if(it->second == str) {
            return it->first;
        }
    }
    throw invalid_value_exception("Unregistered accel full scale range type");
}

}  // namespace utils
}  // namespace libobsensor

std::ostream &operator<<(std::ostream &os, const OBFormat &type) {
    os << libobsensor::utils::obFormatToStr(type);
    return os;
}

std::ostream &operator<<(std::ostream &os, const OBFrameType &type) {
    os << libobsensor::utils::obFrameToStr(type);
    return os;
}

std::ostream &operator<<(std::ostream &os, const OBStreamType &type) {
    os << libobsensor::utils::obStreamToStr(type);
    return os;
}

std::ostream &operator<<(std::ostream &os, const OBSensorType &type) {
    os << libobsensor::utils::obSensorToStr(type);
    return os;
}

std::ostream &operator<<(std::ostream &os, const OBIMUSampleRate &type) {
    os << libobsensor::utils::obImuRateToStr(type);
    return os;
}

std::ostream &operator<<(std::ostream &os, const OBGyroFullScaleRange &type) {
    os << libobsensor::utils::GyroFullScaleRangeToStr(type);
    return os;
}

std::ostream &operator<<(std::ostream &os, const OBAccelFullScaleRange &type) {
    os << libobsensor::utils::AccelFullScaleRangeToStr(type);
    return os;
}

std::ostream &operator<<(std::ostream &os, const OBCameraParam &params) {
    os << "{\n"
       << "depthIntrinsic:{ fx:" << params.depthIntrinsic.fx << ",fy:" << params.depthIntrinsic.fy << ",cx:" << params.depthIntrinsic.cx
       << ",cy:" << params.depthIntrinsic.cy << ",width:" << params.depthIntrinsic.width << ",height:" << params.depthIntrinsic.height << "}\n"
       << "rgbIntrinsic:{ fx:" << params.rgbIntrinsic.fx << ",fy:" << params.rgbIntrinsic.fy << ",cx:" << params.rgbIntrinsic.cx
       << ",cy:" << params.rgbIntrinsic.cy << ",width:" << params.rgbIntrinsic.width << ",height:" << params.rgbIntrinsic.height << "}\n"
       << "depthDistortion:{ model:" << params.depthDistortion.model << ",k1:" << params.depthDistortion.k1 << ",k2:" << params.depthDistortion.k2
       << ",k3:" << params.depthDistortion.k3 << ",k4:" << params.depthDistortion.k4 << ",k5:" << params.depthDistortion.k5
       << ",k6:" << params.depthDistortion.k6 << ",p1:" << params.depthDistortion.p1 << ",p2:" << params.depthDistortion.p2 << "}\n"
       << "rgbDistortion:{ model:" << params.rgbDistortion.model << ",k1:" << params.rgbDistortion.k1 << ",k2:" << params.rgbDistortion.k2
       << ",k3:" << params.rgbDistortion.k3 << ",k4:" << params.rgbDistortion.k4 << ",k5:" << params.rgbDistortion.k5 << ",k6:" << params.rgbDistortion.k6
       << ",p1:" << params.rgbDistortion.p1 << ",p2:" << params.rgbDistortion.p2 << "}\n"
       << "transform: rot:[" << params.transform.rot[0] << "," << params.transform.rot[1] << "," << params.transform.rot[2] << "," << params.transform.rot[3]
       << "," << params.transform.rot[4] << "," << params.transform.rot[5] << "," << params.transform.rot[6] << "," << params.transform.rot[7] << ","
       << params.transform.rot[8] <<  "],trans:" << params.transform.trans[0] << "," << params.transform.trans[1] << ","
       << params.transform.trans[2] << "}\n"
       << "isMirror:" << params.isMirrored << "\n"
       << "}";
    return os;
}