// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FrameProcessor.hpp"
#include "frame/FrameFactory.hpp"
#include "utils/Utils.hpp"
#include "environment/EnvConfig.hpp"

namespace libobsensor {
FrameProcessorFactory::FrameProcessorFactory(IDevice *owner) : DeviceComponentBase(owner) {
    std::string moduleLoadPath = EnvConfig::getExtensionsDirectory() + "/frameprocessor/";
    dylib_                     = std::make_shared<dylib>(moduleLoadPath.c_str(), "ob_frame_processor");

    auto dylib = dylib_;
    context_   = std::shared_ptr<FrameProcessorContext>(new FrameProcessorContext(), [dylib](FrameProcessorContext *context) {
        if(context && context->destroy_context) {
            ob_error *error = nullptr;
            context->destroy_context(context->context, &error);
            delete error;
        }
    });

    if(dylib_) {
        context_->create_context = dylib_->get_function<ob_frame_processor_context *(ob_device *, ob_error **)>("ob_create_frame_processor_context");
        context_->create_processor =
            dylib_->get_function<ob_frame_processor *(ob_frame_processor_context *, ob_sensor_type type, ob_error **)>("ob_create_frame_processor");
        context_->get_config_schema = dylib_->get_function<const char *(ob_frame_processor *, ob_error **)>("ob_frame_processor_get_config_schema");
        context_->update_config     = dylib_->get_function<void(ob_frame_processor *, size_t, const char **, ob_error **)>("ob_frame_processor_update_config");
        context_->process_frame     = dylib_->get_function<ob_frame *(ob_frame_processor *, ob_frame *, ob_error **)>("ob_frame_processor_process_frame");
        context_->destroy_processor = dylib_->get_function<void(ob_frame_processor *, ob_error **)>("ob_destroy_frame_processor");
        context_->destroy_context   = dylib_->get_function<void(ob_frame_processor_context *, ob_error **)>("ob_destroy_frame_processor_context");
        context_->set_hardware_d2c_params =
            dylib->get_function<void(ob_frame_processor *, ob_camera_param, uint8_t, float, int16_t, int16_t, int16_t, int16_t, bool, ob_error **error)>(
                "ob_frame_processor_set_hardware_d2c_params");
    }

    if(context_->create_context && !context_->context) {
        auto cDevice      = new ob_device;
        cDevice->device   = owner->shared_from_this();
        ob_error *error   = nullptr;
        context_->context = context_->create_context(cDevice, &error);
        if(error) {
            // TODO
            throw std::runtime_error("create frame processor context failed");
        }

        if(cDevice) {
            delete cDevice;
            cDevice = nullptr;
        }
    }
}

FrameProcessorFactory::~FrameProcessorFactory() noexcept {}

std::shared_ptr<FrameProcessor> FrameProcessorFactory::createFrameProcessor(OBSensorType sensorType) {
    if(context_ && (!context_->context || context_->create_processor == nullptr)) {
        return nullptr;
    }

    auto iter = frameProcessors_.find(sensorType);
    if(iter != frameProcessors_.end()) {
        return iter->second;
    }

    auto                            owner = getOwner();
    std::shared_ptr<FrameProcessor> processor;
    TRY_EXECUTE({
        switch(sensorType) {
        case OB_SENSOR_DEPTH: {
            processor = std::make_shared<DepthFrameProcessor>(owner, context_);
        } break;

        default:
            processor = std::make_shared<FrameProcessor>(owner, context_, sensorType);
            break;
        }
    })
    frameProcessors_.insert({ sensorType, processor });
    return processor;
}

FrameProcessor::FrameProcessor(IDevice *owner, std::shared_ptr<FrameProcessorContext> context, OBSensorType sensorType)
    : FilterExtension("FrameProcessor"), DeviceComponentBase(owner), context_(context), sensorType_(sensorType) {
    if(context_->context && context_->create_processor) {
        ob_error *error   = nullptr;
        privateProcessor_ = context_->create_processor(context_->context, sensorType, &error);
        if(error) {
            auto msg = std::string(error->message);
            delete error;
            throw std::runtime_error(msg);
        }
    }

    if(context_->context && context_->get_config_schema) {
        ob_error   *error  = nullptr;
        const char *schema = context_->get_config_schema(privateProcessor_, &error);
        if(error) {
            delete error;
        }
        if(schema) {
            configSchema_ = std::string(schema);
        }
    }
}

FrameProcessor::~FrameProcessor() noexcept {
    if(context_->destroy_processor) {
        ob_error *error = nullptr;
        context_->destroy_processor(privateProcessor_, &error);
        if(error) {
            delete error;
        }
    }
}

std::shared_ptr<Frame> FrameProcessor::process(std::shared_ptr<const Frame> frame) {
    if(!context_->process_frame || !privateProcessor_) {
        return FrameFactory::createFrameFromOtherFrame(frame, true);
    }

    checkAndUpdateConfig();

    ob_frame              *c_frame = new ob_frame();
    ob_error              *error   = nullptr;
    std::shared_ptr<Frame> resultFrame;
    c_frame->frame = std::const_pointer_cast<Frame>(frame);

    auto rst_frame = context_->process_frame(privateProcessor_, c_frame, &error);
    if(rst_frame) {
        resultFrame = rst_frame->frame;
        delete rst_frame;
    }
    delete c_frame;

    if(error) {
        delete error;
        return FrameFactory::createFrameFromOtherFrame(frame, true);
    }

    return resultFrame;
}

const std::string &FrameProcessor::getConfigSchema() const {
    return configSchema_;
}

void FrameProcessor::updateConfig(std::vector<std::string> &params) {
    ob_error                 *error = nullptr;
    std::vector<const char *> c_params;
    for(auto &p: params) {
        c_params.push_back(p.c_str());
    }
    context_->update_config(privateProcessor_, params.size(), c_params.data(), &error);
    if(error) {
        delete error;
    }
}

void FrameProcessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    std::string configSchemaName = "";
    double      configValue      = 0.0;
    switch(propertyId) {
    case OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL: {
        configSchemaName = "DisparityTransform#255";
        configValue      = static_cast<double>(value.intValue);
    } break;
    case OB_PROP_DEPTH_PRECISION_LEVEL_INT: {
        auto level       = static_cast<OBDepthPrecisionLevel>(value.intValue);
        auto depthUnit   = utils::depthPrecisionLevelToUnit(level);
        configSchemaName = "DisparityTransform#2";
        configValue      = static_cast<double>(depthUnit);
    } break;
    case OB_PROP_DEPTH_UNIT_FLEXIBLE_ADJUSTMENT_FLOAT: {
        configSchemaName = "DisparityTransform#2";
        configValue      = static_cast<double>(value.floatValue);
    } break;
    case OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_BOOL: {
        configSchemaName = "NoiseRemovalFilter#255";
        configValue      = static_cast<double>(value.intValue);
    } break;
    case OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_SPECKLE_SIZE_INT: {
        configSchemaName = "NoiseRemovalFilter#0";
        configValue      = static_cast<double>(value.intValue);
    } break;
    case OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_DIFF_INT: {
        configSchemaName = "NoiseRemovalFilter#1";
        configValue      = static_cast<double>(value.intValue);
    } break;
    case OB_PROP_DISPARITY_TO_DEPTH_BOOL: {
        configSchemaName = "HardwareD2DCorrectionFilter#255";
        configValue      = static_cast<double>(value.intValue);
    } break;
    default: {
        throw invalid_value_exception("Invalid property id");
    }
    }

