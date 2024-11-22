// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "param/AlgParamManager.hpp"
#include "stream/StreamIntrinsicsManager.hpp"
#include "stream/StreamExtrinsicsManager.hpp"
#include "stream/StreamProfileFactory.hpp"
#include <vector>
#include <memory>

namespace libobsensor {

class DaBaiAAlgParamManager : public DisparityAlgParamManagerBase {
public:
    DaBaiAAlgParamManager(IDevice *owner);
    virtual ~DaBaiAAlgParamManager() = default;

    void bindIntrinsic(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) override;
    void reFetchDisparityParams();

private:
    void fetchParamFromDevice() override;
    void registerBasicExtrinsics() override;
    void fixD2CParmaList();
    bool findBestMatchedCameraParam(const std::vector<OBCameraParam> &cameraParamList, const std::shared_ptr<const VideoStreamProfile> &profile,
                                    OBCameraParam &result);

private:
    std::vector<OBDepthCalibrationParam> depthCalibParamList_;
    std::vector<OBCameraParam>           originCalibrationCameraParamList_;
    std::vector<OBD2CProfile>            originD2cProfileList_;
};

}  // namespace libobsensor
