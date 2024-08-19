#pragma once
#include "IProperty.hpp"
#include "IDevice.hpp"

namespace libobsensor {
class FemtoBoltPropertyAccessor : public IBasicPropertyAccessor {
public:
    explicit FemtoBoltPropertyAccessor(IDevice *owner);
    virtual ~FemtoBoltPropertyAccessor() noexcept = default;

    virtual void setPropertyValue(uint32_t propertyId, OBPropertyValue value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;
private:
    IDevice *owner_;
};
}  // namespace libobsensor