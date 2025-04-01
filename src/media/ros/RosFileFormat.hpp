// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "libobsensor/h/ObTypes.h"
#include "sensor_msgs/image_encodings.h"
#include "rosbag/view.h"
#include "sensor_msgs/Image.h"

#include <string>
#include <chrono>
#include <regex>
#include <vector>
#include <map>
#include <sstream>

namespace libobsensor {
class MultipleRegexTopicQuery {
public:
    MultipleRegexTopicQuery(const std::vector<std::string> &regexps) {
        for(auto &&regexp: regexps) {

            exps_.emplace_back(regexp);
        }
    }

    bool operator()(rosbag::ConnectionInfo const *info) const {
        return std::any_of(std::begin(exps_), std::end(exps_), [info](const std::regex &exp) { return std::regex_search(info->topic, exp); });
    }

private:
    std::vector<std::regex> exps_;
};

class RegexTopicQuery : public MultipleRegexTopicQuery {
public:
    RegexTopicQuery(std::string const &regexp) : MultipleRegexTopicQuery({ regexp }) {}

private:
    std::regex exp_;
};

class FrameQuery : public RegexTopicQuery {
public:
    FrameQuery() : RegexTopicQuery(R"RRR(/cam/sensor_\d+/frameType_\d+)RRR") {}
    FrameQuery(const OBSensorType &sensorType) : RegexTopicQuery("/cam/sensor_" + std::to_string((uint8_t)sensorType) + "/frameType_\\d+") {}
};

class DeviceInfoQuery : public RegexTopicQuery {
public:
    DeviceInfoQuery() : RegexTopicQuery(R"RRR(/cam/deviceInfo)RRR") {}
};

class StreamProfileQuery : public RegexTopicQuery {
public:
    StreamProfileQuery() : RegexTopicQuery("/cam/streamProfileType_\\d+") {}
    StreamProfileQuery(const OBStreamType &streamType) : RegexTopicQuery("/cam/streamProfileType_" + std::to_string((uint8_t)streamType)) {}
};

class PropertyQuery : public RegexTopicQuery {
public:
    PropertyQuery() : RegexTopicQuery(R"RRR(/cam/property)RRR") {}
};

class DisparityParamQuery : public RegexTopicQuery {
public:
    DisparityParamQuery() : RegexTopicQuery(R"RRR(/cam/disparityParam)RRR") {}
};

class FalseQuery {
public:
    bool operator()(rosbag::ConnectionInfo const *info) const {
        (void)info;
        return false;
    }
};

class RosTopic {
public:
    static constexpr const char *elementsSeparator() {
        return "/";
    }
    static std::string cameraPrefix() {
        return "cam";
    }
    static std::string deviceInfoPrefix() {
        return "deviceInfo";
    }
    static std::string disparityParamPrefix() {
        return "disparityParam";
    }
    static std::string propertyPrefix() {
        return "property";
    }
    static std::string streamProfilePrefix(uint8_t streamType) {
        return "streamProfileType_" + std::to_string(streamType);
    }
    static std::string frameTypePrefix(uint8_t frameType) {
        return "frameType_" + std::to_string(frameType);
    }
    static std::string sensorPrefix(uint8_t sensorType) {
        return "sensor_" + std::to_string(sensorType);
    }
    static OBFrameType getFrameTypeIdentifier(std::string &topic) {
        std::regex  pattern(R"(/frameType_(\d+))");
        std::smatch match;

        std::regex_search(topic, match, pattern);
        int FrameType = std::stoi(match[1].str());
        return static_cast<OBFrameType>(FrameType);
    }
    static OBStreamType getStreamProfilePrefix(std::string topic) {
        std::regex  pattern(R"(/streamProfileType_(\d+))");
        std::smatch match;

        std::regex_search(topic, match, pattern);
        int FrameType = std::stoi(match[1].str());
        return static_cast<OBStreamType>(FrameType);
    }
    static std::string frameDataTopic(const uint8_t &sensorName, const uint8_t &frameType) {
        return create_from({ cameraPrefix(), sensorPrefix(sensorName), frameTypePrefix(frameType) });
    }
    static std::string imuDataTopic(const uint8_t &sensorName, const uint8_t &frameType) {
        return create_from({ cameraPrefix(), sensorPrefix(sensorName), frameTypePrefix(frameType) });
    }
    static std::string propertyTopic() {
        return create_from({ cameraPrefix(), propertyPrefix() });
    }
    static std::string deviceTopic() {
        return create_from({ cameraPrefix(), deviceInfoPrefix() });
    }
    static std::string streamProfileTopic(const uint8_t &streamType) {
        return create_from({ cameraPrefix(), streamProfilePrefix(streamType) });
    }
    static std::string disparityParmTopic() {
        return create_from({ cameraPrefix(), disparityParamPrefix() });
    }
    static std::string create_from(const std::vector<std::string> &parts) {
        std::ostringstream oss;
        oss << elementsSeparator();
        if(parts.empty() == false) {
            std::copy(parts.begin(), parts.end() - 1, std::ostream_iterator<std::string>(oss, elementsSeparator()));
            oss << parts.back();
        }
        return oss.str();
    };
};

inline std::string convertFormatToString(OBFormat format) {
    if(format == OB_FORMAT_RGB)
        return sensor_msgs::image_encodings::RGB8;
    else if(format == OB_FORMAT_BGR)
        return sensor_msgs::image_encodings::BGR8;
    else if(format == OB_FORMAT_BGRA)
        return sensor_msgs::image_encodings::BGRA8;
    else if(format == OB_FORMAT_RGBA)
        return sensor_msgs::image_encodings::RGBA8;
    else if(format == OB_FORMAT_Y16)
        return sensor_msgs::image_encodings::MONO16;
    else if(format == OB_FORMAT_Z16)
        return sensor_msgs::image_encodings::Z16;
    else if(format == OB_FORMAT_Y8)
        return sensor_msgs::image_encodings::MONO8;
    else if(format == OB_FORMAT_UYVY)
        return sensor_msgs::image_encodings::YUV422;
    else if(format == OB_FORMAT_YUYV)
        return sensor_msgs::image_encodings::YUYV;
    else if(format == OB_FORMAT_I420)
        return sensor_msgs::image_encodings::I420;
    else if(format == OB_FORMAT_NV21)
        return sensor_msgs::image_encodings::NV21;
    else if(format == OB_FORMAT_MJPG)
        return sensor_msgs::image_encodings::MJPG;
    else if(format == OB_FORMAT_H264)
        return sensor_msgs::image_encodings::H264;
    else if(format == OB_FORMAT_H265)
        return sensor_msgs::image_encodings::H265;
    else if(format == OB_FORMAT_RLE)
        return sensor_msgs::image_encodings::RLE;
    else if(format == OB_FORMAT_YUY2)
        return sensor_msgs::image_encodings::YUY2;
    else if(format == OB_FORMAT_NV12)
        return sensor_msgs::image_encodings::NV12;
    else if(format == OB_FORMAT_Y10)
        return sensor_msgs::image_encodings::Y10;
    else if(format == OB_FORMAT_Y11)
        return sensor_msgs::image_encodings::Y11;
    else if(format == OB_FORMAT_Y12)
        return sensor_msgs::image_encodings::Y12;
    else if(format == OB_FORMAT_GRAY)
        return sensor_msgs::image_encodings::GRAY;
    else if(format == OB_FORMAT_HEVC)
        return sensor_msgs::image_encodings::HEVC;
    else if(format == OB_FORMAT_Y14)
        return sensor_msgs::image_encodings::Y14;
    else if(format == OB_FORMAT_COMPRESSED)
        return sensor_msgs::image_encodings::COMPRESSED;
    else if(format == OB_FORMAT_RVL)
        return sensor_msgs::image_encodings::RVL;
    else if(format == OB_FORMAT_YV12)
        return sensor_msgs::image_encodings::YV12;
    else if(format == OB_FORMAT_BA81)
        return sensor_msgs::image_encodings::BA81;
    else if(format == OB_FORMAT_BYR2)
        return sensor_msgs::image_encodings::BYR2;
    else if(format == OB_FORMAT_RW16)
        return sensor_msgs::image_encodings::RW16;
    else
        return "";
}
inline OBFormat convertStringToFormat(const std::string &encoding) {
    if(encoding == sensor_msgs::image_encodings::RGB8)
        return OB_FORMAT_RGB;
    else if(encoding == sensor_msgs::image_encodings::RGBA8)
        return OB_FORMAT_RGBA;
    else if(encoding == sensor_msgs::image_encodings::BGR8)
        return OB_FORMAT_BGR;
    else if(encoding == sensor_msgs::image_encodings::BGRA8)
        return OB_FORMAT_BGRA;
    else if(encoding == sensor_msgs::image_encodings::MONO16)
        return OB_FORMAT_Y16;
    else if(encoding == sensor_msgs::image_encodings::MONO8)
        return OB_FORMAT_Y8;
    else if(encoding == sensor_msgs::image_encodings::YUV422)
        return OB_FORMAT_UYVY;
    else if(encoding == sensor_msgs::image_encodings::YUYV)
        return OB_FORMAT_YUYV;
    else if(encoding == sensor_msgs::image_encodings::I420)
        return OB_FORMAT_I420;
    else if(encoding == sensor_msgs::image_encodings::NV21)
        return OB_FORMAT_NV21;
    else if(encoding == sensor_msgs::image_encodings::MJPG)
        return OB_FORMAT_MJPG;
    else if(encoding == sensor_msgs::image_encodings::H264)
        return OB_FORMAT_H264;
    else if(encoding == sensor_msgs::image_encodings::H265)
        return OB_FORMAT_H265;
    else if(encoding == sensor_msgs::image_encodings::RLE)
        return OB_FORMAT_RLE;
    else if(encoding == sensor_msgs::image_encodings::YUY2)
        return OB_FORMAT_YUY2;
    else if(encoding == sensor_msgs::image_encodings::NV12)
        return OB_FORMAT_NV12;
    else if(encoding == sensor_msgs::image_encodings::Y10)
        return OB_FORMAT_Y10;
    else if(encoding == sensor_msgs::image_encodings::Y11)
        return OB_FORMAT_Y11;
    else if(encoding == sensor_msgs::image_encodings::Y12)
        return OB_FORMAT_Y12;
    else if(encoding == sensor_msgs::image_encodings::GRAY)
        return OB_FORMAT_GRAY;
    else if(encoding == sensor_msgs::image_encodings::HEVC)
        return OB_FORMAT_HEVC;
    else if(encoding == sensor_msgs::image_encodings::Y14)
        return OB_FORMAT_Y14;
    else if(encoding == sensor_msgs::image_encodings::COMPRESSED)
        return OB_FORMAT_COMPRESSED;
    else if(encoding == sensor_msgs::image_encodings::RVL)
        return OB_FORMAT_RVL;
    else if(encoding == sensor_msgs::image_encodings::Z16)
        return OB_FORMAT_Z16;
    else if(encoding == sensor_msgs::image_encodings::YV12)
        return OB_FORMAT_YV12;
    else if(encoding == sensor_msgs::image_encodings::BA81)
        return OB_FORMAT_BA81;
    else if(encoding == sensor_msgs::image_encodings::BYR2)
        return OB_FORMAT_BYR2;
    else if(encoding == sensor_msgs::image_encodings::RW16)
        return OB_FORMAT_RW16;
    else
        return OB_FORMAT_UNKNOWN;
}
}  // namespace libobsensor
