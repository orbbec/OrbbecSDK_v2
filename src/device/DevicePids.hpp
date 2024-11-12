// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <vector>

namespace libobsensor {
const std::vector<uint16_t> BootDevPids = {
    0x0501,  // bootloader
    0x0300,  // openni bootloader
    0x0301,  // openni bootloader
    0x0400,  // openni Mx400/Mx600 bootloader
};

const std::vector<uint16_t> Astra2DevPids = {
    0x0660,  // astra2
};

const std::vector<uint16_t> G330DevPids = {
    0x06d0,  // Gemini 2R
    0x06d1,  // Gemini 2RL
    0x0800,  // Gemini 335 / 335e
    0x0801,  // Gemini 330
    0x0802,  // Gemini dm330
    0x0803,  // Gemini 336 / 336e
    0x0804,  // Gemini 335L
    0x0805,  // Gemini 330L
    0x0806,  // Gemini dm330L
    0x0807,  // Gemini 336L
    0x080B,  // Gemini 335Lg
    0x080C,  // Gemini 330Lg
    0x080D,  // Gemini 336Lg
    0x080E,  // Gemini 335Le
    0x080F,  // Gemini 330Le
    0x0810,  // Gemini 336Le

    // DaBai A and daBai AL are new product line base on Gemini 330 series
    0x0A12,  // DaBai A
    0x0A13,  // DaBai AL
};

const uint16_t OB_DEVICE_G336_PID   = 0x0803;
const uint16_t OB_DEVICE_G336L_PID  = 0x0807;
const uint16_t OB_DEVICE_G335LE_PID = 0x080E;

const std::vector<uint16_t> G330LDevPids = {
    0x06d1,  // Gemini 2RL
    0x0804,  // Gemini 335L
    0x0805,  // Gemini 330L
    0x0806,  // Gemini dm330L
    0x0807,  // Gemini 336L
    0x080B,  // Gemini 335Lg
    0x080C,  // Gemini 330Lg
    0x080D,  // Gemini 336Lg
    0x080E,  // Gemini 335Le
    0x080F,  // Gemini 330Le
    0x0810,  // Gemini 336Le
};

const std::vector<uint16_t> DaBaiDevPids = {
    0x0A12,  // DaBai A
    0x0A13,  // DaBai AL
};

const std::vector<uint16_t> Gemini2DevPids = {
    0x0670,  // Gemini2
    0x0673,  // Gemini2L
    // 0x0671,  // Gemini2XL // remove g2xl support temporarily as it is currently not fully supported
};

const std::vector<uint16_t> FemtoMegaDevPids = {
    0x0669,  // Femto Mega
    0x06c0,  // Femto Mega i
};
#define OB_FEMTO_MEGA_PID 0x0669

const std::vector<uint16_t> FemtoBoltDevPids = {
    0x066B,  // Femto Bolt
};
}  // namespace libobsensor
