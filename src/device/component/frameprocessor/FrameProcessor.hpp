// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDevice.hpp"
#include "FilterDecorator.hpp"
#include "PrivFrameProcessorTypes.h"
#include "DeviceComponentBase.hpp"
#include "InternalTypes.hpp"
#include <dylib.hpp>
#include <map>

namespace libobsensor {
class FrameProcessor;
struct FrameProcessorContext {
    ob_frame_processor_context *context = nullptr;

    pfunc_ob_create_frame_processor_context          create_context          = nullptr;
    pfunc_ob_create_frame_processor                  create_processor        = nullptr;
    pfunc_ob_frame_processor_get_config_schema       get_config_schema       = nullptr;
    pfunc_ob_frame_processor_update_config           update_config           = nullptr;
    pfunc_ob_frame_processor_process_frame           process_frame           = nullptr;
    pfunc_ob_destroy_frame_processor                 destroy_processor       = nullptr;
    pfunc_ob_destroy_frame_processor_context         destroy_context         = nullptr;
    pfunc_ob_frame_processor_set_hardware_d2c_params set_hardware_d2c_params = nullptr;
};

class FrameProcessorFactory : public DeviceComponentBase {
public:
    explicit FrameProcessorFactory(IDevice *owner);
    ~FrameProcessorFactory() noexcept;

    std::shared_ptr<FrameProcessor> createFrameProcessor(OBSensorType sensorType);

private:
    std::shared_ptr<dylib> dylib_;

    std::shared_ptr<FrameProcessorContext> context_;

    std::map<OBSensorType, std::shared_ptr<FrameProcessor>> frameProcessors_;
};

class FrameProcessor : public FilterExtension, public IBasicPropertyAccessor, public DeviceComponentBase {
public:
    using IBasicPropertyAccessor::getPropertyRange;

    FrameProcessor(IDevice *owner, std::shared_ptr<FrameProcessorContext> context, OBSensorType sensorType);
    virtual ~FrameProcessor() noexcept;

    const std::string &getConfigSchema() const override;
    void updateConfig(std::vector<std::string> &params) override;

    OBSensorType getSensorType() {
        return sensorType_;
    }

    virtual void getPropertyRange(const std::string &configName, OBPropertyRange *range);

    std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;

protected:
    std::shared_ptr<FrameProcessorContext> context_;

    ob_frame_processor *privateProcessor_;

private:
    OBSensorType sensorType_;

    std::string configSchema_;
};

class DepthFrameProcessor : public FrameProcessor {
public:
    using FrameProcessor::getPropertyRange;

    DepthFrameProcessor(IDevice *owner, std::shared_ptr<FrameProcessorContext> context);
    virtual ~DepthFrameProcessor() noexcept;

    void setHardwareD2CProcessParams(std::shared_ptr<const VideoStreamProfile> colorVideoStreamProfile,std::shared_ptr<const VideoStreamProfile> depthVideoStreamProfile,std::vector<OBCameraParam> calibrationCameraParams, std::vector<OBD2CProfile> d2cProfiles, bool matchTargetResolution);

    void enableHardwareD2CProcess(bool enable);

private:
    OBD2CProfile  currentD2CProfile_ = {};

    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;
};

class ColorFrameProcessor : public FrameProcessor {
public:
    using FrameProcessor::getPropertyRange;

    ColorFrameProcessor(IDevice *owner, std::shared_ptr<FrameProcessorContext> context);
    virtual ~ColorFrameProcessor() noexcept = default;

    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;
};

class IRFrameProcessor : public FrameProcessor {
public:
    using FrameProcessor::getPropertyRange;

    IRFrameProcessor(IDevice *owner, std::shared_ptr<FrameProcessorContext> context, OBSensorType sensorType = OB_SENSOR_IR);
    virtual ~IRFrameProcessor() noexcept = default;

    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;
};

class IRRightFrameProcessor : public FrameProcessor {
public:
    using FrameProcessor::getPropertyRange;

    IRRightFrameProcessor(IDevice *owner, std::shared_ptr<FrameProcessorContext> context);
    virtual ~IRRightFrameProcessor() noexcept = default;

    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;
};
}  // namespace libobsensor
