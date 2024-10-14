// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "param/AlgParamManager.hpp"

namespace libobsensor {

class FemtoBoltAlgParamManager : public TOFDeviceCommandAlgParamManager {
public:
    FemtoBoltAlgParamManager(IDevice *owner);
    ~FemtoBoltAlgParamManager() = default;

    const std::vector<OBD2CProfile> &getD2CProfileList() const override;

private:
    void fixD2CProfile();

private:
    std::vector<OBD2CProfile> fixedD2CProfile_;
};

}  // namespace libobsensor
