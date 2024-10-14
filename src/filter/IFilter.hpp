// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "IFrame.hpp"
#include "IStreamProfile.hpp"
#include "libobsensor/h/ObTypes.h"

namespace libobsensor {

typedef std::function<void(std::shared_ptr<Frame>)> FilterCallback;

class IFilterBase {
public:
    virtual ~IFilterBase() noexcept = default;

    // Config
    virtual void               updateConfig(std::vector<std::string> &params) = 0;
    virtual const std::string &getConfigSchema() const                        = 0;

    virtual void reset() = 0;  // Stop thread, clean memory, reset status

    // Synchronize
    virtual std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) = 0;
};

class IFilterExtension {
public:
    virtual const std::string &getName() const = 0;

    virtual void enable(bool en)   = 0;
    virtual bool isEnabled() const = 0;

    virtual const std::vector<OBFilterConfigSchemaItem> &getConfigSchemaVec()                                      = 0;
    virtual void                                         setConfigValue(const std::string &name, double value)     = 0;
    virtual void                                         setConfigValueSync(const std::string &name, double value) = 0;
    virtual double                                       getConfigValue(const std::string &name)                   = 0;
    virtual OBFilterConfigSchemaItem                     getConfigSchemaItem(const std::string &name)              = 0;

    // Asynchronous
    virtual void pushFrame(std::shared_ptr<const Frame> frame) = 0;
    virtual void setCallback(FilterCallback cb)                = 0;

    virtual void resizeFrameQueue(size_t size) = 0;
};

class IFilter : public IFilterBase, public IFilterExtension {
public:
    virtual ~IFilter() noexcept = default;
};

class IFilterCreator {
public:
    virtual ~IFilterCreator() noexcept = default;

    virtual std::shared_ptr<IFilter> create() = 0;
};

class IPrivFilterCreator : public IFilterCreator {
public:
    ~IPrivFilterCreator() noexcept override                                   = default;
    std::shared_ptr<IFilter>         create() override                        = 0;
    virtual std::shared_ptr<IFilter> create(const std::string &activationKey) = 0;
    virtual const std::string       &getVendorSpecificCode() const            = 0;
};

}  // namespace libobsensor

#ifdef __cplusplus
extern "C" {
#endif
struct ob_filter_t {
    std::shared_ptr<libobsensor::IFilter> filter;
};

struct ob_filter_list_t {
    std::vector<std::shared_ptr<libobsensor::IFilter>> filterList;
};

struct ob_filter_config_schema_list_t {
    std::vector<OBFilterConfigSchemaItem> configSchemaList;
};

#ifdef __cplusplus
}
#endif
