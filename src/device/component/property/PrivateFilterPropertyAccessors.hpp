#pragma once

#include "IProperty.hpp"
#include "IDevice.hpp"

namespace libobsensor {

class PrivateFilterPropertyAccessor : public IBasicPropertyAccessor {
public:
    PrivateFilterPropertyAccessor(IDevice *device);
    virtual ~PrivateFilterPropertyAccessor() noexcept = default;

    void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

private:
    IDevice *device_;
};
}  // namespace libobsensor