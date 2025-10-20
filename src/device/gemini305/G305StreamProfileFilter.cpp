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

    // calc size from origin size and factor
    auto calcSize = [](int originSize, int factor) -> uint32_t {
        if(factor <= 0) {
            return static_cast<uint32_t>(originSize);
        }

        // Floor division since originSize, factor >= 0
        auto size = static_cast<uint32_t>(originSize / factor);
        // Round to the nearest even integer
        if(size % 2 != 0) {
            --size;
        }
        return size;
    };

    uint32_t irWidth        = calcSize(config.width, config.irDecimationFactor);
    uint32_t irHeight       = calcSize(config.height, config.irDecimationFactor);
    uint32_t depthWidth     = calcSize(config.width, config.depthDecimationFactor);
    uint32_t depthHeight    = calcSize(config.height, config.depthDecimationFactor);
    uint32_t originalWidth  = static_cast<uint32_t>(config.width);
    uint32_t originalHeight = static_cast<uint32_t>(config.height);

    std::vector<std::shared_ptr<const VideoStreamProfile>> irOriginalResolution;
    std::vector<std::shared_ptr<const VideoStreamProfile>> depthOriginalResolution;

    for(const auto &profile: profiles) {
        if(!profile->is<VideoStreamProfile>()) {
            continue;
        }
        auto vsp        = profile->as<VideoStreamProfile>();
        auto streamType = profile->getType();
        auto sensorType = utils::mapStreamTypeToSensorType(streamType);
        if(isIRSensor(sensorType)) {
            if(vsp->getOriginalWidth() != originalWidth || vsp->getOriginalHeight() != originalHeight) {
                // invalid
                continue;
            }
            irOriginalResolution.push_back(vsp);
        }
        else if(sensorType == OB_SENSOR_DEPTH) {
            if(vsp->getOriginalWidth() != originalWidth || vsp->getOriginalHeight() != originalHeight) {
                // invalid
                continue;
            }
            depthOriginalResolution.push_back(vsp);
        }
    }

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
            if(!irOriginalResolution.empty()) {
                auto it = std::find_if(irOriginalResolution.begin(), irOriginalResolution.end(),
                                       [&](std::shared_ptr<const VideoStreamProfile> p) { return p->getFps() == vsp->getFps(); });
                if(it == irOriginalResolution.end()) {
                    continue;
                }
            }
            auto vspMutable = std::const_pointer_cast<VideoStreamProfile>(vsp);
            vspMutable->setOriginalHeight(originalHeight);
            vspMutable->setOriginalWidth(originalWidth);
        }
        else if(sensorType == OB_SENSOR_DEPTH) {
            if(vsp->getWidth() != depthWidth || vsp->getHeight() != depthHeight) {
                // invalid
                continue;
            }
            if(!depthOriginalResolution.empty()) {
                auto it = std::find_if(depthOriginalResolution.begin(), depthOriginalResolution.end(),
                                       [&](std::shared_ptr<const VideoStreamProfile> p) { return p->getFps() == vsp->getFps(); });
                if(it == depthOriginalResolution.end()) {
                    continue;
                }
            }
            auto vspMutable = std::const_pointer_cast<VideoStreamProfile>(vsp);
            vspMutable->setOriginalHeight(originalHeight);
            vspMutable->setOriginalWidth(originalWidth);
        }

        // ok, add to list
        filteredProfiles.push_back(profile);
        continue;
    }

    return filteredProfiles;
}

}  // namespace libobsensor
