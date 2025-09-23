// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "utils_c.h"
#include "utils_types.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846 /* pi */
#endif

// Select sensors and enable streams
void select_sensors_and_streams(ob_device *device, ob_config *config);
// Print LiDAR point cloud frame information
void print_lidar_point_cloud_info(ob_frame *point_cloud_frame);
// Frame callback function
void frame_callback(ob_frame *frameset, void *user_data);
// Print IMU value
void print_imu_value(const ob_float_3d *value, uint64_t index, uint64_t timestamp, float temperature, ob_frame_type type, const char *unit);

// Select index from user input
int select_index(const char *prompt, int min_value, int max_value) {
    int value = 0;
    printf("\n%s (Input index or \'q\' to exit): ", prompt);
    while(true) {
        char input;
        int  ret = scanf("%c", &input);
        (void)ret;
        getchar();

        if(input == 'q' || input == 'Q') {
            value = -1;
            break;
        }
        if(input >= '0' && input <= '9' && input - '0' >= min_value && input - '0' <= max_value) {
            value = input - '0';
            break;
        }
        printf("Invalid input, please input a number between %d and %d or \'q\' to exit: ", min_value, max_value);
    }
    return value;
}

int main(void) {

    // Used to return SDK interface error information.
    ob_error *error = NULL;

    // Create a pipeline to manage the streams
    ob_pipeline *pipe = ob_create_pipeline(&error);
    CHECK_OB_ERROR_EXIT(&error);

    // Get the device from the pipeline
    ob_device *device = ob_pipeline_get_device(pipe, &error);
    CHECK_OB_ERROR_EXIT(&error);

    // Get properties of the device
    ob_device_info *device_info = ob_device_get_device_info(device, &error);
    CHECK_OB_ERROR_EXIT(&error);
    printf("\n------------------------------------------------------------------------\n");
    printf("Current Device: name: %s, vid: 0x%04x, pid: 0x%04x, uid: 0x%s\n", ob_device_info_get_name(device_info, &error),
           ob_device_info_get_vid(device_info, &error), ob_device_info_get_pid(device_info, &error), ob_device_info_get_uid(device_info, &error));
    CHECK_OB_ERROR_EXIT(&error);
    //  Delete device info
    ob_delete_device_info(device_info, &error);
    CHECK_OB_ERROR_EXIT(&error);

    // Get serial number
    uint32_t data_size = 64;
    uint8_t  data[64]  = { 0 };
    ob_device_get_structured_data(device, OB_RAW_DATA_LIDAR_SERIAL_NUMBER, data, &data_size, &error);
    CHECK_OB_ERROR_EXIT(&error);
    printf("LiDAR SN: %s\n\n", (const char *)data);

    // Set property: OB_PROP_LIDAR_TAIL_FILTER_LEVEL_INT to 0
    ob_device_set_int_property(device, OB_PROP_LIDAR_TAIL_FILTER_LEVEL_INT, 0, &error);
    CHECK_OB_ERROR_EXIT(&error);

    // Create a configuration for the pipeline
    ob_config *config = ob_create_config(&error);
    CHECK_OB_ERROR_EXIT(&error);

    // Set frame aggregate output mode to require all types of frames
    ob_config_set_frame_aggregate_output_mode(config, OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE, &error);
    CHECK_OB_ERROR_EXIT(&error);

    // Disable all streams
    ob_config_disable_all_stream(config, &error);
    CHECK_OB_ERROR_EXIT(&error);

    // Select sensors and enable streams
    select_sensors_and_streams(device, config);
    ob_pipeline_start_with_callback(pipe, config, frame_callback, (void *)NULL, &error);
    CHECK_OB_ERROR_EXIT(&error);

    // Print instructions
    printf("Streams have been started!\n");
    printf("Press ESC to exit!\n");

    while(true) {
        // Wait for a key press
        char key = ob_smpl_wait_for_key_press(10);
        if(key == ESC_KEY)
            break;
    }

    // Stop the pipeline
    ob_pipeline_stop(pipe, &error);
    CHECK_OB_ERROR_EXIT(&error);

    // Delete the device
    ob_delete_config(config, &error);
    CHECK_OB_ERROR_EXIT(&error);

    // Delete the pipeline
    ob_delete_pipeline(pipe, &error);
    CHECK_OB_ERROR_EXIT(&error);

    return 0;
}

