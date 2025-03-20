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

StreamProfileList G330NetStreamProfileFilter::filter(const StreamProfileList &profiles) const {
    StreamProfileList filteredProfiles;
    if(perFormanceMode_ == ADAPTIVE_PERFORMANCE_MODE) {
        filteredProfiles = profiles;
        return filteredProfiles;
    }

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
}

}  // namespace libobsensor
