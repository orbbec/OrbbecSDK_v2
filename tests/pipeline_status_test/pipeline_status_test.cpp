// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

extern "C" {
#include <libobsensor/h/Context.h>
#include <libobsensor/h/Device.h>
#include <libobsensor/h/Error.h>
#include <libobsensor/h/Frame.h>
#include <libobsensor/h/Pipeline.h>
}

namespace {

std::atomic<int> g_statusCallbackCount{ 0 };

bool checkObError(ob_error **error, const char *step) {
    if(error && *error) {
        std::fprintf(stderr, "[FAIL] %s: %s\n", step, ob_error_get_message(*error));
        ob_delete_error(*error);
        *error = nullptr;
        return false;
    }
    return true;
}

void printIssueReadable(OBPipelineIssue issue) {
    std::printf("    issueReadable=");
    if(issue == OB_PIPELINE_ISSUE_NONE) {
        std::printf("NONE\n");
        return;
    }

    bool first = true;
    if(issue & OB_PIPELINE_ISSUE_SDK) {
        std::printf("%sSDK", first ? "" : "|");
        first = false;
    }
    if(issue & OB_PIPELINE_ISSUE_DRIVER) {
        std::printf("%sDRIVER", first ? "" : "|");
        first = false;
    }
    if(issue & OB_PIPELINE_ISSUE_FW) {
        std::printf("%sFW", first ? "" : "|");
        first = false;
    }
    if(issue & OB_PIPELINE_ISSUE_HW) {
        std::printf("%sHW", first ? "" : "|");
    }
    std::printf("\n");
}

void printSdkStatusReadable(uint64_t sdkStatus) {
    std::printf("    sdkStatusReadable=");
    if(sdkStatus == 0) {
        std::printf("NONE\n");
        return;
    }

    bool first = true;
    if(sdkStatus & OB_SDK_STATUS_FRAME_DROP_DATA) {
        std::printf("%sFRAME_DROP_DATA", first ? "" : "|");
        first = false;
    }
    if(sdkStatus & OB_SDK_STATUS_FRAME_DROP_TIMESTAMP) {
        std::printf("%sFRAME_DROP_TIMESTAMP", first ? "" : "|");
        first = false;
    }
    if(sdkStatus & OB_SDK_STATUS_FRAME_DROP_MATCH) {
        std::printf("%sFRAME_DROP_MATCH", first ? "" : "|");
        first = false;
    }
    if(sdkStatus & OB_SDK_STATUS_FRAME_QUEUE_OVERFLOW) {
        std::printf("%sFRAME_QUEUE_OVERFLOW", first ? "" : "|");
        first = false;
    }
    if(sdkStatus & OB_SDK_STATUS_FRAME_WAIT_TIMEOUT) {
        std::printf("%sFRAME_WAIT_TIMEOUT", first ? "" : "|");
        first = false;
    }
    if(sdkStatus & OB_SDK_STATUS_STREAM_NO_FRAME) {
        std::printf("%sSTREAM_NO_FRAME", first ? "" : "|");
    }
    std::printf("\n");
}

void printStatus(const char *tag, ob_pipeline_status status) {
    std::printf("[%s] issue=0x%X, sdkStatus=0x%llX, devStatus=0x%llX, drvStatus=0x%llX\n", tag, (unsigned int)status.issue,
                (unsigned long long)status.sdkStatus, (unsigned long long)status.devStatus, (unsigned long long)status.drvStatus);
    printIssueReadable(status.issue);
    printSdkStatusReadable(status.sdkStatus);
}

void pipelineStatusCallback(ob_pipeline_status status, void * /*userData*/) {
    g_statusCallbackCount.fetch_add(1, std::memory_order_relaxed);
    printStatus("health-monitor-callback", status);
}

}  // namespace

