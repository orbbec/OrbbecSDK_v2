#pragma once
#include "FilterBase.hpp"
#include <map>

namespace libobsensor {

class HDRMerge : public FilterBase {
public:
    HDRMerge(const std::string &name);
    virtual ~HDRMerge() noexcept;

    void               updateConfig(std::vector<std::string> &params) override;
    const std::string &getConfigSchema() const override;

private:
    std::shared_ptr<Frame> processFunc(std::shared_ptr<const Frame> frame) override;

    void                   discardDepthMergedFrameIfNeeded(std::shared_ptr<const Frame> frame);
    std::shared_ptr<Frame> merge(std::shared_ptr<const Frame> first_f, std::shared_ptr<const Frame> second_f);

protected:
    std::map<uint64_t, std::shared_ptr<const Frame>> frames_;
    std::shared_ptr<Frame>                depth_merged_frame_;
};

}  // namespace libobsensor
