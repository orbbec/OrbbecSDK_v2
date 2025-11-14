// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/DeviceSeriesInfo.hpp"

#include "environment/EnvConfig.hpp"
#include "logger/Logger.hpp"

#include <iostream>
#include <iomanip>
#include <set>

namespace libobsensor {
/**
 * @brief Supported vendor IDs for USB device filtering.
 *
 * Only USB devices whose VID is included in this list will be considered
 * valid during enumeration.
 */
std::vector<uint16_t> supportedUsbVids = {};
/**
 * @brief Mapping of manufacturer names to vendor IDs for supported devices.
 *
 * This container serves different purposes depending on the device type:
 * - For network (GVCP-based) devices: used to resolve the vendor ID (VID)
 *   from the reported manufacturer name during discovery, and to filter
 *   out unsupported manufacturers.
 * - For USB devices: used to retrieve the manufacturer name from a given VID;
 *   no filtering is performed.
 */
std::unordered_map<std::string, uint16_t> manufacturerVidMap = {};

// Device VID/PID lists by series
std::vector<DeviceIdentifier> G330DevPids;    // G330
std::vector<DeviceIdentifier> G330LDevPids;   // G330L
std::vector<DeviceIdentifier> DaBaiADevPids;  // DaBaiA
std::vector<DeviceIdentifier> G335LeDevPids;  // G335Le
std::vector<DeviceIdentifier> G435LeDevPids;  // G435Le

// DeviceInfo lists by series
std::vector<DeviceInfoEntry> G330DeviceInfoList;    // G330
std::vector<DeviceInfoEntry> G435LeDeviceInfoList;  // G435Le

// Map of all series to their DeviceInfo lists
std::unordered_map<std::string, std::vector<DeviceInfoEntry>> allDeviceInfoMap_;  // all devices

std::weak_ptr<DeviceSeriesInfoManager>   DeviceSeriesInfoManager::instanceWeakPtr_;
std::mutex                               DeviceSeriesInfoManager::instanceMutex_;
std::shared_ptr<DeviceSeriesInfoManager> DeviceSeriesInfoManager::getInstance() {
    std::lock_guard<std::mutex> lock(instanceMutex_);
    auto                        instance = instanceWeakPtr_.lock();
    if(!instance) {
        instance         = std::shared_ptr<DeviceSeriesInfoManager>(new DeviceSeriesInfoManager());
        instanceWeakPtr_ = instance;
    }
    return instance;
}

DeviceSeriesInfoManager::DeviceSeriesInfoManager() {
    LoadXmlConfig();
}

// Load device configuration from XML file
void DeviceSeriesInfoManager::LoadXmlConfig() {
    auto                     devInfoConfig = DevInfoConfig::getInstance();  // Get EnvConfig singleton
    std::string              baseNode      = NODE_DEVICE_CONFIG_BASE;       // Base node for devices
    std::vector<std::string> devNodes;
    if(!devInfoConfig->getChildNodeNames(baseNode + "." + std::string(NODE_DEVLIST), devNodes)) {
        LOG_DEBUG("DevList not found!");
        return;
    }

    // Load device info
    std::string vidStr;
    std::string pidStr;
    std::string deviceName       = "DeviceName";
    std::string manuFacturerName = "Manufacturer";
    for(auto &dev: devNodes) {
        std::string node = std::string(NODE_DEVLIST) + "." + dev;
        devInfoConfig->getStringValue(node + "." + std::string(FIELD_VID), vidStr);
        devInfoConfig->getStringValue(node + "." + std::string(FIELD_PID), pidStr);
        devInfoConfig->getStringValue(node + "." + std::string(FIELD_NAME), deviceName);
        devInfoConfig->getStringValue(node + "." + std::string(FIELD_MANUFACTURER), manuFacturerName);
        if(vidStr.empty() || pidStr.empty()) {
            LOG_DEBUG("Invalid device node: {}", dev.c_str());
            continue;
        }

        uint16_t        vid = static_cast<uint16_t>(std::stoi(vidStr, nullptr, 16));
        uint16_t        pid = static_cast<uint16_t>(std::stoi(pidStr, nullptr, 16));
        DeviceInfoEntry entry{};
        entry.vid_ = vid;
        entry.pid_ = pid;
        std::snprintf(entry.deviceName_, sizeof(entry.deviceName_), "%s", deviceName.c_str());
        std::snprintf(entry.manufacturer_, sizeof(entry.manufacturer_), "%s", manuFacturerName.c_str());
        allDeviceInfoMap_[dev].push_back(entry);
        if(std::find(supportedUsbVids.begin(), supportedUsbVids.end(), vid) == supportedUsbVids.end()) {
            supportedUsbVids.push_back(vid);
        }
    }

    std::vector<std::string> gvcpConfigNodes;
    if(devInfoConfig->getChildNodeNames(NODE_GVCP_CONFIG, gvcpConfigNodes)) {
        for(const auto &gvcpNode: gvcpConfigNodes) {
            std::string node = std::string(NODE_GVCP_CONFIG) + "." + gvcpNode;
            std::string vidString;
            std::string manufacturerName;
            devInfoConfig->getAttributeValue(node, FIELD_MANUFACTURER_VID, vidString);
            devInfoConfig->getAttributeValue(node, FIELD_MANUFACTURER_NAME, manufacturerName);
            if(!vidStr.empty() && !manufacturerName.empty()) {
                uint16_t vid                         = static_cast<uint16_t>(std::stoi(vidString, nullptr, 16));
                manufacturerVidMap[manufacturerName] = vid;
            }
        }
    }

    auto handleCategory = [&](const std::string &categoryNode, std::vector<DeviceIdentifier> &container, std::vector<DeviceInfoEntry> &deviceInfoEntries) {
        std::vector<std::string> deviceNames;
        if(!devInfoConfig->getChildNodeTextList(categoryNode + "." + std::string(FIELD_DEVICE), deviceNames)) {
            return;
        }
        for(const auto &devName: deviceNames) {
            auto it = allDeviceInfoMap_.find(devName);
            if(it != allDeviceInfoMap_.end()) {
                for(const auto &devInfo: it->second) {
                    auto it2 = std::find_if(container.begin(), container.end(), [&devInfo](const DeviceIdentifier &existingVidPid) {
                        return existingVidPid.vid_ == devInfo.vid_ && existingVidPid.pid_ == devInfo.pid_;
                    });
                    if(it2 != container.end()) {
                        continue;
                    }
                    container.push_back(DeviceIdentifier{ devInfo.vid_, devInfo.pid_ });
                    DeviceInfoEntry entry{};
                    entry.vid_ = devInfo.vid_;
                    entry.pid_ = devInfo.pid_;
                    std::snprintf(entry.deviceName_, sizeof(entry.deviceName_), "%s", devInfo.deviceName_);
                    std::snprintf(entry.manufacturer_, sizeof(entry.manufacturer_), "%s", devInfo.manufacturer_);
                    deviceInfoEntries.push_back(entry);
                }
            }
        }
    };

    std::vector<DeviceInfoEntry> emptyMap;
    handleCategory(CATEGORY_G330_DEVICE, G330DevPids, G330DeviceInfoList);
    handleCategory(CATEGORY_G330L_DEVICE, G330LDevPids, emptyMap);
    handleCategory(CATEGORY_DABAIA_DEVICE, DaBaiADevPids, emptyMap);
    handleCategory(CATEGORY_G335LE_DEVICE, G335LeDevPids, emptyMap);
    handleCategory(CATEGORY_G435LE_DEVICE, G435LeDevPids, G435LeDeviceInfoList);
}

}  // namespace libobsensor
