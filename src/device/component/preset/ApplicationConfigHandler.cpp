// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "ApplicationConfigHandler.hpp"

#include "ApplicationConfig.hpp"
#include "IDevice.hpp"
#include "IPresetManager.hpp"
#include "ISensor.hpp"
#include "PresetDefinitions.hpp"
#include "exception/ObException.hpp"
#include "logger/Logger.hpp"
#include "stream/StreamProfile.hpp"
#include "utils/PublicTypeHelper.hpp"

namespace libobsensor {
namespace {

using jsonmodel::ExportField;
using jsonmodel::ExportValue;
using jsonmodel::makeField;

// Current Application config version
constexpr const uint32_t kAppConfigSchemaVersion = 1;
constexpr const char     kAppConfigRoot[]        = "application_config";

constexpr const char kSchemaVersion[]       = "schema_version";
constexpr const char kSensors[]             = "sensors";
constexpr const char kProfile[]             = "profile";
constexpr const char kStreamEnabled[]       = "stream_enabled";
constexpr const char kUndistortionEnabled[] = "undistortion_enabled";
constexpr const char kDeviceDecimation[]    = "device_decimation";
constexpr const char kPointCloud[]          = "point_cloud";
constexpr const char kHdrMerge[]            = "hdr_merge";
constexpr const char kIrEnabled[]           = "ir_enabled";
constexpr const char kDepthPoint[]          = "DEPTH_POINT";
constexpr const char kRgbdPoint[]           = "RGBD_POINT";

// profile
constexpr const char kFormat[]         = "format";
constexpr const char kWidth[]          = "width";
constexpr const char kHeight[]         = "height";
constexpr const char kFps[]            = "fps";
constexpr const char kSampleRate[]     = "sample_rate";
constexpr const char kFullScaleRange[] = "full_scale_range";
constexpr const char kDecimation[]     = "decimation";
constexpr const char kFactor[]         = "factor";

constexpr const char kOriginWidth[]  = "origin_width";
constexpr const char kOriginHeight[] = "origin_height";
constexpr const char kDepthFactor[]  = "depth_factor";
constexpr const char kIrFactor[]     = "ir_factor";

constexpr const char kDecimationFactor[]      = "decimation_factor";
constexpr const char kAlignMode[]             = "align_mode";
constexpr const char kFrameSync[]             = "frame_sync";
constexpr const char kAllFrameTypeRequired[]  = "all_frame_type_required";
constexpr const char kMatchTargetResolution[] = "match_target_resolution";

struct SchemaField {
    const char *key;
    bool        optional;
};

constexpr SchemaField kRootLeafFields[] = {
    { kSchemaVersion, false },
};

constexpr SchemaField kRootObjectFields[] = {
    { kSensors, false },
    { kDeviceDecimation, true },
    { kPointCloud, false },
    { kHdrMerge, false },
};

constexpr SchemaField kDeviceDecimationFields[] = {
    { kEnableKey, false }, { kOriginWidth, true }, { kOriginHeight, true }, { kDepthFactor, true }, { kIrFactor, true },
};

constexpr SchemaField kPointCloudFields[] = {
    { kEnableKey, false },
    { kFormat, false },
    { kDecimationFactor, false },
    { kAlignMode, false },
    { kFrameSync, false },
    { kAllFrameTypeRequired, false },
    { kMatchTargetResolution, false },
};

constexpr SchemaField kHdrMergeFields[] = {
    { kEnableKey, false },
    { kIrEnabled, false },
};

constexpr SchemaField kSensorFields[] = {
    { kStreamEnabled, false },
    { kProfile, false },
};

constexpr SchemaField kVideoSensorFields[] = {
    { kUndistortionEnabled, false },
};

constexpr SchemaField kVideoProfileFields[] = {
    { kWidth, false },
    { kHeight, false },
    { kFps, false },
    { kFormat, false },
};

constexpr SchemaField kImuProfileFields[] = {
    { kSampleRate, false },
    { kFullScaleRange, false },
};

constexpr SchemaField kProfileDecimationFields[] = {
    { kOriginWidth, false },
    { kOriginHeight, false },
    { kFactor, false },
};

bool getBool(const ApplicationConfig &config, OBAppConfigItem item) {
    return config.getBool(item);
}

int32_t getInt(const ApplicationConfig &config, OBAppConfigItem item) {
    return config.getInt(item);
}

std::string pointCloudFormatToWireString(OBFormat format) {
    return format == OB_FORMAT_RGB_POINT ? kRgbdPoint : kDepthPoint;
}

OBFormat wireStringToPointCloudFormat(const std::string &format) {
    if(format == kRgbdPoint) {
        return OB_FORMAT_RGB_POINT;
    }
    return OB_FORMAT_POINT;
}

void requireObject(const Json::Value &json, const char *field, const std::string &context) {
    if(!json.isMember(field) || !json[field].isObject()) {
        THROW_INVALID_PARAM_EXCEPTION("Missing required object in " + context + ": " + std::string(field));
    }
}

void requireField(const Json::Value &json, const char *field, const std::string &context) {
    if(!json.isMember(field)) {
        THROW_INVALID_PARAM_EXCEPTION("Missing required field in " + context + ": " + std::string(field));
    }
}

template <size_t N> void requireRequiredFields(const Json::Value &json, const SchemaField (&fields)[N], const std::string &context) {
    for(const auto &field: fields) {
        if(!field.optional) {
            requireField(json, field.key, context);
        }
    }
}

template <size_t N> void requireAllFields(const Json::Value &json, const SchemaField (&fields)[N], const std::string &context) {
    for(const auto &field: fields) {
        requireField(json, field.key, context);
    }
}

template <size_t N> void requireRequiredObjects(const Json::Value &json, const SchemaField (&fields)[N], const std::string &context) {
    for(const auto &field: fields) {
        if(!field.optional) {
            requireObject(json, field.key, context);
        }
        else if(json.isMember(field.key) && !json[field.key].isObject()) {
            THROW_INVALID_PARAM_EXCEPTION("Node type mismatch in " + context + ": " + std::string(field.key));
        }
    }
}

ExportValue profileToExportValue(const std::shared_ptr<const StreamProfile> &profile) {
    std::vector<ExportField> fields;
    if(!profile) {
        return ExportValue::object(std::move(fields));
    }

    if(profile->is<VideoStreamProfile>()) {
        auto vsp = profile->as<VideoStreamProfile>();
        fields.emplace_back(makeField(kWidth, static_cast<unsigned int>(vsp->getWidth())));
        fields.emplace_back(makeField(kHeight, static_cast<unsigned int>(vsp->getHeight())));
        fields.emplace_back(makeField(kFps, static_cast<unsigned int>(vsp->getFps())));
        fields.emplace_back(makeField(kFormat, utils::obFormatToStr(profile->getFormat())));

        auto decimation = vsp->getDecimationConfig();
        if(decimation.originWidth != 0 || decimation.originHeight != 0 || decimation.factor != 0) {
            std::vector<ExportField> decimationFields;
            decimationFields.emplace_back(makeField(kOriginWidth, static_cast<unsigned int>(decimation.originWidth)));
            decimationFields.emplace_back(makeField(kOriginHeight, static_cast<unsigned int>(decimation.originHeight)));
            decimationFields.emplace_back(makeField(kFactor, static_cast<unsigned int>(decimation.factor)));
            fields.emplace_back(makeField(kDecimation, ExportValue::object(std::move(decimationFields))));
        }
    }
    else if(profile->is<AccelStreamProfile>()) {
        auto asp = profile->as<AccelStreamProfile>();
        fields.emplace_back(makeField(kSampleRate, utils::mapIMUSampleRateToValue(asp->getSampleRate())));
        fields.emplace_back(makeField(kFullScaleRange, utils::AccelFullScaleRangeToStr(asp->getFullScaleRange())));
    }
    else if(profile->is<GyroStreamProfile>()) {
        auto gsp = profile->as<GyroStreamProfile>();
        fields.emplace_back(makeField(kSampleRate, utils::mapIMUSampleRateToValue(gsp->getSampleRate())));
        fields.emplace_back(makeField(kFullScaleRange, utils::GyroFullScaleRangeToStr(gsp->getFullScaleRange())));
    }
    else {
        fields.emplace_back(makeField(kFormat, utils::obFormatToStr(profile->getFormat())));
    }

    return ExportValue::object(std::move(fields));
}

std::shared_ptr<const StreamProfile> findProfile(IDevice *owner, OBSensorType sensorType, const Json::Value &profileJson) {
    if(owner == nullptr || !profileJson.isObject() || !owner->isSensorExists(sensorType)) {
        return nullptr;
    }

    auto sensor = owner->getSensor(sensorType);
    auto list   = sensor->getStreamProfileList();

    if(sensorType == OB_SENSOR_ACCEL) {
        if(!profileJson.isMember(kSampleRate) || !profileJson.isMember(kFullScaleRange)) {
            return nullptr;
        }

        auto sampleRate         = static_cast<OBAccelSampleRate>(utils::mapValueToIMUSampleRate(profileJson[kSampleRate].asFloat()));
        auto range              = utils::strToAccelFullScaleRange(profileJson[kFullScaleRange].asString());
        auto matchedProfileList = matchAccelStreamProfile(list, range, sampleRate);
        return matchedProfileList.empty() ? nullptr : matchedProfileList[0];
    }
    else if(sensorType == OB_SENSOR_GYRO) {
        if(!profileJson.isMember(kSampleRate) || !profileJson.isMember(kFullScaleRange)) {
            return nullptr;
        }

        auto sampleRate         = static_cast<OBGyroSampleRate>(utils::mapValueToIMUSampleRate(profileJson[kSampleRate].asFloat()));
        auto range              = utils::strToGyroFullScaleRange(profileJson[kFullScaleRange].asString());
        auto matchedProfileList = matchGyroStreamProfile(list, range, sampleRate);
        return matchedProfileList.empty() ? nullptr : matchedProfileList[0];
    }

    if(!ob_is_video_sensor_type(sensorType)) {
        return nullptr;
    }

    auto format = utils::strToOBFormat(profileJson[kFormat].asString());
    auto fps    = profileJson[kFps].asUInt();

    if(profileJson.isMember(kDecimation) && profileJson[kDecimation].isObject()) {
        OBHardwareDecimationConfig decimation{};
        auto                       decimationJson = profileJson[kDecimation];
        decimation.originWidth                    = decimationJson[kOriginWidth].asUInt();
        decimation.originHeight                   = decimationJson[kOriginHeight].asUInt();
        decimation.factor                         = decimationJson[kFactor].asUInt();
        auto matchedProfileList                   = matchVideoStreamProfile(list, fps, format, decimation);
        return matchedProfileList.empty() ? nullptr : matchedProfileList[0];
    }

    if(profileJson.isMember(kWidth) && profileJson.isMember(kHeight)) {
        auto matchedProfileList = matchVideoStreamProfile(list, profileJson[kWidth].asUInt(), profileJson[kHeight].asUInt(), fps, format);
        return matchedProfileList.empty() ? nullptr : matchedProfileList[0];
    }

    return nullptr;
}

ExportValue buildApplicationConfigValue(const ApplicationConfig &config) {
    std::vector<ExportField> rootFields;
    rootFields.emplace_back(makeField(kSchemaVersion, kAppConfigSchemaVersion));

    // sensors
    std::vector<ExportField> sensorFields;
    auto                     sensorCount = config.getCount(OB_APP_CONFIG_SENSORS);
    for(uint32_t i = 0; i < sensorCount; i++) {
        auto sensorType = static_cast<OBSensorType>(config.getInt(OB_APP_CONFIG_ARRAY_ITEM(OB_APP_CONFIG_SENSOR_TYPE, i)));

        std::vector<ExportField> fields;
        fields.emplace_back(makeField(kStreamEnabled, getBool(config, OB_APP_CONFIG_ARRAY_ITEM(OB_APP_CONFIG_STREAM_ENABLED, i))));
        auto profile = config.getProfile(OB_APP_CONFIG_ARRAY_ITEM(OB_APP_CONFIG_STREAM_PROFILE, i));
        fields.emplace_back(makeField(kProfile, profileToExportValue(profile)));
        if(ob_is_video_sensor_type(sensorType)) {
            fields.emplace_back(makeField(kUndistortionEnabled, getBool(config, OB_APP_CONFIG_ARRAY_ITEM(OB_APP_CONFIG_UNDISTORTION_ENABLED, i))));
        }
        sensorFields.emplace_back(makeField(utils::obSensorToStr(sensorType), ExportValue::object(std::move(fields))));
    }
    if(sensorFields.empty()) {
        THROW_INVALID_PARAM_EXCEPTION("ApplicationConfig export: sensors list is empty");
    }
    rootFields.emplace_back(makeField(kSensors, ExportValue::object(std::move(sensorFields))));

    // device decimation
    std::vector<ExportField> deviceDecimationFields;
    auto                     enable = getBool(config, OB_APP_CONFIG_ITEM(OB_APP_CONFIG_DEVICE_DECIMATION_ENABLED));
    deviceDecimationFields.emplace_back(makeField(kEnableKey, enable));
    if(enable) {
        OBPresetResolutionConfig deviceDecimation{};
        config.getStruct(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_DEVICE_DECIMATION), &deviceDecimation, sizeof(deviceDecimation));

        deviceDecimationFields.emplace_back(makeField(kOriginWidth, static_cast<int32_t>(deviceDecimation.width)));
        deviceDecimationFields.emplace_back(makeField(kOriginHeight, static_cast<int32_t>(deviceDecimation.height)));
        deviceDecimationFields.emplace_back(makeField(kDepthFactor, deviceDecimation.depthDecimationFactor));
        deviceDecimationFields.emplace_back(makeField(kIrFactor, deviceDecimation.irDecimationFactor));
    }
    rootFields.emplace_back(makeField(kDeviceDecimation, ExportValue::object(std::move(deviceDecimationFields))));

