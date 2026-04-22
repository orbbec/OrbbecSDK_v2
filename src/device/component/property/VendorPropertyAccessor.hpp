// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IProperty.hpp"
#include "ISourcePort.hpp"
#include "IDevice.hpp"
#include "IDeviceComponent.hpp"
#include "protocol/Protocol.hpp"  // protocol::HpStatus — required for executeAndCheck return type
#include <atomic>
#include <mutex>

namespace libobsensor {
class VendorPropertyAccessor : public IDeviceComponent,
                               public IBasicPropertyAccessor,
                               public IStructureDataAccessor,
                               public IStructureDataAccessorV1_1,
                               public IRawDataAccessor {
public:
    explicit VendorPropertyAccessor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend);
    ~VendorPropertyAccessor() noexcept override = default;

    IDevice *getOwner() const override;

    void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

    void                        setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) override;
    const std::vector<uint8_t> &getStructureData(uint32_t propertyId) override;

    void getRawData(uint32_t propertyId, GetDataCallback callback) override;

    uint16_t                    getCmdVersionProtoV1_1(uint32_t propertyId) override;
    const std::vector<uint8_t> &getStructureDataProtoV1_1(uint32_t propertyId, uint16_t cmdVersion) override;
    void                        setStructureDataProtoV1_1(uint32_t propertyId, const std::vector<uint8_t> &data, uint16_t cmdVersion) override;
    const std::vector<uint8_t> &getStructureDataListProtoV1_1(uint32_t propertyId, uint16_t cmdVersion) override;

    void setRawdataTransferPacketSize(uint32_t size);
    void setStructListDataTransferPacketSize(uint32_t size);

    // Enable automatic device reboot when a fatal protocol-level
    // getXu IO fault (LIBUSB_ERROR_IO) is detected. Default: false (disabled).
    void setAutoRebootEnabled(bool enable);

private:
    // Unified execute + fault detection + checkStatus entry point.
    // Replaces all protocol::execute() + protocol::checkStatus() call pairs.
    // On fault: triggers triggerReboot() (if enabled), then lets the
    // exception propagate normally so callers remain unaffected.
    protocol::HpStatus executeAndCheck(const std::shared_ptr<IVendorDataPort> &port,
                                       uint8_t  *reqData,
                                       uint16_t  reqDataSize,
                                       uint8_t  *respData,
                                       uint16_t *respDataSize,
                                       uint32_t  propertyId,
                                       uint16_t  expectedRespLen = 0);

    // Send OB_PROP_DEVICE_RESET_BOOL=1 via backend_ to reboot the device.
    // Guaranteed to execute at most once per VendorPropertyAccessor instance.
    void triggerReboot();

    void clearBuffers();

private:
    IDevice                          *owner_;
    std::shared_ptr<ISourcePort>      backend_;
    std::mutex                        mutex_;
    std::vector<uint8_t>              recvData_;
    std::vector<uint8_t>              sendData_;
    std::vector<uint8_t>              outputData_;
    std::vector<std::vector<uint8_t>> structureDataList_;  // for cmd version 1.1
    uint32_t                          rawdataTransferPacketSize_;
    uint32_t                          structListDataTransferPacketSize_;

    bool                 autoRebootEnabled_{ false };  // controlled by setAutoRebootEnabled()
    std::atomic<bool>    rebootTriggered_{ false };    // prevents duplicate reboot triggers
};
}  // namespace libobsensor

