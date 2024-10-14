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
    const char *get_message() const noexcept {
        return _msg.c_str();
    }

    ob_exception_type get_exception_type() const noexcept {
        return _exception_type;
    }

    const char *what() const noexcept override {
        return _msg.c_str();
    }

public:
    libobsensor_exception(const std::string &msg, ob_exception_type exception_type) noexcept : _msg(msg), _exception_type(exception_type) {}

private:
    std::string       _msg;
    ob_exception_type _exception_type;
};

class recoverable_exception : public libobsensor_exception {
public:
    recoverable_exception(const std::string &msg, ob_exception_type exception_type) noexcept;
};

class unrecoverable_exception : public libobsensor_exception {
public:
    unrecoverable_exception(const std::string &msg, ob_exception_type exception_type) noexcept : libobsensor_exception(msg, exception_type) {
        LOG_WARN(msg);
    }
};

class io_exception : public unrecoverable_exception {
public:
    io_exception(const std::string &msg) noexcept : unrecoverable_exception(msg, OB_EXCEPTION_TYPE_IO) {}
};

class memory_exception : public unrecoverable_exception {
public:
    memory_exception(const std::string &msg) noexcept : unrecoverable_exception(msg, OB_EXCEPTION_TYPE_MEMORY) {}
};

class camera_disconnected_exception : public unrecoverable_exception {
public:
    camera_disconnected_exception(const std::string &msg) noexcept : unrecoverable_exception(msg, OB_EXCEPTION_TYPE_CAMERA_DISCONNECTED) {}
};

class pal_exception : public unrecoverable_exception {
public:
    pal_exception(const std::string &msg) noexcept : unrecoverable_exception(msg, OB_EXCEPTION_TYPE_PLATFORM) {}
};

class invalid_value_exception : public recoverable_exception {
public:
    invalid_value_exception(const std::string &msg) noexcept : recoverable_exception(msg, OB_EXCEPTION_TYPE_INVALID_VALUE) {}
};

class unsupported_operation_exception : public recoverable_exception {
public:
    unsupported_operation_exception(const std::string &msg) noexcept : recoverable_exception(msg, OB_EXCEPTION_TYPE_UNSUPPORTED_OPERATION) {}
};
class wrong_api_call_sequence_exception : public recoverable_exception {
public:
    wrong_api_call_sequence_exception(const std::string &msg) noexcept : recoverable_exception(msg, OB_EXCEPTION_TYPE_WRONG_API_CALL_SEQUENCE) {}
};

class not_implemented_exception : public recoverable_exception {
public:
    not_implemented_exception(const std::string &msg) noexcept : recoverable_exception(msg, OB_EXCEPTION_TYPE_NOT_IMPLEMENTED) {}
};

#define BEGIN_TRY_EXECUTE(statement) \
    try {                            \
        statement;                   \
    }

#define CATCH_EXCEPTION                                                                                                                                      \
    catch(const libobsensor::libobsensor_exception &e) {                                                                                                     \
        LOG_WARN("Execute failure! A libobsensor_exception has occurred!\n - where: {0}({1}):{2}\n - msg: {3}\n - type: {4}", __FILENAME__, __LINE__,        \
                 __FUNCTION__, e.get_message(), typeid(e).name());                                                                                           \
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
        LOG_WARN("Execute failure! A libobsensor_exception has occurred!\n - where: {0}({1}):{2}\n - msg: {3}\n - type: {4}", __FILENAME__, __LINE__,        \
                 __FUNCTION__, e.get_message(), typeid(e).name());                                                                                           \
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

#define VALIDATE(ARG)                                                             \
    if(!(ARG)) {                                                                  \
        throw std::logic_error("Invalid value passed for argument \"" #ARG "\""); \
    }

#define VALIDATE_NOT_NULL(ARG)                                             \
    if(!(ARG)) {                                                           \
        std::string msg = "NULL pointer passed for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                     \
        throw std::logic_error(msg);                                       \
    }

#define VALIDATE_EQUAL(ARG, CMP)                                            \
    if(ARG != CMP) {                                                        \
        std::string msg = "Invalid value passed for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                      \
        throw std::logic_error(msg);                                        \
    }

#define VALIDATE_NOT_EQUAL(ARG, CMP)                                        \
    if(ARG == CMP) {                                                        \
        std::string msg = "Invalid value passed for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                      \
        throw std::logic_error(msg);                                        \
    }

#define VALIDATE_ENUM(ARG, COUNT)                                         \
    if(!(((ARG) >= 0) && ((ARG) < (COUNT)))) {                            \
        std::string msg = "Invalid enum value for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                    \
        throw std::logic_error(msg);                                      \
    }

#define VALIDATE_LE(ARG, MAX)                                                           \
    if((ARG) >= (MAX)) {                                                                \
        std::string msg = "Value not less than \"" #MAX "\" for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                                  \
        throw std::logic_error(msg);                                                    \
    }

#define VALIDATE_GE(ARG, MIN)                                                              \
    if((ARG) <= (MIN)) {                                                                   \
        std::string msg = "Value not greater than \"" #MIN "\" for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                                     \
        throw std::logic_error(msg);                                                       \
    }

#define VALIDATE_NOT_LE(ARG, MIN)                                                   \
    if((ARG) < (MIN)) {                                                             \
        std::string msg = "Value less than \"" #MIN "\" for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                              \
        throw std::logic_error(msg);                                                \
    }

#define VALIDATE_NOT_GE(ARG, MAX)                                                      \
    if((ARG) > (MAX)) {                                                                \
        std::string msg = "Value greater than \"" #MAX "\" for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                                 \
        throw std::logic_error(msg);                                                   \
    }

#define VALIDATE_RANGE(ARG, MIN, MAX)                                     \
    if((ARG) < (MIN) || (ARG) > (MAX)) {                                  \
        std::string msg = "Out of range value for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                    \
        throw std::logic_error(msg);                                      \
    }

#define VALIDATE_INDEX(ARG, COUNT)                                         \
    if((ARG) < 0 || (ARG) >= (COUNT)) {                                    \
        std::string msg = "Invalid index value for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                     \
        throw std::logic_error(msg);                                       \
    }

#define VALIDATE_UNSIGNED_INDEX(ARG, COUNT)                                \
    if((ARG) >= (COUNT)) {                                                 \
        std::string msg = "Invalid index value for argument \"" #ARG "\""; \
        LOG_WARN(msg);                                                     \
        throw std::logic_error(msg);                                       \
    }
}  // namespace libobsensor
