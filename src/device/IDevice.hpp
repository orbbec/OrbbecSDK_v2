#pragma once
#include "libobsensor/h/ObTypes.h"
#include "IProperty.hpp"
#include "ISensor.hpp"
#include "IFilter.hpp"
#include "IPresetManager.hpp"
#include "IDeviceComponent.hpp"

namespace libobsensor {

struct DeviceInfo {
    // identifier of the device
    int         pid_ = 0;
    int         vid_ = 0;
    std::string uid_;  // Unique identifier of the port the device is connected to (platform specific)
    std::string name_;

    std::string connectionType_;  // Device connection type
    uint16_t    type_ = 0;        // 0: Monocular disparity based; 1: Binocular disparity based; 2: tof
    std::string fwVersion_;
    std::string hwVersion_;
    std::string supportedSdkVersion_;
    std::string asicName_;
    std::string deviceSn_;
};

typedef std::function<void(OBFwUpdateState state, const char *message, uint8_t percent)> DeviceFwUpdateCallback;

class IDevice : public std::enable_shared_from_this<IDevice> {
public:
    virtual ~IDevice() = default;

    // device life control
    virtual void reset()      = 0;
    virtual void reboot()     = 0;
    virtual void deactivate() = 0;

    // device info
    virtual std::shared_ptr<const DeviceInfo> getInfo() const                                    = 0;
    virtual const std::string                &getExtensionInfo(const std::string &infoKey) const = 0;

    // device components management
    virtual bool                                 isComponentExists(DeviceComponentId compId) const                     = 0;
    virtual bool                                 isComponentCreated(DeviceComponentId compId) const                    = 0;  // for lazy creation
    virtual DeviceComponentPtr<IDeviceComponent> getComponent(DeviceComponentId compId, bool throwExIfNotFound = true) = 0;
    virtual DeviceComponentPtr<IPropertyServer>  getPropertyServer()                                                   = 0;

    // device sensors (specify components) management
    virtual bool                        isSensorExists(OBSensorType type) const  = 0;
    virtual bool                        isSensorCreated(OBSensorType type) const = 0;  // for lazy creation
    virtual DeviceComponentPtr<ISensor> getSensor(OBSensorType type)             = 0;
    virtual std::vector<OBSensorType>   getSensorTypeList() const                = 0;
    virtual bool                        hasAnySensorStreamActivated()            = 0;

    // todo: Add a filter manager as a component and move this function to it
    virtual std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type)                                     = 0;
    virtual std::shared_ptr<IFilter>              getSensorFrameFilter(const std::string &name, OBSensorType type, bool throwIfNotFound = true) = 0;

    // device firmware update
    virtual void updateFirmware(const std::vector<uint8_t> &firmware, DeviceFwUpdateCallback updateCallback, bool async) = 0;

public:
    // templated functions
    template <typename T> DeviceComponentPtr<T> getComponentT(DeviceComponentId compId, bool throwExIfNotFound = true) {
        auto comp = getComponent(compId, throwExIfNotFound);
        if(comp) {
            return comp.as<T>();
        }
        return DeviceComponentPtr<T>(nullptr);
    }

protected:
    // device initialization, called on constructor and reset()
    virtual void init() = 0;
};

}  // namespace libobsensor

#ifdef __cplusplus
extern "C" {
#endif
struct ob_device_t {
    std::shared_ptr<libobsensor::IDevice> device;
};

struct ob_device_info_t {
    std::shared_ptr<const libobsensor::DeviceInfo> info;
};

#ifdef __cplusplus
}
#endif