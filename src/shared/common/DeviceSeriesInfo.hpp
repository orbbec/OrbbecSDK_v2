#pragma once

#include "libobsensor/h/ObTypes.h"
#include "CommonFields.hpp"
#include "utils/Utils.hpp"
#include "xml/XmlReader.hpp"

#include <vector>
#include <map>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include <mutex>
#include <unordered_set>
#include <string>
#include <initializer_list>

namespace libobsensor {

extern std::vector<DeviceIdentifier> G330DevPids;
extern std::vector<DeviceIdentifier> G330LDevPids;
extern std::vector<DeviceIdentifier> DaBaiADevPids;
extern std::vector<DeviceIdentifier> G335LeDevPids;
extern std::vector<DeviceIdentifier> G435LeDevPids;

extern std::vector<DeviceInfoEntry> G330DeviceInfoList;
extern std::vector<DeviceInfoEntry> G435LeDeviceInfoList;

// List of allowed vendor IDs
extern std::unordered_set<uint16_t>              supportedUsbVids;
extern std::unordered_map<std::string, uint16_t> manufacturerVidMap;

bool isDeviceInContainer(const std::vector<DeviceIdentifier> &deviceContainer, const uint32_t &vid, const uint32_t &pid);
bool isDeviceInOrbbecSeries(const std::vector<uint16_t> &deviceContainer, const uint32_t &vid, const uint32_t &pid);

class DeviceSeriesInfoManager {
public:
    // Get singleton instance
    static std::shared_ptr<DeviceSeriesInfoManager> getInstance();

private:
    DeviceSeriesInfoManager();

    // Load additional device configuration from XML
    void loadXmlConfig(const std::string &configFileName, bool isExternalConfig);

private:
    static std::weak_ptr<DeviceSeriesInfoManager> instanceWeakPtr_;
    static std::mutex                             instanceMutex_;
};

}  // namespace libobsensor
