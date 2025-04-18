// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IAlgParamManager.hpp"
#include "PlaybackDevicePort.hpp"
#include "component/DeviceComponentBase.hpp"

namespace libobsensor {

class PlaybackDeviceParamManager : public IAlgParamManager,public IDisparityAlgParamManager, public DeviceComponentBase {
public:
    PlaybackDeviceParamManager(IDevice *owner, std::shared_ptr<PlaybackDevicePort> port);
    virtual ~PlaybackDeviceParamManager() noexcept override = default;

    virtual void bindStreamProfileParams(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) override;
    virtual const std::vector<OBD2CProfile> & getD2CProfileList() const override;
    virtual const std::vector<OBCameraParam> &getCalibrationCameraParamList() const override;
    virtual const OBIMUCalibrateParams &      getIMUCalibrationParam() const override;

    virtual const OBDisparityParam &getDisparityParam() const override;

    void bindDisparityParam(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList);

private:
    void initDeviceParams();

private:
    std::shared_ptr<PlaybackDevicePort> port_;
    std::vector<OBD2CProfile>           d2cProfileList_;
    std::vector<OBCameraParam>          cameraParamList_;
    OBIMUCalibrateParams                imuCalibrationParam_;
    OBDisparityParam                    disparityParam_;
};

}  // namespace libobsensor