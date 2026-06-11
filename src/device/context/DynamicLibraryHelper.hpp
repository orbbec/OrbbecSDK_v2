// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "common/DeviceSeriesInfo.hpp"
#include "dylib.hpp"
#include "exception/ObException.hpp"
#include "libobsensor/h/ObTypes.h"
#include "logger/Logger.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace libobsensor {
namespace DynamicLibraryHelper {

typedef void (*pfunc_ob_set_device_info_data)(uint16_t type, const uint8_t *data, uint32_t length, ob_error **error);

template <typename Container> void pushToLibrary(std::shared_ptr<dylib> lib, uint16_t type, const Container &container) {
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
}

inline void setExtensionDeviceInfo(const std::string &libraryName, const std::shared_ptr<dylib> &lib) {
    if(!lib) {
        LOG_DEBUG("Library '{}' not loaded", libraryName.c_str());
        return;
    }

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

inline void logExtensionsCommitHashes(const std::string &libraryName, const std::shared_ptr<dylib> &lib) {
#ifdef OB_BUILD_WITH_EXTENSIONS_COMMIT_HASH
    typedef const char *(*pfunc_ob_get_commit_hash)();

    if(!lib) {
        LOG_DEBUG(" - Failed to retrieve commit hash for library '{}'", libraryName);
        return;
    }

    try {
        if(!lib->has_symbol("ob_get_commit_hash")) {
            LOG_DEBUG(" - Failed to retrieve commit hash for library '{}'", libraryName);
            return;
        }

        pfunc_ob_get_commit_hash ob_get_commit_hash = lib->get_function<const char *()>("ob_get_commit_hash");
        if(ob_get_commit_hash) {
            const char *commitHash = ob_get_commit_hash();
            LOG_DEBUG(" - Successfully retrieved commit hash for library '{}' (commit: {})", libraryName, commitHash);
        }
    }
    catch(...) {
        LOG_DEBUG(" - Failed to retrieve commit hash for library '{}', exception occurred", libraryName);
    }
#else
    (void)libraryName;
    (void)lib;
#endif
}

}  // namespace DynamicLibraryHelper
}  // namespace libobsensor