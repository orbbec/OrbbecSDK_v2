// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <exception>
#include <typeinfo>
#include "logger/Logger.hpp"

#ifdef _WIN32
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

namespace libobsensor {
class libobsensor_exception : public std::exception {
public:
    const char *getMessage() const noexcept {
        return msg_.c_str();
    }

    OBExceptionType getExceptionType() const noexcept {
        return exceptionType_;
    }

    OBStatus getStatus() const noexcept {
        return status_;
    }

    const char *what() const noexcept override {
        return msg_.c_str();
    }

public:
    libobsensor_exception(const std::string &msg, OBExceptionType exceptionType, OBStatus status) noexcept
        : msg_(msg), exceptionType_(exceptionType), status_(status) {}

private:
    std::string     msg_{};
    OBExceptionType exceptionType_{ OB_EXCEPTION_TYPE_UNKNOWN };
    OBStatus        status_{ OB_ERROR_UNKNOWN };
};

class recoverable_exception : public libobsensor_exception {
public:
    recoverable_exception(const std::string &msg, OBExceptionType exceptionType, OBStatus status, const char *file, int line) noexcept;
};

class unrecoverable_exception : public libobsensor_exception {
public:
    unrecoverable_exception(const std::string &msg, OBExceptionType exceptionType, OBStatus status, const char *file, int line) noexcept;
};

// recoverable exception
#define THROW_RECOVERABLE(msg, type, status)                                                   \
    do {                                                                                       \
        throw libobsensor::recoverable_exception((msg), (type), (status), __FILE__, __LINE__); \
    } while(0)

#define THROW_INVALID_PARAM_EXCEPTION(msg) THROW_RECOVERABLE((msg), OB_EXCEPTION_TYPE_INVALID_VALUE, OB_ERROR_INVALID_PARAMETER)
#define THROW_INVALID_DATA_EXCEPTION(msg) THROW_RECOVERABLE((msg), OB_EXCEPTION_TYPE_INVALID_DATA, OB_ERROR_INVALID_DATA)
#define THROW_ITEM_NOT_FOUND_EXCEPTION(msg) THROW_RECOVERABLE((msg), OB_EXCEPTION_TYPE_NOT_FOUND, OB_ERROR_ITEM_NOT_FOUND)
#define THROW_UNSUPPORTED_OPERATION_EXCEPTION(msg) THROW_RECOVERABLE((msg), OB_EXCEPTION_TYPE_UNSUPPORTED_OPERATION, OB_ERROR_UNSUPPORTED_OPERATION)
#define THROW_WRONG_API_CALL_SEQUENCE_EXCEPTION(msg) THROW_RECOVERABLE((msg), OB_EXCEPTION_TYPE_WRONG_API_CALL_SEQUENCE, OB_ERROR_WRONG_API_CALL_SEQUENCE)
#define THROW_NOT_IMPLEMENTED_EXCEPTION(msg) THROW_RECOVERABLE((msg), OB_EXCEPTION_TYPE_NOT_IMPLEMENTED, OB_ERROR_NOT_IMPLEMENTED)
#define THROW_ACCESS_DENIED_EXCEPTION(msg) THROW_RECOVERABLE((msg), OB_EXCEPTION_TYPE_ACCESS_DENIED, OB_ERROR_DEVICE_ACCESS_DENIED)
#define THROW_RESOURCE_BUSY_EXCEPTION(msg) THROW_RECOVERABLE((msg), OB_EXCEPTION_TYPE_RESOURCE_BUSY, OB_ERROR_RESOURCE_BUSY)
#define THROW_STANDARD_EXCEPTION(msg) THROW_RECOVERABLE((msg), OB_EXCEPTION_STD_EXCEPTION, OB_ERROR_UNKNOWN)

// unrecoverable exception
#define THROW_UNRECOVERABLE(msg, type, status)                                                   \
    do {                                                                                         \
        throw libobsensor::unrecoverable_exception((msg), (type), (status), __FILE__, __LINE__); \
    } while(0)

#define THROW_IO_EXCEPTION(msg) THROW_UNRECOVERABLE((msg), OB_EXCEPTION_TYPE_IO, OB_ERROR_IO_FAILURE)
#define THROW_IO_EXCEPTION_WITH_ERROR(msg, status) THROW_UNRECOVERABLE((msg), OB_EXCEPTION_TYPE_IO, (status))
#define THROW_MEMORY_EXCEPTION(msg) THROW_UNRECOVERABLE((msg), OB_EXCEPTION_TYPE_MEMORY, OB_ERROR_MEMORY)
#define THROW_DEVICE_DISCONNECTED_EXCEPTION(msg) THROW_UNRECOVERABLE((msg), OB_EXCEPTION_TYPE_CAMERA_DISCONNECTED, OB_ERROR_DEVICE_DISCONNECTED)
#define THROW_DEVICE_UNAVAILABLE_EXCEPTION(msg) THROW_UNRECOVERABLE((msg), OB_EXCEPTION_TYPE_DEVICE_UNAVAILABLE, OB_ERROR_DEVICE_UNAVAILABLE)
#define THROW_PAL_EXCEPTION(msg, status) THROW_UNRECOVERABLE((msg), OB_EXCEPTION_TYPE_PLATFORM, (status))

// execute
#define BEGIN_TRY_EXECUTE(statement) \
    try {                            \
        statement;                   \
    }

#define CATCH_EXCEPTION                                                                                                                                      \
    catch(const libobsensor::libobsensor_exception &e) {                                                                                                     \
        LOG_WARN("Execute failure! A libobsensor_exception has occurred!\n - where: {0}({1}):{2}\n - msg: {3}\n - type: {4}\n - status: {5}", __FILENAME__,  \
                 __LINE__, __FUNCTION__, e.getMessage(), typeid(e).name(), static_cast<uint32_t>(e.getStatus()));                                            \
    }                                                                                                                                                        \
    catch(const std::exception &e) {                                                                                                                         \
        LOG_WARN("Execute failure! A std::exception has occurred!\n - where: {0}({1}):{2}\n - msg: {3}\n - type: {4}", __FILENAME__, __LINE__, __FUNCTION__, \
                 e.what(), typeid(e).name());                                                                                                                \
    }                                                                                                                                                        \
    catch(...) {                                                                                                                                             \
        LOG_WARN("Execute failure! An unknown exception has occurred!\n - where: {0}({1}):{2}", __FILENAME__, __LINE__, __FUNCTION__);                       \
    }

#define CATCH_EXCEPTION_AND_EXECUTE(statement)                                                                                                               \
    catch(const libobsensor::libobsensor_exception &e) {                                                                                                     \
        LOG_WARN("Execute failure! A libobsensor_exception has occurred!\n - where: {0}({1}):{2}\n - msg: {3}\n - type: {4}\n - status: {5}", __FILENAME__,  \
                 __LINE__, __FUNCTION__, e.getMessage(), typeid(e).name(), static_cast<uint32_t>(e.getStatus()));                                            \
        statement;                                                                                                                                           \
    }                                                                                                                                                        \
    catch(const std::exception &e) {                                                                                                                         \
        LOG_WARN("Execute failure! A std::exception has occurred!\n - where: {0}({1}):{2}\n - msg: {3}\n - type: {4}", __FILENAME__, __LINE__, __FUNCTION__, \
                 e.what(), typeid(e).name());                                                                                                                \
        statement;                                                                                                                                           \
    }                                                                                                                                                        \
    catch(...) {                                                                                                                                             \
        LOG_WARN("Execute failure! An unknown exception has occurred!\n - where: {0}({1}):{2}", __FILENAME__, __LINE__, __FUNCTION__);                       \
        statement;                                                                                                                                           \
    }

#define CATCH_EXCEPTION_AND_LOG(severity, ...) CATCH_EXCEPTION_AND_EXECUTE(LOG_##severity(__VA_ARGS__))

#define TRY_EXECUTE(statement)   \
    BEGIN_TRY_EXECUTE(statement) \
    CATCH_EXCEPTION

// Only execute the code block in Debug builds; does nothing in Release builds.
#ifdef _DEBUG
#define DEBUG_EXECUTE(statement) \
    do {                         \
        statement;               \
    } while(0)
#else
#define DEBUG_EXECUTE(statement) \
    do {                         \
    } while(0)
#endif

#define VALIDATE(ARG)                                                                    \
    if(!(ARG)) {                                                                         \
        THROW_INVALID_PARAM_EXCEPTION("Invalid value passed for argument \"" #ARG "\""); \
    }

#define VALIDATE_NOT_NULL(ARG)                                             \
    if(!(ARG)) {                                                           \
        std::string msg = "NULL pointer passed for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                     \
        THROW_INVALID_PARAM_EXCEPTION(msg);                                \
    }

#define VALIDATE_STR_NOT_NULL(str)                                                 \
    if(!(str) || *(str) == '0') {                                                  \
        std::string msg = "NULL or empty string passed for argument \"" #str "\""; \
        LOG_WARN(msg);                                                             \
        THROW_INVALID_PARAM_EXCEPTION(msg);                                        \
    }

#define VALIDATE_EQUAL(ARG, CMP)                                            \
    if(ARG != CMP) {                                                        \
        std::string msg = "Invalid value passed for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                      \
        THROW_INVALID_PARAM_EXCEPTION(msg);                                 \
    }

#define VALIDATE_NOT_EQUAL(ARG, CMP)                                        \
    if(ARG == CMP) {                                                        \
        std::string msg = "Invalid value passed for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                      \
        THROW_INVALID_PARAM_EXCEPTION(msg);                                 \
    }

#define VALIDATE_ENUM(ARG, COUNT)                                         \
    if(!(((ARG) >= 0) && ((ARG) < (COUNT)))) {                            \
        std::string msg = "Invalid enum value for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                    \
        THROW_INVALID_PARAM_EXCEPTION(msg);                               \
    }

#define VALIDATE_LE(ARG, MAX)                                                           \
    if((ARG) >= (MAX)) {                                                                \
        std::string msg = "Value not less than \"" #MAX "\" for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                                  \
        THROW_INVALID_PARAM_EXCEPTION(msg);                                             \
    }

#define VALIDATE_GE(ARG, MIN)                                                              \
    if((ARG) <= (MIN)) {                                                                   \
        std::string msg = "Value not greater than \"" #MIN "\" for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                                     \
        THROW_INVALID_PARAM_EXCEPTION(msg);                                                \
    }

#define VALIDATE_NOT_LE(ARG, MIN)                                                   \
    if((ARG) < (MIN)) {                                                             \
        std::string msg = "Value less than \"" #MIN "\" for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                              \
        THROW_INVALID_PARAM_EXCEPTION(msg);                                         \
    }

#define VALIDATE_NOT_GE(ARG, MAX)                                                      \
    if((ARG) > (MAX)) {                                                                \
        std::string msg = "Value greater than \"" #MAX "\" for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                                 \
        THROW_INVALID_PARAM_EXCEPTION(msg);                                            \
    }

#define VALIDATE_RANGE(ARG, MIN, MAX)                                     \
    if((ARG) < (MIN) || (ARG) > (MAX)) {                                  \
        std::string msg = "Out of range value for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                    \
        THROW_INVALID_PARAM_EXCEPTION(msg);                               \
    }

#define VALIDATE_INDEX(ARG, COUNT)                                         \
    if((ARG) < 0 || (ARG) >= (COUNT)) {                                    \
        std::string msg = "Invalid index value for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                     \
        THROW_INVALID_PARAM_EXCEPTION(msg);                                \
    }

#define VALIDATE_UNSIGNED_INDEX(ARG, COUNT)                                \
    if((ARG) >= (COUNT)) {                                                 \
        std::string msg = "Invalid index value for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                     \
        THROW_INVALID_PARAM_EXCEPTION(msg);                                \
    }
}  // namespace libobsensor
