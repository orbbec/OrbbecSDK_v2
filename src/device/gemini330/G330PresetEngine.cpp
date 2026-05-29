// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G330PresetEngine.hpp"
#include "preset/CompareHandler.hpp"
#include "preset/PropertyConfigHandler.hpp"
#include "preset/PresetHandler.hpp"
#include "preset/PostProcessingFilterHandler.hpp"
#include "preset/StreamProfileHandler.hpp"
#include "preset/PresetDefinitions.hpp"
#include "environment/Version.hpp"

#include <iosfwd>
#include <string>

namespace libobsensor {

G330PresetEngineV1::G330PresetEngineV1(IDevice *owner) : PresetEngineBase(owner) {}

void G330PresetEngineV1::init() {
    if(initialized_) {
        return;
    }

    // depth
    configEngine_.addLeaf("depth_alg_mode", std::make_shared<DepthWorkModeHandler>(owner_));
    configEngine_.addLeaf("laser_state", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_LASER_CONTROL_INT));
    configEngine_.addLeaf("laser_power_level", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_LASER_POWER_LEVEL_CONTROL_INT));
    configEngine_.addLeaf("depth_auto_exposure", std::make_shared<PropertyConfigHandler<bool>>(owner_, OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL));
    configEngine_.addLeaf("depth_exposure_time", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_IR_EXPOSURE_INT));
    configEngine_.addLeaf("depth_gain", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_IR_GAIN_INT));
    configEngine_.addLeaf("target_mean_intensity", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_IR_BRIGHTNESS_INT));
    // rgb
    configEngine_.addLeaf("color_auto_exposure", std::make_shared<PropertyConfigHandler<bool>>(owner_, OB_PROP_COLOR_AUTO_EXPOSURE_BOOL));
    configEngine_.addLeaf("color_exposure_time", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_EXPOSURE_INT));
    configEngine_.addLeaf("color_auto_white_balance", std::make_shared<PropertyConfigHandler<bool>>(owner_, OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL));
    configEngine_.addLeaf("color_white_balance", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_WHITE_BALANCE_INT));
    configEngine_.addLeaf("color_gain", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_GAIN_INT));
    configEngine_.addLeaf("color_contrast", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_CONTRAST_INT));
    configEngine_.addLeaf("color_saturation", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_SATURATION_INT));
    configEngine_.addLeaf("color_sharpness", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_SHARPNESS_INT));
    configEngine_.addLeaf("color_brightness", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_BRIGHTNESS_INT));
    configEngine_.addLeaf("color_hue", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_HUE_INT));
    configEngine_.addLeaf("color_gamma", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_GAMMA_INT));
    configEngine_.addLeaf("color_backlight_compensation", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_BACKLIGHT_COMPENSATION_INT));
    configEngine_.addLeaf("color_power_line_frequency", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT));

    initialized_.store(true);
}

G330PresetEngine::G330PresetEngine(IDevice *owner) : PresetEngineBase(owner) {}

