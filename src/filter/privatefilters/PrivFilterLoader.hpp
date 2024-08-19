#pragma once
#include <string>
#include <memory>
#include <map>
#include <dylib.hpp>
#include "IFilter.hpp"

#ifdef __cplusplus
extern "C" {
#endif
#include "PrivFilterTypes.h"
#ifdef __cplusplus
}
#endif

namespace libobsensor {

struct PrivFilterPackageContext {
    std::string                                   dir;
    std::string                                   package_name;
    std::shared_ptr<dylib>                        dynamic_library;
    pfunc_ob_get_filter_count                     get_filter_count;
    pfunc_ob_get_filter_name                      get_filter_name;
    pfunc_ob_create_filter                        create_filter;
    pfunc_ob_priv_filter_get_vendor_specific_code get_vendor_specific_code;
    pfunc_ob_priv_filter_is_activated             is_activated;
    pfunc_ob_priv_filter_activate                 activate;
};

class PrivFilterCreator : public IPrivFilterCreator {
public:
    PrivFilterCreator(std::shared_ptr<PrivFilterPackageContext> pkgCtx, size_t index);
    ~PrivFilterCreator() override = default;

    std::shared_ptr<IFilter> create() override;
    std::shared_ptr<IFilter> create(const std::string &activationKey) override;
    const std::string       &getVendorSpecificCode() const override;

private:
    std::shared_ptr<PrivFilterPackageContext> pkgCtx_;
    size_t                                    index_;
    std::string                               code_;
};

namespace PrivFilterCreatorLoader {
std::map<std::string, std::shared_ptr<IFilterCreator>> getCreators();
}

}  // namespace libobsensor