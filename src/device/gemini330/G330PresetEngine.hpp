// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "preset/PresetEngine.hpp"
#include "utils/jsonmodel/Handler.hpp"

namespace libobsensor {

// Current preset version
constexpr const uint32_t kG330PresetVersion = 2;

/**
 * @brief G330PresetEngine version 1
 */
class G330PresetEngineV1 : public PresetEngineBase {
public:
    G330PresetEngineV1(IDevice *owner);
    ~G330PresetEngineV1() override = default;

    /**
     * @brief Implementation of IPresetEngine::init
     */
    void init() override;
};

/**
 * @brief G330PresetEngine version 2
 */
class G330PresetEngine : public PresetEngineBase {
public:
    G330PresetEngine(IDevice *owner);
    ~G330PresetEngine() override = default;

    /**
     * @brief Implementation of IPresetEngine::init
     */
    void init() override;
};

}  // namespace libobsensor
