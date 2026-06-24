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
ColorPresetHandler::ColorPresetHandler(IDevice *owner, bool supported) : owner_(owner), supported_(supported) {}

void ColorPresetHandler::set(const std::string &k, const Json::Value &v) {
    utils::unusedVar(k);

    if(!supported_) {
        // Firmware does not support color preset, ignore on import
        return;
    }

    if(!v.isString()) {
        LOG_ERROR("Invalid color preset value '{}'", v.toStyledString());
        return;
    }

    auto colorPresetManager = owner_->getComponentT<IColorPresetManager>(OB_DEV_COMPONENT_COLOR_PRESET_MANAGER);
    colorPresetManager->switchPreset(v.asString());
}

jsonmodel::ExportValue ColorPresetHandler::exportValue(const std::string &k) {
    utils::unusedVar(k);

    if(!supported_) {
        // Firmware does not support color preset, export as null
        return jsonmodel::ExportValue::nullValue();
    }

    auto colorPresetManager = owner_->getComponentT<IColorPresetManager>(OB_DEV_COMPONENT_COLOR_PRESET_MANAGER);
    return jsonmodel::makeScalar(colorPresetManager->getCurrentName());
}

}  // namespace libobsensor
