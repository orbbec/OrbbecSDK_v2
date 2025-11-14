#pragma once

#include "libobsensor/h/ObTypes.h"
#include "CommonFields.hpp"

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

extern std::unordered_map<std::string, std::vector<DeviceInfoEntry>> allDeviceInfoMap_;
// List of allowed vendor IDs
extern std::vector<uint16_t>                     supportedUsbVids;
extern std::unordered_map<std::string, uint16_t> manufacturerVidMap;

template <typename Container> bool isDeviceInContainer(const Container &deviceContainer, uint32_t vid, uint32_t pid) {
    return std::find_if(deviceContainer.begin(), deviceContainer.end(),
                        [vid, pid](const typename Container::value_type &device) { return device.vid_ == vid && device.pid_ == pid; })
           != deviceContainer.end();
}

class DeviceSeriesInfoManager {
public:
    // Get singleton instance
    static std::shared_ptr<DeviceSeriesInfoManager> getInstance();
    // Load additional device configuration from XML
    void LoadXmlConfig();

private:
    DeviceSeriesInfoManager();

    static std::weak_ptr<DeviceSeriesInfoManager> instanceWeakPtr_;
    static std::mutex                             instanceMutex_;
};

}  // namespace libobsensor
