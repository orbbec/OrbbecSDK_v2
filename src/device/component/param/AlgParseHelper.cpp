// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "AlgParseHelper.hpp"
namespace libobsensor {
std::vector<OBCameraParam_Internal_V0> AlgParseHelper::alignCalibParamParse(uint8_t *data, uint32_t size) {
    std::vector<OBCameraParam_Internal_V0> output;
    for(int i = 0; i < static_cast<int>(size / D2C_PARAMS_ITEM_SIZE); i++) {
        output.push_back(*(OBCameraParam_Internal_V0 *)(data + i * D2C_PARAMS_ITEM_SIZE));
    }
    return output;
}

std::vector<OBD2CProfile> AlgParseHelper::d2cProfileInfoParse(uint8_t *data, uint32_t size) {
    std::vector<OBD2CProfile> output;
    for(int i = 0; i < static_cast<int>(size / sizeof(OBD2CProfile)); i++) {
        output.push_back(*(OBD2CProfile *)(data + i * sizeof(OBD2CProfile)));
    }
    return output;
}
}  // namespace libobsensor
