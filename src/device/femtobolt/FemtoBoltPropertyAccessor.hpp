// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IProperty.hpp"
#include "IDevice.hpp"

namespace libobsensor {
class FemtoBoltIrModePropertyAccessor : public IBasicPropertyAccessor {
public:
    explicit FemtoBoltIrModePropertyAccessor(IDevice *owner);
    virtual ~FemtoBoltIrModePropertyAccessor() noexcept = default;

    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

private:
    IDevice *owner_;
};

class FemtoBoltTempPropertyAccessor : public IStructureDataAccessor {
public:
    explicit FemtoBoltTempPropertyAccessor(IDevice *owner);
    virtual ~FemtoBoltTempPropertyAccessor() noexcept = default;

    virtual void                        setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) override;
    virtual const std::vector<uint8_t> &getStructureData(uint32_t propertyId) override;

private:
    IDevice             *owner_;
    std::vector<uint8_t> tempData_;
};

}  // namespace libobsensor
