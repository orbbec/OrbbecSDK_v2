// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "MaxProStreamProfileFilter.hpp"
#include "utils/Utils.hpp"
#include "stream/StreamProfile.hpp"
#include "exception/ObException.hpp"
#include "property/InternalProperty.hpp"

namespace libobsensor {
MaxProStreamProfileFilter::MaxProStreamProfileFilter(IDevice *owner) : DeviceComponentBase(owner) {
    fetchEffectiveStreamProfiles();
}

static bool isMatch(OBSensorType sensorType, std::shared_ptr<const VideoStreamProfile> videoProfile, OBEffectiveStreamProfile effProfile) {
    if((sensorType == OB_SENSOR_COLOR) && effProfile.sensorType == sensorType) {
        return (videoProfile->getFps() == effProfile.maxFps) && (videoProfile->getWidth() == effProfile.width)
               && (videoProfile->getHeight() == effProfile.height);
    }
    return false;
}

StreamProfileList MaxProStreamProfileFilter::filter(const StreamProfileList &profiles) const {
    StreamProfileList filteredProfiles;

    for(const auto &profile: profiles) {
        auto videoProfile = profile->as<VideoStreamProfile>();
        auto streamType   = profile->getType();
        auto sensorType   = utils::mapStreamTypeToSensorType(streamType);
        if(sensorType == OB_SENSOR_COLOR) {
            for(auto &effProfile: effectiveStreamProfiles_) {
                if(isMatch(sensorType, videoProfile, effProfile)) {
                    filteredProfiles.push_back(profile);
                }
            }
        }
        else if(sensorType == OB_SENSOR_DEPTH) {

        }
        
    }
    return filteredProfiles;
}

void MaxProStreamProfileFilter::fetchEffectiveStreamProfiles() {
    /*auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();*/
    effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 5 });
    effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 6 });
    effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 10 });
    effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 15 });
    effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 20 });
    effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 25 });
    effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 30 });

    effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 960, 5 });
    effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 960, 6 });
    effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 960, 10 });
    effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 960, 15 });
    effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 960, 20 });
    effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 960, 25 });
    effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 960, 30 });

    effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1600, 1200, 5 });
    effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1600, 1200, 6 });
    effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1600, 1200, 10 });
    effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1600, 1200, 15 });
    effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1600, 1200, 20 });
    effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1600, 1200, 25 });
    effectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1600, 1200, 30 });
}

}  // namespace libobsensor
