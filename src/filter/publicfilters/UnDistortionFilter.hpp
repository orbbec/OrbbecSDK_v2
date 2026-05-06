// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IFilter.hpp"
#include "IUnDistortionImpl.hpp"
#include "FormatConverterProcess.hpp"
#include "libobsensor/h/ObTypes.h"
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace libobsensor {

class UnDistortionFilter : public IFilterBase {
public:
    UnDistortionFilter();
    ~UnDistortionFilter() noexcept override;

    void               updateConfig(std::vector<std::string> &params) override;
    void               setConfigData(void *data, uint32_t size) override;
    const std::string &getConfigSchema() const override;
    void               reset() override;

private:
    std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;

    // Undistort a single VideoFrame and return a new frame with zeroed distortion.
    std::shared_ptr<Frame> undistortFrame(std::shared_ptr<const Frame> frame);

    bool isCacheValid(const OBCameraIntrinsic  &intrin,
                      const OBCameraDistortion  &disto,
                      OBFormat                   fmt) const;

    std::shared_ptr<IUnDistortionImpl> impl_;
    std::recursive_mutex               mutex_;

    OBStreamType      streamType_;
    OBCameraIntrinsic newCameraIntrinsic_;  // width=0 means pure undistortion (new camera matrix mode disabled)
    int               interpMode_;
    int               mjpgOutputFormat_;  // 0=RGB, 1=BGR; only used when input is MJPG

    // MJPG pre-decoder (lazy-init, reset when mjpgOutputFormat_ changes)
    std::shared_ptr<FormatConverter> mjpgConverter_;

    // LUT cache state
    bool               initialized_;
    OBCameraIntrinsic  cachedIntrinsic_;
    OBCameraDistortion cachedDistortion_;
    OBFormat           cachedFormat_;
};

}  // namespace libobsensor
