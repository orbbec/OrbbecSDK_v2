// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "RecommendedPostFilterStrategy.hpp"
#include "RecommendedPostFilterStrategies.hpp"
#include "common/DevicePids.hpp"

#include <algorithm>

namespace libobsensor {

namespace {

// Devices that recommend no post-processing filters at all (e.g. Gemini 210/215).
class EmptyStrategy : public IRecommendedPostFilterStrategy {
public:
    std::vector<std::shared_ptr<IFilter>> createFilters(IDevice *owner, OBSensorType type) override {
        (void)owner;
        (void)type;
        return {};
    }
};

}  // namespace

std::shared_ptr<IRecommendedPostFilterStrategy> RecommendedPostFilterStrategyFactory::create(uint32_t vid, uint32_t pid) {
    // Note: DaBaiA pids are a subset of G330DevPids, so DaBaiA must be checked first.
    if(isDeviceInContainer(DaBaiADevPids, vid, pid)) {
        return createDabaiAPostFilterStrategy();
    }
    else if(isDeviceInContainer(G330DevPids, vid, pid)) {
        return createGemini330PostFilterStrategy();
    }
    else if(isDeviceInContainer(G435LeDevPids, vid, pid)) {
        return createG435LePostFilterStrategy();
    }
    else if(vid == ORBBEC_DEVICE_VID) {
        if(pid == 0x0671) {
            return createGemini2XLPostFilterStrategy();
        }
        // Gemini 210 / 215: handled by G210Device, which recommends no filters.
        else if(pid == 0x0808 || pid == 0x0809) {
            return std::make_shared<EmptyStrategy>();
        }
        else if(std::find(Gemini2DevPids.begin(), Gemini2DevPids.end(), pid) != Gemini2DevPids.end()) {
            return createGemini2PostFilterStrategy();
        }
        else if(std::find(Astra2DevPids.begin(), Astra2DevPids.end(), pid) != Astra2DevPids.end()) {
            return createAstra2PostFilterStrategy();
        }
        else if(std::find(FemtoMegaDevPids.begin(), FemtoMegaDevPids.end(), pid) != FemtoMegaDevPids.end()
                || std::find(FemtoBoltDevPids.begin(), FemtoBoltDevPids.end(), pid) != FemtoBoltDevPids.end()) {
            return createFemtoMegaBoltPostFilterStrategy();
        }
        else if(std::find(G305DevPids.begin(), G305DevPids.end(), pid) != G305DevPids.end()) {
            return createGemini305PostFilterStrategy();
        }
        else if(std::find(OpenNIDevPids.begin(), OpenNIDevPids.end(), pid) != OpenNIDevPids.end()) {
            return createOpenNIPostFilterStrategy();
        }
    }
    return nullptr;
}

}  // namespace libobsensor
