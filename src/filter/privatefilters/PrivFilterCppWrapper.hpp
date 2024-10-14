// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "IFilter.hpp"
#include "frame/FrameQueue.hpp"
#include <mutex>

extern "C" {
#include "PrivFilterTypes.h"
}

namespace libobsensor {
class PrivFilterCppWrapper : public IFilterBase {
public:
    PrivFilterCppWrapper(const std::string &filterName, std::shared_ptr<ob_priv_filter_context> filterCtx);
    virtual ~PrivFilterCppWrapper() noexcept;

    // Config
    void               updateConfig(std::vector<std::string> &params) override;
    const std::string &getConfigSchema() const override;

    void reset() override;  // Stop thread, clean memory, reset status

private:
    std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;  // Filter function function, implemented on child class

private:
    std::string                               name_;
    std::shared_ptr<ob_priv_filter_context_t> privFilterCtx_;
    std::string                               configSchema_;
};
}  // namespace libobsensor
