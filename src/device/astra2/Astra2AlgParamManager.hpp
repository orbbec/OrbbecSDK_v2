// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "param/AlgParamManager.hpp"

namespace libobsensor {
class Astra2AlgParamManager : public DisparityAlgParamManagerBase {
public:
    Astra2AlgParamManager(IDevice *owner);
    virtual ~Astra2AlgParamManager() = default;

    void fetchParamFromDevice() override;
    void registerBasicExtrinsics() override;

private:
    std::vector<OBDepthCalibrationParam> depthCalibParamList_;
};
}  // namespace libobsensor
