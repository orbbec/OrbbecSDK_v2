#pragma once
#include "IDevice.hpp"
#include "IFrameInterleaveManager.hpp"
#include "InternalTypes.hpp"
#include "DeviceComponentBase.hpp"

#include <map>
#include <string>
#include <vector>

namespace libobsensor {

class G305FrameInterleaveManager : public IFrameInterleaveManager, public DeviceComponentBase {
public:
    G305FrameInterleaveManager(IDevice *owner);
    ~G305FrameInterleaveManager() override = default;

    void                            loadFrameInterleave(const std::string &frameInterleaveName) override;
    const std::vector<std::string> &getAvailableFrameInterleaveList() const override;

private:
    void updateFrameInterleaveParam(uint32_t propertyId);

private:
    std::vector<std::string> availableFrameInterleaves_;
    std::string              currentFrameInterleave_;

    int currentIndex_;

    FrameInterleaveParam hdr_[2];
    FrameInterleaveParam hdrDefault_[2];
};

}  // namespace libobsensor