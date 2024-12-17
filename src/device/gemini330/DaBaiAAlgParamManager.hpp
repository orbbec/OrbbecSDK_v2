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
    void updateD2CProfileList(const std::string currentDepthAlgMode);

private:
    void fetchParamFromDevice() override;
    void registerBasicExtrinsics() override;
    void fixD2CParmaList();
    bool findBestMatchedCameraParam(const std::vector<OBCameraParam> &cameraParamList, const std::shared_ptr<const VideoStreamProfile> &profile,
                                    OBCameraParam &result);
    void d2CProfileListFilter(const std::string currentDepthAlgMode);

private:
    std::vector<OBDepthCalibrationParam>     depthCalibParamList_;
    std::vector<OBCameraParam>               originCalibrationCameraParamList_;
    std::vector<OBD2CProfile>                originD2cProfileList_;
    std::vector<OBD2CColorPreProcessProfile> originD2cColorPreProcessProfileList_;
    // save depth mode filter calibration camera list
    std::vector<OBCameraParam> depthModefilterCameraParamList_;
    //  pre process calibration camera list
    std::vector<OBCameraParam>               preProcessCameraParamList_;
    std::string                              currentDepthAlgMode_;
    std::map<int, bool>                      calibrationParamValidMap_;
    std::vector<OBD2CColorPreProcessProfile> d2cColorPreProcessProfileList_;
};

}  // namespace libobsensor