// Frame count
uint32_t frame_count = 0;
// Frame callback function
void frame_callback(ob_frame *frameset, void *user_data) {
    (void)user_data;
    ob_error *error = NULL;

    // If frameset is NULL, return directly.
    if(!frameset) {
        printf("Frame is NULL\n");
        return;
    }

    // Print frame information every 50 frames
    if(frame_count % 50 == 0) {
        // Get the number of frames in the frameset
        uint32_t count = ob_frameset_get_count(frameset, &error);
        CHECK_OB_ERROR_EXIT(&error);

        for(uint32_t i = 0; i < count; i++) {
            // Get each frame in the frameset
            ob_frame *frame = ob_frameset_get_frame_by_index(frameset, i, &error);
            CHECK_OB_ERROR_EXIT(&error);

            // check frame type
            ob_frame_type frame_type = ob_frame_get_type(frame, &error);
            CHECK_OB_ERROR_EXIT(&error);

            switch(frame_type) {
            case OB_FRAME_LIDAR_POINTS:
                // Handle LiDAR point cloud frame
                print_lidar_point_cloud_info(frame);
                break;
            case OB_FRAME_ACCEL: {
                // Handle accelerometer frame
                const ob_float_3d value = ob_accel_frame_get_value(frame, &error);
                CHECK_OB_ERROR_EXIT(&error);

                uint64_t index = ob_frame_get_index(frame, &error);
                CHECK_OB_ERROR_EXIT(&error);

                uint64_t timestamp = ob_frame_get_timestamp_us(frame, &error);
                CHECK_OB_ERROR_EXIT(&error);

                float temperature = ob_accel_frame_get_temperature(frame, &error);
                CHECK_OB_ERROR_EXIT(&error);

                print_imu_value(&value, index, timestamp, temperature, frame_type, "m/s^2");

                break;
            }
            case OB_FRAME_GYRO: {
                // Handle gyroscope frame
                const ob_float_3d value = ob_gyro_frame_get_value(frame, &error);
                CHECK_OB_ERROR_EXIT(&error);

                uint64_t index = ob_frame_get_index(frame, &error);
                CHECK_OB_ERROR_EXIT(&error);

                uint64_t timestamp = ob_frame_get_timestamp_us(frame, &error);
                CHECK_OB_ERROR_EXIT(&error);

                float temperature = ob_gyro_frame_get_temperature(frame, &error);
                CHECK_OB_ERROR_EXIT(&error);

                print_imu_value(&value, index, timestamp, temperature, frame_type, "rad/s");
                break;
            }
            default:
                break;
            }

            ob_delete_frame(frame, &error);
            CHECK_OB_ERROR_EXIT(&error);
        }
    }

    frame_count++;
    ob_delete_frame(frameset, &error);
    CHECK_OB_ERROR_EXIT(&error);
}

// Select sensors and enable streams
void print_imu_value(const ob_float_3d *value, uint64_t index, uint64_t timestamp, float temperature, ob_frame_type type, const char *unit) {
    if(value == NULL) {
        printf("IMU value is NULL\n");
        return;
    }

    printf("%s Frame: \n{\n", ob_frame_type_to_string(type));
#ifdef _WIN32
    printf("  frame index = %llu\n", index);
    printf("  tsp = %llu\n", timestamp);
#else
    printf("  frame index = %lu\n", index);
    printf("  tsp = %lu\n", timestamp);
#endif

    printf("  temperature = %.2f C\n", temperature);
    printf("  x = %.6f %s\n", value->x, unit);
    printf("  y = %.6f %s\n", value->y, unit);
    printf("  z = %.6f %s\n", value->z, unit);
    printf("}\n\n");
}

