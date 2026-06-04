// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "ApplicationConfig.hpp"

#include "IDevice.hpp"
#include "IProperty.hpp"
#include "ISensor.hpp"
#include "exception/ObException.hpp"
#include "libobsensor/h/Property.h"
#include "stream/StreamProfile.hpp"
#include "common/DeviceSeriesInfo.hpp"

#include <algorithm>
#include <cstring>
#include <iterator>

namespace libobsensor {
namespace {

enum class KeyScope {
    Default,
    Sensor,
};

struct KeyInfo {
    OBAppConfigKey       key;
    KeyScope             itemScope;
    OBAppConfigValueType type;
    uint32_t             structSize;
};

const KeyInfo *findKeyInfo(OBAppConfigKey key) {
    static const KeyInfo kInfos[] = {
        { OB_APP_CONFIG_STREAM_ENABLED, KeyScope::Sensor, OB_APP_CONFIG_VALUE_BOOL, 0 },
        { OB_APP_CONFIG_UNDISTORTION_ENABLED, KeyScope::Sensor, OB_APP_CONFIG_VALUE_BOOL, 0 },
        { OB_APP_CONFIG_SENSOR_TYPE, KeyScope::Sensor, OB_APP_CONFIG_VALUE_INT, 0 },
        { OB_APP_CONFIG_DEVICE_DECIMATION, KeyScope::Default, OB_APP_CONFIG_VALUE_STRUCT, sizeof(OBPresetResolutionConfig) },
        { OB_APP_CONFIG_DEVICE_DECIMATION_ENABLED, KeyScope::Default, OB_APP_CONFIG_VALUE_BOOL, 0 },
        { OB_APP_CONFIG_POINTCLOUD_ENABLED, KeyScope::Default, OB_APP_CONFIG_VALUE_BOOL, 0 },
        { OB_APP_CONFIG_POINTCLOUD_FORMAT, KeyScope::Default, OB_APP_CONFIG_VALUE_INT, 0 },
        { OB_APP_CONFIG_POINTCLOUD_DECIMATION_FACTOR, KeyScope::Default, OB_APP_CONFIG_VALUE_INT, 0 },
        { OB_APP_CONFIG_POINTCLOUD_ALIGN_MODE, KeyScope::Default, OB_APP_CONFIG_VALUE_INT, 0 },
        { OB_APP_CONFIG_POINTCLOUD_FRAME_SYNC, KeyScope::Default, OB_APP_CONFIG_VALUE_BOOL, 0 },
        { OB_APP_CONFIG_POINTCLOUD_ALL_FRAME_TYPE_REQUIRED, KeyScope::Default, OB_APP_CONFIG_VALUE_BOOL, 0 },
        { OB_APP_CONFIG_POINTCLOUD_MATCH_TARGET_RESOLUTION, KeyScope::Default, OB_APP_CONFIG_VALUE_BOOL, 0 },
        { OB_APP_CONFIG_HDR_MERGE_ENABLED, KeyScope::Default, OB_APP_CONFIG_VALUE_BOOL, 0 },
        { OB_APP_CONFIG_HDR_MERGE_IR_ENABLED, KeyScope::Default, OB_APP_CONFIG_VALUE_BOOL, 0 },
    };
    for(const auto &info: kInfos) {
        if(info.key == key) {
            return &info;
        }
    }
    return nullptr;
}

}  // namespace

ApplicationConfig::ApplicationConfig(IDevice *owner) : owner_(owner) {
    resetDefaultValues();
}

ApplicationConfig::ApplicationConfig(const ApplicationConfig &other)
    : owner_(other.owner_), values_(other.values_), profiles_(other.profiles_), sensorOrder_(other.sensorOrder_) {}

ApplicationConfig &ApplicationConfig::operator=(const ApplicationConfig &other) {
    if(this == &other) {
        return *this;
    }

    owner_       = other.owner_;
    values_      = other.values_;
    profiles_    = other.profiles_;
    sensorOrder_ = other.sensorOrder_;
    return *this;
}

IDevice *ApplicationConfig::owner() const {
    return owner_;
}

void ApplicationConfig::reset() {
    resetDefaultValues();
}

void ApplicationConfig::setBool(OBAppConfigItem item, bool value) {
    validateItem(item, OB_APP_CONFIG_VALUE_BOOL);

    ApplicationConfigValue v;
    v.type        = OB_APP_CONFIG_VALUE_BOOL;
    v.boolValue   = value;
    values_[item] = v;
}

bool ApplicationConfig::getBool(OBAppConfigItem item) const {
    validateItem(item, OB_APP_CONFIG_VALUE_BOOL);
    auto iter = values_.find(item);
    if(iter == values_.end()) {
        THROW_INVALID_PARAM_EXCEPTION("Application config key not found");
    }
    return iter->second.boolValue;
}

void ApplicationConfig::setInt(OBAppConfigItem item, int32_t value) {
    validateItem(item, OB_APP_CONFIG_VALUE_INT);

    ApplicationConfigValue v;
    v.type        = OB_APP_CONFIG_VALUE_INT;
    v.intValue    = value;
    values_[item] = v;
}

int32_t ApplicationConfig::getInt(OBAppConfigItem item) const {
    validateItem(item, OB_APP_CONFIG_VALUE_INT);
    auto iter = values_.find(item);
    if(iter == values_.end()) {
        THROW_INVALID_PARAM_EXCEPTION("Application config key not found");
    }
    return iter->second.intValue;
}

void ApplicationConfig::setFloat(OBAppConfigItem item, float value) {
    validateItem(item, OB_APP_CONFIG_VALUE_FLOAT);

    ApplicationConfigValue v;
    v.type        = OB_APP_CONFIG_VALUE_FLOAT;
    v.floatValue  = value;
    values_[item] = v;
}

float ApplicationConfig::getFloat(OBAppConfigItem item) const {
    validateItem(item, OB_APP_CONFIG_VALUE_FLOAT);
    auto iter = values_.find(item);
    if(iter == values_.end()) {
        THROW_INVALID_PARAM_EXCEPTION("Application config key not found");
    }
    return iter->second.floatValue;
}

void ApplicationConfig::setString(OBAppConfigItem item, const char *value) {
    validateItem(item, OB_APP_CONFIG_VALUE_STRING);

    ApplicationConfigValue v;
    v.type        = OB_APP_CONFIG_VALUE_STRING;
    v.stringValue = value;
    values_[item] = v;
}

const char *ApplicationConfig::getString(OBAppConfigItem item) const {
    validateItem(item, OB_APP_CONFIG_VALUE_STRING);
    auto iter = values_.find(item);
    if(iter == values_.end()) {
        THROW_INVALID_PARAM_EXCEPTION("Application config key not found");
    }
    return iter->second.stringValue.c_str();
}

void ApplicationConfig::setStruct(OBAppConfigItem item, const void *value, uint32_t valueSize) {
    validateItem(item, OB_APP_CONFIG_VALUE_STRUCT);

    auto key  = OB_APP_CONFIG_ITEM_KEY(item);
    auto info = findKeyInfo(key);
    if(valueSize != info->structSize) {
        THROW_INVALID_PARAM_EXCEPTION("Application config struct size mismatch");
    }

    ApplicationConfigValue v;
    v.type = OB_APP_CONFIG_VALUE_STRUCT;
    v.structValue.resize(valueSize);
    std::memcpy(v.structValue.data(), value, valueSize);
    values_[item] = v;
}

void ApplicationConfig::getStruct(OBAppConfigItem item, void *value, uint32_t valueSize) const {
    validateItem(item, OB_APP_CONFIG_VALUE_STRUCT);

    auto iter = values_.find(item);
    if(iter == values_.end()) {
        THROW_INVALID_PARAM_EXCEPTION("Application config key not found");
    }
    if(iter->second.structValue.size() != valueSize) {
        THROW_INVALID_PARAM_EXCEPTION("Application config struct size mismatch");
    }
    std::memcpy(value, iter->second.structValue.data(), valueSize);
}

void ApplicationConfig::setProfile(OBAppConfigItem item, std::shared_ptr<const StreamProfile> profile) {
    validateProfileItem(item);
    profiles_[sensorTypeAt(OB_APP_CONFIG_ITEM_INDEX(item))] = profile;
}

std::shared_ptr<const StreamProfile> ApplicationConfig::getProfile(OBAppConfigItem item) const {
    validateProfileItem(item);

    auto iter = profiles_.find(sensorTypeAt(OB_APP_CONFIG_ITEM_INDEX(item)));
    if(iter == profiles_.end()) {
        return nullptr;
    }

    return iter->second;
}

uint32_t ApplicationConfig::getCount(OBAppConfigKey key) const {
    if(key != OB_APP_CONFIG_SENSORS) {
        THROW_INVALID_PARAM_EXCEPTION("Application config count only supports array keys");
    }
    return static_cast<uint32_t>(sensorOrder_.size());
}

uint32_t ApplicationConfig::getSensorIndex(OBSensorType sensorType, bool createIfMissing) {
    auto iter = std::find(sensorOrder_.begin(), sensorOrder_.end(), sensorType);
    if(iter != sensorOrder_.end()) {
        return static_cast<uint32_t>(std::distance(sensorOrder_.begin(), iter));
    }
    if(!createIfMissing) {
        THROW_INVALID_PARAM_EXCEPTION("Application config sensor item not found");
    }

    if(sensorType >= OB_SENSOR_TYPE_COUNT) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid application config sensor type");
    }

