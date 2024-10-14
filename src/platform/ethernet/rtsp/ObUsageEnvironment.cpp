// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "ObUsageEnvironment.hpp"

#include "logger/Logger.hpp"

namespace libobsensor {

ObUsageEnvironment::ObUsageEnvironment(TaskScheduler &taskScheduler) : BasicUsageEnvironment(taskScheduler), destroy_(false), newLog_(false) {
    outputLogThread_ = std::thread(&ObUsageEnvironment::outputLog, this);
}

ObUsageEnvironment::~ObUsageEnvironment() {
    destroy_ = true;
    newLogCv_.notify_all();
    outputLogThread_.join();
}

ObUsageEnvironment *ObUsageEnvironment::createNew(TaskScheduler &taskScheduler) {
    return new ObUsageEnvironment(taskScheduler);
}

void ObUsageEnvironment::outputLog() {
    while(!destroy_) {
        std::unique_lock<std::mutex> lk(mutex_);
        newLogCv_.wait(lk, [&]() { return newLog_ || destroy_; });
        newLogCv_.wait_for(lk, std::chrono::microseconds(100), [&]() {
            bool exit = !newLog_ || destroy_;
            newLog_   = false;
            return exit;
        });

        if(logMsg_.length() && !destroy_) {
            LOG_DEBUG(logMsg_);
            logMsg_.clear();
        }
    }
}

UsageEnvironment &ObUsageEnvironment::operator<<(char const *str) {
    std::unique_lock<std::mutex> lk(mutex_);
    if(str == nullptr) {
        logMsg_ += "(NULL)";  // sanity check
    }
    else {
        logMsg_ += str;
    }
    newLog_ = true;
    newLogCv_.notify_all();
    return dynamic_cast<UsageEnvironment &>(*this);
}

UsageEnvironment &ObUsageEnvironment::operator<<(int i) {
    std::unique_lock<std::mutex> lk(mutex_);
    logMsg_ += std::to_string(i);
    newLog_ = true;
    newLogCv_.notify_all();
    return dynamic_cast<UsageEnvironment &>(*this);
}

UsageEnvironment &ObUsageEnvironment::operator<<(unsigned u) {
    std::unique_lock<std::mutex> lk(mutex_);
    logMsg_ += std::to_string(u);
    newLog_ = true;
    newLogCv_.notify_all();
    return dynamic_cast<UsageEnvironment &>(*this);
}

UsageEnvironment &ObUsageEnvironment::operator<<(double d) {
    std::unique_lock<std::mutex> lk(mutex_);
    logMsg_ += std::to_string(d);
    newLog_ = true;
    newLogCv_.notify_all();
    return dynamic_cast<UsageEnvironment &>(*this);
}

UsageEnvironment &ObUsageEnvironment::operator<<(void *p) {
    std::unique_lock<std::mutex> lk(mutex_);
    logMsg_ += std::to_string((uint64_t)p);
    newLog_ = true;
    newLogCv_.notify_all();
    return dynamic_cast<UsageEnvironment &>(*this);
}


}  // namespace libobsensor

