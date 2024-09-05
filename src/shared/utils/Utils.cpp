#include "Utils.hpp"

#ifdef _WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <chrono>
#include <logger/Logger.hpp>

namespace libobsensor {
namespace utils {

uint64_t getNowTimesMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
uint64_t getNowTimesUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void sleepMs(uint64_t msec) {
#ifdef WIN32
    Sleep((DWORD)msec);
#else
    sleep((double)msec / 1000);
#endif
}

void sleepUs(uint64_t usec) {
#ifdef WIN32
    Sleep((DWORD)(usec / 1000));
#else
    usleep((useconds_t)usec);
#endif
}

bool checkJpgImageData(const uint8_t *data, size_t dataLen) {
    bool validImage = dataLen >= 2 && data[0] == 0xFF && data[1] == 0xD8;
    if(validImage) {
        // 为了解决MJPG数据尾部（0xFF 0xD9）由于对齐等情况未按照标准MJPG排列导致的误判情况
        size_t startIndex = dataLen - 1;
        size_t endIndex   = startIndex - 10;
        for(size_t index = startIndex; index > endIndex && index > 0; index--) {
            if(data[index] != 0x00) {
                if(data[index] == 0xD9 && data[index - 1] == 0xFF) {
                    validImage = true;
                }
                else {
                    LOG_DEBUG("check mjpg end flag failed:data[index]:{0},data[index - 1]:{1}", data[index], data[index - 1]);
                    validImage = false;
                }
                break;
            }
        }
    }
    return validImage;
}

}  // namespace utils
}  // namespace libobsensor