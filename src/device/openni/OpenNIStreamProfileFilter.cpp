// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "OpenNIStreamProfileFilter.hpp"
#include "utils/Utils.hpp"
#include "stream/StreamProfile.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "exception/ObException.hpp"
#include "property/InternalProperty.hpp"
#include "DevicePids.hpp"

namespace libobsensor {
OpenNIStreamProfileFilter::OpenNIStreamProfileFilter(IDevice *owner) : DeviceComponentBase(owner) {
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

StreamProfileList OpenNIStreamProfileFilter::filter(const StreamProfileList &profiles) const {
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

void OpenNIStreamProfileFilter::fetchEffectiveStreamProfiles() {
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

    if(owner->getInfo()->pid_ == OB_DEVICE_DABAI_DW2_PID || owner->getInfo()->pid_ == OB_DEVICE_DABAI_DCW2_PID
       || owner->getInfo()->pid_ == OB_DEVICE_GEMINI_EW_PID || owner->getInfo()->pid_ == OB_DEVICE_GEMINI_EW_LITE_PID) {
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 320, 240, 5 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 320, 240, 10 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 320, 240, 15 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 320, 240, 30 });

        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 5 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 10 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 15 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 30 });

        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 320, 180, 5 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 320, 180, 10 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 320, 180, 15 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 320, 180, 30 });

        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 360, 5 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 360, 10 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 360, 15 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 360, 30 });

        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 720, 5 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 720, 10 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 720, 15 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 720, 30 });

        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1920, 1080, 5 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1920, 1080, 10 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1920, 1080, 15 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1920, 1080, 30 });
    }

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

    if(owner->getInfo()->pid_ == OB_DEVICE_DABAI_DC1_PID) {
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 5 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 10 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 15 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 20 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 25 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 640, 480, 30 });

        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 720, 5 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 720, 10 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 720, 15 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 720, 20 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 720, 25 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 720, 30 });

        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 960, 5 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 960, 10 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 960, 15 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 960, 20 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 960, 25 });
        colorEffectiveStreamProfiles_.push_back({ OB_SENSOR_COLOR, OB_FORMAT_MJPG, 1280, 960, 30 });
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
