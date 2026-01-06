#pragma once
#include "IDevice.hpp"
#include "IFrameInterleaveManager.hpp"
#include "InternalTypes.hpp"
#include "DeviceComponentBase.hpp"

#include <map>
#include <string>
#include <vector>

namespace libobsensor {

class G330FrameInterleaveManager : public IFrameInterleaveManager, public DeviceComponentBase {
public:
    G330FrameInterleaveManager(IDevice *owner);
    ~G330FrameInterleaveManager() override = default;

    void                            loadFrameInterleave(const std::string &frameInterleaveName) override;
    const std::vector<std::string> &getAvailableFrameInterleaveList() const override;

    const std::string &getCurrentFrameInterleaveName() const override {
        return currentFrameInterleave_;
    }

    int32_t getParamCount() const override {
        return kDefaultFrameInterleaveCount;
    }

    const FrameInterleaveParam &getParam(const std::string &frameInterleaveName, int32_t index) const override;
    void                        updateParam(const std::string &frameInterleaveName, const FrameInterleaveParam &param, int32_t index) override;

private:
    void updateFrameInterleaveParam(uint32_t propertyId);

private:
    std::vector<std::string> availableFrameInterleaves_;
    std::string              currentFrameInterleave_;

    int currentIndex_;

    FrameInterleaveParam hdr_[kDefaultFrameInterleaveCount];
    FrameInterleaveParam hdrDefault_[kDefaultFrameInterleaveCount];

    FrameInterleaveParam laserInterleave_[kDefaultFrameInterleaveCount];
    FrameInterleaveParam laserInterleaveDefault_[kDefaultFrameInterleaveCount];
};

}  // namespace libobsensor