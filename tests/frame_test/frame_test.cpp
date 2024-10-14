// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

extern "C" {
#include <libobsensor/h/Frame.h>
#include <libobsensor/h/Filter.h>
#include <libobsensor/h/Error.h>
}

#include <iostream>

void check_ob_error(ob_error **err) {
    if(*err) {
        std::cerr << "Error: " << ob_error_get_message(*err) << std::endl;
        ob_delete_error(*err);
        exit(-1);
    }
    *err = nullptr;
}

int main() {
    ob_error *err   = nullptr;
    auto      frame = ob_create_video_frame(OB_FRAME_DEPTH, OB_FORMAT_Y16, 640, 480, OB_DEFAULT_STRIDE_BYTES, &err);
    check_ob_error(&err);

    auto data = ob_frame_get_data(frame, &err);
    check_ob_error(&err);
    std::cout << "Data pointer: " << (uint64_t)data << std::endl;

    auto data_size = ob_frame_get_data_size(frame, &err);
    check_ob_error(&err);
    std::cout << "Data size: " << data_size << std::endl;

    auto filter = ob_create_filter("NoiseRemovalFilter", &err);
    check_ob_error(&err);

    auto config_schema = ob_filter_get_config_schema(filter, &err);
    check_ob_error(&err);
    std::cout << "NoiseRemovalFilter Config schema: \n" << config_schema << std::endl;

    ob_delete_filter(filter, &err);
    check_ob_error(&err);

    ob_delete_frame(frame, &err);
    check_ob_error(&err);
    return 0;
}
