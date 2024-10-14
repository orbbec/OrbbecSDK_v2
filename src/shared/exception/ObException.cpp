// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "ObException.hpp"
#include "logger/Logger.hpp"
namespace libobsensor{
recoverable_exception::recoverable_exception(const std::string &msg, ob_exception_type exception_type) noexcept : libobsensor_exception(msg, exception_type) {
    LOG_WARN(msg);
}
}  // namespace ob
