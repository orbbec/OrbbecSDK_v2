#pragma once
#include "IProperty.hpp"
#include "IDevice.hpp"

namespace libobsensor {
class FemtoMegaTempPropertyAccessor : public IStructureDataAccessor {
public:
    explicit FemtoMegaTempPropertyAccessor(IDevice *owner);
    virtual ~FemtoMegaTempPropertyAccessor() noexcept = default;

    virtual void                        setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) override;
    virtual const std::vector<uint8_t> &getStructureData(uint32_t propertyId) override;

private:
    IDevice             *owner_;
    std::vector<uint8_t> tempData_;
};
}  // namespace libobsensor