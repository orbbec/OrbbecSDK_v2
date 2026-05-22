// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "libobsensor/h/ObTypes.h"
#include "utils/jsonmodel/Handler.hpp"
#include "IDevice.hpp"
#include "preset/CompareHandler.hpp"
#include "IFrameInterleaveManager.hpp"
#include <map>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

namespace libobsensor {

/**
 * @brief Handler for disparity to depth
 */
class CommentHandler : public jsonmodel::ILeafHandler {
public:
    CommentHandler(const std::string &comments) : comments_(comments) {};
    ~CommentHandler() override = default;

    /**
     * @brief Implementation of ILeafHandler::set
     */
    void set(const std::string &k, const Json::Value &v) override {
        utils::unusedVar(k);
        utils::unusedVar(v);
        return;
    }

    /**
     * @brief Implementation of ILeafHandler::exportValue
     */
    jsonmodel::ExportValue exportValue(const std::string &k) override {
        utils::unusedVar(k);
        return jsonmodel::makeScalar(comments_);
    }

private:
    std::string comments_;
};

/**
 * @brief Handler for frame interleave
 */
class FrameInterleaveHandler : public jsonmodel::IObjectHandler {
public:
    FrameInterleaveHandler(IDevice *owner);
    ~FrameInterleaveHandler() override = default;

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
    IDevice                                                         *owner_ = nullptr;
    bool                                                             enable_{ false };
    std::string                                                      currentModeName_;
    int                                                              currentIndex_{ 0 };
    std::vector<FrameInterleaveParam>                                params_;
    std::vector<std::pair<std::string, int FrameInterleaveParam::*>> paramsKeyList_;
    std::shared_ptr<IFrameInterleaveManager>                         frameInterleaveManager_;
};

/**
 * @brief Handler for ROI
 */
class RoiHandler : public jsonmodel::IObjectHandler {
public:
    RoiHandler(IDevice *owner, uint32_t propertyId, OBSensorType sensorType);
    ~RoiHandler() override;

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
     * @brief Called once before generating all child nodes during export
     *
     * @return A vector of additional child node keys to include
     */
    std::vector<std::string> onPreChildrenGet() override;

    /**
     * @brief Implementation of IObjectHandler::exportChildValue
     */
    jsonmodel::ExportValue exportChildValue(const std::string &k) override;

private:
    void stopSetting();
    void unregisterStreamCallback();
    void setRoiProperties();

private:
    IDevice                         *owner_      = nullptr;
    uint32_t                         propertyId_ = 0;
    OBSensorType                     sensorType_{ OB_SENSOR_UNKNOWN };
    AE_ROI                           roi_{};
    bool                             writeSupported_{ true };
    bool                             readSupported_{ true };
    std::map<std::string, int16_t *> valueMap_;
    std::atomic<bool>                needSetting_{ false };
    std::atomic<bool>                needUnregister_{ false };
    uint32_t                         callbackToken_{ 0 };
    std::thread                      settingThread_;
};

/**
 * @brief Handler for disparity to depth
 */
class D2DHandler : public jsonmodel::ILeafHandler {
public:
    D2DHandler(IDevice *owner);
    ~D2DHandler() override = default;

    /**
     * @brief Implementation of ILeafHandler::set
     */
    void set(const std::string &k, const Json::Value &v) override;

    /**
     * @brief Implementation of ILeafHandler::exportValue
     */
    jsonmodel::ExportValue exportValue(const std::string &k) override;

protected:
    enum class Mode {
        Disable,   // Disable
        Hardware,  // Hardware d2d
        Software,  // Software d2d
    };

    IDevice                    *owner_ = nullptr;
    std::map<std::string, Mode> valueMap_;
};

/**
 * @brief Handler for software noise removal filter
 */
class SoftwareNoiseRemovalFilterHandler : public jsonmodel::IObjectHandler {
public:
    SoftwareNoiseRemovalFilterHandler(IDevice *owner) : owner_(owner) {};
    ~SoftwareNoiseRemovalFilterHandler() override = default;

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
    IDevice *owner_ = nullptr;
    bool     enable_{ false };
    int32_t  minDiff_{ 0 };
    int32_t  maxSize_{ 0 };
};

/**
 * @brief Handler for hardware noise removal filter
 */
class HardwareNoiseRemovalFilterHandler : public jsonmodel::IObjectHandler {
public:
    HardwareNoiseRemovalFilterHandler(IDevice *owner) : owner_(owner) {};
    ~HardwareNoiseRemovalFilterHandler() override = default;

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
    IDevice *owner_ = nullptr;
    bool     enable_{ false };
    float    threshold_{ .0f };
};

/**
 * @brief Handler for disparity search
 */
class DisparitySearchHandler : public jsonmodel::IObjectHandler {
public:
    DisparitySearchHandler(IDevice *owner) : owner_(owner) {}
    ~DisparitySearchHandler() override;

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
    void stopSetting();
    void unregisterStreamCallback();
    void setDisparitySearchProperties();

private:
    IDevice           *owner_ = nullptr;
    int                rangeMode_{ 0 };
    int                searchOffset_{ 0 };
    std::map<int, int> rangeModeMapping_{ { 0, 64 }, { 1, 128 }, { 2, 256 } };
    bool               writeSupported_{ true };
    bool               readSupported_{ true };
    // for setting
    uint32_t          callbackToken_{ 0 };
    std::atomic<bool> needUnregister_{ false };
    std::atomic<bool> needSetting_{ false };
    std::thread       settingThread_;
};

/**
 * @brief Handler for the depth work mode configuration
 */
class DepthWorkModeHandler : public jsonmodel::ILeafHandler {
public:
    DepthWorkModeHandler(IDevice *owner) : owner_(owner) {}
    ~DepthWorkModeHandler() override = default;

    /**
     * @brief Implementation of ILeafHandler::set
     */
    void set(const std::string &k, const Json::Value &v) override;

    /**
     * @brief Implementation of ILeafHandler::exportValue
     */
    jsonmodel::ExportValue exportValue(const std::string &k) override;

private:
    IDevice *owner_ = nullptr;
};

/**
 * @brief Handler for device heartbeat state
 */
class HeartbeatHandler : public jsonmodel::ILeafHandler {
public:
    HeartbeatHandler(IDevice *owner, bool firmwareLog) : owner_(owner), firmwareLog_(firmwareLog) {}
    ~HeartbeatHandler() override = default;

    void                   set(const std::string &k, const Json::Value &v) override;
    jsonmodel::ExportValue exportValue(const std::string &k) override;

private:
    IDevice *owner_       = nullptr;
    bool     firmwareLog_ = false;
};

}  // namespace libobsensor
