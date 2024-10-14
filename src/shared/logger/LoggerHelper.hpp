// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "Logger.hpp"
#include <map>
#include <mutex>
#include <string>

struct ObLogRecord {
    uint32_t count;
    uint64_t lastTime;
};

#define LOG_FREQ_CALC_OBJECT_TAG std::string(__FILE__) + std::to_string(__LINE__) + std::to_string((uint64_t)this)
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif

#define VA_ARGS(...) , ##__VA_ARGS__

#ifdef __clang__
#pragma clang diagnostic pop
#endif
// Count the call frequency and then output the log
#define LOG_FREQ_CALC(level, duration, msg, ...)                                                                                           \
    do {                                                                                                                                   \
        static std::map<std::string, std::shared_ptr<ObLogRecord>> logIntvlRecordMap_;                                                     \
        static std::mutex                                          mtx;                                                                    \
        std::unique_lock<std::mutex>                               logLock(mtx);                                                           \
        auto                                                       iter = logIntvlRecordMap_.find(LOG_FREQ_CALC_OBJECT_TAG);               \
        uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); \
        if(iter == logIntvlRecordMap_.end()) {                                                                                             \
            auto record      = std::make_shared<ObLogRecord>();                                                                            \
            record->count    = 0;                                                                                                          \
            record->lastTime = now;                                                                                                        \
            logIntvlRecordMap_.insert(std::make_pair(LOG_FREQ_CALC_OBJECT_TAG, record));                                                   \
            iter = logIntvlRecordMap_.find(LOG_FREQ_CALC_OBJECT_TAG);                                                                      \
        }                                                                                                                                  \
        iter->second->count++;                                                                                                             \
        if(now - iter->second->lastTime > duration) {                                                                                      \
            float       freq   = iter->second->count / ((now - iter->second->lastTime) / 1000.0f);                                         \
            std::string outMsg = msg;                                                                                                      \
            outMsg.replace(outMsg.find("{freq}"), 6, std::to_string(freq));                                                                \
            LOG_##level(outMsg VA_ARGS(__VA_ARGS__));                                                                                      \
            iter->second->count    = 0;                                                                                                    \
            iter->second->lastTime = now;                                                                                                  \
        }                                                                                                                                  \
    } while(0)

