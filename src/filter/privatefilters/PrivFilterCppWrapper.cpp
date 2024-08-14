#include "PrivFilterCppWrapper.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"

namespace libobsensor {
PrivFilterCppWrapper::PrivFilterCppWrapper(const std::string &filterName, std::shared_ptr<ob_priv_filter_context> filterCtx)
    : FilterBase(filterName), privFilterCtx_(filterCtx) {
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

const std::string &PrivFilterCppWrapper::getConfigSchema() const {
    return configSchema_;
}

void PrivFilterCppWrapper::reset() {
    ob_error *error = nullptr;
    FilterBase::reset();
    privFilterCtx_->reset(privFilterCtx_->filter, &error);
    if(error) {
        LOG_WARN("Private filter {} reset failed: {}", name_, error->message);
        delete error;
    }
}

std::shared_ptr<Frame> PrivFilterCppWrapper::processFunc(std::shared_ptr<const Frame> frame) {
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
        throw unrecoverable_exception(std::string(error->message),error->exception_type);
    }
    return resultFrame;
}

}  // namespace libobsensor