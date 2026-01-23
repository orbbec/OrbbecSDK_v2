// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "utils_c.h"
#include "utils_types.h"
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__linux__) || defined(__APPLE__)
#ifdef __linux__
#include <termio.h>
#else
#include <termios.h>
#endif
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define gets_s gets

int getch(void) {
    struct termios tm, tm_old;
    int            fd = 0, ch;

    if(tcgetattr(fd, &tm) < 0) {  // Save the current terminal settings
        return -1;
    }

    tm_old = tm;
    cfmakeraw(&tm);                        // Change the terminal settings to raw mode, in which all input data is processed in bytes
    if(tcsetattr(fd, TCSANOW, &tm) < 0) {  // Settings after changes on settings
        return -1;
    }

    ch = getchar();
    if(tcsetattr(fd, TCSANOW, &tm_old) < 0) {  // Change the settings to what they were originally
        return -1;
    }

    return ch;
}

int kbhit(void) {
    struct termios oldt, newt;
    int            ch;
    int            oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if(ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}

#include <sys/time.h>
uint64_t ob_smpl_get_current_timestamp_ms(void) {
    struct timeval te;
    long long      milliseconds;
    gettimeofday(&te, NULL);                                // Get the current time
    milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;  // Calculate milliseconds
    return milliseconds;
}

char ob_smpl_wait_for_key_press(uint32_t timeout_ms) {  // Get the current time
    struct timeval te;
    long long      start_time;
    gettimeofday(&te, NULL);
    start_time = te.tv_sec * 1000LL + te.tv_usec / 1000;

    while(true) {
        long long current_time;
        if(kbhit()) {
            return getch();
        }
        gettimeofday(&te, NULL);
        current_time = te.tv_sec * 1000LL + te.tv_usec / 1000;
        if(timeout_ms > 0 && current_time - start_time > timeout_ms) {
            return 0;
        }
        usleep(100);
    }
}

int ob_smpl_support_ansi_escape(void) {
    if(isatty(fileno(stdout)) == 0) {
        // unsupport
        return 0;
    }
    return 1;
}

#else  // Windows
#include <conio.h>
#include <windows.h>
#include <io.h>
#include <stdio.h>

uint64_t ob_smpl_get_current_timestamp_ms() {
    FILETIME      ft;
    LARGE_INTEGER li;
    GetSystemTimeAsFileTime(&ft);
    li.LowPart             = ft.dwLowDateTime;
    li.HighPart            = ft.dwHighDateTime;
    long long milliseconds = li.QuadPart / 10000LL;
    return milliseconds;
}

char ob_smpl_wait_for_key_press(uint32_t timeout_ms) {
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if(hStdin == INVALID_HANDLE_VALUE) {
        return 0;
    }
    DWORD mode = 0;
    if(!GetConsoleMode(hStdin, &mode)) {
        return 0;
    }
    mode &= ~ENABLE_ECHO_INPUT;
    if(!SetConsoleMode(hStdin, mode)) {
        return 0;
    }
    DWORD start_time = GetTickCount();
    while(true) {
        if(_kbhit()) {
            char ch = (char)_getch();
            SetConsoleMode(hStdin, mode);
            return ch;
        }
        if(timeout_ms > 0 && GetTickCount() - start_time > timeout_ms) {
            SetConsoleMode(hStdin, mode);
            return 0;
        }
        Sleep(1);
    }
}

int ob_smpl_support_ansi_escape(void) {
    if(_isatty(_fileno(stdout)) == 0) {
        // unsupport
        return 0;
    }

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if(hOut == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD mode = 0;
    if(!GetConsoleMode(hOut, &mode)) {
        return 0;
    }
    if((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0) {
        return 0;
    }
    return 1;
}

#endif

bool ob_smpl_is_lidar_device(ob_device *device) {
    ob_error       *error       = NULL;
    ob_sensor_list *sensorList  = NULL;
    uint32_t        sensorCount = 0;

    if(device == NULL) {
        return false;
    }

    sensorList = ob_device_get_sensor_list(device, &error);
    CHECK_OB_ERROR_EXIT(&error);

    sensorCount = ob_sensor_list_get_count(sensorList, &error);
    CHECK_OB_ERROR_EXIT(&error);

    for(uint32_t index = 0; index < sensorCount; index++) {
        OBSensorType sensorType = ob_sensor_list_get_sensor_type(sensorList, index, &error);
        CHECK_OB_ERROR_EXIT(&error);

        if(sensorType == OB_SENSOR_LIDAR) {
            ob_delete_sensor_list(sensorList, &error);
            CHECK_OB_ERROR_EXIT(&error);
            return true;
        }
    }

    ob_delete_sensor_list(sensorList, &error);
    CHECK_OB_ERROR_EXIT(&error);

    return false;
}

bool ob_smpl_is_gemini305_device(int vid, int pid) {
    return (vid == OB_DEVICE_VID && (pid == 0x0840 || pid == 0x0841 || pid == 0x0842 || pid == 0x0843));
}

bool ob_smpl_is_astra_mini_device(int vid, int pid) {
    return (vid == OB_DEVICE_VID && (pid == 0x069d || pid == 0x065b || pid == 0x065e));
}

#ifdef __cplusplus
}
#endif
