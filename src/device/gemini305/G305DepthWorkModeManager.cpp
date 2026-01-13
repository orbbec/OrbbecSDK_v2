// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G305DepthWorkModeManager.hpp"
#include "property/InternalProperty.hpp"
#include "logger/Logger.hpp"

namespace libobsensor {

G305DepthWorkModeManager::G305DepthWorkModeManager(IDevice *owner) : DeviceComponentBase(owner) {
    fetchDepthWorkModeList();
}

std::vector<OBDepthWorkMode_Internal> G305DepthWorkModeManager::getDepthWorkModeList() const {
    return depthWorkModeList_;
}

const OBDepthWorkMode_Internal &G305DepthWorkModeManager::getCurrentDepthWorkMode() const {
    return currentWorkMode_;
}

void G305DepthWorkModeManager::switchDepthWorkMode(const std::string &modeName) {
    auto iter =
        std::find_if(depthWorkModeList_.begin(), depthWorkModeList_.end(), [&modeName](const OBDepthWorkMode_Internal &mode) { return modeName == mode.name; });

    if(iter == depthWorkModeList_.end()) {
        std::string totalNames;
        std::for_each(depthWorkModeList_.begin(), depthWorkModeList_.end(), [&totalNames](const OBDepthWorkMode_Internal &mode) {
            if(!totalNames.empty()) {
                totalNames += ",";
            }
            totalNames += mode.name;
        });
        throw unsupported_operation_exception("Invalid depth mode: " + modeName + ", support depth work mode list: " + totalNames);
    }

    OBDepthWorkMode_Internal dstMode = *iter;
    switchDepthWorkMode(dstMode);
}

void G305DepthWorkModeManager::switchDepthWorkMode(const OBDepthWorkMode_Internal &targetDepthMode) {
    auto        owner           = getOwner();
    std::string currentModeName = currentWorkMode_.name;
    std::string targetModeName  = targetDepthMode.name;

    if(owner->hasAnySensorStreamActivated()) {
        throw unsupported_operation_exception(utils::string::to_string()
                                              << "Cannot switch depth work mode while any stream is started. Please stop all stream first!");
    }

    if(strncmp(currentWorkMode_.name, targetDepthMode.name, sizeof(targetDepthMode.name)) == 0) {
        LOG_DEBUG("switchDepthWorkMode done with same mode: {1}", currentWorkMode_.name, targetDepthMode.name);
        return;
    }

    {
        auto propServer = owner->getPropertyServer();  // get property server first to lock resource to avoid start stream at the same time
        propServer->setStructureDataProtoV1_1_T<OBDepthWorkMode_Internal, 0>(OB_STRUCT_CURRENT_DEPTH_ALG_MODE, targetDepthMode);
    }
    currentWorkMode_ = targetDepthMode;

    LOG_DEBUG("Device depth work mode have been switch to: {}, device will be reinitialize to apply the new mode.", targetDepthMode.name);

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

    depthWorkModeList_ = propServer->getStructureDataListProtoV1_1_T<OBDepthWorkMode_Internal, 0>(OB_RAW_DATA_DEPTH_ALG_MODE_LIST);
    currentWorkMode_   = propServer->getStructureDataProtoV1_1_T<OBDepthWorkMode_Internal, 0>(OB_STRUCT_CURRENT_DEPTH_ALG_MODE);
}

}  // namespace libobsensor
