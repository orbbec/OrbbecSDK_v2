#pragma once
#include <mutex>
#include <memory>

#include "utils/Utils.hpp"
#include "exception/ObException.hpp"

namespace libobsensor {
class IDevice;

class IDeviceComponent {
public:
    virtual ~IDeviceComponent()       = default;
    virtual IDevice *getOwner() const = 0;
};

typedef std::unique_lock<std::recursive_timed_mutex> DeviceComponentLock;

template <typename T> class DeviceComponentPtr {
public:
    DeviceComponentPtr(std::shared_ptr<T> ptr, DeviceComponentLock &&lock) : ptr_(ptr), lock_(std::move(lock)) {}
    DeviceComponentPtr(std::shared_ptr<T> ptr) : ptr_(ptr), lock_() {}
    DeviceComponentPtr() : ptr_(), lock_() {}

    // copy constructor and assignment operator are deleted to avoid accidental copies of the lock
    DeviceComponentPtr(const DeviceComponentPtr &other)            = delete;
    DeviceComponentPtr &operator=(const DeviceComponentPtr &other) = delete;

    DeviceComponentPtr(DeviceComponentPtr &&other)            = default;
    DeviceComponentPtr &operator=(DeviceComponentPtr &&other) = default;

    T *operator->() const {
        if(ptr_ == nullptr) {
            throw std::runtime_error("DeviceComponentPtr is nullptr");
        }
        return ptr_.get();
    }

    operator bool() const {
        return ptr_ != nullptr;
    }

    void reset() {
        ptr_.reset();
        lock_ = DeviceComponentLock();
    }

    template <typename U> DeviceComponentPtr<U> as() {
        auto uPtr = std::dynamic_pointer_cast<U>(ptr_);
        if(uPtr == nullptr) {
            throw invalid_value_exception(utils::string::to_string() << "DeviceComponentPtr is not of type " << typeid(U).name());
        }
        ptr_ = nullptr;
        return DeviceComponentPtr<U>(uPtr, std::move(lock_));
    }

    std::shared_ptr<T> get() const {
        return ptr_;
    }

private:
    std::shared_ptr<T>  ptr_;
    DeviceComponentLock lock_;
};
typedef enum {
    OB_DEV_COMPONENT_UNKNOWN = -1,

    OB_DEV_COMPONENT_PROPERTY_SERVER = 0,
    OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR,

    OB_DEV_COMPONENT_DEPTH_SENSOR,
    OB_DEV_COMPONENT_IR_SENSOR,
    OB_DEV_COMPONENT_LEFT_IR_SENSOR,
    OB_DEV_COMPONENT_RIGHT_IR_SENSOR,
    OB_DEV_COMPONENT_COLOR_SENSOR,
    OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY,
    OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR,
    OB_DEV_COMPONENT_IR_FRAME_PROCESSOR,
    OB_DEV_COMPONENT_LEFT_IR_FRAME_PROCESSOR,
    OB_DEV_COMPONENT_RIGHT_IR_FRAME_PROCESSOR,
    OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR,
    OB_DEV_COMPONENT_GYRO_SENSOR,
    OB_DEV_COMPONENT_ACCEL_SENSOR,
    OB_DEV_COMPONENT_IMU_STREAMER,
    OB_DEV_COMPONENT_SENSOR_STREAM_STRATEGY,
    OB_DEV_COMPONENT_STREAM_PROFILE_FILTER,

    OB_DEV_COMPONENT_COLOR_FRAME_METADATA_CONTAINER,
    OB_DEV_COMPONENT_DEPTH_FRAME_METADATA_CONTAINER,

    OB_DEV_COMPONENT_PRESET_MANAGER,
    OB_DEV_COMPONENT_ALG_PARAM_MANAGER,
    OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER,

    OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER,
    OB_DEV_COMPONENT_DEVICE_SYNC_CONFIGURATOR,
    OB_DEV_COMPONENT_DEVICE_CLOCK_SYNCHRONIZER,
    OB_DEV_COMPONENT_DEVICE_MONITOR,

    OB_DEV_COMPONENT_RAW_PHASE_STREAMER,

    OB_DEV_COMPONENT_COUNT
} DeviceComponentId;

}  // namespace libobsensor