// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IProperty.hpp"
#include "ISourcePort.hpp"
#include "IDevice.hpp"
#include "IDeviceComponent.hpp"

#include <mutex>

namespace libobsensor {

class LiDARPropertyAccessor : public IDeviceComponent, public IBasicPropertyAccessor, public IStructureDataAccessor {
public:
    explicit LiDARPropertyAccessor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend);
    ~LiDARPropertyAccessor() noexcept override = default;

    IDevice *getOwner() const override;

    void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

    void                        setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) override;
    const std::vector<uint8_t> &getStructureData(uint32_t propertyId) override;

private:
    uint32_t getLiDARPid();
    void     clearBuffers();

    std::pair<uint16_t, OBPropertyType> OBPropertyToOpCode(uint32_t propertyId, bool set);

private:
    IDevice *                    owner_;
    std::shared_ptr<ISourcePort> backend_;
    std::mutex                   mutex_;
    std::vector<uint8_t>         recvData_;
    std::vector<uint8_t>         sendData_;
    std::vector<uint8_t>         outputData_;
};

}  // namespace libobsensor
