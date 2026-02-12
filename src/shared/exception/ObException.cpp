// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "ObException.hpp"
#include "logger/Logger.hpp"

namespace libobsensor {

static inline const char *getFileName(const char *path) noexcept {
    if(!path) {
        return "";
    }

    const char *last = path;
    for(const char *p = path; *p; ++p) {
        if(*p == '/' || *p == '\\') {
            last = p + 1;
        }
    }
    return last;
}

recoverable_exception::recoverable_exception(const std::string &msg, ob_exception_type exceptionType, OBStatus status, const char *file, int line) noexcept
    : libobsensor_exception(msg, exceptionType, status) {
    LOG_WARN("A recoverable_exception has occurred!\n - where: {0}({1})\n - msg: {2}\n - type: {3}\n - status: {4}", getFileName(file), line, msg,
             exceptionType, static_cast<uint32_t>(status));
}

unrecoverable_exception::unrecoverable_exception(const std::string &msg, OBExceptionType exceptionType, OBStatus status, const char *file, int line) noexcept
    : libobsensor_exception(msg, exceptionType, status) {
    LOG_WARN("A unrecoverable_exception has occurred!\n - where: {0}({1})\n - msg: {2}\n - type: {3}\n - status: {4}", getFileName(file), line, msg,
             exceptionType, static_cast<uint32_t>(status));
}

}  // namespace libobsensor
