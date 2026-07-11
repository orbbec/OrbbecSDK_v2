// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDevice.hpp"
#include "DeviceComponentBase.hpp"
#include "InternalTypes.hpp"
#include "IDeviceLicenseInfoManager.hpp"

#include <string>
#include <memory>
#include <mutex>
#include <vector>

namespace libobsensor {

class G330DeviceLicenseInfoManager : public IDeviceLicenseInfoManager, public DeviceComponentBase {
public:
    G330DeviceLicenseInfoManager(IDevice *owner);
    virtual ~G330DeviceLicenseInfoManager() override;

    std::string getLicenseInfo() override;
    void        writeLicenseInfo(const std::string &jsonInfo) override;
    void        clearLicenseInfo() override;

private:
    struct LicenseStorageContext;

    void                 initializeLicenseStorageContext();
    void                 refreshLicenseInfoCacheLocked();
    std::vector<uint8_t> readLicenseStorage(uint32_t offset, uint32_t size) const;
    void                 writeLicenseStorage(const uint8_t *data, uint32_t size) const;

private:
    mutable std::mutex                     licenseInfoMutex_;
    std::string                            licenseInfoCache_;  // cache for license info.
    std::shared_ptr<LicenseStorageContext> licenseStorageCtx_;
};

}  // namespace libobsensor
