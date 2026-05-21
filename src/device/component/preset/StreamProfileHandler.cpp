// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "StreamProfileHandler.hpp"
#include "stream/StreamProfile.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "PresetDefinitions.hpp"

namespace libobsensor {

StreamProfileHandler::StreamProfileHandler(IDevice *owner, OBSensorType sensorType) : owner_(owner), sensorType_(sensorType) {}

bool StreamProfileHandler::onPreChildrenSet(const Json::Value &value) {
    utils::unusedVar(value);
    // clear data
    format_ = OB_FORMAT_UNKNOWN;
    fps_    = 0;
    width_  = 0;
    height_ = 0;
    return true;
}

void StreamProfileHandler::onSetChild(const std::string &k, const Json::Value &v, const Json::Value &parent) {
    utils::unusedVar(parent);
    // Save all values
    if(k == kStreamProfileFormatKey) {
        format_ = utils::strToOBFormat(jsonmodel::JsonTraits<std::string>::from(v));
    }
    else if(k == kStreamProfileFpsKey) {
        fps_ = jsonmodel::JsonTraits<uint32_t>::from(v);
    }
    else if(k == kStreamProfileWidthKey) {
        width_ = jsonmodel::JsonTraits<uint32_t>::from(v);
    }
    else if(k == kStreamProfileHeightKey) {
        height_ = jsonmodel::JsonTraits<uint32_t>::from(v);
    }
    else {
        THROW_INVALID_PARAM_EXCEPTION("Invalid key of frame stream profile: '" + k + "'");
    }
}

void StreamProfileHandler::onPostChildrenSet() {
    // Set all values
    auto sensor             = owner_->getSensor(sensorType_);
    auto spList             = sensor->getStreamProfileList();
    auto matchedProfileList = matchVideoStreamProfile(spList, width_, height_, fps_, format_);
    if(matchedProfileList.empty()) {
        LOG_WARN("No matched profile found for sensor: {}, width: {}, height: {}, fps: {}, format: {}", sensorType_, width_, height_, fps_, format_);
        return;
    }
    sensor->updateDefaultStreamProfile(matchedProfileList[0]);
}

std::vector<std::string> StreamProfileHandler::onPreChildrenGet() {
    // clear data
    format_ = OB_FORMAT_UNKNOWN;
    fps_    = 0;
    width_  = 0;
    height_ = 0;

    auto sensor  = owner_->getSensor(sensorType_);
    auto profile = sensor->getActivatedStreamProfile();

    if(profile == nullptr) {
        // Stream is off, get the default profile
        auto spList = sensor->getStreamProfileList();
        if(spList.empty()) {
            LOG_WARN("No any profile found for sensor: {}", sensorType_);
            return {};
        }
        profile = spList[0];
    }
    auto vsp = profile->as<VideoStreamProfile>();
    format_  = vsp->getFormat();
    fps_     = vsp->getFps();
    width_   = vsp->getWidth();
    height_  = vsp->getHeight();

    return {};
}

jsonmodel::ExportValue StreamProfileHandler::exportChildValue(const std::string &k) {
    if(k == kStreamProfileFormatKey) {
        return jsonmodel::makeScalar(utils::obFormatToStr(format_));
    }
    else if(k == kStreamProfileFpsKey) {
        return jsonmodel::makeScalar(fps_);
    }
    else if(k == kStreamProfileWidthKey) {
        return jsonmodel::makeScalar(width_);
    }
    else if(k == kStreamProfileHeightKey) {
        return jsonmodel::makeScalar(height_);
    }
    else {
        THROW_INVALID_PARAM_EXCEPTION("Invalid key of frame stream profile: '" + k + "'");
    }
}

}  // namespace libobsensor
