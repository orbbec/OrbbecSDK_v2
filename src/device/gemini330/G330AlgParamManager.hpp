// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "param/AlgParamManager.hpp"

#include <vector>
#include <memory>

namespace libobsensor {

class G330AlgParamManager : public DisparityAlgParamManagerBase {
public:
    G330AlgParamManager(IDevice *owner);
    virtual ~G330AlgParamManager() = default;

    void bindIntrinsic(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) override;
    void reFetchDisparityParams();

private:
    void fetchParamFromDevice() override;
    void registerBasicExtrinsics() override;
    void fixD2CParmaList();

private:
    std::vector<OBDepthCalibrationParam> depthCalibParamList_;
    std::vector<OBCameraParam>           originCalibrationCameraParamList_;
    std::vector<OBD2CProfile>            originD2cProfileList_;
};

}  // namespace libobsensor
