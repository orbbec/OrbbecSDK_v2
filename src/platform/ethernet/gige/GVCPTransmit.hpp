// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "libobsensor/h/ObTypes.h"
#include "ethernet/socket/SocketTypes.hpp"
#include "common/DeviceSeriesInfo.hpp"
#include "SourcePortInfo.hpp"
#include "GVCPTypes.hpp"
#include <vector>
#include <string>
#include <mutex>
#include <set>
#include <memory>

namespace libobsensor {

class GVCPTransmit : public std::enable_shared_from_this<GVCPTransmit> {
public:
    /**
     * @brief GVCPTransmit constructor
     *
     * @param[in] portInfo source port info of the target device.
     * @param[in] timeoutMs Timeout in milliseconds to wait for a response (default: 200 ms).
     * @param[in] retryCount Number of retransmission attempts if no response (default: 3).
     */
    GVCPTransmit(std::shared_ptr<const NetSourcePortInfo> portInfo, uint32_t timeoutMs = 200, uint8_t retryCount = 3);
    virtual ~GVCPTransmit();

    /**
     * @brief Read a register from a GigE device
     *
     * @param[in] registerAddress Address of the register to read
     *
     * @return A pair where:
     *         - first: acknowledgment status code as defined by GigE (uint16_t)
     *         - second: register value (uint32_t)
     */
    std::pair<uint16_t, uint32_t> readRegister(uint32_t registerAddress);

    /**
     * @brief Write a register to a GigE device
     *
     * @param[in] registerAddress Address of the register to write
     * @param[in] value Value to be written
     *
     * @return Acknowledgment status code as defined by GigE
     */
    uint16_t writeRegister(uint32_t registerAddress, uint32_t value);

private:
    /**
     * @brief Initiate with network source port info
     *
     * @param[in] portInfo source port info of the target device
     * @param[in] timeoutMs Timeout in milliseconds to wait for a response (default: 200 ms).
     */
    void init(std::shared_ptr<const NetSourcePortInfo> portInfo, uint32_t timeoutMs);

    /**
     * @brief DeInitiate
     */
    void deInit();

    /**
     * @brief Discard all data in the socket receive buffer
     */
    void clearSocketReceiveBuffer();

    /**
     * @brief Transmit a GVCP packet and receive the response.
     *
     * @param[in] data Pointer to the data buffer to send. The buffer must begin with a valid GVCP command header
     * @param[in] dataLength Length of the data to send in bytes
     *
     * @return A vector<char> containing the GVCP response, beginning with the
     *         standard GVCP acknowledge header. Returns an empty vector on failure
     *         or timeout; use lastError() to obtain the detailed error code.
     */
    std::vector<char> transmit(const void *data, int dataLength);

    /**
     * @brief Get request id
     *
     * @return new request id
     */
    uint16_t getReqId() {
        ++reqId_;
        if(reqId_ == 0) {
            reqId_ = 1;
        }
        return reqId_;
    }

private:
    std::string ipAddress_;
    SOCKET      sock_       = INVALID_SOCKET;
    uint16_t    reqId_      = 0;  // Request ID for tracking the write operation
    int32_t     lastError_  = 0;
    uint32_t    timeoutMs_  = 200;
    uint8_t     retryCount_ = 3;
};

}  // namespace libobsensor
