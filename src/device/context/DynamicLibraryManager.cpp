// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DynamicLibraryManager.hpp"
#include "exception/ObException.hpp"
namespace libobsensor {

std::mutex                           DynamicLibraryManager::instanceMutex_;
std::weak_ptr<DynamicLibraryManager> DynamicLibraryManager::instanceWeakPtr_;

std::shared_ptr<DynamicLibraryManager> DynamicLibraryManager::getInstance() {
    std::unique_lock<std::mutex> lock(instanceMutex_);
    auto                         instance = instanceWeakPtr_.lock();
    if(!instance) {
        instance         = std::shared_ptr<DynamicLibraryManager>(new DynamicLibraryManager());
        instanceWeakPtr_ = instance;
    }
    return instance;
}

DynamicLibraryManager::DynamicLibraryManager() {
    deviceSeriesInfoManager_                                                                 = DeviceSeriesInfoManager::getInstance();
    const std::unordered_map<std::string, std::pair<std::string, std::string>> extensionsMap = {
        { "frameprocessor", { "/frameprocessor/", "ob_frame_processor" } },
        { "privfilter", { "/filters/", "ob_priv_filter" } },
        { "filterprocessor", { "/filters/", "FilterProcessor" } },
        { "firmwareupdater", { "/firmwareupdater/", "firmwareupdater" } }
    };

    auto cwd = utils::getCurrentWorkDirectory();
    LOG_DEBUG("Current working directory: {}", cwd);
    for(const auto &libInfo: extensionsMap) {
        try {
            std::string moduleLoadPath = EnvConfig::getExtensionsDirectory() + libInfo.second.first;
            dylib_[libInfo.first]      = std::make_shared<dylib>(moduleLoadPath.c_str(), libInfo.second.second.c_str());
        }
        catch(...) {
            LOG_DEBUG(" - Failed to init library '{}'", libInfo.first);
        }
    }
    setExtensionDeviceInfo();
}

std::shared_ptr<dylib> DynamicLibraryManager::getLibrary(const std::string &extensionName) const {
    auto it = dylib_.find(extensionName);
    return it != dylib_.end() ? it->second : nullptr;
}

typedef void (*pfunc_ob_set_device_info_data)(uint16_t type, const uint8_t *data, uint32_t length, ob_error **error);
template <typename Container> void DynamicLibraryManager::pushToLibrary(std::shared_ptr<dylib> lib, uint16_t type, const Container &container) {
    if(!lib || container.empty()) {
        return;
    }
    ob_error                     *error = nullptr;
    pfunc_ob_set_device_info_data func  = nullptr;
    func                                = lib->get_function<void(uint16_t, const uint8_t *, uint32_t, ob_error **)>("ob_set_device_info_data");
    if(!func) {
        LOG_DEBUG("ob_set_device_info_data is null, ignored.");
        return;
    }
    using ValueType = typename std::remove_reference<decltype(container)>::type::value_type;
    uint32_t len    = static_cast<uint32_t>(container.size() * sizeof(ValueType));
    func(type, reinterpret_cast<const uint8_t *>(container.data()), len, &error);
    LOG_DEBUG(" - Set data for type {} in library", type);
}

template void DynamicLibraryManager::pushToLibrary<std::vector<DeviceIdentifier>>(std::shared_ptr<dylib>, uint16_t, const std::vector<DeviceIdentifier> &);

template void DynamicLibraryManager::pushToLibrary<std::vector<DeviceInfoEntry>>(std::shared_ptr<dylib>, uint16_t, const std::vector<DeviceInfoEntry> &);

void DynamicLibraryManager::setExtensionDeviceInfo() {
    std::vector<std::string> extensions = { "frameprocessor", "firmwareupdater" };

    auto cwd = utils::getCurrentWorkDirectory();
    LOG_DEBUG("Current working directory: {}", cwd);

    std::vector<DeviceIdentifier> pure330;
    std::copy_if(G330DevPids.begin(), G330DevPids.end(), std::back_inserter(pure330), [&](const DeviceIdentifier &dev) {
        return std::find_if(DaBaiADevPids.begin(), DaBaiADevPids.end(), [&](const DeviceIdentifier &d2) { return d2.vid_ == dev.vid_ && d2.pid_ == dev.pid_; })
               == DaBaiADevPids.end();
    });

    std::vector<std::pair<DeviceGroupType, std::vector<DeviceIdentifier> *>> vidPidContainers = {
        { OB_DEVICE_GROUP_GEMINI330_PURE, &pure330 },       { OB_DEVICE_GROUP_DABAIA, &DaBaiADevPids },      { OB_DEVICE_GROUP_GEMINI335LE, &G335LeDevPids },
        { OB_DEVICE_GROUP_GEMINI330_DABAIA, &G330DevPids }, { OB_DEVICE_GROUP_GEMINI435LE, &G435LeDevPids },
    };
    std::vector<std::pair<DeviceGroupType, std::vector<DeviceInfoEntry> *>> infoContainers = {
        { OB_DEVICE_GROUP_GEMINI330_INFO, &G330DeviceInfoList },
    };

    for(const auto &libName: extensions) {
        auto lib = getLibrary(libName);
        if(!lib) {
            LOG_DEBUG("Library '{}' not loaded", libName.c_str());
            continue;
        }

        TRY_EXECUTE({
            for(const auto &c: vidPidContainers) {
                DeviceGroupType type      = c.first;
                auto           &container = *(c.second);
                pushToLibrary(lib, static_cast<uint16_t>(type), container);
            }
            for(const auto &c: infoContainers) {
                DeviceGroupType type      = c.first;
                auto           &container = *(c.second);
                pushToLibrary(lib, static_cast<uint16_t>(type), container);
            }
        });
    }
}

}  // namespace libobsensor
