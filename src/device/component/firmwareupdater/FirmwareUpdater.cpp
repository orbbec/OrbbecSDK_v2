// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FirmwareUpdater.hpp"
#include "environment/EnvConfig.hpp"
#include "exception/ObException.hpp"
#include "logger/Logger.hpp"

namespace libobsensor {
FirmwareUpdater::FirmwareUpdater(IDevice *owner) : DeviceComponentBase(owner) {
    std::string moduleLoadPath = EnvConfig::getExtensionsDirectory() + "/firmwareupdater/";
    try {
        ctx_                      = std::make_shared<FirmwareUpdateContext>();
        ctx_->dylib_              = std::make_shared<dylib>(moduleLoadPath.c_str(), "firmwareupdater");
        ctx_->update_firmware_ext = ctx_->dylib_->get_function<void(ob_device *, const char *, ob_device_fw_update_callback, bool, void *, ob_error **)>(
            "ob_device_update_firmware_ext");
        ctx_->update_firmware_from_raw_data_ext =
            ctx_->dylib_->get_function<void(ob_device *, const uint8_t *, uint32_t, ob_device_fw_update_callback, bool, void *, ob_error **)>(
                "ob_device_update_firmware_from_raw_data_ext");
    }
    catch(const std::exception &e) {
        LOG_DEBUG("Failed to load firmwareupdater library: {}", e.what());
    }
}

FirmwareUpdater::~FirmwareUpdater() noexcept {}

void FirmwareUpdater::onDeviceFwUpdateCallback(ob_fw_update_state state, const char *message, uint8_t percent, void *userData) {
    FirmwareUpdater *updater = reinterpret_cast<FirmwareUpdater *>(userData);
    if(updater && updater->deviceFwUpdateCallback_) {
        LOG_DEBUG("Firmware update callback: state={}, message={}, percent={}", state, message, percent);
        updater->deviceFwUpdateCallback_(state, message, percent);
    }
}

void FirmwareUpdater::updateFirmwareExt(const std::string &path, DeviceFwUpdateCallback callback, bool async) {
    deviceFwUpdateCallback_ = callback;
    auto func               = [this, path, async]() {
        ob_error *error  = nullptr;
        auto      device = std::make_shared<ob_device>();
        device->device   = getOwner()->shared_from_this();
        ctx_->update_firmware_ext(device.get(), path.c_str(), onDeviceFwUpdateCallback, async, this, &error);
        if(error) {
            throw libobsensor_exception(std::string(error->message), error->exception_type);
        }
    };
    if(async) {
        std::thread([func]() {
            try {
                func();
            }
            catch(const std::exception &e) {
                LOG_ERROR("Failed to update firmware: {}", e.what());
            }
        }).detach();
    }
    else {
        func();
    }
}

void FirmwareUpdater::updateFirmwareFromRawDataExt(const uint8_t *firmwareData, uint32_t firmwareSize, DeviceFwUpdateCallback callback, bool async) {
    deviceFwUpdateCallback_ = callback;
    deviceFwUpdateCallback_(STAT_FILE_TRANSFER, "Ready to update firmware...", 0);

    // Prevent asynchronous call data from being destroyed
    std::vector<uint8_t> data(firmwareData, firmwareData + firmwareSize);
    auto func               = [this, data, firmwareSize, async]() {
        ob_error *error  = nullptr;
        auto      device = std::make_shared<ob_device>();
        device->device   = getOwner()->shared_from_this();
        ctx_->update_firmware_from_raw_data_ext(device.get(), data.data(), firmwareSize, onDeviceFwUpdateCallback, async, this, &error);
        if(error) {
            throw libobsensor_exception(std::string(error->message), error->exception_type);
        }
    };
    if(async) {
        std::thread([func]() {
            try {
                func();
            }
            catch(const std::exception &e) {
                LOG_ERROR("Failed to update firmware: {}", e.what());
            }
        }).detach();
    }
    else {
        func();
    }
}

}  // namespace libobsensor
