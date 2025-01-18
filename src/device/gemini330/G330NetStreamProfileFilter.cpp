// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G330NetStreamProfileFilter.hpp"
#include "utils/Utils.hpp"
#include "stream/StreamProfile.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "exception/ObException.hpp"
#include "property/InternalProperty.hpp"


namespace libobsensor {
G330NetStreamProfileFilter::G330NetStreamProfileFilter(IDevice *owner) : DeviceComponentBase(owner), perFormanceMode_(ADAPTIVE_PERFORMANCE_MODE) {

}

//static bool isMatch(OBSensorType sensorType, std::shared_ptr<const VideoStreamProfile> videoProfile, OBEffectiveStreamProfile effProfile) {
//    bool isSensorTypeEqual = sensorType == effProfile.sensorType;
//    return isSensorTypeEqual && (videoProfile->getFps() <= effProfile.maxFps) && (videoProfile->getWidth() == effProfile.width)
//           && (videoProfile->getHeight() == effProfile.height);
//}

StreamProfileList G330NetStreamProfileFilter::filter(const StreamProfileList &profiles) const {
    StreamProfileList filteredProfiles;
    if(perFormanceMode_ == ADAPTIVE_PERFORMANCE_MODE) {
        filteredProfiles = profiles;
        return filteredProfiles;
    }

    /*for(const auto &profile: profiles) {
        auto videoProfile = profile->as<VideoStreamProfile>();
        auto streamType   = profile->getType();
        auto sensorType   = utils::mapStreamTypeToSensorType(streamType);
        for(auto &effProfile: effectiveStreamProfiles_) {
            if(isMatch(sensorType, videoProfile, effProfile)) {
                filteredProfiles.push_back(profile);
            }
        }
    }*/

    for(const auto &profile: profiles) {
        auto videoProfile = profile->as<VideoStreamProfile>();
        if(videoProfile->getWidth() == 640 && videoProfile->getHeight() == 480 && videoProfile->getFps() == 90) {
            continue;
        }
        filteredProfiles.push_back(profile);
    }
    return filteredProfiles;
}

void G330NetStreamProfileFilter::switchFilterMode(OBCameraPerformanceMode mode) {
    perFormanceMode_ = mode;
    fetchEffectiveStreamProfiles();
}

void G330NetStreamProfileFilter::fetchEffectiveStreamProfiles() {
    /*auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();*/
    effectiveStreamProfiles_.clear();
    if(perFormanceMode_ == HIGH_PERFORMANCE_MODE) {
        // Color
        effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 320, 200, 90 });
        effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 360, 90 });
        effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 400, 90 });
        effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 60 });
        effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 848, 530, 60 });
        effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 720, 60 });
        effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 800, 60 });
        // Depth
        effectiveStreamProfiles_.push_back({ OB_SENSOR_DEPTH, OB_FORMAT_Y16, 320, 200, 30 });
        effectiveStreamProfiles_.push_back({ OB_SENSOR_DEPTH, OB_FORMAT_Y16, 424, 266, 30 });
        effectiveStreamProfiles_.push_back({ OB_SENSOR_DEPTH, OB_FORMAT_Y16, 640, 400, 30 });
        effectiveStreamProfiles_.push_back({ OB_SENSOR_DEPTH, OB_FORMAT_Y16, 640, 480, 60 });
        effectiveStreamProfiles_.push_back({ OB_SENSOR_DEPTH, OB_FORMAT_Y16, 848, 530, 60 });
        effectiveStreamProfiles_.push_back({ OB_SENSOR_DEPTH, OB_FORMAT_Y16, 1280, 800, 30 });
        // Left IR
        effectiveStreamProfiles_.push_back({ OB_SENSOR_IR_LEFT, OB_FORMAT_Y8, 320, 200, 30 });
        effectiveStreamProfiles_.push_back({ OB_SENSOR_IR_LEFT, OB_FORMAT_Y8, 424, 266, 30 });
        effectiveStreamProfiles_.push_back({ OB_SENSOR_IR_LEFT, OB_FORMAT_Y8, 640, 400, 30 });
        effectiveStreamProfiles_.push_back({ OB_SENSOR_IR_LEFT, OB_FORMAT_Y8, 640, 480, 60 });
        effectiveStreamProfiles_.push_back({ OB_SENSOR_IR_LEFT, OB_FORMAT_Y8, 848, 530, 60 });
        effectiveStreamProfiles_.push_back({ OB_SENSOR_IR_LEFT, OB_FORMAT_Y8, 1280, 800, 30 });
        // Right IR
        effectiveStreamProfiles_.push_back({ OB_SENSOR_IR_RIGHT, OB_FORMAT_Y8, 320, 200, 30 });
        effectiveStreamProfiles_.push_back({ OB_SENSOR_IR_RIGHT, OB_FORMAT_Y8, 424, 266, 30 });
        effectiveStreamProfiles_.push_back({ OB_SENSOR_IR_RIGHT, OB_FORMAT_Y8, 640, 400, 30 });
        effectiveStreamProfiles_.push_back({ OB_SENSOR_IR_RIGHT, OB_FORMAT_Y8, 640, 480, 60 });
        effectiveStreamProfiles_.push_back({ OB_SENSOR_IR_RIGHT, OB_FORMAT_Y8, 848, 530, 60 });
        effectiveStreamProfiles_.push_back({ OB_SENSOR_IR_RIGHT, OB_FORMAT_Y8, 1280, 800, 30 });
    }
    else {
        effectiveStreamProfiles_.clear();
    }
}

}  // namespace libobsensor
