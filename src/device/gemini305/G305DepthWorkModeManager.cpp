// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G305DepthWorkModeManager.hpp"
#include "property/InternalProperty.hpp"
#include "component/comprehensivefilter/DepthPostFilterParamsManager.hpp"
#include "logger/Logger.hpp"

namespace libobsensor {

G305DepthWorkModeManager::G305DepthWorkModeManager(IDevice *owner) : DeviceComponentBase(owner) {
    currentWorkMode_ = {};
    fetchDepthWorkModeList();
}

std::vector<DepthWorkModeItem> G305DepthWorkModeManager::getDepthWorkModeList() const {
    return depthWorkModeList_;
}

const DepthWorkModeItem &G305DepthWorkModeManager::getCurrentDepthWorkMode() const {
    return currentWorkMode_;
}

void G305DepthWorkModeManager::switchDepthWorkMode(const std::string &modeName, const std::string &version) {
    utils::unusedVar(version);
    auto iter =
        std::find_if(depthWorkModeList_.begin(), depthWorkModeList_.end(), [&modeName](const DepthWorkModeItem &item) { return modeName == item.mode.name; });

    if(iter == depthWorkModeList_.end()) {
        std::string totalNames;
        std::for_each(depthWorkModeList_.begin(), depthWorkModeList_.end(), [&totalNames](const DepthWorkModeItem &item) {
            if(!totalNames.empty()) {
                totalNames += ",";
            }
            totalNames += item.mode.name;
        });
        THROW_UNSUPPORTED_OPERATION_EXCEPTION("Invalid depth mode: " + modeName + ", support depth work mode list: " + totalNames);
    }

    switchDepthWorkMode(iter->mode);
}

void G305DepthWorkModeManager::switchDepthWorkMode(const OBDepthWorkModeV2_Internal &targetDepthMode) {
    auto        owner           = getOwner();
    std::string currentModeName = currentWorkMode_.mode.name;
    std::string targetModeName  = targetDepthMode.name;

    if(owner->hasAnySensorStreamActivated()) {
        THROW_UNSUPPORTED_OPERATION_EXCEPTION(utils::string::to_string()
                                              << "Cannot switch depth work mode while any stream is started. Please stop all stream first!");
    }

    if(strncmp(currentWorkMode_.mode.name, targetDepthMode.name, sizeof(targetDepthMode.name)) == 0) {
        LOG_DEBUG("switchDepthWorkMode done with same mode: {1}", currentWorkMode_.mode.name, targetDepthMode.name);
        return;
    }

    {
        auto                     propServer = owner->getPropertyServer();  // get property server first to lock resource to avoid start stream at the same time
        OBDepthWorkMode_Internal mode;
        memcpy(&mode, &targetDepthMode, sizeof(OBDepthWorkMode_Internal));
        propServer->setStructureDataProtoV1_1_T<OBDepthWorkMode_Internal, 0>(OB_STRUCT_CURRENT_DEPTH_ALG_MODE, mode);
    }
    currentWorkMode_ = { targetDepthMode, "" };

    LOG_DEBUG("Device depth work mode have been switch to: {}, device will be reinitialize to apply the new mode.", targetDepthMode.name);

    {
        auto depthPostrFilterParamsManager = owner->getComponentT<DepthPostFilterParamsManager>(OB_DEV_COMPONENT_DEPTH_POST_FILTER_PARAMS_MANAGER, false);
        if(depthPostrFilterParamsManager) {
            TRY_EXECUTE({
                depthPostrFilterParamsManager->fetchParamFromDevice();
                owner->updateDepthPostProcessingFilterList();
            });
        }
    }

    if((currentModeName != kDoubleRgbMode && targetModeName == kDoubleRgbMode) || (targetModeName != kDoubleRgbMode && currentModeName == kDoubleRgbMode)) {
        owner->reset();
    }
    else {
        // refresh device error state after depth work mode changed
        TRY_EXECUTE({ owner->fetchDeviceErrorState(); });
    }
}

void G305DepthWorkModeManager::fetchDepthWorkModeList() {
    auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();

    depthWorkModeList_.clear();
    for(auto &mode: propServer->getStructureDataListProtoV1_1_T<OBDepthWorkMode_Internal, 0>(OB_RAW_DATA_DEPTH_ALG_MODE_LIST)) {
        OBDepthWorkModeV2_Internal item{};
        memcpy(&item, &mode, sizeof(OBDepthWorkMode_Internal));
        depthWorkModeList_.push_back({ item, "" });
    }
    auto v1Current = propServer->getStructureDataProtoV1_1_T<OBDepthWorkMode_Internal, 0>(OB_STRUCT_CURRENT_DEPTH_ALG_MODE);
    memcpy(&currentWorkMode_.mode, &v1Current, sizeof(OBDepthWorkMode_Internal));
    currentWorkMode_.version.clear();
}

}  // namespace libobsensor
