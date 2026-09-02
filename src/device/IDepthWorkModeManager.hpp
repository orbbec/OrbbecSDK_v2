// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <string>
#include <vector>
#include "InternalTypes.hpp"

namespace libobsensor {

struct DepthWorkModeItem {
    OBDepthWorkModeV2_Internal mode;
    std::string                version;
};

class IDepthWorkModeManager {
public:
    virtual ~IDepthWorkModeManager() = default;

    virtual std::vector<DepthWorkModeItem> getDepthWorkModeList() const                                                  = 0;
    virtual const DepthWorkModeItem       &getCurrentDepthWorkMode() const                                               = 0;
    virtual void                           switchDepthWorkMode(const std::string &name, const std::string &version = "") = 0;
    virtual void                           fetchDepthWorkModeList()                                                      = 0;
    virtual bool                           isVersionSupported() const {
        return false;
    }
};
}  // namespace libobsensor

#ifdef __cplusplus
extern "C" {
#endif

struct ob_depth_work_mode_list_t {
    std::vector<libobsensor::DepthWorkModeItem> workModeList;
};

#ifdef __cplusplus
}
#endif
