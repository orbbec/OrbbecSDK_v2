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

    virtual bool getPreProcessParam(uint16_t colorWidth, uint16_t colorHeight, OBD2CPreProcessParam &param) const {
        (void)colorWidth;
        (void)colorHeight;
        (void)param;
        return false;
    }

    // Returns the hardware D2C color pre-process profile list, which is parallel (by index) to the
    // first N hardware-D2C entries of getD2CProfileList(). Used by RecordDevice to persist this data
    // into the recording file so PlaybackDeviceParamManager can reconstruct the correct preProcessParam.
    // Default implementation returns an empty list; only DaBaiA devices override this.
    virtual const std::vector<OBD2CColorPreProcessProfile> &getD2CColorPreProcessProfileList() const {
        static const std::vector<OBD2CColorPreProcessProfile> empty;
        return empty;
    }
};

class IDisparityAlgParamManager {
public:
    virtual ~IDisparityAlgParamManager() = default;

    virtual const OBDisparityParam &getDisparityParam() const = 0;
};

}  // namespace libobsensor
