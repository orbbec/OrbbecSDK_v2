// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <string>
#include <vector>
#include "libobsensor/h/ObTypes.h"
#include "InternalTypes.hpp"

namespace libobsensor {

class IPresetResolutionConfigeListManager {

public:
    virtual ~IPresetResolutionConfigeListManager() = default;

    virtual std::vector<OBPresetResolutionConfig> getPresetResolutionConfigeList() const = 0;
};
}  // namespace libobsensor

#ifdef __cplusplus
extern "C" {
#endif

struct ob_preset_resolution_config_list_t {
    std::vector<OBPresetResolutionConfig> configList;
};

#ifdef __cplusplus
}
#endif
