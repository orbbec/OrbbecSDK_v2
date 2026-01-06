#pragma once
#include "IDevice.hpp"
#include "IFrameInterleaveManager.hpp"
#include "InternalTypes.hpp"
#include "DeviceComponentBase.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"

#include <map>
#include <string>
#include <vector>

namespace libobsensor {

class G435LeFrameInterleaveManager : public IFrameInterleaveManager, public DeviceComponentBase {
public:
    G435LeFrameInterleaveManager(IDevice *owner);
    ~G435LeFrameInterleaveManager() override = default;

    void                            loadFrameInterleave(const std::string &frameInterleaveName) override;
    const std::vector<std::string> &getAvailableFrameInterleaveList() const override;

    const std::string &getCurrentFrameInterleaveName() const override {
        return currentFrameInterleave_;
    }

    int32_t getParamCount() const override {
        return 2;
    }

    const FrameInterleaveParam &getParam(const std::string &frameInterleaveName, int32_t index) const override {
        utils::unusedVar(frameInterleaveName);
        utils::unusedVar(index);
        THROW_NOT_IMPLEMENTED_EXCEPTION("The current api is not implemented");
    }

    void updateParam(const std::string &frameInterleaveName, const FrameInterleaveParam &param, int32_t index) override {
        utils::unusedVar(frameInterleaveName);
        utils::unusedVar(param);
        utils::unusedVar(index);
        THROW_NOT_IMPLEMENTED_EXCEPTION("The current api is not implemented");
    }

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