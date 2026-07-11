// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "IFilter.hpp"
#include "frame/FrameQueue.hpp"
#include "IDevice.hpp"
#include <mutex>

extern "C" {
#include "PrivFilterTypes.h"
}

namespace libobsensor {
class PrivFilterCppWrapper : public IFilterBase {
public:
    PrivFilterCppWrapper(const std::string &filterName, std::shared_ptr<ob_priv_filter_context> filterCtx,
                         pfunc_ob_priv_filter_activate_ex activateExFunc = nullptr);
    virtual ~PrivFilterCppWrapper() noexcept override;

    // Config
    void               updateConfig(std::vector<std::string> &params) override;
    void               setConfigData(void *data, uint32_t size) override;
    const std::string &getConfigSchema() const override;

    void                     reset() override;  // Stop thread, clean memory, reset status
    std::shared_ptr<IDevice> getActivatedDevice() const override;
    void                     activate(std::shared_ptr<IDevice> device, const ob_priv_filter_activate_options *options = nullptr) override;

private:
    std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;  // Filter function function, implemented on child class

private:
    std::string                               name_;
    std::shared_ptr<ob_priv_filter_context_t> privFilterCtx_;
    std::string                               configSchema_;
    ob_device_t                               activatedDeviceImpl_;
    pfunc_ob_priv_filter_activate_ex          activateExFunc_ = nullptr;  // package-level entrypoint, kept out of the filter context ABI
};
}  // namespace libobsensor
