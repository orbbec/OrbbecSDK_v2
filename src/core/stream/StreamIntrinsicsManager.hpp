// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "libobsensor/h/ObTypes.h"
#include "StreamProfile.hpp"
#include <map>
#include <memory>
#include <vector>
#include <mutex>

namespace libobsensor {

class StreamIntrinsicsManager {
private:
    StreamIntrinsicsManager();

    static std::mutex                             instanceMutex_;
    static std::weak_ptr<StreamIntrinsicsManager> instanceWeakPtr_;

public:
    static std::shared_ptr<StreamIntrinsicsManager> getInstance();

    ~StreamIntrinsicsManager() noexcept;

    void               registerVideoStreamIntrinsics(const std::shared_ptr<const StreamProfile> &profile, const OBCameraIntrinsic &intrinsics);
    OBCameraIntrinsic  getVideoStreamIntrinsics(const std::shared_ptr<const StreamProfile> &profile);
    bool               containsVideoStreamIntrinsics(const std::shared_ptr<const StreamProfile> &profile);
    void               registerVideoStreamDistortion(const std::shared_ptr<const StreamProfile> &profile, const OBCameraDistortion &distortion);
    OBCameraDistortion getVideoStreamDistortion(const std::shared_ptr<const StreamProfile> &profile);
    bool               containsVideoStreamDistortion(const std::shared_ptr<const StreamProfile> &profile);
    void               registerGyroStreamIntrinsics(const std::shared_ptr<const StreamProfile> &profile, const OBGyroIntrinsic &intrinsics);
    OBGyroIntrinsic    getGyroStreamIntrinsics(const std::shared_ptr<const StreamProfile> &profile);
    bool               containsGyroStreamIntrinsics(const std::shared_ptr<const StreamProfile> &profile);
    void               registerAccelStreamIntrinsics(const std::shared_ptr<const StreamProfile> &profile, const OBAccelIntrinsic &intrinsics);
    OBAccelIntrinsic   getAccelStreamIntrinsics(const std::shared_ptr<const StreamProfile> &profile);
    bool               containsAccelStreamIntrinsics(const std::shared_ptr<const StreamProfile> &profile);
    void               registerDisparityBasedStreamDisparityParam(const std::shared_ptr<const StreamProfile> &profile, const OBDisparityParam &disparityParam);
    OBDisparityParam   getDisparityBasedStreamDisparityParam(const std::shared_ptr<const StreamProfile> &profile);
    bool               containsDisparityBasedStreamDisparityParam(const std::shared_ptr<const StreamProfile> &profile);

private:
    std::mutex mutex_;

    std::map<std::weak_ptr<const StreamProfile>, OBCameraIntrinsic, std::owner_less<std::weak_ptr<const StreamProfile>>>  videoStreamIntrinsicsMap_;
    std::map<std::weak_ptr<const StreamProfile>, OBCameraDistortion, std::owner_less<std::weak_ptr<const StreamProfile>>> videoStreamDistortionMap_;
    std::map<std::weak_ptr<const StreamProfile>, OBGyroIntrinsic, std::owner_less<std::weak_ptr<const StreamProfile>>>    gyroStreamIntrinsicsMap_;
    std::map<std::weak_ptr<const StreamProfile>, OBAccelIntrinsic, std::owner_less<std::weak_ptr<const StreamProfile>>>   accelStreamIntrinsicsMap_;
    std::map<std::weak_ptr<const StreamProfile>, OBDisparityParam, std::owner_less<std::weak_ptr<const StreamProfile>>>   disparityParamMap_;
};

}  // namespace libobsensor
