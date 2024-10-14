// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "libobsensor/h/Pipeline.h"

#include "ImplTypes.hpp"
#include "exception/ObException.hpp"
#include "utils/Utils.hpp"
#include "pipeline/Pipeline.hpp"
#include "pipeline/Config.hpp"
#include "context/Context.hpp"

#ifdef __cplusplus
extern "C" {
#endif

ob_pipeline *ob_create_pipeline(ob_error **error) BEGIN_API_CALL {
    auto ctx        = libobsensor::Context::getInstance();
    auto devMgr     = ctx->getDeviceManager();
    auto deviceList = devMgr->getDeviceInfoList();
    if(deviceList.empty()) {
        throw libobsensor::camera_disconnected_exception("No device found");
    }
    const auto &devInfo  = deviceList.front();
    auto        dev      = devMgr->createDevice(devInfo);
    auto        pipeline = std::make_shared<libobsensor::Pipeline>(dev);

    auto impl      = new ob_pipeline();
    impl->pipeline = pipeline;
    return impl;
}
NO_ARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

ob_pipeline *ob_create_pipeline_with_device(const ob_device *dev, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(dev);
    auto pipeline = std::make_shared<libobsensor::Pipeline>(dev->device);

    auto impl      = new ob_pipeline();
    impl->pipeline = pipeline;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, dev)

void ob_delete_pipeline(ob_pipeline *pipeline, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(pipeline);
    delete pipeline;
}
HANDLE_EXCEPTIONS_NO_RETURN(pipeline)

void ob_pipeline_start(ob_pipeline *pipeline, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(pipeline);
    pipeline->pipeline->start();
}
HANDLE_EXCEPTIONS_NO_RETURN(pipeline)

void ob_pipeline_start_with_config(ob_pipeline *pipeline, const ob_config *config, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(pipeline);
    if(!config) {
        ob_pipeline_start(pipeline, error);
    }
    else {
        VALIDATE_NOT_NULL(config);
        pipeline->pipeline->start(config->config);
    }
}
HANDLE_EXCEPTIONS_NO_RETURN(pipeline)

void ob_pipeline_start_with_callback(ob_pipeline *pipeline, const ob_config *config, ob_frameset_callback callback, void *user_data,
                                     ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(pipeline);
    if(!config) {
        ob_pipeline_start(pipeline, error);
    }
    else {
        VALIDATE_NOT_NULL(config);
        pipeline->pipeline->start(config->config, [callback, user_data](std::shared_ptr<const libobsensor::Frame> frame) {
            auto impl   = new ob_frame();
            impl->frame = std::const_pointer_cast<libobsensor::Frame>(frame);  // todo: it's not safe to cast const to non-const, fix it
            callback(impl, user_data);
        });
    }
}
HANDLE_EXCEPTIONS_NO_RETURN(pipeline)

void ob_pipeline_stop(ob_pipeline *pipeline, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(pipeline);
    pipeline->pipeline->stop();
}
HANDLE_EXCEPTIONS_NO_RETURN(pipeline)

ob_config *ob_pipeline_get_config(const ob_pipeline *pipeline, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(pipeline);
    auto config    = new ob_config();
    config->config = std::const_pointer_cast<libobsensor::Config>(pipeline->pipeline->getConfig());  // todo: it's not safe to cast const to non-const, fix it
    return config;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, pipeline)

void ob_pipeline_switch_config(ob_pipeline *pipeline, ob_config *config, ob_error **error) BEGIN_API_CALL {
    pipeline->pipeline->switchConfig(config->config);
}
HANDLE_EXCEPTIONS_NO_RETURN(pipeline)

ob_frame *ob_pipeline_wait_for_frameset(ob_pipeline *pipeline, uint32_t timeout_ms, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(pipeline);
    auto frame = pipeline->pipeline->waitForFrame(timeout_ms);
    if(!frame) {
        return nullptr;
    }
    auto impl   = new ob_frame();
    impl->frame = std::const_pointer_cast<libobsensor::Frame>(frame);  // todo: it's not safe to cast const to non-const, fix it
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, pipeline)

ob_device *ob_pipeline_get_device(const ob_pipeline *pipeline, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(pipeline);
    auto device  = pipeline->pipeline->getDevice();
    auto impl    = new ob_device();
    impl->device = device;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, pipeline)

ob_stream_profile_list *ob_pipeline_get_stream_profile_list(const ob_pipeline *pipeline, ob_sensor_type sensorType, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(pipeline);
    auto device       = pipeline->pipeline->getDevice();
    auto sensor       = device->getSensor(sensorType);
    auto profiles     = sensor->getStreamProfileList();
    auto impl         = new ob_stream_profile_list();
    impl->profileList = profiles;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, pipeline)

void ob_pipeline_enable_frame_sync(ob_pipeline *pipeline, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(pipeline);
    pipeline->pipeline->enableFrameSync();
}
HANDLE_EXCEPTIONS_NO_RETURN(pipeline)

void ob_pipeline_disable_frame_sync(ob_pipeline *pipeline, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(pipeline);
    pipeline->pipeline->disableFrameSync();
}
HANDLE_EXCEPTIONS_NO_RETURN(pipeline)

ob_stream_profile_list *ob_get_d2c_depth_profile_list(const ob_pipeline *pipeline, const ob_stream_profile *color_profile, ob_align_mode align_mode,
                                                      ob_error **error) BEGIN_API_CALL {
    auto innerProfiles = pipeline->pipeline->getD2CDepthProfileList(color_profile->profile, align_mode);
    auto impl          = new ob_stream_profile_list();
    impl->profileList  = innerProfiles;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, pipeline)

ob_camera_param ob_pipeline_get_camera_param(ob_pipeline *pipeline, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(pipeline);
    return pipeline->pipeline->getCameraParam();
}
HANDLE_EXCEPTIONS_AND_RETURN(ob_camera_param(), pipeline)

ob_camera_param ob_pipeline_get_camera_param_with_profile(ob_pipeline *pipeline, uint32_t colorWidth, uint32_t colorHeight, uint32_t depthWidth,
                                                          uint32_t depthHeight, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(pipeline);
    return pipeline->pipeline->getCameraParam(colorWidth, colorHeight, depthWidth, depthHeight);
}
HANDLE_EXCEPTIONS_AND_RETURN(ob_camera_param(), pipeline, colorWidth, colorHeight, depthWidth, depthHeight)

ob_calibration_param ob_pipeline_get_calibration_param(ob_pipeline *pipeline, ob_config *config, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(pipeline);
    return pipeline->pipeline->getCalibrationParam(config ? config->config : nullptr);
}
HANDLE_EXCEPTIONS_AND_RETURN(ob_calibration_param(), pipeline, config)

ob_config *ob_create_config(ob_error **error) BEGIN_API_CALL {
    auto config    = new ob_config();
    config->config = std::make_shared<libobsensor::Config>();
    return config;
}
NO_ARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

void ob_delete_config(ob_config *config, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    delete config;
}
HANDLE_EXCEPTIONS_NO_RETURN(config)

void ob_config_enable_stream_with_stream_profile(ob_config *config, const ob_stream_profile *profile, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    VALIDATE_NOT_NULL(profile);
    config->config->enableStream(profile->profile);
}
HANDLE_EXCEPTIONS_NO_RETURN(config, profile)

void ob_config_enable_stream(ob_config *config, ob_stream_type stream_type, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    config->config->enableStream(stream_type);
}
HANDLE_EXCEPTIONS_NO_RETURN(config, stream_type)

void ob_config_enable_all_stream(ob_config *config, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    for(int i = 0; i < OB_STREAM_TYPE_COUNT; i++) {
        config->config->enableStream(static_cast<ob_stream_type>(i));
    }
}
HANDLE_EXCEPTIONS_NO_RETURN(config)

void ob_config_enable_video_stream(ob_config *config, ob_stream_type stream_type, uint32_t width, uint32_t height, uint32_t fps, ob_format format,
                                   ob_error **error) BEGIN_API_CALL {
    config->config->enableVideoStream(stream_type, width, height, fps, format);
}
HANDLE_EXCEPTIONS_NO_RETURN(config, stream_type, width, height, fps, format)

void ob_config_enable_accel_stream(ob_config *config, ob_accel_full_scale_range full_scale_range, ob_accel_sample_rate sample_rate,
                                   ob_error **error) BEGIN_API_CALL {
    config->config->enableAccelStream(full_scale_range, sample_rate);
}
HANDLE_EXCEPTIONS_NO_RETURN(config, full_scale_range, sample_rate)

void ob_config_enable_gyro_stream(ob_config *config, ob_gyro_full_scale_range full_scale_range, ob_gyro_sample_rate sample_rate,
                                  ob_error **error) BEGIN_API_CALL {
    config->config->enableGyroStream(full_scale_range, sample_rate);
}
HANDLE_EXCEPTIONS_NO_RETURN(config, full_scale_range, sample_rate)

ob_stream_profile_list *ob_config_get_enabled_stream_profile_list(const ob_config *config, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    auto profiles     = config->config->getEnabledStreamProfileList();
    auto impl         = new ob_stream_profile_list();
    impl->profileList = profiles;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, config)

void ob_config_disable_stream(ob_config *config, ob_stream_type type, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    config->config->disableStream(type);
}
HANDLE_EXCEPTIONS_NO_RETURN(config, type)

void ob_config_disable_all_stream(ob_config *config, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    config->config->disableAllStream();
}
HANDLE_EXCEPTIONS_NO_RETURN(config)

void ob_config_set_align_mode(ob_config *config, ob_align_mode mode, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    config->config->setAlignMode(mode);
}
HANDLE_EXCEPTIONS_NO_RETURN(config, mode)

void ob_config_set_depth_scale_after_align_require(ob_config *config, bool enable, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    config->config->setDepthScaleAfterAlignRequire(enable);
}
HANDLE_EXCEPTIONS_NO_RETURN(config, enable)

void ob_config_set_frame_aggregate_output_mode(ob_config *config, ob_frame_aggregate_output_mode mode, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    config->config->setFrameAggregateOutputMode(mode);
}
HANDLE_EXCEPTIONS_NO_RETURN(config, mode)

#ifdef __cplusplus
}
#endif
