// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "libobsensor/h/RecordPlayback.h"

#include "ImplTypes.hpp"
#include "record/RecordDevice.hpp"
#include "playback/PlaybackDevice.hpp"
#include "IDevice.hpp"

#include <memory>

#ifdef __cplusplus
extern "C" {
#endif

ob_record_device *ob_create_record_device(ob_device *device, const char *file_path, bool compression_enabled, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(file_path);
    auto recorder = std::make_shared<libobsensor::RecordDevice>(device->device, file_path, compression_enabled);

    auto impl      = new ob_record_device();
    impl->recorder = recorder;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device, file_path, compression_enabled)

void ob_delete_record_device(ob_record_device *recorder, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(recorder);
    delete recorder;
}
HANDLE_EXCEPTIONS_NO_RETURN(recorder)

void ob_record_device_pause(ob_record_device *recorder, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(recorder);
    recorder->recorder->pause();
}
HANDLE_EXCEPTIONS_NO_RETURN(recorder)

void ob_record_device_resume(ob_record_device *recorder, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(recorder);
    recorder->recorder->resume();
}
HANDLE_EXCEPTIONS_NO_RETURN(recorder)

ob_device *ob_create_playback_device(const char *file_path, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(file_path);
    auto device = std::make_shared<libobsensor::PlaybackDevice>(file_path);
    device->activateDeviceAccessor();
    device->fetchProperties();

    auto impl    = new ob_device();
    impl->device = device;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, file_path)

void ob_playback_device_pause(ob_device *player, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(player);
    auto playerPtr = std::dynamic_pointer_cast<libobsensor::PlaybackDevice>(player->device);
    playerPtr->pause();
}
HANDLE_EXCEPTIONS_NO_RETURN(player)

void ob_playback_device_resume(ob_device *player, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(player);
    auto playerPtr = std::dynamic_pointer_cast<libobsensor::PlaybackDevice>(player->device);
    playerPtr->resume();
}
HANDLE_EXCEPTIONS_NO_RETURN(player)

void ob_playback_device_seek(ob_device *player, const uint64_t timestamp, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(player);
    auto playerPtr = std::dynamic_pointer_cast<libobsensor::PlaybackDevice>(player->device);
    playerPtr->seek(timestamp);
}
HANDLE_EXCEPTIONS_NO_RETURN(player)

void ob_playback_device_set_playback_rate(ob_device *player, const float rate, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(player);
    VALIDATE_NOT_NULL(rate);
    auto playerPtr = std::dynamic_pointer_cast<libobsensor::PlaybackDevice>(player->device);
    playerPtr->setPlaybackRate(rate);
}
HANDLE_EXCEPTIONS_NO_RETURN(player, rate)

ob_playback_status ob_playback_device_get_current_playback_status(ob_device *player, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(player);
    auto playerPrt = std::dynamic_pointer_cast<libobsensor::PlaybackDevice>(player->device);
    return playerPrt->getCurrentPlaybackStatus();
}
HANDLE_EXCEPTIONS_AND_RETURN(OB_PLAYBACK_UNKNOWN, player)

void ob_playback_device_set_playback_status_changed_callback(ob_device *player, ob_playback_status_changed_callback callback, void *user_data,
                                                             ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(player);
    VALIDATE_NOT_NULL(callback);
    auto playerPrt = std::dynamic_pointer_cast<libobsensor::PlaybackDevice>(player->device);
    playerPrt->setPlaybackStatusCallback([callback, user_data](ob_playback_status status) { callback(status, user_data); });
}
HANDLE_EXCEPTIONS_NO_RETURN(player, callback)

uint64_t ob_playback_device_get_position(ob_device *player, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(player);
    auto playerPtr = std::dynamic_pointer_cast<libobsensor::PlaybackDevice>(player->device);
    return playerPtr->getPosition();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, player)

uint64_t ob_playback_device_get_duration(ob_device *player, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(player);
    auto playerPtr = std::dynamic_pointer_cast<libobsensor::PlaybackDevice>(player->device);
    return playerPtr->getDuration();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, player)

#ifdef __cplusplus
}  // extern "C"
#endif