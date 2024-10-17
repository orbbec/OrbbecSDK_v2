// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FemtoBoltAlgParamManager.hpp"

namespace libobsensor {

FemtoBoltAlgParamManager::FemtoBoltAlgParamManager(IDevice *owner) : TOFDeviceCommonAlgParamManager(owner) {
    fixD2CProfile();
}

void FemtoBoltAlgParamManager::fixD2CProfile() {
    for(auto item: d2cProfileList_) {
        if(item.alignType == ALIGN_D2C_SW) {
            fixedD2CProfile_.emplace_back(item);
        }
    }
}

const std::vector<OBD2CProfile> &FemtoBoltAlgParamManager::getD2CProfileList() const {
    return fixedD2CProfile_;
}

}  // namespace libobsensor

