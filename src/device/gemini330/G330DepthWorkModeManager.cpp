// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G330DepthWorkModeManager.hpp"
#include "property/InternalProperty.hpp"
#include "component/comprehensivefilter/DepthPostFilterParamsManager.hpp"
#include "logger/Logger.hpp"

namespace libobsensor {

G330DepthWorkModeManager::G330DepthWorkModeManager(IDevice *owner) : DeviceComponentBase(owner) {
    currentWorkMode_  = {};
    versionSupported_ = false;
    fetchDepthWorkModeList();
}

std::vector<DepthWorkModeItem> G330DepthWorkModeManager::getDepthWorkModeList() const {
    return depthWorkModeItemList_;
}

const DepthWorkModeItem &G330DepthWorkModeManager::getCurrentDepthWorkMode() const {
    return currentWorkMode_;
}

bool G330DepthWorkModeManager::isVersionSupported() const {
    return versionSupported_;
}

void G330DepthWorkModeManager::switchDepthWorkMode(const std::string &modeName, const std::string &version) {
    auto iter = depthWorkModeItemList_.end();
    for(auto it = depthWorkModeItemList_.begin(); it != depthWorkModeItemList_.end(); ++it) {
        if(modeName != it->mode.name) {
            continue;
        }
        if(iter == depthWorkModeItemList_.end() || it->mode.version > iter->mode.version) {
            iter = it;
        }
    }
    if(!version.empty()) {
        auto exactIter = std::find_if(depthWorkModeItemList_.begin(), depthWorkModeItemList_.end(), [&modeName, &version](const DepthWorkModeItem &item) {
            return (modeName == item.mode.name) && (version == item.version);
        });
        if(exactIter == depthWorkModeItemList_.end()) {
            LOG_ERROR("Invalid depth mode version: {}, depth mode: {}", version, modeName);
            THROW_INVALID_PARAM_EXCEPTION("Invalid depth mode version: " + version + ", depth mode: " + modeName);
        }
        iter = exactIter;
    }

    if(iter == depthWorkModeItemList_.end()) {
        std::string totalNames;
        std::for_each(depthWorkModeItemList_.begin(), depthWorkModeItemList_.end(), [&totalNames](const DepthWorkModeItem &item) {
            if(!totalNames.empty()) {
                totalNames += ",";
            }
            totalNames += item.mode.name;
        });
        THROW_UNSUPPORTED_OPERATION_EXCEPTION("Invalid depth mode: " + modeName + ", support depth work mode list: " + totalNames);
    }

    switchDepthWorkMode(iter->mode);
}

void G330DepthWorkModeManager::switchDepthWorkMode(const OBDepthWorkModeV2_Internal &targetDepthMode) {
    auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();  // get property server first to lock resource to avoid start stream at the same time

    if(owner->hasAnySensorStreamActivated()) {
        THROW_UNSUPPORTED_OPERATION_EXCEPTION(utils::string::to_string()
                                              << "Cannot switch depth work mode while any stream is started. Please stop all stream first!");
    }

    if(strncmp(currentWorkMode_.mode.name, targetDepthMode.name, sizeof(targetDepthMode.name)) == 0
       && currentWorkMode_.mode.version == targetDepthMode.version) {
        LOG_DEBUG("switchDepthWorkMode done with same mode: {0}, version: {1}", currentWorkMode_.mode.name, targetDepthMode.version);
        return;
    }

    if(versionSupported_) {
        propServer->setStructureDataProtoV1_1_T<OBDepthWorkModeV2_Internal, 1>(OB_STRUCT_CURRENT_DEPTH_ALG_MODE_V2, targetDepthMode);
    }
    else {
        OBDepthWorkMode_Internal mode;
        memcpy(&mode, &targetDepthMode, sizeof(OBDepthWorkMode_Internal));
        propServer->setStructureDataProtoV1_1_T<OBDepthWorkMode_Internal, 0>(OB_STRUCT_CURRENT_DEPTH_ALG_MODE, mode);
    }
    auto targetIter  = std::find_if(depthWorkModeItemList_.begin(), depthWorkModeItemList_.end(), [&targetDepthMode](const DepthWorkModeItem &item) {
        return strncmp(targetDepthMode.name, item.mode.name, sizeof(item.mode.name)) == 0 && targetDepthMode.version == item.mode.version;
    });
    currentWorkMode_ = targetIter != depthWorkModeItemList_.end() ? *targetIter : DepthWorkModeItem{ targetDepthMode, "" };
    LOG_DEBUG("switchDepthWorkMode done with mode: {0}, version: {1}", currentWorkMode_.mode.name, targetDepthMode.version);

    auto depthPostrFilterParamsManager = owner->getComponentT<DepthPostFilterParamsManager>(OB_DEV_COMPONENT_DEPTH_POST_FILTER_PARAMS_MANAGER, false);
    if(depthPostrFilterParamsManager) {
        TRY_EXECUTE({
            depthPostrFilterParamsManager->fetchParamFromDevice();
            owner->updateDepthPostProcessingFilterList();
        });
    }

    // refresh device error state after depth work mode changed
    TRY_EXECUTE({ owner->fetchDeviceErrorState(); });
}

void G330DepthWorkModeManager::fetchDepthWorkModeList() {
    auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();

    depthWorkModeItemList_.clear();
    versionSupported_ = false;

    // V2 path: used exclusively when available; fall back to V1 on any failure
    if(propServer->isPropertySupported(OB_RAW_DATA_DEPTH_ALG_MODE_LIST_V2, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
        try {
            auto workModeList = propServer->getStructureDataListProtoV1_1_T<OBDepthWorkModeV2_Internal, 1>(OB_RAW_DATA_DEPTH_ALG_MODE_LIST_V2);
            if(!workModeList.empty()) {
                auto currentMode  = propServer->getStructureDataProtoV1_1_T<OBDepthWorkModeV2_Internal, 1>(OB_STRUCT_CURRENT_DEPTH_ALG_MODE_V2);
                versionSupported_ = true;
                for(auto &mode: workModeList) {
                    uint32_t exposedVersion = (mode.optionCode & CUSTOM_DEPTH_MODE_TAG) ? mode.version : 0;
                    depthWorkModeItemList_.push_back({ mode, utils::string::versionToString(exposedVersion) });
                }
                currentWorkMode_ = { currentMode, utils::string::versionToString((currentMode.optionCode & CUSTOM_DEPTH_MODE_TAG) ? currentMode.version : 0) };
                // keep the current item's version string aligned with the list entry
                auto currentIter = std::find_if(depthWorkModeItemList_.begin(), depthWorkModeItemList_.end(), [&currentMode](const DepthWorkModeItem &item) {
                    return strncmp(currentMode.name, item.mode.name, sizeof(item.mode.name)) == 0 && currentMode.version == item.mode.version;
                });
                if(currentIter != depthWorkModeItemList_.end()) {
                    currentWorkMode_ = *currentIter;
                }
                return;
            }
        }
        catch(std::exception &e) {
            LOG_INFO("V2 depth work mode protocol unavailable, fallback to V1: {}", e.what());
        }
        depthWorkModeItemList_.clear();
    }

    // V1 path (legacy firmware): fill the V2 layout, version is always 0
    auto v1List = propServer->getStructureDataListProtoV1_1_T<OBDepthWorkMode_Internal, 0>(OB_RAW_DATA_DEPTH_ALG_MODE_LIST);
    for(auto &mode: v1List) {
        OBDepthWorkModeV2_Internal item{};
        memcpy(&item, &mode, sizeof(OBDepthWorkMode_Internal));
        depthWorkModeItemList_.push_back({ item, "" });
    }
    auto v1Current = propServer->getStructureDataProtoV1_1_T<OBDepthWorkMode_Internal, 0>(OB_STRUCT_CURRENT_DEPTH_ALG_MODE);
    memcpy(&currentWorkMode_.mode, &v1Current, sizeof(OBDepthWorkMode_Internal));
    currentWorkMode_.version.clear();
}

}  // namespace libobsensor
