// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DeviceSeriesInfo.hpp"
#include "DeviceInfoConfig.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"

#include <iostream>
#include <iomanip>
#include <set>
#include <cmrc/cmrc.hpp>
CMRC_DECLARE(ob);

namespace libobsensor {

/**
 * @brief Supported vendor IDs for USB device filtering.
 *
 * Only USB devices whose VID is included in this list will be considered
 * valid during enumeration.
 */
std::unordered_set<uint16_t> supportedUsbVids;
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
std::unordered_map<std::string, uint16_t> manufacturerVidMap;

// Device VID/PID lists by series
std::vector<DeviceIdentifier> G330DevPids;    // G330
std::vector<DeviceIdentifier> G330LDevPids;   // G330L
std::vector<DeviceIdentifier> DaBaiADevPids;  // DaBaiA
std::vector<DeviceIdentifier> G335LeDevPids;  // G335Le
std::vector<DeviceIdentifier> G435LeDevPids;  // G435Le

// DeviceInfo lists by series
std::vector<DeviceInfoEntry> G330DeviceInfoList;    // G330
std::vector<DeviceInfoEntry> G435LeDeviceInfoList;  // G435Le

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
    // load built-in config
    loadXmlConfig("config/DeviceInfoConfig.xml", false);
    // try load external config
    loadXmlConfig("DeviceInfoConfigOEM.xml", true);
}

// merge unique b into a
template <typename Container, typename Equal> void merge_unique_into(Container &a, const Container &b, Equal equal) {
    for(const auto &item_b: b) {
        auto it = std::find_if(a.begin(), a.end(), [&](const typename Container::value_type &item_a) { return equal(item_a, item_b); });
        if(it == a.end()) {
            a.push_back(item_b);
        }
    }
}

// Load device configuration from XML file
// This function reads device definitions and manufacturer information from the specified XML config file.
// It populates the global device information map, series containers,
// supporting both internal and external XML configurations.
void DeviceSeriesInfoManager::loadXmlConfig(const std::string &configFileName, bool isExternalConfig) {
    std::function<void(DeviceInfoConfig &)> throwError = [](DeviceInfoConfig &config) {
        throw libobsensor::invalid_value_exception("Error: " + config.getLastError());
    };
    std::function<void(DeviceInfoConfig &)> ignoreError = [](DeviceInfoConfig &config) { LOG_DEBUG("Ignore error: {}", config.getLastError()); };
    std::function<void(DeviceInfoConfig &)> errHandle   = isExternalConfig ? ignoreError : throwError;

    // Load config
    DeviceInfoConfig config;
    if(!config.loadConfigFile(configFileName, isExternalConfig)) {
        errHandle(config);
        return;
    }

    auto                          vidList = config.getVidList();
    std::vector<DeviceIdentifier> g330VidpidList;
    std::vector<DeviceIdentifier> g330LVidpidList;
    std::vector<DeviceIdentifier> daBaiAVidpidList;
    std::vector<DeviceIdentifier> g335LeVidpidList;
    std::vector<DeviceIdentifier> g435LeVidpidList;
    std::vector<DeviceInfoEntry>  g330DevInfoList;
    std::vector<DeviceInfoEntry>  g435LeDevInfoList;
    std::vector<DeviceInfoEntry>  tmpDevInfoList;

    // Get device information for all categories
    if(!config.readDeviceList(CATEGORY_G330_DEVICE, g330VidpidList, g330DevInfoList)) {
        errHandle(config);
        return;
    }
    tmpDevInfoList.clear();
    if(!config.readDeviceList(CATEGORY_G330L_DEVICE, g330LVidpidList, tmpDevInfoList)) {
        errHandle(config);
        return;
    }
    tmpDevInfoList.clear();
    if(!config.readDeviceList(CATEGORY_DABAIA_DEVICE, daBaiAVidpidList, tmpDevInfoList)) {
        errHandle(config);
        return;
    }
    tmpDevInfoList.clear();
    if(!config.readDeviceList(CATEGORY_G335LE_DEVICE, g335LeVidpidList, tmpDevInfoList)) {
        errHandle(config);
        return;
    }
    if(!config.readDeviceList(CATEGORY_G435LE_DEVICE, g435LeVidpidList, g435LeDevInfoList)) {
        errHandle(config);
        return;
    }
    // Read GVCP config
    std::unordered_map<std::string, uint16_t> gvcpConfigList;
    if(!config.readGVCPConfig(gvcpConfigList)) {
        errHandle(config);
        return;
    }

    // Merge config
    // DeviceIdentifier
    auto compareDevId = [](const DeviceIdentifier &a, const DeviceIdentifier &b) { return a.vid_ == b.vid_ && a.pid_ == b.pid_; };
    merge_unique_into(G330DevPids, g330VidpidList, compareDevId);
    merge_unique_into(G330LDevPids, g330LVidpidList, compareDevId);
    merge_unique_into(DaBaiADevPids, daBaiAVidpidList, compareDevId);
    merge_unique_into(G335LeDevPids, g335LeVidpidList, compareDevId);
    merge_unique_into(G435LeDevPids, g435LeVidpidList, compareDevId);
    // DeviceInfoEntry
    auto compareDevInfo = [](const DeviceInfoEntry &a, const DeviceInfoEntry &b) { return a.vid_ == b.vid_ && a.pid_ == b.pid_; };
    merge_unique_into(G330DeviceInfoList, g330DevInfoList, compareDevInfo);
    merge_unique_into(G435LeDeviceInfoList, g435LeDevInfoList, compareDevInfo);
    // vid list
    for(auto vid: vidList) {
        // If the value already exists, insertion is ignored
        supportedUsbVids.insert(vid);
    }
    // GVCP config: Insert key-value pair into the map.
    for(const auto &kv: gvcpConfigList) {
        // If the value already exists, insertion is ignored
        manufacturerVidMap.insert(kv);
    }
}

bool isDeviceInContainer(const std::vector<DeviceIdentifier> &deviceContainer, const uint32_t &vid, const uint32_t &pid) {
    return std::find_if(deviceContainer.begin(), deviceContainer.end(),
                        [vid, pid](const DeviceIdentifier &device) { return device.vid_ == vid && device.pid_ == pid; })
           != deviceContainer.end();
}

bool isDeviceInOrbbecSeries(const std::vector<uint16_t> &deviceContainer, const uint32_t &vid, const uint32_t &pid) {
    if(vid != ORBBEC_DEVICE_VID) {
        return false;
    }
    return std::find(deviceContainer.begin(), deviceContainer.end(), pid) != deviceContainer.end();
}

}  // namespace libobsensor
