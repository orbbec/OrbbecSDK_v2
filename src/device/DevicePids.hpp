#pragma once
#include <vector>

namespace libobsensor {
const std::vector<uint16_t> gBootPids = {
    0x0501,  // winUSB bootloader
    0x0300,  // openni bootloader
    0x0301,  // openni bootloader
    0x0400,  // openni Mx400/Mx600 bootloader
};

const std::vector<uint16_t> gAstra2DevPids = {
    0x0660,  // astra2
};

const std::vector<uint16_t> gG330Pids = {
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
};

const std::vector<uint16_t> gG330LPids = {
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

const std::vector<uint16_t> gMX6600DevPids = {
    0x0660,  // astra2
};

const std::vector<uint16_t> gGemini2Pids = {
    0x0670,  // Gemini2
    0x0701,  // Dabai DCL
    0x0674,  // Gemini2i
};

const std::vector<uint16_t> gGemini2LPids = {
    0x0673  // Gemini2L
};

const std::vector<uint16_t> gGemini2VLPids = {
    0x0675  // Gemini2VL
};

const std::vector<uint16_t> gGemini2XLPids = {
    0x0671  // Gemini2XL
};

const std::vector<uint16_t> gFemtoMegaPids = {
    0x0669,  // Femto Mega
    0x06c0,  // Femto Mega i
};

const std::vector<uint16_t> gFemtoBoltPids = {
    0x066B,  // Femto Bolt
};

const std::vector<uint16_t> gFrameSyncAbleDevPids = {
    0x0404,  // Astra Mini
    0x0407,  // Astra Mini S
    0x065b,  // Astra Mini Pro
    0x065e,  // Astra mini S Pro
    0x0635,  // Femto
    0x0638,  // Femto W
    0x0668,  // Femto Live
    0x0660,  // Astra2
    0x0669,  // FemtoMega
    0x0670,  // Gemini2
    0x066B,  // Femto Bolt
    0x069C,  // Gemini scan
    0x0673,  // Gemini2L
    0x0671,  // Gemini2XL
    0x0675,  // Gemini2VL
    0x0701,  // Dabai DCL
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
    0x080D,  // Gemini 336Lg
    0x080E,  // Gemini 335Le
    0x0810,  // Gemini 336Le
    0x0674,  // Gemini2i
    0x06C0,  // Femto Mega i
    0x0808,  // Gemini 205
    0x0809,  // Gemini 210
};
}