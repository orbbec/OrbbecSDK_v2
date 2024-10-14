// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IProperty.hpp"
#include "ISourcePort.hpp"

namespace libobsensor {

class UvcPropertyAccessor : public IBasicPropertyAccessor {
public:
    UvcPropertyAccessor(const std::shared_ptr<ISourcePort> &backend);
    ~UvcPropertyAccessor() noexcept override = default;

    void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

private:
    std::shared_ptr<ISourcePort> backend_;
};

}  // namespace libobsensor

