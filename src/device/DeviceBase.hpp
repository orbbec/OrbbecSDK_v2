// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDevice.hpp"
#include "IDeviceManager.hpp"

#include <memory>
#include <map>
#include <atomic>

namespace libobsensor {

class Context;

class DeviceBase : public IDevice {
private:
    struct ComponentItem {
        DeviceComponentId                                  compId = OB_DEV_COMPONENT_UNKNOWN;
        std::shared_ptr<IDeviceComponent>                  component;  // If is nullptr, try to create it
        std::function<std::shared_ptr<IDeviceComponent>()> creator;    // lazy creation
        bool                                               initialized  = false;
        bool                                               lockRequired = false;
    };

public:
    DeviceBase(const std::shared_ptr<const IDeviceEnumInfo> &info, OBDeviceAccessMode accessMode = OB_DEVICE_DEFAULT_ACCESS);
    DeviceBase();  // declared for playback device

    virtual ~DeviceBase() noexcept override;

    void reset() override;
    void reboot() override;
    void deactivate() override;

    bool isPlaybackDevice() const override {
        return isPlaybackDevice_.load();
    }
    bool hasAccessControl() const override {
        return hasAccessControl_.load();
    }
    OBDeviceAccessMode getAccessMode() const override {
        if(hasAccessControl_.load()) {
            return accessMode_.load();
        }
        return OB_DEVICE_DEFAULT_ACCESS;
    }

    std::shared_ptr<const DeviceInfo> getInfo() const override;
    bool                              isDeactivated() const override;
    const std::string                &getExtensionInfo(const std::string &infoKey) const override;
    bool                              isExtensionInfoExists(const std::string &infoKey) const override;
    uint64_t                          getDeviceErrorState() const override;
    void                              fetchDeviceErrorState() override;

    const std::string &getSn() const override {
        // Hold a local reference to keep the device info valid during access.
        auto info = deviceInfo_;
        return info->deviceSn_;
    }

    void registerComponent(DeviceComponentId compId, std::function<std::shared_ptr<IDeviceComponent>()> creator, bool lockRequired = false);
    void registerComponent(DeviceComponentId compId, std::shared_ptr<IDeviceComponent> component, bool lockRequired = false);
    void deregisterComponent(DeviceComponentId compId);
    bool isComponentExists(DeviceComponentId compId) const override;
    bool isComponentCreated(DeviceComponentId compId) const override;  // for lazy creation
    DeviceComponentPtr<IDeviceComponent> getComponent(DeviceComponentId compId, bool throwExIfNotFound = true) override;

    void                                         registerSensorPortInfo(OBSensorType type, std::shared_ptr<const SourcePortInfo> sourcePortInfo);
    void                                         deregisterSensor(OBSensorType type);
    const std::shared_ptr<const SourcePortInfo> &getSensorPortInfo(OBSensorType type) const;
    bool                                         isSensorExists(OBSensorType type) const override;
    bool                                         isSensorCreated(OBSensorType type) const override;  // for lazy creation
    DeviceComponentPtr<ISensor>                  getSensor(OBSensorType type) override;
    std::vector<OBSensorType>                    getSensorTypeList() const override;
    bool                                         hasAnySensorStreamActivated() override;

    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type) override;
    std::shared_ptr<IFilter>              getSensorFrameFilter(const std::string &name, OBSensorType type, bool throwIfNotFound = true) override;

    DeviceComponentPtr<IPropertyServer> getPropertyServer() override;

    void updateFirmware(const std::vector<uint8_t> &firmware, DeviceFwUpdateCallback updateCallback, bool async) override;
    void setFirmwareUpdateState(bool isUpdating) override;
    bool isFirmwareUpdating() const override;
    void updateOptionalDepthPresets(const char filePathList[][OB_PATH_MAX], uint8_t pathCount, DeviceFwUpdateCallback updateCallback) override;
    static std::map<std::string, std::string> parseExtensionInfo(std::string extensionInfo);

    void activateDeviceAccessor() override;
    int  getFirmwareVersionInt() override;

    void registerRebootCallback(DeviceRebootCallback callback) override;

protected:
    // implement on subclass, and must be called to initialize the device info on construction
    virtual void        fetchDeviceInfo();
    virtual void        fetchExtensionInfo();
    DeviceComponentLock tryLockResource();
    bool                hasWriteAccess() const;

    std::shared_ptr<ISourcePort> getSourcePort(std::shared_ptr<const SourcePortInfo> sourcePortInfo) const;

    /**
     * @brief Enable heartbeat if "DefaultHeartBeat" config is true.
     *        Reads the "DefaultHeartBeat" option from config and starts heartbeat if enabled.
     *
     * @note Must be called after device initialization is complete.
     */
    void checkAndStartHeartbeat();

protected:
    const std::shared_ptr<const IDeviceEnumInfo> enumInfo_;
    std::shared_ptr<DeviceInfo>                  deviceInfo_;
    std::map<std::string, std::string>           extensionInfo_;
    uint64_t                                     deviceErrorState_ = 0;
    std::atomic<bool>                            isPlaybackDevice_{ false };
    std::atomic<bool>                            hasAccessControl_{ false };
    std::atomic<OBDeviceAccessMode>              accessMode_{ OB_DEVICE_DEFAULT_ACCESS };

private:
    std::shared_ptr<Context> ctx_;  // handle the lifespan of the context

    std::recursive_timed_mutex   resourceMutex_;
    mutable std::recursive_mutex componentsMutex_;
    std::vector<ComponentItem>   components_;  // using vector to control destroy order of components

    std::atomic<bool> isDeactivated_;

    std::map<OBSensorType, std::shared_ptr<const SourcePortInfo>> sensorPortInfos_;
    std::map<OBSensorType, std::shared_ptr<IFilter>>              sensorFrameFilters_;

    std::atomic<bool>                     isFirmwareUpdating_;
    std::shared_ptr<DeviceRebootCallback> rebootCallback_;
};

}  // namespace libobsensor
