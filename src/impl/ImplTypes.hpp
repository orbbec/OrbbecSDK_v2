// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "libobsensor/h/ObTypes.h"

#include "frame/Frame.hpp"
#include "stream/StreamProfile.hpp"
#include "ISensor.hpp"
#include "IDevice.hpp"

#include "utils/Utils.hpp"

namespace libobsensor {
class Context;
}

#ifdef __cplusplus
extern "C" {
#endif

void translate_exception(const char *name, std::string args, ob_error **result);

#ifdef __cplusplus
}
#endif

#define BEGIN_API_CALL try
#define NOEXCEPT_RETURN(...)                                                     \
    catch(...) {                                                                 \
        std::ostringstream ss;                                                   \
        libobsensor::utils::string::ArgsToStream(ss, #__VA_ARGS__, __VA_ARGS__); \
        return translate_exception(__FUNCTION__, ss.str());                      \
    }
#define HANDLE_EXCEPTIONS_NO_RETURN(...)                                         \
    catch(...) {                                                                 \
        std::ostringstream ss;                                                   \
        libobsensor::utils::string::ArgsToStream(ss, #__VA_ARGS__, __VA_ARGS__); \
        translate_exception(__FUNCTION__, ss.str(), error);                      \
    }
#define NO_ARGS_HANDLE_EXCEPTIONS_NO_RETURN(...)      \
    catch(...) {                                      \
        translate_exception(__FUNCTION__, "", error); \
    }
#define HANDLE_EXCEPTIONS_AND_RETURN(R, ...)                                     \
    catch(...) {                                                                 \
        std::ostringstream ss;                                                   \
        libobsensor::utils::string::ArgsToStream(ss, #__VA_ARGS__, __VA_ARGS__); \
        translate_exception(__FUNCTION__, ss.str(), error);                      \
        return R;                                                                \
    }
#define NO_ARGS_HANDLE_EXCEPTIONS_AND_RETURN(R)       \
    catch(...) {                                      \
        translate_exception(__FUNCTION__, "", error); \
        return R;                                     \
    }

