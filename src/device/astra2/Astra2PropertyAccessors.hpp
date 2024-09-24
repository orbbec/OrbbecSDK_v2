#pragma once
#include "IProperty.hpp"
#include "IDevice.hpp"

namespace libobsensor {
class Astra2Disp2DepthPropertyAccessor : public IBasicPropertyAccessor, public IStructureDataAccessor {
public:
    explicit Astra2Disp2DepthPropertyAccessor(IDevice *owner);
    virtual ~Astra2Disp2DepthPropertyAccessor() noexcept = default;

    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

    void                        setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) override;
    const std::vector<uint8_t> &getStructureData(uint32_t propertyId) override;

private:
    void markOutputDisparityFrame(bool enable);

private:
    IDevice *owner_;

    bool hwDisparityToDepthEnabled_;
    bool swDisparityToDepthEnabled_;
};

class Astra2TempPropertyAccessor : public IStructureDataAccessor {
public:
    explicit Astra2TempPropertyAccessor(IDevice *owner);
    virtual ~Astra2TempPropertyAccessor() noexcept = default;

    virtual void                        setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) override;
    virtual const std::vector<uint8_t> &getStructureData(uint32_t propertyId) override;

private:
    IDevice             *owner_;
    std::vector<uint8_t> tempData_;
};
}  // namespace libobsensor