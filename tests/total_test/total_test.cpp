// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <stdio.h>
#include <stdlib.h>

#include <libobsensor/h/Error.h>
#include <libobsensor/h/Context.h>
#include <libobsensor/h/Device.h>
#include <libobsensor/h/Sensor.h>
#include <libobsensor/h/Frame.h>
#include <libobsensor/h/StreamProfile.h>

#include <windows.h>

// helper function to check for errors and exit if there is one
void check_ob_error(ob_error **err) {
    if(*err) {
        const char *error_message = ob_error_get_message(*err);
        fprintf(stderr, "Error: %s\n", error_message);
        ob_delete_error(*err);
        exit(-1);
    }
    *err = NULL;
}

void printf_device_list(ob_device_list *device_list) {
    ob_error *err = NULL;

    uint32_t count = ob_device_list_get_count(device_list, &err);
    check_ob_error(&err);

    for(uint32_t i = 0; i < count; i++) {
        const char *name = ob_device_list_get_device_name(device_list, i, &err);
        check_ob_error(&err);

        const char *serial_number = ob_device_list_get_device_serial_number(device_list, i, &err);
        check_ob_error(&err);

        int pid = ob_device_list_get_device_pid(device_list, i, &err);
        check_ob_error(&err);

        int vid = ob_device_list_get_device_vid(device_list, i, &err);
        check_ob_error(&err);

        const char *uid = ob_device_list_get_device_uid(device_list, i, &err);
        check_ob_error(&err);

        const char *connection_type = ob_device_list_get_device_connection_type(device_list, i, &err);
        check_ob_error(&err);

        printf("device#%d:\n \tname: %s\n \tserial_number: %s\n \tpid: %d\n \tvid: %d\n \tuid: %s\n \tconnection_type: %s\n", i, name, serial_number, pid, vid,
               uid, connection_type);
    }
}

void printf_device_info(ob_device_info *device_info) {
    ob_error *err = NULL;

    const char *name = ob_device_info_get_name(device_info, &err);
    check_ob_error(&err);

    const char *serial_number = ob_device_info_get_serial_number(device_info, &err);
    check_ob_error(&err);

    int pid = ob_device_info_get_pid(device_info, &err);
    check_ob_error(&err);

    int vid = ob_device_info_get_vid(device_info, &err);
    check_ob_error(&err);

    const char *uid = ob_device_info_get_uid(device_info, &err);
    check_ob_error(&err);

    const char *connection_type = ob_device_info_get_connection_type(device_info, &err);
    check_ob_error(&err);

    const char *fw_version = ob_device_info_get_firmware_version(device_info, &err);
    check_ob_error(&err);

    const char *hw_version = ob_device_info_get_hardware_version(device_info, &err);
    check_ob_error(&err);

    const char *min_sdk_ver = ob_device_info_get_supported_min_sdk_version(device_info, &err);
    check_ob_error(&err);

    printf("device:\n \tname: %s\n \tserial_number: %s\n \tpid: %d\n \tvid: %d\n \tuid: %s\n \tconnection_type: %s\n \tfw_version: %s\n \thw_version: %s\n "
           "\tmin_sdk_ver: %s\n",
           name, serial_number, pid, vid, uid, connection_type, fw_version, hw_version, min_sdk_ver);
}

void printf_sensor_list(ob_sensor_list *sensor_list) {
    ob_error *err = NULL;

    uint32_t count = ob_sensor_list_get_count(sensor_list, &err);
    check_ob_error(&err);

    printf("sensor_list:\n");
    for(uint32_t i = 0; i < count; i++) {
        ob_sensor_type type = ob_sensor_list_get_sensor_type(sensor_list, i, &err);
        check_ob_error(&err);
        printf("\tsensor#%d: type: %d\n", i, type);
    }
}

