// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "StreamIntrinsicsManager.hpp"
#include "logger/Logger.hpp"

namespace libobsensor {

std::mutex                             StreamIntrinsicsManager::instanceMutex_;
std::weak_ptr<StreamIntrinsicsManager> StreamIntrinsicsManager::instanceWeakPtr_;

std::shared_ptr<StreamIntrinsicsManager> StreamIntrinsicsManager::getInstance() {
    std::unique_lock<std::mutex> lock(instanceMutex_);
    auto                         instance = instanceWeakPtr_.lock();
    if(!instance) {
        instance         = std::shared_ptr<StreamIntrinsicsManager>(new StreamIntrinsicsManager());
        instanceWeakPtr_ = instance;
    }
    return instance;
}

StreamIntrinsicsManager::StreamIntrinsicsManager() {}

StreamIntrinsicsManager::~StreamIntrinsicsManager() noexcept = default;

void StreamIntrinsicsManager::registerVideoStreamIntrinsics(const std::shared_ptr<const StreamProfile> &profile, const OBCameraIntrinsic &intrinsics) {
    if(!profile) {
        throw invalid_value_exception("Input stream profile is null.");
    }
    if(!profile->is<const VideoStreamProfile>()) {
        throw invalid_value_exception("Input stream profile is not a video stream profile.");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    // remove expired stream profiles
    for(auto it = videoStreamIntrinsicsMap_.begin(); it != videoStreamIntrinsicsMap_.end();) {
        if(it->first.expired()) {
            it = videoStreamIntrinsicsMap_.erase(it);
        }
        else {
            it++;
        }
    }

    for(auto &pair: videoStreamIntrinsicsMap_) {
        if(pair.first.lock() == profile) {
            pair.second = intrinsics;
            return;
        }
    }

    videoStreamIntrinsicsMap_[std::weak_ptr<const StreamProfile>(profile)] = intrinsics;
}

OBCameraIntrinsic StreamIntrinsicsManager::getVideoStreamIntrinsics(const std::shared_ptr<const StreamProfile> &profile) {
    if(!profile) {
        throw invalid_value_exception("Input stream profile is null.");
    }
    if(!profile->is<const VideoStreamProfile>()) {
        throw invalid_value_exception("Input stream profile is not a video stream profile.");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for(const auto &pair: videoStreamIntrinsicsMap_) {
        if(pair.first.lock() == profile) {
            return pair.second;
        }
    }
    throw invalid_value_exception("Intrinsics for the input stream profile is not found.");
}

bool StreamIntrinsicsManager::containsVideoStreamIntrinsics(const std::shared_ptr<const StreamProfile> &profile) {
    if(!profile) {
        throw invalid_value_exception("Input stream profile is null.");
    }
    if(!profile->is<const VideoStreamProfile>()) {
        throw invalid_value_exception("Input stream profile is not a video stream profile.");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for(const auto &pair: videoStreamIntrinsicsMap_) {
        if(pair.first.lock() == profile) {
            return true;
        }
    }
    return false;
}

void StreamIntrinsicsManager::registerVideoStreamDistortion(const std::shared_ptr<const StreamProfile> &profile, const OBCameraDistortion &distortion) {
    if(!profile) {
        throw invalid_value_exception("Input stream profile is null.");
    }
    if(!profile->is<const VideoStreamProfile>()) {
        throw invalid_value_exception("Input stream profile is not a video stream profile.");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    // remove expired stream profiles
    for(auto it = videoStreamDistortionMap_.begin(); it != videoStreamDistortionMap_.end();) {
        if(it->first.expired()) {
            it = videoStreamDistortionMap_.erase(it);
        }
        else {
            it++;
        }
    }

    videoStreamDistortionMap_[std::weak_ptr<const StreamProfile>(profile)] = distortion;
}

OBCameraDistortion StreamIntrinsicsManager::getVideoStreamDistortion(const std::shared_ptr<const StreamProfile> &profile) {
    if(!profile) {
        throw invalid_value_exception("Input stream profile is null.");
    }
    if(!profile->is<const VideoStreamProfile>()) {
        throw invalid_value_exception("Input stream profile is not a video stream profile.");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for(const auto &pair: videoStreamDistortionMap_) {
        if(pair.first.lock() == profile) {
            return pair.second;
        }
    }
    throw invalid_value_exception("Distortion for the input stream profile is not found.");
}

bool StreamIntrinsicsManager::containsVideoStreamDistortion(const std::shared_ptr<const StreamProfile> &profile) {
    if(!profile) {
        throw invalid_value_exception("Input stream profile is null.");
    }
    if(!profile->is<const VideoStreamProfile>()) {
        throw invalid_value_exception("Input stream profile is not a video stream profile.");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for(const auto &pair: videoStreamDistortionMap_) {
        if(pair.first.lock() == profile) {
            return true;
        }
    }
    return false;
}

void StreamIntrinsicsManager::registerGyroStreamIntrinsics(const std::shared_ptr<const StreamProfile> &profile, const OBGyroIntrinsic &intrinsics) {
    if(!profile) {
        throw invalid_value_exception("Input stream profile is null.");
    }
    if(!profile->is<const GyroStreamProfile>()) {
        throw invalid_value_exception("Input stream profile is not a gyro stream profile.");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    // remove expired stream profiles
    for(auto it = gyroStreamIntrinsicsMap_.begin(); it != gyroStreamIntrinsicsMap_.end();) {
        if(it->first.expired()) {
            it = gyroStreamIntrinsicsMap_.erase(it);
        }
        else {
            it++;
        }
    }

    gyroStreamIntrinsicsMap_[std::weak_ptr<const StreamProfile>(profile)] = intrinsics;
}

OBGyroIntrinsic StreamIntrinsicsManager::getGyroStreamIntrinsics(const std::shared_ptr<const StreamProfile> &profile) {
    if(!profile) {
        throw invalid_value_exception("Input stream profile is null.");
    }
    if(!profile->is<const GyroStreamProfile>()) {
        throw invalid_value_exception("Input stream profile is not a gyro stream profile.");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for(const auto &pair: gyroStreamIntrinsicsMap_) {
        if(pair.first.lock() == profile) {
            return pair.second;
        }
    }
    throw invalid_value_exception("Intrinsics for the input stream profile is not found.");
}

bool StreamIntrinsicsManager::containsGyroStreamIntrinsics(const std::shared_ptr<const StreamProfile> &profile) {
    if(!profile) {
        throw invalid_value_exception("Input stream profile is null.");
    }
    if(!profile->is<const GyroStreamProfile>()) {
        throw invalid_value_exception("Input stream profile is not a gyro stream profile.");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for(const auto &pair: gyroStreamIntrinsicsMap_) {
        if(pair.first.lock() == profile) {
            return true;
        }
    }
    return false;
}

void StreamIntrinsicsManager::registerAccelStreamIntrinsics(const std::shared_ptr<const StreamProfile> &profile, const OBAccelIntrinsic &intrinsics) {
    if(!profile) {
        throw invalid_value_exception("Input stream profile is null.");
    }
    if(!profile->is<const AccelStreamProfile>()) {
        throw invalid_value_exception("Input stream profile is not a accel stream profile.");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    // remove expired stream profiles
    for(auto it = accelStreamIntrinsicsMap_.begin(); it != accelStreamIntrinsicsMap_.end();) {
        if(it->first.expired()) {
            it = accelStreamIntrinsicsMap_.erase(it);
        }
        else {
            ++it;
        }
    }

    accelStreamIntrinsicsMap_[std::weak_ptr<const StreamProfile>(profile)] = intrinsics;
}

OBAccelIntrinsic StreamIntrinsicsManager::getAccelStreamIntrinsics(const std::shared_ptr<const StreamProfile> &profile) {
    if(!profile) {
        throw invalid_value_exception("Input stream profile is null.");
    }
    if(!profile->is<const AccelStreamProfile>()) {
        throw invalid_value_exception("Input stream profile is not a accel stream profile.");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for(const auto &pair: accelStreamIntrinsicsMap_) {
        if(pair.first.lock() == profile) {
            return pair.second;
        }
    }
    throw invalid_value_exception("Intrinsics for the input stream profile is not found.");
}

bool StreamIntrinsicsManager::containsAccelStreamIntrinsics(const std::shared_ptr<const StreamProfile> &profile) {
    if(!profile) {
        throw invalid_value_exception("Input stream profile is null.");
    }
    if(!profile->is<const AccelStreamProfile>()) {
        throw invalid_value_exception("Input stream profile is not a accel stream profile.");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for(const auto &pair: accelStreamIntrinsicsMap_) {
        if(pair.first.lock() == profile) {
            return true;
        }
    }
    return false;
}

void StreamIntrinsicsManager::registerDisparityBasedStreamDisparityParam(const std::shared_ptr<const StreamProfile> &profile,
                                                                         const OBDisparityParam                     &disparityParam) {
    if(!profile) {
        throw invalid_value_exception("Input stream profile is null.");
    }
    if(!profile->is<const DisparityBasedStreamProfile>()) {
        throw invalid_value_exception("Input stream profile is not a disparity based stream profile.");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    // remove expired stream profiles
    for(auto it = disparityParamMap_.begin(); it != disparityParamMap_.end();) {
        if(it->first.expired()) {
            it = disparityParamMap_.erase(it);
        }
        else {
            ++it;
        }
    }

    disparityParamMap_[std::weak_ptr<const StreamProfile>(profile)] = disparityParam;
}

OBDisparityParam StreamIntrinsicsManager::getDisparityBasedStreamDisparityParam(const std::shared_ptr<const StreamProfile> &profile) {
    if(!profile) {
        throw invalid_value_exception("Input stream profile is null.");
    }
    if(!profile->is<const DisparityBasedStreamProfile>()) {
        throw invalid_value_exception("Input stream profile is not a disparity based stream profile.");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for(const auto &pair: disparityParamMap_) {
        if(pair.first.lock() == profile) {
            return pair.second;
        }
    }
    throw invalid_value_exception("Disparity parameter for the input stream profile is not found.");
}

bool StreamIntrinsicsManager::containsDisparityBasedStreamDisparityParam(const std::shared_ptr<const StreamProfile> &profile) {
    if(!profile) {
        throw invalid_value_exception("Input stream profile is null.");
    }
    if(!profile->is<const DisparityBasedStreamProfile>()) {
        throw invalid_value_exception("Input stream profile is not a disparity based stream profile.");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for(const auto &pair: disparityParamMap_) {
        if(pair.first.lock() == profile) {
            return true;
        }
    }
    return false;
}
}  // namespace libobsensor
