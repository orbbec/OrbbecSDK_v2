// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IProperty.hpp"
#include "IDevice.hpp"

namespace libobsensor {
class G2Disp2DepthPropertyAccessor : public IBasicPropertyAccessor, public IStructureDataAccessor {
public:
    explicit G2Disp2DepthPropertyAccessor(IDevice *owner);
    virtual ~G2Disp2DepthPropertyAccessor() noexcept override = default;

    IDevice *getOwner();
    void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

    void                        setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) override;
    const std::vector<uint8_t> &getStructureData(uint32_t propertyId) override;

private:
    void markOutputDisparityFrame(bool enable);

protected:
    bool hwDisparityToDepthEnabled_;
    std::vector<uint16_t> hwD2DSupportList_ = { OB_PRECISION_0MM8, OB_PRECISION_0MM4, OB_PRECISION_0MM2 };

private:
    IDevice                    *owner_;
    const std::vector<uint16_t> swD2DSupportList_ = { OB_PRECISION_1MM, OB_PRECISION_0MM8, OB_PRECISION_0MM4, OB_PRECISION_0MM2, OB_PRECISION_0MM1 };
};

class G210Disp2DepthPropertyAccessor : public G2Disp2DepthPropertyAccessor {
public:
    explicit G210Disp2DepthPropertyAccessor(IDevice *owner);
    virtual ~G210Disp2DepthPropertyAccessor() noexcept = default;

    void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
};

class G2FrameTransformPropertyAccessor : public IBasicPropertyAccessor {
public:
    explicit G2FrameTransformPropertyAccessor(IDevice *owner);
    virtual ~G2FrameTransformPropertyAccessor() noexcept = default;

    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

private:
    OBDepthModeOptionCode getOptionCode();

private:
    IDevice              *owner_;
    OBDepthModeOptionCode optionCode_;
};

class G435LeDisp2DepthPropertyAccessor : public G2Disp2DepthPropertyAccessor {
public:
    explicit G435LeDisp2DepthPropertyAccessor(IDevice *owner);
    virtual ~G435LeDisp2DepthPropertyAccessor() noexcept = default;

    const std::vector<uint8_t> &getStructureData(uint32_t propertyId) override;    
};

class G210FrameTransformPropertyAccessor : public IBasicPropertyAccessor {
public:
    explicit G210FrameTransformPropertyAccessor(IDevice *owner);
    virtual ~G210FrameTransformPropertyAccessor() noexcept = default;

    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

private:
    IDevice *owner_;
};
}  // namespace libobsensor
