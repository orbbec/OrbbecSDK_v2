// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDevice.hpp"
#include "IDeviceManager.hpp"

#include <memory>
#include <map>

namespace libobsensor {

class Context;

class DeviceBase : public IDevice {
private:
    struct ComponentItem {
        DeviceComponentId                                  compId;
        std::shared_ptr<IDeviceComponent>                  component;  // If is nullptr, try to create it
        std::function<std::shared_ptr<IDeviceComponent>()> creator;    // lazy creation
        bool                                               initialized;
        bool                                               lockRequired;
    };

public:
    DeviceBase(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~DeviceBase() noexcept;

    void reset() override;
    void reboot() override;
    void deactivate() override;

    std::shared_ptr<const DeviceInfo> getInfo() const override;
    const std::string                &getExtensionInfo(const std::string &infoKey) const override;
    bool                              isExtensionInfoExists(const std::string &infoKey) const override;

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

    void                                      updateFirmware(const std::vector<uint8_t> &firmware, DeviceFwUpdateCallback updateCallback, bool async) override;
    static std::map<std::string, std::string> parseExtensionInfo(std::string extensionInfo);

protected:
    // implement on subclass, and must be called to initialize the device info on construction
    virtual void        fetchDeviceInfo();
    virtual void        fetchExtensionInfo();
    DeviceComponentLock tryLockResource();
    int                 getFirmwareVersionInt();

protected:
    const std::shared_ptr<const IDeviceEnumInfo> enumInfo_;
    std::shared_ptr<DeviceInfo>                  deviceInfo_;
    std::shared_ptr<NetDeviceInfo>               netDeviceInfo_;
    std::map<std::string, std::string>           extensionInfo_;

private:
    std::shared_ptr<Context> ctx_;  // handle the lifespan of the context

    std::recursive_timed_mutex   resourceMutex_;
    mutable std::recursive_mutex componentsMutex_;
    std::vector<ComponentItem>   components_;  // using vector to control destroy order of components

    std::atomic<bool> isDeactivated_;

    std::map<OBSensorType, std::shared_ptr<const SourcePortInfo>> sensorPortInfos_;
    std::map<OBSensorType, std::shared_ptr<IFilter>>              sensorFrameFilters_;
};

}  // namespace libobsensor

