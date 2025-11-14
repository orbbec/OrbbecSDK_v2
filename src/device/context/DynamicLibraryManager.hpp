// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include "dylib.hpp"
#include "utils/Utils.hpp"
#include "logger/Logger.hpp"
#include "environment/EnvConfig.hpp"
#include "common/DeviceSeriesInfo.hpp"

namespace libobsensor {

class DynamicLibraryManager {
public:
    static std::shared_ptr<DynamicLibraryManager> getInstance();
    std::shared_ptr<dylib>                        getLibrary(const std::string &moduleName) const;
    template <typename Container> void            pushToLibrary(std::shared_ptr<dylib> lib, uint16_t type, const Container &container);
    void                                          setExtensionDeviceInfo();

private:
    DynamicLibraryManager();

    static std::mutex                                       instanceMutex_;
    static std::weak_ptr<DynamicLibraryManager>             instanceWeakPtr_;
    std::shared_ptr<DeviceSeriesInfoManager>                deviceSeriesInfoManager_;
    std::unordered_map<std::string, std::shared_ptr<dylib>> dylib_;
};

}  // namespace libobsensor
