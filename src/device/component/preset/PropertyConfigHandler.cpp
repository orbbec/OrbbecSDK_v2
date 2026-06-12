// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "PropertyConfigHandler.hpp"

namespace libobsensor {

// ColorPowerLineFrequencyHandler
ColorPowerLineFrequencyHandler::ColorPowerLineFrequencyHandler(IDevice *owner)
    : PropertyConfigHandler<int>(owner, OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT),
      valueMapping_{ { 0, "Disabled" }, { 1, "50Hz" }, { 2, "60Hz" }, { 3, "Auto" } } {}

void ColorPowerLineFrequencyHandler::set(const std::string &k, const Json::Value &v) {
    if(!v.isString()) {
        PropertyConfigHandler<int>::set(k, v);
        return;
    }

    const auto stringValue = v.asString();
    for(const auto &entry: valueMapping_) {
        if(entry.second == stringValue) {
            PropertyConfigHandler<int>::set(k, Json::Value(entry.first));
            return;
        }
    }
    // try to set as int
    try {
        TRY_EXECUTE({ PropertyConfigHandler<int>::set(k, v); });
    }
    catch(...) {
        LOG_ERROR("Invalid color power line frequency value '{}'", stringValue);
    }
}

jsonmodel::ExportValue ColorPowerLineFrequencyHandler::exportValue(const std::string &k) {
    auto value = PropertyConfigHandler<int>::exportValue(k);
    if(value.isNull()) {
        return value;
    }

    const int intValue = jsonmodel::JsonTraits<int>::from(value.scalarValue);
    auto      it       = valueMapping_.find(intValue);
    return it != valueMapping_.end() ? jsonmodel::makeScalar(it->second) : value;
}

// ColorPresetHandler
ColorPresetHandler::ColorPresetHandler(IDevice *owner) : PropertyConfigHandler<int>(owner, OB_PROP_COLOR_PRESET_PRIORITY_INT) {}

void ColorPresetHandler::set(const std::string &k, const Json::Value &v) {
#if 1
    // TODO: Temporarily disabled, will be enabled after device issue is resolved
    utils::unusedVar(k);
    utils::unusedVar(v);
    return;
#else
    // try to set as int
    try {
        TRY_EXECUTE({ PropertyConfigHandler<int>::set(k, v); });
    }
    catch(...) {
        LOG_ERROR("Invalid color preset value '{}'", v.asString(););
    }
#endif
}

jsonmodel::ExportValue ColorPresetHandler::exportValue(const std::string &k) {
#if 1
    // TODO: Temporarily disabled, will be enabled after device issue is resolved
    utils::unusedVar(k);
    return jsonmodel::ExportValue::nullValue();
#else
    return PropertyConfigHandler<int>::exportValue(k);
#endif
}

}  // namespace libobsensor
