// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FirmwareUpdater.hpp"
#include "context/DynamicLibraryManager.hpp"
#include "environment/EnvConfig.hpp"
#include "exception/ObException.hpp"
#include "logger/Logger.hpp"
#include "firmwareupdateguard/FirmwareUpdateGuards.hpp"

namespace libobsensor {

namespace {

/**
 * @brief RAII guard for exclusive device updates.
 *
 * Claims the device update slot on construction and releases it on destruction.
 * While active, the device is marked as updating.
 * Asynchronous updates should retain the guard via shared_ptr until completion.
 */
class ScopedUpdateClaim {
public:
    ScopedUpdateClaim(std::atomic<bool> &flag, IDevice *owner) : flag_(flag), owner_(owner) {
        bool expected = false;
        if(!flag_.compare_exchange_strong(expected, true)) {
            THROW_WRONG_API_CALL_SEQUENCE_EXCEPTION("Another firmware or preset update is already in progress, please wait for it to finish.");
        }
        if(owner_) {
            owner_->setFirmwareUpdateState(true);
        }
    }

    ~ScopedUpdateClaim() noexcept {
        if(owner_) {
            owner_->setFirmwareUpdateState(false);
        }
        flag_.store(false);
    }

    ScopedUpdateClaim(const ScopedUpdateClaim &)            = delete;
    ScopedUpdateClaim &operator=(const ScopedUpdateClaim &) = delete;

private:
    std::atomic<bool> &flag_;
    IDevice           *owner_;
};
}  // namespace

FirmwareUpdater::FirmwareUpdater(IDevice *owner) : DeviceComponentBase(owner) {
    try {
        ctx_         = std::make_shared<FirmwareUpdateContext>();
        ctx_->dylib_ = DynamicLibraryManager::getInstance()->getLibrary("firmwareupdater");
        if(!ctx_->dylib_) {
            ctx_->dylib_ = DynamicLibraryManager::getInstance()->loadLibrary(EnvConfig::getExtensionsDirectory() + "/firmwareupdater/", "firmwareupdater");
        }
        if(ctx_ && ctx_->dylib_) {
            ctx_->update_firmware_ext = ctx_->dylib_->get_function<void(ob_device *, const char *, ob_device_fw_update_callback, bool, void *, ob_error **)>(
                "ob_device_update_firmware_ext");
            ctx_->update_firmware_from_raw_data_ext =
                ctx_->dylib_->get_function<void(ob_device *, const uint8_t *, uint32_t, ob_device_fw_update_callback, bool, void *, ob_error **)>(
                    "ob_device_update_firmware_from_raw_data_ext");
            ctx_->update_optional_depth_presets_ext =
                ctx_->dylib_
                    ->get_function<void(ob_device *, const char filePathList[][OB_PATH_MAX], uint8_t, ob_device_fw_update_callback, void *, ob_error **)>(
                        "ob_device_update_optional_depth_presets_ext");
            ctx_->read_customer_data_ext = ctx_->dylib_->get_function<void(ob_device *, void *, uint32_t *, ob_error **)>("ob_device_read_customer_data_ext");
            ctx_->write_customer_data_ext =
                ctx_->dylib_->get_function<void(ob_device *, const void *, uint32_t, ob_error **)>("ob_device_write_customer_data_ext");
        }
    }
    catch(const std::exception &e) {
        LOG_WARN("Failed to load firmwareupdater library: {}", e.what());
        THROW_STANDARD_EXCEPTION(e.what());
    }
}

FirmwareUpdater::~FirmwareUpdater() noexcept {
    if(updateThread_.joinable()) {
        updateThread_.join();
    }
}

void FirmwareUpdater::onDeviceFwUpdateCallback(ob_fw_update_state state, const char *message, uint8_t percent, void *userData) {
    FirmwareUpdater *updater = reinterpret_cast<FirmwareUpdater *>(userData);
    if(updater && updater->deviceFwUpdateCallback_) {
        LOG_DEBUG("Firmware update callback: state={}, message={}, percent={}", static_cast<int>(state), message, percent);
        updater->deviceFwUpdateCallback_(state, message, percent);
    }
}

void FirmwareUpdater::updateFirmwareExt(const std::string &path, DeviceFwUpdateCallback callback, bool async) {
    // Reject concurrent updates and mark the device as updating
    // Released when the worker thread (last owner) finishes
    auto claim = std::make_shared<ScopedUpdateClaim>(updateInProgress_, getOwner());

    if(updateThread_.joinable()) {
        updateThread_.join();
    }

    deviceFwUpdateCallback_ = callback;
    deviceFwUpdateCallback_(STAT_FILE_TRANSFER, "Ready to update firmware...", 0);

    updateThread_ = std::thread([this, path, async, claim]() {
        ob_error *error  = nullptr;
        auto      device = std::make_shared<ob_device>();
        device->device   = getOwner()->shared_from_this();

        std::shared_ptr<IFirmwareUpdateGuard> guard;
        auto factory = device->device->getComponentT<FirmwareUpdateGuardFactory>(OB_DEV_COMPONENT_FIRMWARE_UPDATE_GUARD_FACTORY, false);
        if(factory) {
            guard = factory->create();
        }

        ctx_->update_firmware_ext(device.get(), path.c_str(), onDeviceFwUpdateCallback, async, this, &error);
        if(error) {
            LOG_ERROR("Firmware update failed: {}", error->message);
            delete error;
        }
    });
    if(!async) {
        updateThread_.join();
    }
}

void FirmwareUpdater::updateFirmwareFromRawDataExt(const uint8_t *firmwareData, uint32_t firmwareSize, DeviceFwUpdateCallback callback, bool async) {
    // Reject concurrent updates and mark the device as updating
    // Released when the worker thread (last owner) finishes
    auto claim = std::make_shared<ScopedUpdateClaim>(updateInProgress_, getOwner());

    if(updateThread_.joinable()) {
        updateThread_.join();
    }

    deviceFwUpdateCallback_ = callback;
    deviceFwUpdateCallback_(STAT_FILE_TRANSFER, "Ready to update firmware...", 0);

    // Prevent asynchronous call data from being destroyed
    std::vector<uint8_t> data(firmwareData, firmwareData + firmwareSize);
    updateThread_ = std::thread([this, data, firmwareSize, async, claim]() {
        ob_error *error  = nullptr;
        auto      device = std::make_shared<ob_device>();
        device->device   = getOwner()->shared_from_this();

        std::shared_ptr<IFirmwareUpdateGuard> guard;
        auto factory = device->device->getComponentT<FirmwareUpdateGuardFactory>(OB_DEV_COMPONENT_FIRMWARE_UPDATE_GUARD_FACTORY, false);
        if(factory) {
            guard = factory->create();
        }

        ctx_->update_firmware_from_raw_data_ext(device.get(), data.data(), firmwareSize, onDeviceFwUpdateCallback, async, this, &error);
        if(error) {
            LOG_ERROR("Firmware update failed: {}", error->message);
            delete error;
        }
    });
    if(!async) {
        updateThread_.join();
    }
}

void FirmwareUpdater::updateOptionalDepthPresetsExt(const char filePathList[][OB_PATH_MAX], uint8_t pathCount, DeviceFwUpdateCallback callback) {
    // Reject concurrent updates and mark the device as updating
    // Released when this (synchronous) call returns
    ScopedUpdateClaim claim(updateInProgress_, getOwner());

    deviceFwUpdateCallback_ = callback;
    deviceFwUpdateCallback_(STAT_START, "Ready to update optional depth preset...", 0);

    // Synchronous: run directly on the caller's thread.
    ob_error *error  = nullptr;
    auto      device = std::make_shared<ob_device>();
    device->device   = getOwner()->shared_from_this();
    ctx_->update_optional_depth_presets_ext(device.get(), filePathList, pathCount, onDeviceFwUpdateCallback, this, &error);
    if(error) {
        LOG_ERROR("Preset update failed: {}", error->message);
        delete error;
    }
}

void FirmwareUpdater::writeCustomerDataExt(const uint8_t *customerData, uint32_t customerDataSize, ob_error **error) {
    auto device    = std::make_shared<ob_device>();
    device->device = getOwner()->shared_from_this();
    ctx_->write_customer_data_ext(device.get(), customerData, customerDataSize, error);
    if(*error) {
        LOG_ERROR("Firmware update failed: {}", (*error)->message);
    }
}

void FirmwareUpdater::readCustomerDataExt(uint8_t *customerData, uint32_t *customerDataSize, ob_error **error) {
    auto device    = std::make_shared<ob_device>();
    device->device = getOwner()->shared_from_this();
    ctx_->read_customer_data_ext(device.get(), customerData, customerDataSize, error);
    if(*error) {
        LOG_ERROR("Firmware update failed: {}", (*error)->message);
    }
}

}  // namespace libobsensor
