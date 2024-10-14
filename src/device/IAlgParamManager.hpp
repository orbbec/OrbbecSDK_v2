// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <string>
#include <vector>
#include <memory>

#include "IStreamProfile.hpp"
#include "libobsensor/h/ObTypes.h"
#include "InternalTypes.hpp"

namespace libobsensor {

class IAlgParamManager {
public:
    virtual ~IAlgParamManager() = default;

    virtual void bindStreamProfileParams(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) = 0;

    virtual const std::vector<OBD2CProfile>  &getD2CProfileList() const             = 0;
    virtual const std::vector<OBCameraParam> &getCalibrationCameraParamList() const = 0;
    virtual const OBIMUCalibrateParams       &getIMUCalibrationParam() const        = 0;
};

class IDisparityAlgParamManager {
public:
    virtual ~IDisparityAlgParamManager() = default;

    virtual const OBDisparityParam &getDisparityParam() const = 0;
};

}  // namespace libobsensor
