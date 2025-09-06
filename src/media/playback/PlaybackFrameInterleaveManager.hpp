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
    ~PlaybackFrameInterleaveManager();

    void                            loadFrameInterleave(const std::string &frameInterleaveName) override;
    const std::vector<std::string> &getAvailableFrameInterleaveList() const override;

private:
    std::shared_ptr<IFrameInterleaveManager> deviceFrameInterleaveManager_;
};

}  // namespace libobsensor
