// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IAlgParamManager.hpp"
#include "PlaybackDevicePort.hpp"
#include "component/DeviceComponentBase.hpp"
#include "InternalTypes.hpp"

namespace libobsensor {

class PlaybackDeviceParamManager : public IAlgParamManager,public IDisparityAlgParamManager, public DeviceComponentBase {
public:
    PlaybackDeviceParamManager(IDevice *owner, std::shared_ptr<PlaybackDevicePort> port);
    virtual ~PlaybackDeviceParamManager() noexcept override = default;

    virtual void bindStreamProfileParams(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) override;
    virtual const std::vector<OBD2CProfile>  &getD2CProfileList() const override;
    virtual const std::vector<OBCameraParam> &getCalibrationCameraParamList() const override;
    virtual const OBIMUCalibrateParams       &getIMUCalibrationParam() const override;

    // Looks up the recorded OBD2CPreProcessParam for the given color resolution.
    // Returns true and fills param if a match is found in d2cColorPreProcessProfileList_.
    // Returns false if the recording predates this fix or the device does not support hardware D2C
    // pre-processing, in which case Pipeline::checkHardwareD2CConfig() skips setPreProcessParam()
    // and behavior is the same as before this fix (backward compatible with old recordings).
    virtual bool getPreProcessParam(uint16_t colorWidth, uint16_t colorHeight, OBD2CPreProcessParam &param) const override;

    virtual const OBDisparityParam &getDisparityParam() const override;

    virtual const OpenNIFrameProcessParam &getOpenNIFrameProcessParam() const;

    void bindDisparityParam(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList);

private:
    void initDeviceParams();

private:
    std::shared_ptr<PlaybackDevicePort> port_;
    std::vector<OBD2CProfile>           d2cProfileList_;
    // Hardware D2C color pre-process profiles, parallel to the first N entries of d2cProfileList_
    // (hardware D2C entries only). Populated from OB_RAW_DATA_D2C_ALIGN_COLOR_PRE_PROCESS_PROFILE_LIST
    // in the recording file. Empty for non-DaBaiA devices or recordings made before this fix.
    std::vector<OBD2CColorPreProcessProfile> d2cColorPreProcessProfileList_;
    std::vector<OBCameraParam>               cameraParamList_;
    OBIMUCalibrateParams                     imuCalibrationParam_;
    OBDisparityParam                         disparityParam_;
    OpenNIFrameProcessParam                  frameProcessParam_;
};

}  // namespace libobsensor