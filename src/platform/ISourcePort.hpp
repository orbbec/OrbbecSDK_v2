// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <memory>
#include <vector>
#include <string>

#include "libobsensor/h/ObTypes.h"
#include "IFrame.hpp"
#include "IStreamProfile.hpp"
#include "IStreamer.hpp"
#include "SourcePortInfo.hpp"

#include <functional>

namespace libobsensor {
class ISourcePort {
public:
    virtual ~ISourcePort() noexcept = default;

    virtual std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const = 0;

    /**
     * @brief Get driver-level status bits for diagnostics.
     * @return Driver-specific diagnostic bitmask. 0 means no issues.
     */
    virtual uint64_t getDriverStatus() const {
        return 0;
    }

    /**
     * @brief Get SDK-level status bits accumulated by the port (e.g. frame wait errors).
     * @return SDK status bitmask composed of OB_SDK_STATUS_* values. 0 means no issues.
     *         Implementations should clear the accumulator on each call.
     */
    virtual uint64_t getPortStatus() const {
        return 0;
    }
};

// for vendor command
class IVendorDataPort : virtual public ISourcePort {  // Virtual inheritance solves diamond inheritance problem
public:
    ~IVendorDataPort() noexcept override = default;

    virtual uint32_t sendAndReceive(const uint8_t *sendData, uint32_t sendLen, uint8_t *recvData, uint32_t exceptedRecvLen) = 0;
};

// for imu data stream
class IDataStreamPort : virtual public ISourcePort {  // Virtual inheritance solves diamond inheritance problem
public:
    ~IDataStreamPort() noexcept override = default;

    virtual void startStream(MutableFrameCallback callback) = 0;
    virtual void stopStream()                               = 0;
};

// for video data stream: depth, color, ir, etc.
class IVideoStreamPort : virtual public ISourcePort, public IStreamer {  // Virtual inheritance solves diamond inheritance problem
public:
    ~IVideoStreamPort() noexcept override = default;

    virtual StreamProfileList getStreamProfileList() = 0;
    virtual void              stopAllStream()        = 0;
};

}  // namespace libobsensor
