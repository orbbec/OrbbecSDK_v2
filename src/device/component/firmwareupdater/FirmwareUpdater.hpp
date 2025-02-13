// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDevice.hpp"
#include "DeviceComponentBase.hpp"
#include "FirmwareUpdaterTypes.h"

#include <dylib.hpp>

#include <string>
#include <thread>

namespace libobsensor {

class FirmwareUpdater;
struct FirmwareUpdateContext {
    std::shared_ptr<dylib>                            dylib_                            = nullptr;
    pfunc_ob_device_update_firmware_ext               update_firmware_ext               = nullptr;
    pfunc_ob_device_update_firmware_from_raw_data_ext update_firmware_from_raw_data_ext = nullptr;
    pfunc_ob_device_optional_depth_presets_ext        update_optional_depth_presets_ext = nullptr;
};

class FirmwareUpdater : public DeviceComponentBase {
public:
    FirmwareUpdater(IDevice *owner);
    virtual ~FirmwareUpdater();

    static void onDeviceFwUpdateCallback(ob_fw_update_state state, const char *message, uint8_t percent, void *user_data);

    void updateFirmwareExt(const std::string &path, DeviceFwUpdateCallback callback, bool async);
    void updateFirmwareFromRawDataExt(const uint8_t *firmwareData, uint32_t firmwareSize, DeviceFwUpdateCallback callback, bool async);
    void updateOptionalDepthPresetsExt(const char filePathList[][OB_PATH_MAX], uint8_t pathCount, DeviceFwUpdateCallback callback);

private:
    std::shared_ptr<FirmwareUpdateContext> ctx_;
    DeviceFwUpdateCallback deviceFwUpdateCallback_;
    std::thread                            updateThread_;
};

}  // namespace libobsensor

