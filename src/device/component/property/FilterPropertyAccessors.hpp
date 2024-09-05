#include "IProperty.hpp"
#include "IFilter.hpp"

namespace libobsensor {

class FilterStatePropertyAccessor : public IBasicPropertyAccessor {
public:
    FilterStatePropertyAccessor(std::shared_ptr<IFilter> filter);
    virtual ~FilterStatePropertyAccessor() noexcept = default;

    void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

private:
    std::shared_ptr<IFilter> filter_;
};
}  // namespace libobsensor
