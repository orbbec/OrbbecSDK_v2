// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <cstdint>
#include <map>
#include <string>

namespace libobsensor {

// Canonical color preset value->name mappings, shared by live devices and the playback device so that
// the same ColorPresetManager component (and therefore the same preset names) is used in both cases.

inline const std::map<int32_t, std::string> &getG330ColorPresetMap() {
    static const std::map<int32_t, std::string> map = { { 0, "Default" }, { 1, "Warm Biased AWB" } };
    return map;
}

inline const std::map<int32_t, std::string> &getG305ColorPresetMap() {
    static const std::map<int32_t, std::string> map = { { 0, "Default" }, { 1, "Warm Biased AWB" }, { 2, "Cold Biased AWB" } };
    return map;
}

}  // namespace libobsensor
