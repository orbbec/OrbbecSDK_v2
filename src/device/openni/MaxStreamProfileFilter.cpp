// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "MaxStreamProfileFilter.hpp"
#include "utils/Utils.hpp"
#include "stream/StreamProfile.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "exception/ObException.hpp"
#include "property/InternalProperty.hpp"
#include "DevicePids.hpp"

namespace libobsensor {
MaxStreamProfileFilter::MaxStreamProfileFilter(IDevice *owner) : DeviceComponentBase(owner) {
    fetchEffectiveStreamProfiles();
}

static bool isMatch(OBSensorType sensorType, std::shared_ptr<const VideoStreamProfile> videoProfile, OBEffectiveStreamProfile effProfile) {
    if((sensorType == OB_SENSOR_COLOR) && effProfile.sensorType == sensorType) {
        return (videoProfile->getFps() == effProfile.maxFps) && (videoProfile->getWidth() == effProfile.width)
               && (videoProfile->getHeight() == effProfile.height);
    }

    if((sensorType == OB_SENSOR_DEPTH) && effProfile.sensorType == sensorType) {
        return (videoProfile->getFps() == effProfile.maxFps) && (videoProfile->getWidth() == effProfile.width)
               && (videoProfile->getHeight() == effProfile.height);
    }
    return false;
}

StreamProfileList MaxStreamProfileFilter::filter(const StreamProfileList &profiles) const {
    StreamProfileList filteredProfiles;
    for(const auto &profile: profiles) {
        auto videoProfile = profile->as<VideoStreamProfile>();
        auto streamType   = profile->getType();
        auto sensorType   = utils::mapStreamTypeToSensorType(streamType);
        if(sensorType == OB_SENSOR_COLOR) {
            for(auto &effProfile: colorEffectiveStreamProfiles_) {
                if(isMatch(sensorType, videoProfile, effProfile)) {
                    filteredProfiles.push_back(profile);
                }
            }
        }

        if(sensorType == OB_SENSOR_DEPTH) {
            if(depthEffectiveStreamProfiles_.size() != 0) {
                for(auto &effProfile: depthEffectiveStreamProfiles_) {
                    if(isMatch(sensorType, videoProfile, effProfile)) {
                        filteredProfiles.push_back(profile);
                    }
                }
            }
            else {
                filteredProfiles.push_back(profile);
            }
        }
    }
    return filteredProfiles;
}

void MaxStreamProfileFilter::fetchEffectiveStreamProfiles() {
    auto owner      = getOwner();
    if(owner->getInfo()->pid_ == OB_DEVICE_MAX_PRO_PID) {
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 5 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 6 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 10 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 15 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 20 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 25 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 30 });

        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 960, 5 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 960, 6 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 960, 10 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 960, 15 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 960, 20 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 960, 25 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 960, 30 });

        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1600, 1200, 5 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1600, 1200, 6 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1600, 1200, 10 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1600, 1200, 15 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1600, 1200, 20 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1600, 1200, 25 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1600, 1200, 30 });
    }
    else if(owner->getInfo()->pid_ == OB_DEVICE_DABAI_MAX_PID) {
        depthEffectiveStreamProfiles_.push_back({ OB_SENSOR_DEPTH, OB_FORMAT_Y12, 640, 320, 5 });
        depthEffectiveStreamProfiles_.push_back({ OB_SENSOR_DEPTH, OB_FORMAT_Y12, 640, 320, 8 });
        depthEffectiveStreamProfiles_.push_back({ OB_SENSOR_DEPTH, OB_FORMAT_Y12, 640, 320, 10 });
        depthEffectiveStreamProfiles_.push_back({ OB_SENSOR_DEPTH, OB_FORMAT_Y12, 640, 320, 15 });
        depthEffectiveStreamProfiles_.push_back({ OB_SENSOR_DEPTH, OB_FORMAT_Y12, 640, 320, 30 });

        depthEffectiveStreamProfiles_.push_back({ OB_SENSOR_DEPTH, OB_FORMAT_Y12, 320, 160, 5 });
        depthEffectiveStreamProfiles_.push_back({ OB_SENSOR_DEPTH, OB_FORMAT_Y12, 320, 160, 8 });
        depthEffectiveStreamProfiles_.push_back({ OB_SENSOR_DEPTH, OB_FORMAT_Y12, 320, 160, 10 });
        depthEffectiveStreamProfiles_.push_back({ OB_SENSOR_DEPTH, OB_FORMAT_Y12, 320, 160, 15 });
        depthEffectiveStreamProfiles_.push_back({ OB_SENSOR_DEPTH, OB_FORMAT_Y12, 320, 160, 30 });
    }
}

}  // namespace libobsensor
