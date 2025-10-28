// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.h>
#include <libobsensor/h/Utils.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils_c.h"
#include "utils_types.h"

int main(void) {

    // Used to return SDK interface error information.
    ob_error *error = NULL;

    // Create a pipeline to manage the streams
    ob_pipeline *pipe = ob_create_pipeline(&error);
    CHECK_OB_ERROR_EXIT(&error);

    // Get the device from the pipeline
    ob_device *device = ob_pipeline_get_device(pipe, &error);
    CHECK_OB_ERROR_EXIT(&error);

    // Check LiDAR device
    if(!ob_smpl_is_lidar_device(device)) {
        printf("Invalid device, please connect a LiDAR device!\n");
        return -1;
    }

    // Start Pipeline with default configuration
    // Modify the default configuration by the configuration file: "OrbbecSDKConfig.xml"
    ob_pipeline_start(pipe, &error);
    CHECK_OB_ERROR_EXIT(&error);

    // Print instructions
    printf("LiDAR stream is started!\n");
    printf("Press R or r to create LiDAR PointCloud and save to ply file!\n");
    printf("Press ESC to exit!\n");

    // Main loop, continuously wait for frames and print their index and rate.
    while(true) {
        char key = ob_smpl_wait_for_key_press(10);
        if(key == ESC_KEY) {
            break;
        }

        if(key == 'r' || key == 'R') {
            printf("Save LiDAR PointCloud to ply file, this will take some time...\n");
            // Wait for frameset from pipeline, with a timeout of 1000 milliseconds.
            ob_frame *frameset = ob_pipeline_wait_for_frameset(pipe, 1000, &error);
            CHECK_OB_ERROR_EXIT(&error);

            // If frameset is NULL, timeout occurred, continue to next iteration.
            if(frameset == NULL) {
                continue;
            }

            ob_frame *frame = ob_frameset_get_frame(frameset, OB_FRAME_LIDAR_POINTS, &error);
            CHECK_OB_ERROR_EXIT(&error);

            // Save point cloud data to ply file
            bool result = ob_save_lidar_pointcloud_to_ply("LiDARPoints.ply", frame, false, &error);
            CHECK_OB_ERROR_EXIT(&error);
            if(!result) {
                printf("Failed to save LiDARPoints.ply\n");
                ob_delete_frame(frame, &error);
                CHECK_OB_ERROR_EXIT(&error);
                continue;
            }

            printf("LiDARPoints.ply Saved\n");

            // Delete the frameset
            ob_delete_frame(frameset, &error);
            CHECK_OB_ERROR_EXIT(&error);
        }
    }

    // Stop Pipeline
    ob_pipeline_stop(pipe, &error);
    CHECK_OB_ERROR_EXIT(&error);

    // Delete pipeline
    ob_delete_pipeline(pipe, &error);
    CHECK_OB_ERROR_EXIT(&error);

    return 0;
}