    // check config schema supported
    try {
        getConfigValue(configSchemaName);
    }
    catch(...) {
        return;
    }

    setConfigValue(configSchemaName, configValue);
}

void FrameProcessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    switch(propertyId) {
    case OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL: {
        auto getValue   = getConfigValue("DisparityTransform#255");
        value->intValue = static_cast<int32_t>(getValue);
    } break;
    case OB_PROP_DEPTH_PRECISION_LEVEL_INT: {
        auto getValue       = getConfigValue("DisparityTransform#2");
        auto precisionLevel = utils::depthUnitToPrecisionLevel(static_cast<float>(getValue));
        value->intValue     = static_cast<int32_t>(precisionLevel);
    } break;
    case OB_PROP_DEPTH_UNIT_FLEXIBLE_ADJUSTMENT_FLOAT: {
        auto getValue     = getConfigValue("DisparityTransform#2");
        value->floatValue = static_cast<float>(getValue);
    } break;
    case OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_BOOL: {
        auto getValue   = getConfigValue("NoiseRemovalFilter#255");
        value->intValue = static_cast<int32_t>(getValue);
    } break;
    case OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_SPECKLE_SIZE_INT: {
        auto getValue   = getConfigValue("NoiseRemovalFilter#0");
        value->intValue = static_cast<int32_t>(getValue);
    } break;
    case OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_DIFF_INT: {
        auto getValue   = getConfigValue("NoiseRemovalFilter#1");
        value->intValue = static_cast<int32_t>(getValue);
    } break;
    default:
        throw invalid_value_exception("Invalid property id");
    }
}

void FrameProcessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    std::string configName = "";

    switch(propertyId) {
    case OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL:
        configName = "DisparityTransform#255";
        break;
    case OB_PROP_DEPTH_UNIT_FLEXIBLE_ADJUSTMENT_FLOAT:
        configName = "DisparityTransform#2";
        break;
    case OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_BOOL:
        configName = "NoiseRemovalFilter#255";
        break;
    case OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_SPECKLE_SIZE_INT:
        configName = "NoiseRemovalFilter#0";
        break;
    case OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_DIFF_INT:
        configName = "NoiseRemovalFilter#1";
        break;
    case OB_PROP_DEPTH_PRECISION_LEVEL_INT:
        OBPropertyValue cur;
        getPropertyValue(OB_PROP_DEPTH_PRECISION_LEVEL_INT, &cur);

        range->cur.intValue  = cur.intValue;
        range->def.intValue  = static_cast<int32_t>(OB_PRECISION_1MM);
        range->max.intValue  = static_cast<int32_t>(OB_PRECISION_1MM);
        range->min.intValue  = static_cast<int32_t>(OB_PRECISION_0MM05);
        range->step.intValue = 1;
        return;
    default:
        throw invalid_value_exception("Invalid property id");
    }

    double                   value = getConfigValue(configName);
    OBFilterConfigSchemaItem item  = getConfigSchemaItem(configName);

    if(item.type == OB_FILTER_CONFIG_VALUE_TYPE_FLOAT) {
        range->cur.floatValue  = static_cast<float>(value);
        range->def.floatValue  = static_cast<float>(item.def);
        range->max.floatValue  = static_cast<float>(item.max);
        range->min.floatValue  = static_cast<float>(item.min);
        range->step.floatValue = static_cast<float>(item.step);
    }
    else {
        range->cur.intValue  = static_cast<int32_t>(value);
        range->def.intValue  = static_cast<int32_t>(item.def);
        range->max.intValue  = static_cast<int32_t>(item.max);
        range->min.intValue  = static_cast<int32_t>(item.min);
        range->step.intValue = static_cast<int32_t>(item.step);
    }
}
// Depth frame processor
DepthFrameProcessor::DepthFrameProcessor(IDevice *owner, std::shared_ptr<FrameProcessorContext> context) : FrameProcessor(owner, context, OB_SENSOR_DEPTH) {}

DepthFrameProcessor::~DepthFrameProcessor() noexcept {}

void DepthFrameProcessor::setHardwareD2CProcessParams(uint32_t colorWidth, uint32_t colorHeight, uint32_t depthWidth, uint32_t depthHeight,
                                                      std::vector<OBCameraParam> calibrationCameraParams, std::vector<OBD2CProfile> d2cProfiles,
                                                      bool matchTargetResolution) {
    OBCameraParam currentCameraParam = {};
    OBD2CProfile  currentD2CProfile  = {};
    for(const auto &d2cProfile: d2cProfiles) {
        if(d2cProfile.colorWidth == colorWidth && d2cProfile.colorHeight == colorHeight && d2cProfile.depthWidth == depthWidth
           && d2cProfile.depthHeight == depthHeight && (d2cProfile.alignType & ALIGN_D2C_HW)) {
            currentD2CProfile = d2cProfile;
            break;
        }
    }

    bool valid =
        currentD2CProfile.colorWidth != 0 && currentD2CProfile.colorHeight != 0 && currentD2CProfile.depthWidth != 0 && currentD2CProfile.depthHeight != 0;
    if(!valid || static_cast<size_t>(currentD2CProfile.paramIndex) + 1 > calibrationCameraParams.size()) {
        throw invalid_value_exception("Current stream profile is not support hardware d2c process");
        return;
    }

    currentCameraParam = calibrationCameraParams.at(currentD2CProfile.paramIndex);

    if(context_->set_hardware_d2c_params) {
        ob_error *error = nullptr;
        context_->set_hardware_d2c_params(privateProcessor_, currentCameraParam, currentD2CProfile.paramIndex, currentD2CProfile.postProcessParam.depthScale,
                                          currentD2CProfile.postProcessParam.alignLeft, currentD2CProfile.postProcessParam.alignTop,
                                          currentD2CProfile.postProcessParam.alignRight, currentD2CProfile.postProcessParam.alignBottom, matchTargetResolution,
                                          &error);
        if(error) {
            LOG_ERROR("set hardware d2c params failed");
            delete error;
        }
    }
}

void DepthFrameProcessor::enableHardwareD2CProcess(bool enable) {
    TRY_EXECUTE(setConfigValueSync("HardwareD2CProcessor#255", static_cast<double>(enable)));
}

}  // namespace libobsensor