    // point cloud
    std::vector<ExportField> pointCloudFields;
    pointCloudFields.emplace_back(makeField(kEnableKey, getBool(config, OB_APP_CONFIG_ITEM(OB_APP_CONFIG_POINTCLOUD_ENABLED))));

    auto intValue = getInt(config, OB_APP_CONFIG_ITEM(OB_APP_CONFIG_POINTCLOUD_FORMAT));
    pointCloudFields.emplace_back(makeField(kFormat, pointCloudFormatToWireString(static_cast<OBFormat>(intValue))));
    intValue = getInt(config, OB_APP_CONFIG_ITEM(OB_APP_CONFIG_POINTCLOUD_DECIMATION_FACTOR));
    pointCloudFields.emplace_back(makeField(kDecimationFactor, intValue));
    intValue = getInt(config, OB_APP_CONFIG_ITEM(OB_APP_CONFIG_POINTCLOUD_ALIGN_MODE));
    pointCloudFields.emplace_back(makeField(kAlignMode, utils::obAlignModeToStr(static_cast<OBAlignMode>(intValue))));
    pointCloudFields.emplace_back(makeField(kFrameSync, getBool(config, OB_APP_CONFIG_ITEM(OB_APP_CONFIG_POINTCLOUD_FRAME_SYNC))));
    pointCloudFields.emplace_back(makeField(kAllFrameTypeRequired, getBool(config, OB_APP_CONFIG_ITEM(OB_APP_CONFIG_POINTCLOUD_ALL_FRAME_TYPE_REQUIRED))));
    pointCloudFields.emplace_back(makeField(kMatchTargetResolution, getBool(config, OB_APP_CONFIG_ITEM(OB_APP_CONFIG_POINTCLOUD_MATCH_TARGET_RESOLUTION))));
    rootFields.emplace_back(makeField(kPointCloud, ExportValue::object(std::move(pointCloudFields))));

