// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "libobsensor/h/ObTypes.h"
#include "utils/jsonmodel/Handler.hpp"
#include "IDevice.hpp"
#include <map>
#include <vector>
#include <string>

namespace libobsensor {

/**
 * @brief Handler for video stream profiles
 */
class StreamProfileHandler : public jsonmodel::IObjectHandler {
public:
    StreamProfileHandler(IDevice *owner, OBSensorType sensorType);
    ~StreamProfileHandler() override = default;

    /**
     * @brief Implementation of IObjectHandler::onPreChildrenSet
     */
    bool onPreChildrenSet(const Json::Value &value) override;

    /**
     * @brief Implementation of IObjectHandler::onSetChild
     */
    void onSetChild(const std::string &k, const Json::Value &v, const Json::Value &parent) override;

    /**
     * @brief Implementation of IObjectHandler::onPostChildrenSet
     */
    void onPostChildrenSet() override;

    /**
     * @brief Implementation of IObjectHandler::onPreChildrenGet
     */
    std::vector<std::string> onPreChildrenGet() override;

    /**
     * @brief Implementation of IObjectHandler::exportChildValue
     */
    jsonmodel::ExportValue exportChildValue(const std::string &k) override;

private:
    IDevice     *owner_{ nullptr };
    OBSensorType sensorType_{ OB_SENSOR_UNKNOWN };
    OBFormat     format_{ OB_FORMAT_UNKNOWN };
    uint32_t     fps_{ 0 };
    uint32_t     width_{ 0 };
    uint32_t     height_{ 0 };
};

}  // namespace libobsensor
