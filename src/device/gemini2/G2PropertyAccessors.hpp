#pragma once
#include "IProperty.hpp"
#include "IDevice.hpp"

namespace libobsensor {
class G2Disp2DepthPropertyAccessor : public IBasicPropertyAccessor, public IStructureDataAccessor {
public:
    explicit G2Disp2DepthPropertyAccessor(IDevice *owner);
    virtual ~G2Disp2DepthPropertyAccessor() noexcept = default;

    void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

    void                        setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) override;
    const std::vector<uint8_t> &getStructureData(uint32_t propertyId) override;

private:
    void markOutputDisparityFrame(bool enable);

private:
    IDevice *owner_;

    bool hwDisparityToDepthEnabled_;
};

}  // namespace libobsensor