void printf_stream_profile_list(const ob_stream_profile_list *stream_profile_list) {
    ob_error *err = NULL;

    uint32_t count = ob_stream_profile_list_get_count(stream_profile_list, &err);
    check_ob_error(&err);

    printf("stream_profile_list:\n");
    for(uint32_t i = 0; i < count; i++) {
        const ob_stream_profile *stream_profile = ob_stream_profile_list_get_profile(stream_profile_list, i, &err);
        check_ob_error(&err);

        uint32_t width = ob_video_stream_profile_get_width(stream_profile, &err);
        check_ob_error(&err);

        uint32_t height = ob_video_stream_profile_get_height(stream_profile, &err);
        check_ob_error(&err);

        uint32_t fps = ob_video_stream_profile_get_fps(stream_profile, &err);
        check_ob_error(&err);

        ob_format format = ob_stream_profile_get_format(stream_profile, &err);
        check_ob_error(&err);

        ob_delete_stream_profile(stream_profile, &err);
        check_ob_error(&err);

        printf("\tstream_profile#%d: width: %d, height: %d, fps: %d, format: %d\n", i, width, height, fps, format);
    }
}

void sensor_frame_callback(ob_frame *frame, void *user_data) {
    ob_error *err   = NULL;
    uint64_t  index = ob_frame_get_index(frame, &err);
    check_ob_error(&err);

    uint32_t width = ob_video_frame_get_width(frame, &err);
    check_ob_error(&err);

    uint32_t height = ob_video_frame_get_height(frame, &err);
    check_ob_error(&err);

    ob_format format = ob_frame_get_format(frame, &err);
    check_ob_error(&err);

    uint32_t size = ob_frame_get_data_size(frame, &err);
    check_ob_error(&err);

    const uint8_t *data = ob_frame_get_data(frame, &err);
    check_ob_error(&err);

    ob_delete_frame(frame, &err);
    check_ob_error(&err);

    char *str = (char *)user_data;
    printf("%s: frame#%lld: width: %d, height: %d, format: %d, size: %d, dataPtr: %p\n", str, index, width, height, format, size, data);
}

int main() {
    // define the error pointer to handle errors
    ob_error *err = NULL;

    // set the logger to console with debug severity
    ob_set_logger_to_console(OB_LOG_SEVERITY_DEBUG, &err);
    check_ob_error(&err);

    // create a context
    ob_context *ctx = ob_create_context(&err);
    check_ob_error(&err);

    ob_device_list *device_list = ob_query_device_list(ctx, &err);
    check_ob_error(&err);

    printf_device_list(device_list);

    ob_device *device = ob_device_list_get_device(device_list, 0, &err);
    check_ob_error(&err);

    ob_device_info *device_info = ob_device_get_device_info(device, &err);
    check_ob_error(&err);

    printf_device_info(device_info);

    ob_sensor_list *sensor_list = ob_device_get_sensor_list(device, &err);
    check_ob_error(&err);

    printf_sensor_list(sensor_list);

    ob_sensor *sensor = ob_sensor_list_get_sensor_by_type(sensor_list, OB_SENSOR_COLOR, &err);
    check_ob_error(&err);

    const ob_stream_profile_list *stream_profile_list = ob_sensor_get_stream_profile_list(sensor, &err);
    check_ob_error(&err);

    printf_stream_profile_list(stream_profile_list);

    const ob_stream_profile *stream_profile = ob_stream_profile_list_get_profile(stream_profile_list, 0, &err);
    check_ob_error(&err);

    ob_sensor_start(sensor, stream_profile, sensor_frame_callback, "hello, world", &err);
    check_ob_error(&err);

    // sleep for 5 seconds
    Sleep(5000);

    ob_sensor_stop(sensor, &err);
    check_ob_error(&err);

    ob_delete_stream_profile_list(stream_profile_list, &err);
    check_ob_error(&err);

    ob_delete_sensor_list(sensor_list, &err);
    check_ob_error(&err);

    ob_delete_device_info(device_info, &err);
    check_ob_error(&err);

    ob_delete_device_list(device_list, &err);
    check_ob_error(&err);

    // delete the context
    ob_delete_context(ctx, &err);
    check_ob_error(&err);
}
