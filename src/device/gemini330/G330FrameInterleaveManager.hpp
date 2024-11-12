#pragma once
#include "IDevice.hpp"
#include "IFrameInterleaveManager.hpp"
#include "InternalTypes.hpp"
#include "DeviceComponentBase.hpp"

#include <map>
#include <string>
#include <vector>

namespace libobsensor {

struct FrameInterleaveParam {
    int depthExposureTime;  // exposure
    int depthGain;          // gain
    int depthBrightness;    // target brightness
    int depthMaxExposure;   // max exposure
    int laserSwitch;        // laser on/off switch
};

class G330FrameInterleaveManager : public IFrameInterleaveManager, public DeviceComponentBase {
public:
    G330FrameInterleaveManager(IDevice *owner);
    ~G330FrameInterleaveManager() override = default;

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

    FrameInterleaveParam laserInterleave_[2];
    FrameInterleaveParam laserInterleaveDefault_[2];
};

}  // namespace libobsensor