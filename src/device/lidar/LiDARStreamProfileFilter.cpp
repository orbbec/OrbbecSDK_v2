// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "LiDARStreamProfileFilter.hpp"

#include "utils/Utils.hpp"
#include "stream/StreamProfile.hpp"
#include "exception/ObException.hpp"
#include "property/InternalProperty.hpp"

namespace libobsensor {

LiDARStreamProfileFilter::LiDARStreamProfileFilter(IDevice *owner) : DeviceComponentBase(owner) {
}

StreamProfileList LiDARStreamProfileFilter::filter(const StreamProfileList &profiles) const {
    StreamProfileList filteredProfiles;
    auto              owner      = getOwner();
    auto              propServer = owner->getPropertyServer();
    bool              isCalibrationMode = false; // default is normal mode

    if(propServer->isPropertySupported(OB_PROP_LIDAR_WORK_MODE_INT, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
        auto workMode     = propServer->getPropertyValueT<int32_t>(OB_PROP_LIDAR_WORK_MODE_INT, PROP_ACCESS_INTERNAL);
        isCalibrationMode = workMode == 0x03;
    }

    for(const auto &profile: profiles) {
        if(!profile->is<LiDARStreamProfile>()) {
            filteredProfiles.push_back(profile);
            continue;
        }
        auto format = profile->getFormat();
        if(format == OB_FORMAT_LIDAR_CALIBRATION ) {
            // for calibration mode
            if(!isCalibrationMode) {
                continue;
            }
        }
        else {
            // for normal mode
            if ( isCalibrationMode ) {
                continue;
            }
        }
        // ok
        filteredProfiles.push_back(profile);
    }

    return filteredProfiles;
}

}  // namespace libobsensor
