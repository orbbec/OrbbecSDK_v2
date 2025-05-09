// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IProperty.hpp"
#include "IDevice.hpp"
#include "DeviceComponentBase.hpp"

#include <atomic>

namespace libobsensor {

class PlaybackVendorPropertyAccessor : public IBasicPropertyAccessor, public IStructureDataAccessor, public DeviceComponentBase {
public:
    explicit PlaybackVendorPropertyAccessor(const std::shared_ptr<ISourcePort> &backend, IDevice *owner);
    virtual ~PlaybackVendorPropertyAccessor() noexcept = default;

    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

    virtual void                        setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) override;
    virtual const std::vector<uint8_t> &getStructureData(uint32_t propertyId) override;

public:
    template <typename T> T *allocateData(std::vector<uint8_t> &data);

private:
    const std::shared_ptr<ISourcePort> port_;
    std::vector<uint8_t>               data_;
};

class PlaybackFilterPropertyAccessor : public IBasicPropertyAccessor {
public:
    explicit PlaybackFilterPropertyAccessor(const std::shared_ptr<ISourcePort> &backend, IDevice *owner);
    virtual ~PlaybackFilterPropertyAccessor() noexcept = default;

    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

private:
    void getRecordPropertyValue(uint32_t propertyId, OBPropertyValue *value);

    const std::shared_ptr<ISourcePort> port_;
    IDevice                           *owner_;
};

class PlaybackFrameTransformPropertyAccessor : public IBasicPropertyAccessor {
public:
    explicit PlaybackFrameTransformPropertyAccessor(const std::shared_ptr<ISourcePort> &backend, IDevice *owner);
    virtual ~PlaybackFrameTransformPropertyAccessor() noexcept = default;

    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

    void initFrameTransformProperty();

private:
    const std::shared_ptr<ISourcePort>      port_;
    IDevice                                *owner_;
    std::shared_ptr<IBasicPropertyAccessor> accessor_;
};

}  // namespace libobsensor