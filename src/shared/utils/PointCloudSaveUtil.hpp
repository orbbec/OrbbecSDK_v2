// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "libobsensor/h/ObTypes.h"
#include "frame/Frame.hpp"

namespace libobsensor {

class PointCloudSaveUtil {
public:
    static bool savePointCloudToPly(const char *fileName, std::shared_ptr<Frame> frame, bool saveBinary = false, bool useMesh = false,
                                    float meshThreshold = 50.0);
};
}  // namespace libobsensor
