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
    std::string moduleLoadPath_ = "./extensions/frameprocessor/";

    std::shared_ptr<dylib> dylib_;

    std::shared_ptr<FrameProcessorContext> context_;

    std::map<OBSensorType, std::shared_ptr<FrameProcessor>> frameProcessors_;
};

class FrameProcessor : public FilterExtension, public IBasicPropertyAccessor, public DeviceComponentBase {
public:
    FrameProcessor(IDevice *owner, std::shared_ptr<FrameProcessorContext> context, OBSensorType sensorType);

    virtual ~FrameProcessor() noexcept;

    const std::string &getConfigSchema() const override;

    void updateConfig(std::vector<std::string> &params) override;

    OBSensorType getSensorType() {
        return sensorType_;
    }

    void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;

    void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;

    void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

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
    DepthFrameProcessor(IDevice *owner, std::shared_ptr<FrameProcessorContext> context);
    virtual ~DepthFrameProcessor() noexcept;

    void setHardwareD2CProcessParams(uint32_t colorWidth, uint32_t colorHeight, uint32_t depthWidth, uint32_t depthHeight,
                                     std::vector<OBCameraParam> calibrationCameraParams, std::vector<OBD2CProfile> d2cProfiles);

    void enableHardwareD2CProcess(bool enable);
};

}  // namespace libobsensor