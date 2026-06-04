// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "utils/jsonmodel/Handler.hpp"
#include <json/json.h>

namespace libobsensor {
class ApplicationConfig;
class IDevice;
class IPresetManager;

class ApplicationConfigHandler {
public:
    static const char            *rootKeyName();
    static jsonmodel::ExportValue exportToValue(const ApplicationConfig &config);
    static void                   importFromJson(ApplicationConfig &config, IDevice *owner, const Json::Value &root, bool optional = true);

    // Append application_config to rootValue if manager supports application_config; no-op otherwise.
    static void appendToExportValue(jsonmodel::ExportValue &rootValue, IPresetManager &manager);

    // Apply application_config from root if manager supports it and root contains the node; no-op otherwise.
    static void applyFromJsonRoot(IPresetManager &manager, IDevice *owner, const Json::Value &root);
};

}  // namespace libobsensor
