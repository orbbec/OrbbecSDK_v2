// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "libobsensor/h/Sensor.h"

#include "ImplTypes.hpp"
#include "exception/ObException.hpp"

#include "ISensor.hpp"

#ifdef __cplusplus
extern "C" {
#endif

ob_sensor_type ob_sensor_get_type(const ob_sensor *sensor, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(sensor);
    return sensor->type;
}
HANDLE_EXCEPTIONS_AND_RETURN(OB_SENSOR_UNKNOWN, sensor)

ob_stream_profile_list *ob_sensor_get_stream_profile_list(const ob_sensor *sensor, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(sensor);
    auto internalSensor = sensor->device->getSensor(sensor->type);
    auto spList         = internalSensor->getStreamProfileList();

    auto impl         = new ob_stream_profile_list();
    impl->profileList = spList;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, sensor)

void ob_sensor_start(ob_sensor *sensor, const ob_stream_profile *profile, ob_frame_callback callback, void *user_data, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(sensor);
    VALIDATE_NOT_NULL(profile);
    VALIDATE_NOT_NULL(callback);
    auto internalSensor = sensor->device->getSensor(sensor->type);
    internalSensor->start(profile->profile, [callback, user_data](std::shared_ptr<const libobsensor::Frame> frame) {
        auto implFrame   = new ob_frame();
        implFrame->frame = std::const_pointer_cast<libobsensor::Frame>(frame);  // todo: this is a hack，need to fix
        callback(implFrame, user_data);
    });
}
HANDLE_EXCEPTIONS_NO_RETURN(sensor, profile, callback, user_data)

void ob_sensor_stop(ob_sensor *sensor, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(sensor);
    auto internalSensor = sensor->device->getSensor(sensor->type);
    internalSensor->stop();
}
HANDLE_EXCEPTIONS_NO_RETURN(sensor)

void ob_sensor_switch_profile(ob_sensor *sensor, ob_stream_profile *profile, ob_error **error) BEGIN_API_CALL {
    auto innerSensor = sensor->device->getSensor(sensor->type);
    if(innerSensor->isStreamActivated()) {
        throw libobsensor::invalid_value_exception("sensor stream is not started!");
    }
    innerSensor->stop();
    innerSensor->start(profile->profile, innerSensor->getFrameCallback());
}
HANDLE_EXCEPTIONS_NO_RETURN(sensor)

ob_filter_list *ob_sensor_create_recommended_filter_list(const ob_sensor *sensor, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(sensor);
    auto filterList  = sensor->device->createRecommendedPostProcessingFilters(sensor->type);
    auto impl        = new ob_filter_list();
    impl->filterList = filterList;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, sensor)

void ob_delete_sensor_list(ob_sensor_list *sensor_list, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(sensor_list);
    delete sensor_list;
}
HANDLE_EXCEPTIONS_NO_RETURN(sensor_list)

uint32_t ob_sensor_list_get_count(const ob_sensor_list *sensor_list, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(sensor_list);
    return static_cast<uint32_t>(sensor_list->sensorTypes.size());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, sensor_list)

ob_sensor_type ob_sensor_list_get_sensor_type(const ob_sensor_list *sensor_list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(sensor_list);
    VALIDATE_UNSIGNED_INDEX(index, sensor_list->sensorTypes.size());
    return sensor_list->sensorTypes[index];
}
HANDLE_EXCEPTIONS_AND_RETURN(OB_SENSOR_UNKNOWN, sensor_list, index)

ob_sensor *ob_sensor_list_get_sensor_by_type(const ob_sensor_list *sensor_list, ob_sensor_type sensorType, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(sensor_list);
    for(auto &item: sensor_list->sensorTypes) {
        if(item == sensorType) {
            auto implSensor    = new ob_sensor();
            implSensor->device = sensor_list->device;
            implSensor->type   = item;
            return implSensor;
        }
    }
    throw libobsensor::invalid_value_exception("Sensor type not found in sensor list");
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, sensor_list, sensorType)

ob_sensor *ob_sensor_list_get_sensor(const ob_sensor_list *sensor_list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(sensor_list);
    VALIDATE_UNSIGNED_INDEX(index, sensor_list->sensorTypes.size());
    auto implSensor    = new ob_sensor();
    implSensor->device = sensor_list->device;
    implSensor->type   = sensor_list->sensorTypes[index];
    return implSensor;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, sensor_list, index)

void ob_delete_sensor(ob_sensor *sensor, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(sensor);
    delete sensor;
}
HANDLE_EXCEPTIONS_NO_RETURN(sensor)

#ifdef __cplusplus
}
#endif