    // hdr merge
    std::vector<ExportField> hdrFields;
    hdrFields.emplace_back(makeField(kEnableKey, getBool(config, OB_APP_CONFIG_ITEM(OB_APP_CONFIG_HDR_MERGE_ENABLED))));
    hdrFields.emplace_back(makeField(kIrEnabled, getBool(config, OB_APP_CONFIG_ITEM(OB_APP_CONFIG_HDR_MERGE_IR_ENABLED))));
    rootFields.emplace_back(makeField(kHdrMerge, ExportValue::object(std::move(hdrFields))));

    return ExportValue::object(std::move(rootFields));
}

// schema_version 1
void applyApplicationConfigJsonV1(ApplicationConfig &config, IDevice *owner, const Json::Value &json) {
    requireRequiredObjects(json, kRootObjectFields, kAppConfigRoot);

    {
        const auto sensors = json[kSensors];
        for(const auto &name: sensors.getMemberNames()) {
            OBSensorType sensorType = OB_SENSOR_UNKNOWN;
            try {
                sensorType = utils::strToOBSensor(name);
            }
            catch(...) {
                LOG_WARN("Unknown {} sensor '{}'", kAppConfigRoot, name);
                continue;
            }

            auto        index      = config.getSensorIndex(sensorType, true);
            const auto &sensorJson = sensors[name];
            auto        context    = std::string("sensor ") + name;
            if(!sensorJson.isObject()) {
                THROW_INVALID_PARAM_EXCEPTION("Node type mismatch in " + std::string(kAppConfigRoot) + " sensors: " + name);
            }
            requireRequiredFields(sensorJson, kSensorFields, context);
            if(ob_is_video_sensor_type(sensorType)) {
                requireRequiredFields(sensorJson, kVideoSensorFields, context);
            }

            const auto &profileJson = sensorJson[kProfile];
            if(!profileJson.isObject()) {
                THROW_INVALID_PARAM_EXCEPTION("Node type mismatch in " + context + ": " + std::string(kProfile));
            }
            if(sensorType == OB_SENSOR_ACCEL || sensorType == OB_SENSOR_GYRO) {
                requireRequiredFields(profileJson, kImuProfileFields, context + " profile");
            }
            else if(ob_is_video_sensor_type(sensorType)) {
                requireRequiredFields(profileJson, kVideoProfileFields, context + " profile");
                if(profileJson.isMember(kDecimation)) {
                    if(!profileJson[kDecimation].isObject()) {
                        THROW_INVALID_PARAM_EXCEPTION("Node type mismatch in " + context + " profile: " + std::string(kDecimation));
                    }
                    requireRequiredFields(profileJson[kDecimation], kProfileDecimationFields, context + " profile decimation");
                }
            }

            config.setBool(OB_APP_CONFIG_ARRAY_ITEM(OB_APP_CONFIG_STREAM_ENABLED, index), sensorJson[kStreamEnabled].asBool());
            if(ob_is_video_sensor_type(sensorType)) {
                config.setBool(OB_APP_CONFIG_ARRAY_ITEM(OB_APP_CONFIG_UNDISTORTION_ENABLED, index), sensorJson[kUndistortionEnabled].asBool());
            }
            auto profile = findProfile(owner, sensorType, profileJson);
            if(!profile) {
                THROW_INVALID_PARAM_EXCEPTION("No matching stream profile for " + std::string(kAppConfigRoot) + " " + context);
            }
            config.setProfile(OB_APP_CONFIG_ARRAY_ITEM(OB_APP_CONFIG_STREAM_PROFILE, index), profile);
        }
    }

    if(json.isMember(kDeviceDecimation)) {
        if(!json[kDeviceDecimation].isObject()) {
            THROW_INVALID_PARAM_EXCEPTION("Node type mismatch in " + std::string(kAppConfigRoot) + ": " + std::string(kDeviceDecimation));
        }
        const auto &decimation = json[kDeviceDecimation];
        requireRequiredFields(decimation, kDeviceDecimationFields, kDeviceDecimation);
        const auto enabled = decimation[kEnableKey].asBool();
        config.setBool(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_DEVICE_DECIMATION_ENABLED), enabled);
        if(enabled) {
            requireAllFields(decimation, kDeviceDecimationFields, kDeviceDecimation);
            OBPresetResolutionConfig cfg{};
            cfg.width                 = static_cast<int16_t>(decimation[kOriginWidth].asInt());
            cfg.height                = static_cast<int16_t>(decimation[kOriginHeight].asInt());
            cfg.depthDecimationFactor = decimation[kDepthFactor].asInt();
            cfg.irDecimationFactor    = decimation[kIrFactor].asInt();
            config.setStruct(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_DEVICE_DECIMATION), &cfg, sizeof(cfg));
        }
    }

    {
        const auto &pc = json[kPointCloud];
        requireRequiredFields(pc, kPointCloudFields, kPointCloud);
        config.setBool(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_POINTCLOUD_ENABLED), pc[kEnableKey].asBool());
        config.setInt(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_POINTCLOUD_FORMAT), wireStringToPointCloudFormat(pc[kFormat].asString()));
        config.setInt(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_POINTCLOUD_DECIMATION_FACTOR), pc[kDecimationFactor].asInt());
        config.setInt(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_POINTCLOUD_ALIGN_MODE), utils::strToOBAlignMode(pc[kAlignMode].asString()));
        config.setBool(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_POINTCLOUD_FRAME_SYNC), pc[kFrameSync].asBool());
        config.setBool(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_POINTCLOUD_ALL_FRAME_TYPE_REQUIRED), pc[kAllFrameTypeRequired].asBool());
        config.setBool(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_POINTCLOUD_MATCH_TARGET_RESOLUTION), pc[kMatchTargetResolution].asBool());
    }

    {
        const auto &hdrMerge = json[kHdrMerge];
        requireRequiredFields(hdrMerge, kHdrMergeFields, kHdrMerge);
        config.setBool(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_HDR_MERGE_ENABLED), hdrMerge[kEnableKey].asBool());
        config.setBool(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_HDR_MERGE_IR_ENABLED), hdrMerge[kIrEnabled].asBool());
    }
}