    sensorOrder_.emplace_back(sensorType);
    auto index = static_cast<uint32_t>(sensorOrder_.size() - 1);
    setInt(OB_APP_CONFIG_ARRAY_ITEM(OB_APP_CONFIG_SENSOR_TYPE, index), static_cast<int32_t>(sensorType));
    setBool(OB_APP_CONFIG_ARRAY_ITEM(OB_APP_CONFIG_STREAM_ENABLED, index), false);
    setBool(OB_APP_CONFIG_ARRAY_ITEM(OB_APP_CONFIG_UNDISTORTION_ENABLED, index), false);
    return index;
}

void ApplicationConfig::resetDefaultValues() {
    values_.clear();
    profiles_.clear();
    sensorOrder_.clear();

    // Sensors
    auto sensorTypeList = owner_->getSensorTypeList();
    for(auto sensorType: sensorTypeList) {
        auto index = getSensorIndex(sensorType, true);

        auto sensor  = owner_->getSensor(sensorType);
        auto profile = sensor->getActivatedStreamProfile();
        bool enable  = false;
        if(profile) {
            enable = true;
        }
        else {
            auto profileList = sensor->getStreamProfileList();
            profile          = profileList.empty() ? nullptr : profileList.front();
        }

        setBool(OB_APP_CONFIG_ARRAY_ITEM(OB_APP_CONFIG_STREAM_ENABLED, index), enable);
        setProfile(OB_APP_CONFIG_ARRAY_ITEM(OB_APP_CONFIG_STREAM_PROFILE, index), profile);
        setBool(OB_APP_CONFIG_ARRAY_ITEM(OB_APP_CONFIG_UNDISTORTION_ENABLED, index), false);
    }

    // Device decimation
    setBool(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_DEVICE_DECIMATION_ENABLED), false);
    auto devInfo = owner_->getInfo();
    if(isDeviceInContainer(G435LeDevPids, devInfo->vid_, devInfo->pid_)) {
        auto propServer = owner_->getPropertyServer();
        if(propServer && propServer->isPropertySupported(OB_STRUCT_PRESET_RESOLUTION_CONFIG, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
            auto deviceDecimation = propServer->getStructureDataT<OBPresetResolutionConfig>(OB_STRUCT_PRESET_RESOLUTION_CONFIG);
            setBool(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_DEVICE_DECIMATION_ENABLED), true);
            setStruct(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_DEVICE_DECIMATION), &deviceDecimation, sizeof(deviceDecimation));
        }
    }

    // Point Cloud
    setBool(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_POINTCLOUD_ENABLED), false);
    setInt(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_POINTCLOUD_FORMAT), static_cast<int32_t>(OB_FORMAT_POINT));
    setInt(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_POINTCLOUD_DECIMATION_FACTOR), 1);
    setInt(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_POINTCLOUD_ALIGN_MODE), static_cast<int32_t>(ALIGN_D2C_SW_MODE));
    setBool(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_POINTCLOUD_FRAME_SYNC), false);
    setBool(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_POINTCLOUD_ALL_FRAME_TYPE_REQUIRED), false);
    setBool(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_POINTCLOUD_MATCH_TARGET_RESOLUTION), true);

    // HDR merge
    setBool(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_HDR_MERGE_ENABLED), false);
    setBool(OB_APP_CONFIG_ITEM(OB_APP_CONFIG_HDR_MERGE_IR_ENABLED), true);
}

