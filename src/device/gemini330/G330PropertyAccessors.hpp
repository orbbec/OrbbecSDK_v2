// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IProperty.hpp"
#include "IDevice.hpp"

namespace libobsensor {
class G330Disp2DepthPropertyAccessor : public IBasicPropertyAccessor {
public:
    explicit G330Disp2DepthPropertyAccessor(IDevice *owner);
    virtual ~G330Disp2DepthPropertyAccessor() noexcept override = default;

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

class G330NetPerformanceModePropertyAccessor : public IBasicPropertyAccessor {
public:
    explicit G330NetPerformanceModePropertyAccessor(IDevice *owner);
    virtual ~G330NetPerformanceModePropertyAccessor() noexcept = default;

    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

private:
    void updatePerformanceMode(uint32_t mode);

private:
    IDevice *owner_;

    uint32_t performanceMode_;
};


class G330HWNoiseRemovePropertyAccessor : public IBasicPropertyAccessor {
public:
    explicit G330HWNoiseRemovePropertyAccessor(IDevice *owner);
    virtual ~G330HWNoiseRemovePropertyAccessor() noexcept = default;

    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

private:
    IDevice *owner_;
};

class G330NetPTPClockSyncPropertyAccessor : public IBasicPropertyAccessor {
public:
    explicit G330NetPTPClockSyncPropertyAccessor(IDevice *owner);
    virtual ~G330NetPTPClockSyncPropertyAccessor() noexcept = default;

    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

private:
    IDevice *owner_;
};
}  // namespace libobsensor
