// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "param/AlgParamManager.hpp"

#include <vector>
#include <memory>

namespace libobsensor {

class LiDARAlgParamManager : public AlgParamManagerBase {
public:
    LiDARAlgParamManager(IDevice *owner);
    virtual ~LiDARAlgParamManager() noexcept override = default;

    void bindIntrinsic(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) override;

private:
    void fetchParamFromDevice() override;
    void registerBasicExtrinsics() override;
};

}  // namespace libobsensor