int main() {
    ob_error          *error       = nullptr;
    bool               success     = true;
    bool               skipped     = false;
    int                passedCase  = 0;
    int                failedCase  = 0;
    int                skippedCase = 0;
    ob_context        *context     = nullptr;
    ob_device_list    *deviceList  = nullptr;
    ob_device         *device      = nullptr;
    ob_pipeline       *pipeline    = nullptr;
    ob_pipeline_status status1{};
    ob_pipeline_status status2{};
    ob_pipeline_status statusAfterStop{};

    auto passCaseFn = [&](const char *name) {
        ++passedCase;
        std::printf("[CASE][PASS] %s\n", name);
    };

    auto failCaseFn = [&](const char *name) {
        ++failedCase;
        success = false;
        std::printf("[CASE][FAIL] %s\n", name);
    };

    auto skipCaseFn = [&](const char *name, const char *reason) {
        ++skippedCase;
        std::printf("[CASE][SKIP] %s (%s)\n", name, reason);
    };

    auto expectCase = [&](const char *name, bool cond) {
        if(cond) {
            passCaseFn(name);
        }
        else {
            failCaseFn(name);
        }
    };

    ob_set_logger_to_console(OB_LOG_SEVERITY_WARN, &error);
    success = success && checkObError(&error, "ob_set_logger_to_console");

    do {
        context = ob_create_context(&error);
        if(!checkObError(&error, "ob_create_context") || !context) {
            success = false;
            break;
        }

        deviceList = ob_query_device_list(context, &error);
        if(!checkObError(&error, "ob_query_device_list") || !deviceList) {
            success = false;
            break;
        }

        {
            uint32_t count = ob_device_list_get_count(deviceList, &error);
            if(!checkObError(&error, "ob_device_list_get_count")) {
                success = false;
                break;
            }

            if(count == 0) {
                skipped = true;
                std::printf("[SKIP] No device found, skip pipeline_status_test.\n");
                break;
            }
        }

        device = ob_device_list_get_device(deviceList, 0, &error);
        if(!checkObError(&error, "ob_device_list_get_device") || !device) {
            success = false;
            break;
        }

        pipeline = ob_create_pipeline_with_device(device, &error);
        if(!checkObError(&error, "ob_create_pipeline_with_device") || !pipeline) {
            success = false;
            break;
        }

        {
            auto statusBeforeStart = ob_pipeline_get_status(pipeline, &error);
            if(!checkObError(&error, "ob_pipeline_get_status(before start)")) {
                success = false;
                break;
            }
            printStatus("get_status_before_start", statusBeforeStart);
            expectCase("status_before_start_is_zero", statusBeforeStart.issue == OB_PIPELINE_ISSUE_NONE && statusBeforeStart.sdkStatus == 0
                                                          && statusBeforeStart.devStatus == 0 && statusBeforeStart.drvStatus == 0);
        }

        ob_pipeline_start(pipeline, &error);
        if(!checkObError(&error, "ob_pipeline_start")) {
            success = false;
            break;
        }

        int frameCount   = 0;
        int timeoutCount = 0;
        for(int i = 0; i < 20; ++i) {
            ob_frame *frame = ob_pipeline_wait_for_frameset(pipeline, 200, &error);
            if(!checkObError(&error, "ob_pipeline_wait_for_frameset")) {
                success = false;
                break;
            }

            if(frame) {
                ++frameCount;
                ob_delete_frame(frame, &error);
                if(!checkObError(&error, "ob_delete_frame")) {
                    success = false;
                    break;
                }
            }
            else {
                ++timeoutCount;
            }
        }
        if(!success) {
            break;
        }

        std::printf("[INFO] frames=%d, timeouts=%d\n", frameCount, timeoutCount);
        expectCase("stream_produces_some_frames", frameCount > 0);

        status1 = ob_pipeline_get_status(pipeline, &error);
        if(!checkObError(&error, "ob_pipeline_get_status#1")) {
            success = false;
            break;
        }
        printStatus("get_status#1", status1);
        expectCase("status_query_returns_valid_struct", true);

        status2 = ob_pipeline_get_status(pipeline, &error);
        if(!checkObError(&error, "ob_pipeline_get_status#2")) {
            success = false;
            break;
        }
        printStatus("get_status#2", status2);
        expectCase("status_reset_after_read",
                   status2.issue == OB_PIPELINE_ISSUE_NONE && status2.sdkStatus == 0 && status2.devStatus == 0 && status2.drvStatus == 0);

        {
            int timeoutProbeCount = 0;
            for(int i = 0; i < 30; ++i) {
                ob_frame *frame = ob_pipeline_wait_for_frameset(pipeline, 5, &error);
                if(!checkObError(&error, "ob_pipeline_wait_for_frameset(timeout probe)")) {
                    success = false;
                    break;
                }

                if(frame) {
                    ob_delete_frame(frame, &error);
                    if(!checkObError(&error, "ob_delete_frame(timeout probe)")) {
                        success = false;
                        break;
                    }
                }
                else {
                    ++timeoutProbeCount;
                }
            }
            if(!success) {
                break;
            }

            auto timeoutProbeStatus = ob_pipeline_get_status(pipeline, &error);
            if(!checkObError(&error, "ob_pipeline_get_status(timeout probe)")) {
                success = false;
                break;
            }
            printStatus("get_status_after_timeout_probe", timeoutProbeStatus);

            if(timeoutProbeCount > 0) {
                expectCase("wait_timeout_bit_reported", (timeoutProbeStatus.sdkStatus & OB_SDK_STATUS_FRAME_WAIT_TIMEOUT) != 0);
            }
            else {
                skipCaseFn("wait_timeout_bit_reported", "no timeout observed during probe");
            }
        }

        ob_pipeline_enable_health_monitor(pipeline, pipelineStatusCallback, nullptr, 1000, &error);
        if(!checkObError(&error, "ob_pipeline_enable_health_monitor")) {
            success = false;
            break;
        }

        {
            int monitorProbeTimeoutCount = 0;
            for(int i = 0; i < 60; ++i) {
                ob_frame *frame = ob_pipeline_wait_for_frameset(pipeline, 5, &error);
                if(!checkObError(&error, "ob_pipeline_wait_for_frameset(monitor probe)")) {
                    success = false;
                    break;
                }

                if(frame) {
                    ob_delete_frame(frame, &error);
                    if(!checkObError(&error, "ob_delete_frame(monitor probe)")) {
                        success = false;
                        break;
                    }
                }
                else {
                    ++monitorProbeTimeoutCount;
                }
            }
            if(!success) {
                break;
            }

            std::printf("[INFO] monitor probe timeouts=%d\n", monitorProbeTimeoutCount);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(3200));

        ob_pipeline_disable_health_monitor(pipeline, &error);
        if(!checkObError(&error, "ob_pipeline_disable_health_monitor")) {
            success = false;
            break;
        }

        {
            int callbackCountAfterDisable = g_statusCallbackCount.load(std::memory_order_relaxed);
            std::printf("[INFO] health-monitor callback count=%d\n", callbackCountAfterDisable);
            expectCase("health_monitor_callback_triggered", callbackCountAfterDisable > 0);

            std::this_thread::sleep_for(std::chrono::milliseconds(1200));
            int callbackCountLater = g_statusCallbackCount.load(std::memory_order_relaxed);
            expectCase("health_monitor_stops_after_disable", callbackCountLater == callbackCountAfterDisable);
        }

        ob_pipeline_stop(pipeline, &error);
        if(!checkObError(&error, "ob_pipeline_stop")) {
            success = false;
            break;
        }

        statusAfterStop = ob_pipeline_get_status(pipeline, &error);
        if(!checkObError(&error, "ob_pipeline_get_status(after stop #1)")) {
            success = false;
            break;
        }
        printStatus("get_status_after_stop#1", statusAfterStop);

        auto statusAfterStop2 = ob_pipeline_get_status(pipeline, &error);
        if(!checkObError(&error, "ob_pipeline_get_status(after stop #2)")) {
            success = false;
            break;
        }
        printStatus("get_status_after_stop#2", statusAfterStop2);
        expectCase("status_after_stop_eventually_zero", statusAfterStop2.issue == OB_PIPELINE_ISSUE_NONE && statusAfterStop2.sdkStatus == 0
                                                            && statusAfterStop2.devStatus == 0 && statusAfterStop2.drvStatus == 0);
    } while(false);

    if(pipeline) {
        ob_pipeline_disable_health_monitor(pipeline, &error);
        checkObError(&error, "cleanup: ob_pipeline_disable_health_monitor");

        ob_pipeline_stop(pipeline, &error);
        checkObError(&error, "cleanup: ob_pipeline_stop");

        ob_delete_pipeline(pipeline, &error);
        checkObError(&error, "cleanup: ob_delete_pipeline");
        pipeline = nullptr;
    }

    if(device) {
        ob_delete_device(device, &error);
        checkObError(&error, "cleanup: ob_delete_device");
        device = nullptr;
    }

    if(deviceList) {
        ob_delete_device_list(deviceList, &error);
        checkObError(&error, "cleanup: ob_delete_device_list");
        deviceList = nullptr;
    }

    if(context) {
        ob_delete_context(context, &error);
        checkObError(&error, "cleanup: ob_delete_context");
        context = nullptr;
    }

    if(skipped) {
        std::printf("[PASS] pipeline_status_test skipped (no device).\n");
        return 0;
    }

    std::printf("[CASE][SUMMARY] pass=%d fail=%d skip=%d\n", passedCase, failedCase, skippedCase);

    std::printf(success ? "[PASS] pipeline_status_test finished successfully.\n" : "[FAIL] pipeline_status_test failed.\n");
    return (success && failedCase == 0) ? 0 : -1;
}
