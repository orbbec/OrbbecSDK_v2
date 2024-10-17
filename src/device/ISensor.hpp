// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "libobsensor/h/ObTypes.h"
#include "IStreamProfile.hpp"
#include "IFrame.hpp"
#include "ISourcePort.hpp"
#include "IDeviceComponent.hpp"
#include <memory>

namespace libobsensor {
class IDevice;

typedef enum {
    STREAM_STATE_STARTING,      // starting (change to this state at the beginning of start function)
    STREAM_STATE_STREAMING,     // streaming (change to this state when recived first frame from backend)
    STREAM_STATE_STOPPING,      // stopping (change to this state at the beginning of stop function)
    STREAM_STATE_STOPPED,       // stoped (change to this state after stop function called, it is also the initial state)
    STREAM_STATE_ERROR,         // error (change to this state when error occurred after start stream)
    STREAM_STATE_START_FAILED,  // start stream failed
} OBStreamState;

#define STREAM_STATE_STR(state)                                  \
    (state == STREAM_STATE_STARTING       ? "STARTING"           \
     : state == STREAM_STATE_STREAMING    ? "STREAMING"          \
     : state == STREAM_STATE_STOPPING     ? "STOPPING"           \
     : state == STREAM_STATE_STOPPED      ? "STOPPED"            \
     : state == STREAM_STATE_ERROR        ? "STREAM_STATE_ERROR" \
     : state == STREAM_STATE_START_FAILED ? "START_FAILED"       \
                                          : "UNKNOWN")

typedef std::function<void(OBStreamState, const std::shared_ptr<const StreamProfile> &)> StreamStateChangedCallback;

class ISensor : virtual public IDeviceComponent {
public:
    virtual ~ISensor() noexcept = default;

    virtual OBSensorType getSensorType() const = 0;

    virtual void start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) = 0;
    virtual void stop()                                                                 = 0;

    virtual void                                 setStreamProfileList(const StreamProfileList &profileList)                 = 0;
    virtual StreamProfileList                    getStreamProfileList() const                                               = 0;
    virtual void                                 setStreamProfileFilter(std::shared_ptr<IStreamProfileFilter> filter)       = 0;
    virtual void                                 updateDefaultStreamProfile(const std::shared_ptr<const StreamProfile> &sp) = 0;
    virtual std::shared_ptr<const StreamProfile> getActivatedStreamProfile() const                                          = 0;
    virtual FrameCallback                        getFrameCallback() const                                                   = 0;

    virtual OBStreamState getStreamState() const                                                  = 0;
    virtual bool          isStreamActivated() const                                               = 0;
    virtual uint32_t      registerStreamStateChangedCallback(StreamStateChangedCallback callback) = 0;
    virtual void          unregisterStreamStateChangedCallback(uint32_t token)                    = 0;
};

struct LazySensor {
    explicit LazySensor(IDevice *device, OBSensorType type) : device(device), sensorType(type) {}
    IDevice     *device;  // sensor is lazy create base on device
    OBSensorType sensorType;
};

}  // namespace libobsensor

#ifdef __cplusplus
extern "C" {
#endif

struct ob_sensor_t {
    std::shared_ptr<libobsensor::IDevice> device;  // sensor is lazy create base on device
    OBSensorType                          type;
};

struct ob_sensor_list_t {
    std::shared_ptr<libobsensor::IDevice> device;
    std::vector<OBSensorType>             sensorTypes;
};

#ifdef __cplusplus
}
#endif