// Print LiDAR point cloud frame information
void print_lidar_point_cloud_info(ob_frame *point_cloud_frame) {
    if(point_cloud_frame == NULL) {
        printf("LiDAR point cloud frame is NULL\n");
        return;
    }

    ob_error *error = NULL;

    ob_format point_cloud_type = ob_frame_get_format(point_cloud_frame, &error);
    CHECK_OB_ERROR_EXIT(&error);

    // Check if the point cloud format is valid
    if(point_cloud_type != OB_FORMAT_LIDAR_SPHERE_POINT && point_cloud_type != OB_FORMAT_LIDAR_POINT && point_cloud_type != OB_FORMAT_LIDAR_SCAN) {
        printf("LiDAR point cloud format invalid\n");
        return;
    }

    const float min_point_value   = 1e-6f;
    uint32_t    valid_point_count = 0;

    if(point_cloud_type == OB_FORMAT_LIDAR_SPHERE_POINT) {
        // Convert spherical coordinates to Cartesian coordinates and count valid points
        const ob_lidar_sphere_point *points = (const ob_lidar_sphere_point *)ob_frame_data(point_cloud_frame, &error);
        CHECK_OB_ERROR_EXIT(&error);

        uint32_t point_count = (uint32_t)(ob_frame_data_size(point_cloud_frame, &error) / sizeof(ob_lidar_sphere_point));
        CHECK_OB_ERROR_EXIT(&error);

        for(uint32_t i = 0; i < point_count; ++i) {
            double theta_rad = points[i].theta * M_PI / 180.0f;
            double phi_rad   = points[i].phi * M_PI / 180.0f;
            float  distance  = points[i].distance;

            if(distance < min_point_value)
                continue;

            // Spherical to Cartesian conversion
            float x = (float)(distance * cos(theta_rad) * cos(phi_rad));
            float y = (float)(distance * sin(theta_rad) * cos(phi_rad));
            float z = (float)(distance * sin(phi_rad));

            if(isfinite(x) && isfinite(y) && isfinite(z))
                valid_point_count++;
        }
    }
    else if(point_cloud_type == OB_FORMAT_LIDAR_POINT) {
        // Count valid points
        const ob_lidar_point *points = (const ob_lidar_point *)ob_frame_data(point_cloud_frame, &error);
        CHECK_OB_ERROR_EXIT(&error);

        uint32_t point_count = (uint32_t)(ob_frame_data_size(point_cloud_frame, &error) / sizeof(ob_lidar_point));
        CHECK_OB_ERROR_EXIT(&error);

        for(uint32_t i = 0; i < point_count; ++i) {
            float x = points[i].x;
            float y = points[i].y;
            float z = points[i].z;

            if(fabs(z) >= min_point_value && isfinite(x) && isfinite(y) && isfinite(z))
                valid_point_count++;
        }
    }
    else if(point_cloud_type == OB_FORMAT_LIDAR_SCAN) {
        // Convert polar coordinates to Cartesian coordinates and count valid points
        const ob_lidar_scan_point *points = (const ob_lidar_scan_point *)ob_frame_data(point_cloud_frame, &error);
        CHECK_OB_ERROR_EXIT(&error);

        uint32_t point_count = (uint32_t)(ob_frame_data_size(point_cloud_frame, &error) / sizeof(ob_lidar_scan_point));
        CHECK_OB_ERROR_EXIT(&error);

        for(uint32_t i = 0; i < point_count; ++i) {
            double angle_rad = points[i].angle * M_PI / 180.0f;
            float  distance  = points[i].distance;
            if(distance < min_point_value)
                continue;

            // Polar to Cartesian conversion
            float x = (float)(distance * cos(angle_rad));
            float y = (float)(distance * sin(angle_rad));
            if(isfinite(x) && isfinite(y))
                valid_point_count++;
        }
    }
    else {
        printf("Invalid LiDAR point cloud format\n");
        return;
    }

    if(valid_point_count == 0) {
        printf("LiDAR point cloud vertices is zero\n");
        return;
    }

    CHECK_OB_ERROR_EXIT(&error);

    printf("LiDAR PointCloud Frame: \n{\n");
#ifdef _WIN32
    printf("  frame index: %llu\n", ob_frame_get_index(point_cloud_frame, &error));
    printf("  tsp = %llu\n", ob_frame_get_timestamp_us(point_cloud_frame, &error));
#else
    printf("  frame index: %lu\n", ob_frame_get_index(point_cloud_frame, &error));
    printf("  tsp = %lu\n", ob_frame_get_timestamp_us(point_cloud_frame, &error));
#endif
    CHECK_OB_ERROR_EXIT(&error);

    printf("  format = %d\n", point_cloud_type);
    printf("  valid point count = %u\n", valid_point_count);
    printf("}\n\n");
}

