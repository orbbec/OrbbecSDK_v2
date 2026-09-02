// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G2DepthWorkModeManager.hpp"
#include "property/InternalProperty.hpp"
#include "logger/Logger.hpp"
#include "environment/EnvConfig.hpp"

namespace libobsensor {

G2DepthWorkModeManager::G2DepthWorkModeManager(IDevice *owner) : DeviceComponentBase(owner) {
    currentWorkMode_ = {};
    fetchDepthWorkModeList();
}

std::vector<DepthWorkModeItem> G2DepthWorkModeManager::getDepthWorkModeList() const {
    return depthWorkModeList_;
}

const DepthWorkModeItem &G2DepthWorkModeManager::getCurrentDepthWorkMode() const {
    return currentWorkMode_;
}

void G2DepthWorkModeManager::switchDepthWorkMode(const std::string &modeName, const std::string &version) {
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

void G2DepthWorkModeManager::switchDepthWorkMode(const OBDepthWorkModeV2_Internal &targetDepthMode) {
    auto owner = getOwner();
    {
        auto propServer = owner->getPropertyServer();  // get property server first to lock resource to avoid start stream at the same time

        if(owner->hasAnySensorStreamActivated()) {
            THROW_UNSUPPORTED_OPERATION_EXCEPTION(utils::string::to_string()
                                                  << "Cannot switch depth work mode while any stream is started. Please stop all stream first!");
        }

        if(strncmp(currentWorkMode_.mode.name, targetDepthMode.name, sizeof(targetDepthMode.name)) == 0) {
            LOG_DEBUG("switchDepthWorkMode done with same mode: {}", currentWorkMode_.mode.name, targetDepthMode.name);
            return;
        }

        OBDepthWorkMode_Internal mode;
        memcpy(&mode, &targetDepthMode, sizeof(OBDepthWorkMode_Internal));
        propServer->setStructureDataProtoV1_1_T<OBDepthWorkMode_Internal, 0>(OB_STRUCT_CURRENT_DEPTH_ALG_MODE, mode);
        currentWorkMode_ = { targetDepthMode, "" };
    }

    LOG_INFO("Device depth work mode have been switch to: {}, device will be reinitialize to apply the new mode.", targetDepthMode.name);
    owner->reset();
}

void G2DepthWorkModeManager::fetchDepthWorkModeList() {
    auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();

    std::vector<OBDepthWorkMode_Internal> modeList = propServer->getStructureDataListProtoV1_1_T<OBDepthWorkMode_Internal, 0>(OB_RAW_DATA_DEPTH_ALG_MODE_LIST);

    auto        envConfig            = EnvConfig::getInstance();
    std::string nodePath             = "Device." + getOwner()->getInfo()->name_;
    nodePath                         = utils::string::removeSpace(nodePath);
    bool isSupportFactoryCalibration = false;
    envConfig->getBooleanValue(nodePath + ".SupportFactoryCalibration", isSupportFactoryCalibration);
    if(!isSupportFactoryCalibration) {
        // remove factory Calibration mode
        modeList.erase(std::remove_if(modeList.begin(), modeList.end(),
                                      [](const OBDepthWorkMode_Internal &mode) {  //
                                          return strncmp(mode.name, "Factory Calibration", sizeof(mode.name)) == 0;
                                      }),
                       modeList.end());
    }

    depthWorkModeList_.clear();
    for(auto &mode: modeList) {
        OBDepthWorkModeV2_Internal item{};
        memcpy(&item, &mode, sizeof(OBDepthWorkMode_Internal));
        depthWorkModeList_.push_back({ item, "" });
    }
    auto v1Current = propServer->getStructureDataProtoV1_1_T<OBDepthWorkMode_Internal, 0>(OB_STRUCT_CURRENT_DEPTH_ALG_MODE);
    memcpy(&currentWorkMode_.mode, &v1Current, sizeof(OBDepthWorkMode_Internal));
    currentWorkMode_.version.clear();
}

}  // namespace libobsensor
