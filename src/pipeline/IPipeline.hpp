// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <atomic>
#include <memory>

namespace libobsensor {
class Pipeline;
class Config;
}  // namespace libobsensor

#ifdef __cplusplus
extern "C" {
#endif
struct ob_pipeline_t {
    std::shared_ptr<libobsensor::Pipeline> pipeline;
};

struct ob_config_t {
    std::shared_ptr<libobsensor::Config> config;
};

#ifdef __cplusplus
}
#endif