// Select sensors and enable streams
void select_sensors_and_streams(ob_device *device, ob_config *config) {

    // error handling
    ob_error *error = NULL;

    // Get sensor list
    ob_sensor_list *sensor_list = ob_device_get_sensor_list(device, &error);
    CHECK_OB_ERROR_EXIT(&error);

    // Get sensor count
    uint32_t sensor_count = ob_sensor_list_get_count(sensor_list, &error);
    CHECK_OB_ERROR_EXIT(&error);

    printf("Sensor list:\n");
    for(uint32_t i = 0; i < sensor_count; i++) {
        ob_sensor_type type = ob_sensor_list_get_sensor_type(sensor_list, i, &error);
        CHECK_OB_ERROR_EXIT(&error);
        printf(" - %u. sensor type: %s\n", i, ob_sensor_type_to_string(type));
    }

    printf(" - %u. all sensors\n", sensor_count);
    // Select a sensor
    int sensor_selected = select_index("Select a sensor to enable", 0, sensor_count);
    if(sensor_selected == -1) {
        ob_delete_sensor_list(sensor_list, &error);
        CHECK_OB_ERROR_EXIT(&error);
        return;
    }

    // Check if the selected sensor index is valid
    for(uint32_t i = 0; i < sensor_count; i++) {

        if(sensor_selected != (int)sensor_count && sensor_selected != (int)i) {
            continue;
        }

        // Get sensor from sensor list
        ob_sensor *sensor = ob_sensor_list_get_sensor(sensor_list, i, &error);
        CHECK_OB_ERROR_EXIT(&error);
        // Get sensor type from sensor
        ob_sensor_type type = ob_sensor_get_type(sensor, &error);
        CHECK_OB_ERROR_EXIT(&error);

        // Get stream profile list from sensor
        ob_stream_profile_list *profile_list = ob_sensor_get_stream_profile_list(sensor, &error);
        CHECK_OB_ERROR_EXIT(&error);

        // Get stream profile count from stream profile list
        uint32_t profile_count = ob_stream_profile_list_get_count(profile_list, &error);
        CHECK_OB_ERROR_EXIT(&error);

        printf("Stream profile list for sensor %s:\n", ob_sensor_type_to_string(type));
        for(uint32_t j = 0; j < profile_count; j++) {
            // Get stream profile from profile list
            ob_stream_profile *profile = ob_stream_profile_list_get_profile(profile_list, j, &error);
            CHECK_OB_ERROR_EXIT(&error);

            // Get stream profile information
            ob_format format = ob_stream_profile_get_format(profile, &error);
            CHECK_OB_ERROR_EXIT(&error);

            if(type == OB_SENSOR_LIDAR) {
                // Get LiDAR scan rate from stream profile
                ob_lidar_scan_rate scan_rate = ob_lidar_stream_profile_get_scan_rate(profile, &error);
                CHECK_OB_ERROR_EXIT(&error);
                printf(" - %u. format: %s,  scan rate: %s\n", j, ob_format_to_string(format), ob_lidar_scan_rate_type_to_string(scan_rate));
            }
            else if(type == OB_SENSOR_ACCEL) {
                // Get accelerometer sample rate from stream profile
                ob_accel_sample_rate fps = ob_accel_stream_profile_get_sample_rate(profile, &error);
                CHECK_OB_ERROR_EXIT(&error);

                printf(" - %u. format: %s,  fps: %s\n", j, ob_format_to_string(format), ob_imu_rate_type_to_string(fps));
            }
            else if(type == OB_SENSOR_GYRO) {
                // Get gyro sample rate from stream profile
                ob_gyro_sample_rate gyro_fps = ob_gyro_stream_profile_get_sample_rate(profile, &error);
                CHECK_OB_ERROR_EXIT(&error);

                printf(" - %u. format: %s,  fps: %s\n", j, ob_format_to_string(format), ob_imu_rate_type_to_string(gyro_fps));
            }
            else {
                // Unsupported sensor type
                printf(" - %u. unsupported sensor type\n", j);
            }

            // Delete stream profile
            ob_delete_stream_profile(profile, &error);
            CHECK_OB_ERROR_EXIT(&error);
        }

        // Select a stream profile
        int profile_selected = select_index("Select a stream profile to enable", 0, profile_count - 1);

        // Check if the selected stream profile index is valid
        if(profile_selected >= 0 && profile_selected < (int)profile_count) {
            // Get the selected stream profile
            ob_stream_profile *selected_profile = ob_stream_profile_list_get_profile(profile_list, profile_selected, &error);
            CHECK_OB_ERROR_EXIT(&error);

            // Enable the selected stream profile
            ob_config_enable_stream_with_stream_profile(config, selected_profile, &error);
            CHECK_OB_ERROR_EXIT(&error);

            // Delete the selected stream profile
            ob_delete_stream_profile(selected_profile, &error);
            CHECK_OB_ERROR_EXIT(&error);
        }

        // Delete stream profile list
        ob_delete_stream_profile_list(profile_list, &error);
        CHECK_OB_ERROR_EXIT(&error);
        // Delete sensor
        ob_delete_sensor(sensor, &error);
        CHECK_OB_ERROR_EXIT(&error);
    }

    // Delete sensor list
    ob_delete_sensor_list(sensor_list, &error);
    CHECK_OB_ERROR_EXIT(&error);
}