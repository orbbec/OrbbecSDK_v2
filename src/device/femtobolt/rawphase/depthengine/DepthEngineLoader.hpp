#pragma once

#include <dylib.hpp>
#include <memory>

#ifdef __cplusplus
extern "C" {
#endif
#include "k4aplugin.h"
#ifdef __cplusplus
}  // extern "C"
#endif
namespace libobsensor {

typedef struct {
    k4a_plugin_t           plugin;
    k4a_register_plugin_fn registerFn;
    volatile bool          loaded;
} deloader_global_context_t;

class DepthEngineLoadFactory {
public:
    DepthEngineLoadFactory();
    ~DepthEngineLoadFactory() noexcept = default;

    std::shared_ptr<deloader_global_context_t> getGlobalContext() {
        return context_;
    }

private:
    std::shared_ptr<dylib>                     dylib_;
    std::shared_ptr<deloader_global_context_t> context_;
};

}  // namespace libobsensor