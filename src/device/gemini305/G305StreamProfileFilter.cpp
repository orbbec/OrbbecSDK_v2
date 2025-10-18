// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G305StreamProfileFilter.hpp"

#include "utils/Utils.hpp"
#include "stream/StreamProfile.hpp"
#include "exception/ObException.hpp"
#include "property/InternalProperty.hpp"

namespace libobsensor {

G305StreamProfileFilter::G305StreamProfileFilter(IDevice *owner) : DeviceComponentBase(owner) {
    fetchPresetResolutionConfig();
}

void G305StreamProfileFilter::fetchPresetResolutionConfig() {
    prestResConfig_ = {};
    TRY_EXECUTE({
        auto owner      = getOwner();
        auto propServer = owner->getPropertyServer();
        prestResConfig_ = propServer->getStructureDataT<OBPresetResolutionConfig>(OB_STRUCT_PRESET_RESOLUTION_CONFIG);
        if(prestResConfig_.depthDecimationFactor <= 0 ) {
            prestResConfig_.depthDecimationFactor = 1;
        }
        if(prestResConfig_.irDecimationFactor <= 0) {
            prestResConfig_.irDecimationFactor = 1;
        }
    });
}

void G305StreamProfileFilter::onPresetResolutionConfigChanged(const OBPresetResolutionConfig* config) {
    if(config == nullptr) {
        fetchPresetResolutionConfig();
    } else {
        prestResConfig_ = *config;
    }
}

StreamProfileList G305StreamProfileFilter::filter(const StreamProfileList &profiles) const {
    StreamProfileList filteredProfiles;
    auto              config = prestResConfig_;

    if(config.width <= 0 || config.height <= 0) {
        // Preset resolution config is unsupported
        filteredProfiles = profiles;
        return filteredProfiles;
    }

    uint32_t irWidth     = static_cast<uint32_t>(config.width / config.irDecimationFactor);
    uint32_t irHeight    = static_cast<uint32_t>(config.height / config.irDecimationFactor);
    uint32_t depthWidth  = static_cast<uint32_t>(config.width / config.depthDecimationFactor);
    uint32_t depthHeight = static_cast<uint32_t>(config.height / config.depthDecimationFactor);
    for(const auto &profile: profiles) {
        if(!profile->is<VideoStreamProfile>()) {
            filteredProfiles.push_back(profile);
            continue;
        }
        auto vsp        = profile->as<VideoStreamProfile>();
        auto streamType = profile->getType();
        auto sensorType = utils::mapStreamTypeToSensorType(streamType);
        if(isIRSensor(sensorType)) {
            if(vsp->getWidth() != irWidth || vsp->getHeight() != irHeight) {
                // invalid
                continue;
            }
        }
        else if(sensorType == OB_SENSOR_DEPTH) {
            if(vsp->getWidth() != depthWidth || vsp->getHeight() != depthHeight) {
                // invalid
                continue;
            }
        }
        // ok, add to list
        filteredProfiles.push_back(profile);
        continue;
    }

    return filteredProfiles;
}

}  // namespace libobsensor
