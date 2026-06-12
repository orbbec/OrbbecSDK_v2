// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IProperty.hpp"
#include "IDevice.hpp"

#include <memory>

namespace libobsensor {

// Refuses OB_PROP_DEPTH_ALIGN_HARDWARE_BOOL writes while any video sensor stream is active.
class HardwareD2CPropertyAccessor : public IBasicPropertyAccessor {
public:
    HardwareD2CPropertyAccessor(IDevice *owner, std::shared_ptr<IBasicPropertyAccessor> delegate);
    virtual ~HardwareD2CPropertyAccessor() noexcept override = default;

    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

private:
    IDevice                                *owner_;
    std::shared_ptr<IBasicPropertyAccessor> delegate_;
};

}  // namespace libobsensor
