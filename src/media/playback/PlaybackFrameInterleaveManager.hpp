// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <vector>
#include "IFrameInterleaveManager.hpp"
#include "DeviceComponentBase.hpp"

namespace libobsensor {

class PlaybackFrameInterleaveManager : public IFrameInterleaveManager, public DeviceComponentBase {
public:
    PlaybackFrameInterleaveManager(std::shared_ptr<IFrameInterleaveManager> frameInterleaveManager);
    ~PlaybackFrameInterleaveManager() override;

    void                            loadFrameInterleave(const std::string &frameInterleaveName) override;
    const std::vector<std::string> &getAvailableFrameInterleaveList() const override;
    const std::string              &getCurrentFrameInterleaveName() const override;
    int32_t                         getParamCount() const override;
    const FrameInterleaveParam     &getParam(const std::string &frameInterleaveName, int32_t index) const override;
    void                            updateParam(const std::string &frameInterleaveName, const FrameInterleaveParam &param, int32_t index) override;

private:
    std::shared_ptr<IFrameInterleaveManager> deviceFrameInterleaveManager_;
};

}  // namespace libobsensor
