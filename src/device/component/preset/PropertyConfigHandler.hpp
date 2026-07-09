// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "utils/jsonmodel/Handler.hpp"
#include "IDevice.hpp"
#include "logger/Logger.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"
#include <atomic>
#include <memory>
#include <thread>

namespace libobsensor {

/**
 * @brief Handler for bool/int/float property config
 */
template <typename T> class PropertyConfigHandler : public jsonmodel::ILeafHandler {
public:
    PropertyConfigHandler(IDevice *owner, uint32_t propertyId) : owner_(owner), propertyId_(propertyId) {};
    virtual ~PropertyConfigHandler() override = default;

    /**
     * @brief Implementation of ILeafHandler::set
     */
    void set(const std::string &k, const Json::Value &v) override {
        utils::unusedVar(k);

        T    value      = jsonmodel::JsonTraits<T>::from(v);
        auto propServer = owner_->getPropertyServer();
        if(propServer->isPropertySupported(propertyId_, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
            propServer->setPropertyValueT<T>(propertyId_, value);
        }
        else if(!owner_->isPlaybackDevice()) {
            // Real device: keep warning. Playback device: silently skip the non-writable property.
            LOG_WARN("Unsupported property '{}', skipping setting", propertyId_);
        }
    }

    /**
     * @brief Implementation of ILeafHandler::exportValue
     */
    jsonmodel::ExportValue exportValue(const std::string &k) override {
        utils::unusedVar(k);
        auto propServer = owner_->getPropertyServer();
        if(propServer->isPropertySupported(propertyId_, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
            T value = propServer->getPropertyValueT<T>(propertyId_);
            return jsonmodel::makeScalar(value);
        }
        else {
            LOG_WARN("Unsupported property '{}', skipping getting, return nullvalue", propertyId_);
            return jsonmodel::ExportValue::nullValue();
        }
    }

    IDevice *getOwner() {
        return owner_;
    }

protected:
    IDevice *owner_{ nullptr };  // owner
    uint32_t propertyId_{};      // property id
};

/**
 * @brief Property config handler that always writes twice: once immediately and once after the
 *        sensor starts streaming.
 *
 * Some properties (e.g. ae_max_exposure) are silently clamped or discarded by the firmware when
 * the stream is not running. Writing again once streaming begins ensures the preset value sticks.
 *
 * If the user reconfigures the same property before the stream starts, the deferred write is
 * cancelled so the user's value takes precedence. A new import (or handler destruction) also
 * cancels any pending deferred write.
 */
template <typename T> class DeferredPropertyConfigHandler : public PropertyConfigHandler<T> {
public:
    DeferredPropertyConfigHandler(IDevice *owner, uint32_t propertyId, OBSensorType sensorType)
        : PropertyConfigHandler<T>(owner, propertyId), sensorType_(sensorType) {}

    ~DeferredPropertyConfigHandler() override {
        stopSetting();
    }

    void onImportReset() override {
        stopSetting();
    }

    jsonmodel::ExportValue exportValue(const std::string &k) override {
        auto sensor = this->getOwner()->getSensor(sensorType_);
        if(sensor && !sensor->isStreamActivated()) {
            LOG_WARN("Exporting property '{}' while stream is not active; the value may differ from what will be applied after streaming starts",
                     this->propertyId_);
        }
        return PropertyConfigHandler<T>::exportValue(k);
    }

    void set(const std::string &k, const Json::Value &v) override {
        utils::unusedVar(k);
        stopSetting();

        auto owner      = this->getOwner();
        auto propServer = owner->getPropertyServer();
        if(!propServer->isPropertySupported(this->propertyId_, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
            if(!owner->isPlaybackDevice()) {
                LOG_WARN("Unsupported property '{}', skipping setting", this->propertyId_);
            }
            return;
        }

        desiredValue_ = jsonmodel::JsonTraits<T>::from(v);
        propServer->template setPropertyValueT<T>(this->propertyId_, desiredValue_);

        if(owner->isPlaybackDevice()) {
            return;
        }

        auto sensor = owner->getSensor(sensorType_);
        if(!sensor) {
            return;
        }

        // Always register a stream callback to write again once streaming starts, ensuring the
        // value takes effect even when the firmware ignores writes made before the stream is active.
        *needSetting_   = true;
        needUnregister_ = true;
        registerUserOverrideGuard();
        callbackToken_ = sensor->registerStreamStateChangedCallback([this](OBStreamState state, const std::shared_ptr<const StreamProfile> &profile) {
            utils::unusedVar(profile);
            if(state != STREAM_STATE_STREAMING) {
                return;
            }
            // Claim this deferred write exactly once: only the invocation that flips true->false starts
            // the thread, so a repeated STREAMING event never reassigns a still-joinable settingThread_.
            bool expected = true;
            if(!needSetting_->compare_exchange_strong(expected, false)) {
                return;
            }
            settingThread_ = std::thread([this]() {
                applyDeferredValue();
                unregisterStreamCallback();
            });
        });
    }

private:
    void applyDeferredValue() {
        TRY_EXECUTE({
            auto propServer = this->getOwner()->getPropertyServer();
            propServer->template setPropertyValueT<T>(this->propertyId_, desiredValue_);
        });
    }

    void stopSetting() {
        unregisterStreamCallback();
        unregisterUserOverrideGuard();
        if(settingThread_.joinable()) {
            settingThread_.join();
        }
    }

    void unregisterStreamCallback() {
        if(needUnregister_) {
            TRY_EXECUTE({
                auto sensor = this->getOwner()->getSensor(sensorType_);
                sensor->unregisterStreamStateChangedCallback(callbackToken_);
                needUnregister_ = false;
            });
        }
    }

    void registerUserOverrideGuard() {
        if(overrideGuardToken_ != 0) {
            return;
        }
        auto propServer = this->getOwner()->getPropertyServer();
        // Capture the flag by shared_ptr rather than this: an access callback may still run from a
        // snapshot taken before unregister, when the handler could already have been destroyed.
        auto needSetting    = needSetting_;
        overrideGuardToken_ = propServer->registerAccessCallback(this->propertyId_, [needSetting](uint32_t, const uint8_t *, size_t, PropertyOperationType op) {
            if(op == PROP_OP_WRITE) {
                needSetting->store(false);
            }
        });
    }

    void unregisterUserOverrideGuard() {
        if(overrideGuardToken_ != 0) {
            TRY_EXECUTE({
                auto propServer = this->getOwner()->getPropertyServer();
                propServer->unregisterAccessCallback(overrideGuardToken_);
            });
            overrideGuardToken_ = 0;
        }
    }

private:
    OBSensorType                       sensorType_{ OB_SENSOR_UNKNOWN };
    T                                  desiredValue_{};
    std::shared_ptr<std::atomic<bool>> needSetting_ = std::make_shared<std::atomic<bool>>(false);
    std::atomic<bool>                  needUnregister_{ false };
    uint32_t                           callbackToken_{ 0 };
    uint64_t                           overrideGuardToken_{ 0 };
    std::thread                        settingThread_;
};

/**
 * @brief Handler for color power line frequency mapping.
 */
class ColorPowerLineFrequencyHandler : public PropertyConfigHandler<int> {
public:
    explicit ColorPowerLineFrequencyHandler(IDevice *owner);
    ~ColorPowerLineFrequencyHandler() override = default;

    void                   set(const std::string &k, const Json::Value &v) override;
    jsonmodel::ExportValue exportValue(const std::string &k) override;

private:
    std::map<int, std::string> valueMapping_;
};

/**
 * @brief Handler for color preset mapping.
 */
class ColorPresetHandler : public jsonmodel::ILeafHandler {
public:
    ColorPresetHandler(IDevice *owner, bool supported);
    ~ColorPresetHandler() override = default;

    void                   set(const std::string &k, const Json::Value &v) override;
    jsonmodel::ExportValue exportValue(const std::string &k) override;

private:
    IDevice *owner_{ nullptr };  // owner
    bool     supported_{ false };
};

}  // namespace libobsensor
