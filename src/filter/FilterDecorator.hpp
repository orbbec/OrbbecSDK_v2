// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IFilter.hpp"
#include "frame/FrameQueue.hpp"
#include "stream/StreamProfile.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <map>

namespace libobsensor {

class FilterExtension : public IFilter {
public:
    FilterExtension(const std::string &name);
    virtual ~FilterExtension() noexcept;

    const std::string &getName() const override;

    void enable(bool en) override;
    bool isEnabled() const override;
    void reset() override;

    const std::vector<OBFilterConfigSchemaItem> &getConfigSchemaVec() override;
    void                                         setConfigValue(const std::string &name, double value) override;
    void                                         setConfigValueSync(const std::string &name, double value) override;
    double                                       getConfigValue(const std::string &name) override;
    OBFilterConfigSchemaItem                     getConfigSchemaItem(const std::string &name) override;

    // Asynchronous, output result to callback function
    void         pushFrame(std::shared_ptr<const Frame> frame) override;
    void         setCallback(FilterCallback cb) override;
    virtual void resizeFrameQueue(size_t size) override;

protected:
    void updateConfigCache(std::vector<std::string> &params);
    void checkAndUpdateConfig();

private:
    const std::string name_;
    std::atomic<bool> enabled_;

    std::mutex     callbackMutex_;
    FilterCallback callback_;

    std::shared_ptr<FrameQueue<const Frame>> srcFrameQueue_;

    std::mutex                            configMutex_;
    std::atomic<bool>                     configChanged_;
    std::map<std::string, double>         configMap_;
    std::vector<OBFilterConfigSchemaItem> configSchemaVec_;
    std::vector<std::vector<std::string>> configSchemaStrSplittedVec_;
};

class FilterDecorator : public FilterExtension {
public:
    FilterDecorator(const std::string &name, std::shared_ptr<IFilterBase> baseFilter);
    virtual ~FilterDecorator() noexcept;

    virtual void                   reset() override;
    virtual void                   updateConfig(std::vector<std::string> &params) override;
    virtual const std::string     &getConfigSchema() const override;
    virtual std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;

    std::shared_ptr<IFilterBase> getBaseFilter() const;

private:
    std::mutex                   processMutex_;
    std::shared_ptr<IFilterBase> baseFilter_;
};

}  // namespace libobsensor