void ApplicationConfig::validateItem(OBAppConfigItem item, OBAppConfigValueType type) const {
    auto key = OB_APP_CONFIG_ITEM_KEY(item);
    if(key == OB_APP_CONFIG_STREAM_PROFILE) {
        THROW_INVALID_PARAM_EXCEPTION("Application config profile key requires profile API");
    }

    auto info = findKeyInfo(key);
    if(!info) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid application config key");
    }
    if(info->type != type) {
        THROW_INVALID_PARAM_EXCEPTION("Application config key value type mismatch");
    }
    if(info->itemScope == KeyScope::Default && OB_APP_CONFIG_ITEM_INDEX(item) != OB_APP_CONFIG_INDEX_NONE) {
        THROW_INVALID_PARAM_EXCEPTION("Application config global item index must be 0");
    }
    if(info->itemScope == KeyScope::Sensor) {
        sensorTypeAt(OB_APP_CONFIG_ITEM_INDEX(item));
    }
}

void ApplicationConfig::validateProfileItem(OBAppConfigItem item) const {
    if(OB_APP_CONFIG_ITEM_KEY(item) != OB_APP_CONFIG_STREAM_PROFILE) {
        THROW_INVALID_PARAM_EXCEPTION("Application config profile item key mismatch");
    }
    sensorTypeAt(OB_APP_CONFIG_ITEM_INDEX(item));
}

OBSensorType ApplicationConfig::sensorTypeAt(uint32_t index) const {
    if(index >= sensorOrder_.size()) {
        THROW_INVALID_PARAM_EXCEPTION("Application config sensor item index out of range");
    }
    return sensorOrder_[index];
}

}  // namespace libobsensor
