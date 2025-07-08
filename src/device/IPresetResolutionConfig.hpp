// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <string>
#include <vector>
#include "libobsensor/h/ObTypes.h"
#include "InternalTypes.hpp"

namespace libobsensor {

class IPresetResolutionConfigListManager {

public:
    virtual ~IPresetResolutionConfigListManager() = default;

    virtual std::vector<OBPresetResolutionConfig> getPresetResolutionConfigList() const = 0;
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
