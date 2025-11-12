// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IProperty.hpp"
#include "IDevice.hpp"

namespace libobsensor {
class G305Disp2DepthPropertyAccessor : public IBasicPropertyAccessor {
public:
    explicit G305Disp2DepthPropertyAccessor(IDevice *owner);
    virtual ~G305Disp2DepthPropertyAccessor() noexcept override = default;

    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

private:
    void markOutputDisparityFrame(bool enable);

private:
    IDevice *owner_;

    bool hwDisparityToDepthEnabled_;
    bool swDisparityToDepthEnabled_;
};

class G305HWNoiseRemovePropertyAccessor : public IBasicPropertyAccessor {
public:
    explicit G305HWNoiseRemovePropertyAccessor(IDevice *owner);
    virtual ~G305HWNoiseRemovePropertyAccessor() noexcept override = default;

    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

private:
    IDevice *owner_;
};

class G305ColorAePropertyAccessor : public IBasicPropertyAccessor {
public:
    explicit G305ColorAePropertyAccessor(IDevice *owner);
    virtual ~G305ColorAePropertyAccessor() noexcept override = default;

    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

private:
    IDevice *owner_;
};

}  // namespace libobsensor
