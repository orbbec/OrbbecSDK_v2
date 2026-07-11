// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "PrivFilterCppWrapper.hpp"
#include "IDevice.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"

namespace libobsensor {
PrivFilterCppWrapper::PrivFilterCppWrapper(const std::string &filterName, std::shared_ptr<ob_priv_filter_context> filterCtx,
                                           pfunc_ob_priv_filter_activate_ex activateExFunc)
    : name_(filterName), privFilterCtx_(filterCtx), activateExFunc_(activateExFunc) {
    ob_error   *error = nullptr;
    const char *desc  = privFilterCtx_->get_config_schema(privFilterCtx_->filter, &error);
    if(error) {
        LOG_WARN("Private filter {} get config schema failed: {}", name_, error->message);
        delete error;
        return;
    }
    if(desc) {
        configSchema_ = desc;
    }
}

PrivFilterCppWrapper::~PrivFilterCppWrapper() noexcept {
    reset();
    LOG_DEBUG("Private filter {} destroyed", name_);
}

void PrivFilterCppWrapper::updateConfig(std::vector<std::string> &params) {
    ob_error                 *error = nullptr;
    std::vector<const char *> c_params;
    for(auto &p: params) {
        c_params.push_back(p.c_str());
    }

    privFilterCtx_->update_config(privFilterCtx_->filter, params.size(), c_params.data(), &error);
    if(error) {
        LOG_WARN("Private filter {} update config failed: {}", name_, error->message);
        delete error;
    }
}

void PrivFilterCppWrapper::setConfigData(void *data, uint32_t size) {
    ob_error *error = nullptr;
    if(data == nullptr || size == 0) {
        LOG_WARN("Private filter set config data is null or size is 0.");
        return;
    }

    privFilterCtx_->set_config_data(privFilterCtx_->filter, data, size, &error);
    if(error) {
        LOG_WARN("Private filter {} set config data failed: {}", name_, error->message);
        delete error;
    }
}

const std::string &PrivFilterCppWrapper::getConfigSchema() const {
    return configSchema_;
}

void PrivFilterCppWrapper::reset() {
    ob_error *error = nullptr;
    privFilterCtx_->reset(privFilterCtx_->filter, &error);
    if(error) {
        LOG_WARN("Private filter {} reset failed: {}", name_, error->message);
        delete error;
    }
}

std::shared_ptr<IDevice> PrivFilterCppWrapper::getActivatedDevice() const {
    return activatedDeviceImpl_.device;
}

void PrivFilterCppWrapper::activate(std::shared_ptr<IDevice> device, const ob_priv_filter_activate_options *options) {
    if(!device) {
        THROW_INVALID_PARAM_EXCEPTION("Device is null");
    }

    if(!activateExFunc_) {
        THROW_UNSUPPORTED_OPERATION_EXCEPTION("Private filter does not support device activation");
    }

    ob_error *error             = nullptr;
    activatedDeviceImpl_.device = std::move(device);
    activateExFunc_(privFilterCtx_->filter, &activatedDeviceImpl_, options, &error);
    if(error) {
        activatedDeviceImpl_.device = nullptr;
        THROW_UNRECOVERABLE(std::string(error->message), error->exception_type, error->status);
    }
}

std::shared_ptr<Frame> PrivFilterCppWrapper::process(std::shared_ptr<const Frame> frame) {
    ob_error              *error   = nullptr;
    ob_frame              *c_frame = new ob_frame();
    std::shared_ptr<Frame> resultFrame;
    c_frame->frame = std::const_pointer_cast<Frame>(frame);

    auto rst_frame = privFilterCtx_->process(privFilterCtx_->filter, c_frame, &error);
    if(rst_frame) {
        resultFrame = rst_frame->frame;
        delete rst_frame;
    }
    delete c_frame;

    if(error) {
        // LOG_WARN("Private filter {} process failed: {}", name_, error->message);
        THROW_UNRECOVERABLE(std::string(error->message), error->exception_type, error->status);
    }
    return resultFrame;
}

}  // namespace libobsensor
