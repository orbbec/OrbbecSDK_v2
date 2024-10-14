// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <string>
#include <vector>
#include "InternalTypes.hpp"

namespace libobsensor {

class IDepthWorkModeManager {
public:
    virtual ~IDepthWorkModeManager() = default;

    virtual std::vector<OBDepthWorkMode_Internal> getDepthWorkModeList() const                 = 0;
    virtual const OBDepthWorkMode_Internal       &getCurrentDepthWorkMode() const              = 0;
    virtual void                                switchDepthWorkMode(const std::string &name) = 0;
};
}  // namespace libobsensor

#ifdef __cplusplus
extern "C" {
#endif

struct ob_depth_work_mode_list_t {
    std::vector<OBDepthWorkMode_Internal> workModeList;
};

#ifdef __cplusplus
}
#endif
