// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "utils/jsonmodel/ConfigEngine.hpp"
#include "IDevice.hpp"
#include "exception/ObException.hpp"

#include <atomic>

namespace libobsensor {

/**
 * @brief Abstract preset engine interface
 */
class IPresetEngine {
public:
    virtual ~IPresetEngine() = default;

    /**
     * @brief Initialize the preset engine
     */
    virtual void init() = 0;

    /**
     * @brief Imports data from a JSON object
     * @param root The source JSON object
     * @throws libobsensor::invalid_value_exception If a required path is missing in the JSON
     */
    virtual void importJson(const Json::Value &root) = 0;

    /**
     * @brief Exports the current state of all handlers into a JSON object
     * @return The resulting JSON object
     */
    virtual Json::Value exportJson() = 0;

    /**
     * @brief Exports the current state of all handlers into ordered JSON text.
     * @param options Formatting options for ordered export
     * @return The resulting JSON string
     */
    virtual std::string exportJson(const jsonmodel::OrderedExportOptions &options) = 0;
};

/**
 * @brief Implementation of base preset engine
 */
class PresetEngineBase : public IPresetEngine {
public:
    PresetEngineBase(IDevice *owner) : owner_(owner) {};
    virtual ~PresetEngineBase() override = default;

    IDevice *getOwner() {
        return owner_;
    }

    /**
     * @brief Implementation of IPresetEngine::importJson
     */
    void importJson(const Json::Value &root) override {
        if(!initialized_) {
            THROW_WRONG_API_CALL_SEQUENCE_EXCEPTION("ConfigEngine has not been initialized");
        }
        configEngine_.importAll(root);
    };

    /**
     * @brief Implementation of IPresetEngine::exportJson
     */
    Json::Value exportJson() override {
        if(!initialized_) {
            THROW_WRONG_API_CALL_SEQUENCE_EXCEPTION("ConfigEngine has not been initialized");
        }
        return configEngine_.exportAll();
    }

    /**
     * @brief Implementation of IPresetEngine::exportJson for ordered text export.
     * @param options Formatting options for ordered export
     * @return The resulting JSON string
     */
    std::string exportJson(const jsonmodel::OrderedExportOptions &options) override {
        if(!initialized_) {
            THROW_WRONG_API_CALL_SEQUENCE_EXCEPTION("ConfigEngine has not been initialized");
        }
        return configEngine_.exportAll(options);
    }

protected:
    IDevice                *owner_;
    jsonmodel::ConfigEngine configEngine_;
    std::atomic<bool>       initialized_{ false };
};

}  // namespace libobsensor