void applyApplicationConfigJson(ApplicationConfig &config, IDevice *owner, const Json::Value &json) {
    requireRequiredFields(json, kRootLeafFields, kAppConfigRoot);

    const auto schemaVersion = json[kSchemaVersion].asUInt();
    switch(schemaVersion) {
    case 1:
        applyApplicationConfigJsonV1(config, owner, json);
        break;
    default:
        THROW_INVALID_PARAM_EXCEPTION("Unsupported " + std::string(kAppConfigRoot) + " " + std::string(kSchemaVersion) + ": " + std::to_string(schemaVersion));
    }
}

}  // namespace

const char *ApplicationConfigHandler::rootKeyName() {
    return kAppConfigRoot;
}

jsonmodel::ExportValue ApplicationConfigHandler::exportToValue(const ApplicationConfig &config) {
    return buildApplicationConfigValue(config);
}

void ApplicationConfigHandler::importFromJson(ApplicationConfig &config, IDevice *owner, const Json::Value &root, bool optional) {
    if(!root.isMember(kAppConfigRoot)) {
        if(optional) {
            return;
        }
        THROW_INVALID_PARAM_EXCEPTION("Missing required field: " + std::string(kAppConfigRoot));
    }
    if(!root[kAppConfigRoot].isObject()) {
        THROW_INVALID_PARAM_EXCEPTION("Node type mismatch: " + std::string(kAppConfigRoot));
    }
    applyApplicationConfigJson(config, owner, root[kAppConfigRoot]);
}

void ApplicationConfigHandler::appendToExportValue(jsonmodel::ExportValue &rootValue, IPresetManager &manager) {
    if(!manager.isApplicationConfigSupported()) {
        return;
    }
    auto config = manager.getApplicationConfig();
    if(!config) {
        return;
    }
    rootValue.objectFields.emplace_back(makeField(kAppConfigRoot, buildApplicationConfigValue(*config)));
}

void ApplicationConfigHandler::applyFromJsonRoot(IPresetManager &manager, IDevice *owner, const Json::Value &root) {
    if(!manager.isApplicationConfigSupported() || !root.isMember(kAppConfigRoot)) {
        return;
    }
    auto config = manager.getApplicationConfig();
    if(!config) {
        return;
    }
    ApplicationConfig working(*config);
    working.reset();
    importFromJson(working, owner, root);
    *config = working;
}

}  // namespace libobsensor
