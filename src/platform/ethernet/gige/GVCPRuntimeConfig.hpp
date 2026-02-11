// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "libobsensor/h/ObTypes.h"
#include "GVCPTypes.hpp"
#include <string>
#include <memory>
#include <atomic>
#include <mutex>

namespace libobsensor {

class GVCPRuntimeConfig : public std::enable_shared_from_this<GVCPRuntimeConfig> {
public:
    GVCPRuntimeConfig();
    virtual ~GVCPRuntimeConfig() = default;

    static std::shared_ptr<GVCPRuntimeConfig> getInstance();

    /**
     * @brief Get current GVCP communication port
     *
     * @return port number
     */
    uint16_t getGvcpPort() const;

    /**
     * @brief Set GVCP port scheme
     *
     * @note Thrown exception if the specified GVCP port scheme is invalid or unsupported
     */
    void setGvcpPortscheme(OBGvcpPortScheme scheme);

    /**
     * @brief Get current GVCP port scheme
     *
     * @return GVCP port scheme
     */
    OBGvcpPortScheme getGvcpPortscheme() const;

private:
    /**
     * @brief Load gvcp config from config file
     */
    void loadGvcpConfig();

private:
    std::atomic<uint16_t>                   gvcpPort_{ GVCP_PORT };
    std::atomic<OBGvcpPortScheme>           gvcpPortScheme_{ OB_GVCP_PORT_SCHEME_STANDARD };
    static std::mutex                       instanceMutex_;
    static std::weak_ptr<GVCPRuntimeConfig> instanceWeakPtr_;
};

}  // namespace libobsensor
