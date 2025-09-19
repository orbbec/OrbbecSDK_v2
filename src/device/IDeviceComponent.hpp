// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <mutex>
#include <memory>
#include <map>
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

// define Device Component Id list here
// For X-macro, 2nd parameter = enum value; leave empty to auto-increment
#define FOREACH_DEV_COMPONENT_ID(X)                                                                \
    X(OB_DEV_COMPONENT_PROPERTY_SERVER, "property server")                                         \
    X(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR, "main property server")                             \
    X(OB_DEV_COMPONENT_DEPTH_SENSOR, "depth sensor")                                               \
    X(OB_DEV_COMPONENT_IR_SENSOR, "ir sensor")                                                     \
    X(OB_DEV_COMPONENT_LEFT_IR_SENSOR, "left ir sensor")                                           \
    X(OB_DEV_COMPONENT_RIGHT_IR_SENSOR, "right ir sensor")                                         \
    X(OB_DEV_COMPONENT_COLOR_SENSOR, "color sensor")                                               \
    X(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY, "frame processor factory")                         \
    X(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, "depth frame processor")                             \
    X(OB_DEV_COMPONENT_IR_FRAME_PROCESSOR, "ir frame processor")                                   \
    X(OB_DEV_COMPONENT_LEFT_IR_FRAME_PROCESSOR, "left ir frame processor")                         \
    X(OB_DEV_COMPONENT_RIGHT_IR_FRAME_PROCESSOR, "right ir frame processor")                       \
    X(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR, "color frame processor")                             \
    X(OB_DEV_COMPONENT_CONFIDENCE_FRAME_PROCESSOR, "confidence frame processor")                   \
    X(OB_DEV_COMPONENT_GYRO_SENSOR, "gyro sensor")                                                 \
    X(OB_DEV_COMPONENT_ACCEL_SENSOR, "accel sensor")                                               \
    X(OB_DEV_COMPONENT_IMU_STREAMER, "imu sensor")                                                 \
    X(OB_DEV_COMPONENT_SENSOR_STREAM_STRATEGY, "sensor stream strategy")                           \
    X(OB_DEV_COMPONENT_STREAM_PROFILE_FILTER, "stream profile filter")                             \
    X(OB_DEV_COMPONENT_COLOR_FRAME_METADATA_CONTAINER, "color frame metadata container")           \
    X(OB_DEV_COMPONENT_DEPTH_FRAME_METADATA_CONTAINER, "depth frame metadata container")           \
    X(OB_DEV_COMPONENT_PRESET_MANAGER, "preset manager")                                           \
    X(OB_DEV_COMPONENT_ALG_PARAM_MANAGER, "alg param manager")                                     \
    X(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER, "depth work mode manager")                         \
    X(OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER, "global timestamp filter")                         \
    X(OB_DEV_COMPONENT_DEVICE_SYNC_CONFIGURATOR, "device sync configurator")                       \
    X(OB_DEV_COMPONENT_DEVICE_CLOCK_SYNCHRONIZER, "device clock synchronizer")                     \
    X(OB_DEV_COMPONENT_DEVICE_MONITOR, "device monitor")                                           \
    X(OB_DEV_COMPONENT_RAW_PHASE_STREAMER, "raw phase streamer")                                   \
    X(OB_DEV_COMPONENT_FIRMWARE_UPDATER, "firmware updater")                                       \
    X(OB_DEV_COMPONENT_FRAME_INTERLEAVE_MANAGER, "frame interleave manager")                       \
    X(OB_DEV_COMPONENT_FIRMWARE_UPDATE_GUARD_FACTORY, "firmware update guard factory")             \
    X(OB_DEV_COMPONENT_CONFIDENCE_SENSOR, "confidence sensor")                                     \
    X(OB_DEV_COMPONENT_DEVICE_ACTIVITY_RECORDER, "device activity recorder")                       \
    X(OB_DEV_COMPONENT_LIDAR_SENSOR, "lidar sensor")                                               \
    X(OB_DEV_COMPONENT_LIDAR_STREAMER, "lidar streamer")                                           \
    X(OB_DEV_COMPONENT_LEFT_COLOR_SENSOR, "left color sensor")                                     \
    X(OB_DEV_COMPONENT_LEFT_COLOR_FRAME_PROCESSOR, "left color frame processor")                   \
    X(OB_DEV_COMPONENT_LEFT_COLOR_FRAME_METADATA_CONTAINER, "left color frame metadata container") \
    X(OB_DEV_COMPONENT_RIGHT_COLOR_SENSOR, "right color sensor")                                   \
    X(OB_DEV_COMPONENT_RIGHT_COLOR_FRAME_PROCESSOR, "right color frame processor")                 \
    X(OB_DEV_COMPONENT_RIGHT_COLOR_FRAME_METADATA_CONTAINER, "right color frame metadata container")

// generate enum
typedef enum {
    // Unknown
    OB_DEV_COMPONENT_UNKNOWN = -1,

// Id list
#define GENERATE_ENUM(ENUM, STR) ENUM,
    FOREACH_DEV_COMPONENT_ID(GENERATE_ENUM)
#undef GENERATE_ENUM

    // Count: placeholder to get the total number of components
    OB_DEV_COMPONENT_COUNT
} DeviceComponentId;

// generate enum map
static const std::map<DeviceComponentId, std::string> DeviceComponentIdMap = {
#define GENERATE_MAP(ENUM, STR) { ENUM, STR },
    FOREACH_DEV_COMPONENT_ID(GENERATE_MAP)
#undef GENERATE_MAP
};

}  // namespace libobsensor
