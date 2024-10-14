// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "param/AlgParamManager.hpp"

namespace libobsensor {
class G2AlgParamManager : public DisparityAlgParamManagerBase {
public:
    G2AlgParamManager(IDevice *owner);
    virtual ~G2AlgParamManager() = default;

    void fetchParamFromDevice() override;
    void registerBasicExtrinsics() override;

private:
    std::vector<OBDepthCalibrationParam> depthCalibParamList_;
};
}  // namespace libobsensor
