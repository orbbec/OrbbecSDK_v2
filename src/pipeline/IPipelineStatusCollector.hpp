// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "libobsensor/h/ObTypes.h"
#include <cstdint>

namespace libobsensor {

/**
 * @brief Interface for pipeline status event reporting.
 *
 * This lightweight interface is used by the pipeline and aggregator layers to report
 * frame drop events and frame reception.
 */
class IPipelineStatusCollector {
public:
    virtual ~IPipelineStatusCollector() = default;

    /**
     * @brief Report an SDK-level status event (lightweight atomic OR operation).
     * @param statusBit One of OB_SDK_STATUS_* bitmask values.
     */
    virtual void reportSdkStatus(uint64_t statusBit) = 0;

    /**
     * @brief Report that a frame has been received for a stream.
     * @param stream The stream type that received the frame.
     */
    virtual void reportFrameReceived(OBStreamType stream) = 0;
};

}  // namespace libobsensor
