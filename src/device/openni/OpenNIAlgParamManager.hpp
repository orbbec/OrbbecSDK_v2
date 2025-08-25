// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "param/AlgParamManager.hpp"

namespace libobsensor {
class OpenNIAlgParamManager : public DisparityAlgParamManagerBase {
public:
    OpenNIAlgParamManager(IDevice *owner);
    virtual ~OpenNIAlgParamManager() override = default;

    virtual void bindDisparityParam(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) override;

private:
    void fetchParamFromDevice() override;
    void registerBasicExtrinsics() override;

private:
    std::vector<OBCalibrationParamContent> depthCalibParamList_;
};
}  // namespace libobsensor