void G330PresetEngine::init() {
    if(initialized_) {
        return;
    }

    auto firmwareVersionInt = owner_->getFirmwareVersionInt();

    // comment
    configEngine_.addLeaf("comment", std::make_shared<CommentHandler>("null means the device doesn't support this property and will ignore it"));
    // version
    configEngine_.declareObject(kApiVersion, [&](jsonmodel::ConfigEngine &engine) {
        engine.addLeaf("preset", std::make_shared<ValueCompareHandler<unsigned int>>(kG330PresetVersion, CompareMode::Equal));
        engine.addLeaf("sdk", std::make_shared<VersionHandler>(OB_LIB_VERSION_STR, CompareMode::Ignore));
    });
    // device info
    configEngine_.declareObject("device_info", [&](jsonmodel::ConfigEngine &engine) {
        auto devInfo = owner_->getInfo();
        engine.addLeaf("fw_version", std::make_shared<VersionHandler>(devInfo->fwVersion_, CompareMode::Ignore));
        engine.addLeaf("vid", std::make_shared<HexValueCompareHandler<int>>(devInfo->vid_, CompareMode::Equal, 4));
        engine.addLeaf("pid", std::make_shared<HexValueCompareHandler<int>>(devInfo->pid_, CompareMode::Equal, 4));
        engine.addLeaf("name", std::make_shared<VersionHandler>(devInfo->fullName_, CompareMode::Ignore));
    });
    // parameters
    configEngine_.declareObject("parameters", [&](jsonmodel::ConfigEngine &engine) {
        // device settings
        engine.declareObject("device", [&](jsonmodel::ConfigEngine &engine) {
            engine.addLeaf("device_ptp_clock_sync", std::make_shared<PropertyConfigHandler<bool>>(owner_, OB_DEVICE_PTP_CLOCK_SYNC_ENABLE_BOOL));
            engine.addLeaf("device_heartbeat", std::make_shared<HeartbeatHandler>(owner_, false));
            engine.addLeaf("device_firmware_log", std::make_shared<HeartbeatHandler>(owner_, true));
            engine.addLeaf("device_usb2_repeat_identify", std::make_shared<PropertyConfigHandler<bool>>(owner_, OB_PROP_DEVICE_USB2_REPEAT_IDENTIFY_BOOL));
        });
        // depth
        engine.declareObject("sensor_depth", [&](jsonmodel::ConfigEngine &engine) {
            engine.addLeaf("depth_preset", std::make_shared<DepthWorkModeHandler>(owner_));
            engine.addLeaf("depth_auto_exposure_priority", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_DEPTH_AUTO_EXPOSURE_PRIORITY_INT));
            engine.addLeaf("depth_auto_exposure", std::make_shared<PropertyConfigHandler<bool>>(owner_, OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL));
            engine.addLeaf("depth_ae_max_exposure", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_IR_AE_MAX_EXPOSURE_INT));
            engine.addLeaf("mean_intensity_set_point", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_IR_BRIGHTNESS_INT));
            engine.addLeaf("depth_exposure_time", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_IR_EXPOSURE_INT));
            engine.addLeaf("depth_gain", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_IR_GAIN_INT));
            engine.addLeaf("depth_unit", std::make_shared<PropertyConfigHandler<float>>(owner_, OB_PROP_DEPTH_UNIT_FLEXIBLE_ADJUSTMENT_FLOAT));
            engine.addLeaf("laser_state", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_LASER_CONTROL_INT));
            engine.addLeaf("laser_power_level", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_LASER_POWER_LEVEL_CONTROL_INT));
            engine.addLeaf("laser_ranging_module", std::make_shared<PropertyConfigHandler<bool>>(owner_, OB_PROP_LDP_BOOL));
            // depth ae_roi
            engine.addObject(
                "depth_ae_roi",
                [&](jsonmodel::ConfigEngine &engine) {
                    // all leaf configs handled by parent object
                    engine.declareLeaf(kLeftKey);    // int
                    engine.declareLeaf(kRightKey);   // int
                    engine.declareLeaf(kTopKey);     // int
                    engine.declareLeaf(kBottomKey);  // int
                },
                std::make_shared<RoiHandler>(owner_, OB_STRUCT_DEPTH_AE_ROI, OB_SENSOR_DEPTH, firmwareVersionInt < 10735));
            // frame interleave
            engine.addObject(
                "frame_interleave",
                [&](jsonmodel::ConfigEngine &engine) {
                    // all leaf configs handled by parent object
                    engine.declareLeaf(kEnableKey);                 // bool
                    engine.declareLeaf(kInterleaveModeKey);         // string
                    engine.declareLeaf(kInterleaveConfigIndexKey);  // int
                    engine.declareLeaf(kInterleaveParmas);          // array
                    // Note: params is an array of configuration parameters, each containing fields such as:
                    // "depth_exposure"          (int)
                    // "depth_gain"              (int)
                    // "depth_brightness"        (int)
                    // "depth_ae_max_exposure"   (int)
                    // "laser_control"           (int)
                },
                std::make_shared<FrameInterleaveHandler>(owner_));
            // D2D
            engine.addLeaf("disparity_to_depth_mode", std::make_shared<D2DHandler>(owner_));
            // disparity search
            engine.addObject(
                "disparity_search",
                [&](jsonmodel::ConfigEngine &engine) {
                    engine.declareLeaf(kDisparitySearchRangeModeKey);  // int
                    engine.declareLeaf(kDisparitySearchOffsetKey);     // int
                },
                std::make_shared<DisparitySearchHandler>(owner_, firmwareVersionInt < 10735));
            // Depth noise removal filter
            engine.declareObject("noise_removal_filter", [&](jsonmodel::ConfigEngine &engine) {
                // hardware noise remove filter
                engine.addObject(
                    "hardware",
                    [&](jsonmodel::ConfigEngine &engine) {
                        engine.declareLeaf(kEnableKey);                       // bool
                        engine.declareLeaf(kNoiseRemovalFilterThresholdKey);  // float
                    },
                    std::make_shared<HardwareNoiseRemovalFilterHandler>(owner_));
                // software noise remove filter
                engine.addObject(
                    "software",
                    [&](jsonmodel::ConfigEngine &engine) {
                        engine.declareLeaf(kEnableKey);                           // bool
                        engine.declareLeaf(kNoiseRemovalFilterMinDifferenceKey);  // int
                        engine.declareLeaf(kNoiseRemovalFilterMaxSizeKey);        // int
                    },
                    std::make_shared<SoftwareNoiseRemovalFilterHandler>(owner_));
            });
            // Post processing filter
            engine.addLeaf("post_processing_filter", std::make_shared<PostProcessingFilterHandler>(owner_, OB_SENSOR_DEPTH));
            // image orientation
            engine.declareObject("image_orientation", [&](jsonmodel::ConfigEngine &engine) {
                engine.addLeaf("flip", std::make_shared<PropertyConfigHandler<bool>>(owner_, OB_PROP_DEPTH_FLIP_BOOL));
                engine.addLeaf("mirror", std::make_shared<PropertyConfigHandler<bool>>(owner_, OB_PROP_DEPTH_MIRROR_BOOL));
                engine.addLeaf("rotation", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_DEPTH_ROTATE_INT));
            });
        });
        // color
        engine.declareObject("sensor_color", [&](jsonmodel::ConfigEngine &engine) {
            engine.addLeaf("color_auto_exposure_priority", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_AUTO_EXPOSURE_PRIORITY_INT));
            engine.addLeaf("color_auto_exposure", std::make_shared<PropertyConfigHandler<bool>>(owner_, OB_PROP_COLOR_AUTO_EXPOSURE_BOOL));
            engine.addLeaf("color_denoising_level", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_DENOISING_LEVEL_INT));
            engine.addLeaf("color_ae_max_exposure", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_AE_MAX_EXPOSURE_INT));
            engine.addLeaf("color_exposure_time", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_EXPOSURE_INT));
            engine.addLeaf("color_gain", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_GAIN_INT));
            engine.addLeaf("color_brightness", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_BRIGHTNESS_INT));
            engine.addLeaf("color_auto_white_balance", std::make_shared<PropertyConfigHandler<bool>>(owner_, OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL));
            engine.addLeaf("color_white_balance", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_WHITE_BALANCE_INT));
            engine.addLeaf("color_sharpness", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_SHARPNESS_INT));
            engine.addLeaf("color_gamma", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_GAMMA_INT));
            engine.addLeaf("color_hue", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_HUE_INT));
            engine.addLeaf("color_backlight_compensation", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_BACKLIGHT_COMPENSATION_INT));
            engine.addLeaf("color_contrast", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_CONTRAST_INT));
            engine.addLeaf("color_saturation", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_SATURATION_INT));
            engine.addLeaf("color_power_line_frequency", std::make_shared<ColorPowerLineFrequencyHandler>(owner_));
            engine.addLeaf("color_anti_flicker", std::make_shared<PropertyConfigHandler<bool>>(owner_, OB_PROP_COLOR_ANTI_FLICKER_BOOL));
            engine.addLeaf("color_preset", std::make_shared<ColorPresetHandler>(owner_));
            // ae_roi
            engine.addObject(
                "color_ae_roi",
                [&](jsonmodel::ConfigEngine &engine) {
                    // all leaf configs handled by parent object
                    engine.declareLeaf(kLeftKey);    // int
                    engine.declareLeaf(kRightKey);   // int
                    engine.declareLeaf(kTopKey);     // int
                    engine.declareLeaf(kBottomKey);  // int
                },
                std::make_shared<RoiHandler>(owner_, OB_STRUCT_COLOR_AE_ROI, OB_SENSOR_COLOR, firmwareVersionInt < 10735));
            // image orientation
            engine.declareObject("image_orientation", [&](jsonmodel::ConfigEngine &engine) {
                engine.addLeaf("flip", std::make_shared<PropertyConfigHandler<bool>>(owner_, OB_PROP_COLOR_FLIP_BOOL));
                engine.addLeaf("mirror", std::make_shared<PropertyConfigHandler<bool>>(owner_, OB_PROP_COLOR_MIRROR_BOOL));
                engine.addLeaf("rotation", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_COLOR_ROTATE_INT));
            });
            // Post processing filter
            engine.addLeaf("post_processing_filter", std::make_shared<PostProcessingFilterHandler>(owner_, OB_SENSOR_COLOR));
        });
        // left ir
        engine.declareObject("sensor_left_ir", [&](jsonmodel::ConfigEngine &engine) {
            // image orientation
            engine.declareObject("image_orientation", [&](jsonmodel::ConfigEngine &engine) {
                engine.addLeaf("flip", std::make_shared<PropertyConfigHandler<bool>>(owner_, OB_PROP_IR_FLIP_BOOL));
                engine.addLeaf("mirror", std::make_shared<PropertyConfigHandler<bool>>(owner_, OB_PROP_IR_MIRROR_BOOL));
                engine.addLeaf("rotation", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_IR_ROTATE_INT));
            });
            // Post processing filter
            engine.addLeaf("post_processing_filter", std::make_shared<PostProcessingFilterHandler>(owner_, OB_SENSOR_IR_LEFT));
        });
        // right ir
        engine.declareObject("sensor_right_ir", [&](jsonmodel::ConfigEngine &engine) {
            // image orientation
            engine.declareObject("image_orientation", [&](jsonmodel::ConfigEngine &engine) {
                engine.addLeaf("flip", std::make_shared<PropertyConfigHandler<bool>>(owner_, OB_PROP_IR_RIGHT_FLIP_BOOL));
                engine.addLeaf("mirror", std::make_shared<PropertyConfigHandler<bool>>(owner_, OB_PROP_IR_RIGHT_MIRROR_BOOL));
                engine.addLeaf("rotation", std::make_shared<PropertyConfigHandler<int>>(owner_, OB_PROP_IR_RIGHT_ROTATE_INT));
            });
            // Post processing filter
            engine.addLeaf("post_processing_filter", std::make_shared<PostProcessingFilterHandler>(owner_, OB_SENSOR_IR_RIGHT));
        });
    });

    initialized_.store(true);
}

}  // namespace libobsensor
