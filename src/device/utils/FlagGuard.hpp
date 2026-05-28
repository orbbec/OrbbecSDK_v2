// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <iostream>
#include <atomic>

namespace libobsensor {

class FlagGuard {
public:
    explicit FlagGuard(std::atomic<bool> &flag) : flag_(flag) {
        flag_ = true;
    }
    ~FlagGuard() {
        flag_ = false;
    }
    FlagGuard(const FlagGuard &)            = delete;
    FlagGuard &operator=(const FlagGuard &) = delete;

private:
    std::atomic<bool> &flag_;
};

}  // namespace libobsensor
