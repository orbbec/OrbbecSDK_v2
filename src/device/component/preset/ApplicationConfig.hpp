// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IStreamProfile.hpp"
#include "libobsensor/h/ApplicationConfig.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace libobsensor {
class IDevice;
class StreamProfile;

struct ApplicationConfigValue {
    OBAppConfigValueType type       = OB_APP_CONFIG_VALUE_BOOL;
    bool                 boolValue  = false;
    int32_t              intValue   = 0;
    float                floatValue = 0.0f;
    std::string          stringValue;
    std::vector<uint8_t> structValue;
};

class ApplicationConfig {
public:
    explicit ApplicationConfig(IDevice *owner);
    ApplicationConfig(const ApplicationConfig &other);
    ApplicationConfig &operator=(const ApplicationConfig &other);
    ~ApplicationConfig() = default;

    IDevice *owner() const;

    void reset();

    void setBool(OBAppConfigItem item, bool value);
    bool getBool(OBAppConfigItem item) const;

    void    setInt(OBAppConfigItem item, int32_t value);
    int32_t getInt(OBAppConfigItem item) const;

    void  setFloat(OBAppConfigItem item, float value);
    float getFloat(OBAppConfigItem item) const;

    void        setString(OBAppConfigItem item, const char *value);
    const char *getString(OBAppConfigItem item) const;

    void setStruct(OBAppConfigItem item, const void *value, uint32_t valueSize);
    void getStruct(OBAppConfigItem item, void *value, uint32_t valueSize) const;

    void                                 setProfile(OBAppConfigItem item, std::shared_ptr<const StreamProfile> profile);
    std::shared_ptr<const StreamProfile> getProfile(OBAppConfigItem item) const;

    uint32_t getCount(OBAppConfigKey key) const;
    uint32_t getSensorIndex(OBSensorType sensorType, bool createIfMissing);

private:
    void         resetDefaultValues();
    void         validateItem(OBAppConfigItem item, OBAppConfigValueType type) const;
    void         validateProfileItem(OBAppConfigItem item) const;
    OBSensorType sensorTypeAt(uint32_t index) const;

private:
    IDevice *owner_ = nullptr;

    std::map<OBAppConfigItem, ApplicationConfigValue>            values_;
    std::map<OBSensorType, std::shared_ptr<const StreamProfile>> profiles_;
    std::vector<OBSensorType>                                    sensorOrder_;
};

}  // namespace libobsensor

#ifdef __cplusplus
extern "C" {
#endif

struct ob_application_config_t {
    std::shared_ptr<libobsensor::ApplicationConfig> config;
};

#ifdef __cplusplus
}
#endif
