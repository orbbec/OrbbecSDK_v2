// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "Logger.hpp"
#include <spdlog/common.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/chrono.h>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>
#include <ctime>

#define LOG_RECORD_MAP_SIZE_LIMIT 500  // Maximum number of log entries
#define MAX_LOG_INTERVAL 60 * 1000     // Maximum log output interval: 60000ms
#define DEF_MIN_LOG_INTVL 3000         // Default minimum log output interval: 3000ms
#define LOG_INTVL_OBJECT_TAG std::string(__FILE__) + std::to_string(__LINE__) + std::to_string((uint64_t)this)

struct ObLogIntvlRecord {
    uint32_t                              count;
    uint64_t                              interval;
    std::chrono::system_clock::time_point lastInvokeTime;
    std::chrono::system_clock::time_point lastLogTime;
    std::thread                           invokeThread;
    std::mutex                            mtx;
    std::condition_variable               cv;
    void                                  flush() {
        cv.notify_all();
        if(invokeThread.joinable()) {
            invokeThread.join();
        }
    }
    ~ObLogIntvlRecord() {
        flush();
    }
};

extern std::atomic<bool>                                        logIntvlRecordMapDestroyed;
extern std::map<std::string, std::shared_ptr<ObLogIntvlRecord>> logIntvlRecordMap;
extern std::mutex                                               logIntvlRecordMapMtx;

template <typename... Args>
void log_intvl_invoke(std::shared_ptr<ObLogIntvlRecord> record, uint64_t minIntvlMsec, spdlog::source_loc src_loc, spdlog::level::level_enum level,
                      std::string fmt, Args &&...args) {
    std::unique_lock<std::mutex> lock(record->mtx);
    record->cv.wait_for(lock, std::chrono::milliseconds(record->interval));
    if(record->count > 0) {
        auto     nowTime  = std::chrono::system_clock::now();
        uint64_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(nowTime - record->lastInvokeTime).count();

        char   timestampStr[100];
        time_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(record->lastLogTime.time_since_epoch()).count();
        std::strftime(timestampStr, sizeof(timestampStr), "%H:%M:%S", std::localtime(&timestamp));

        auto us    = std::chrono::duration_cast<std::chrono::microseconds>(record->lastLogTime.time_since_epoch()).count() % 1000000;
        auto usStr = fmt::format("{:06d}", us);

        fmt = fmt + " [**" + std::to_string(record->count) + " logs in " + std::to_string(duration) + "ms, last: " + timestampStr + "." + usStr.data() + "**]";
        spdlog::default_logger_raw()->log(src_loc, level, fmt, std::forward<Args>(args)...);

        uint64_t avgInterval = duration / record->count;
        if(avgInterval < record->interval) {  // Reduce log output frequency
            record->interval *= 2;
            if(record->interval > MAX_LOG_INTERVAL) {
                record->interval = MAX_LOG_INTERVAL;
            }
        }
        else {
            avgInterval = minIntvlMsec;  // Restore log output frequency
        }

        record->lastInvokeTime = nowTime;
        record->count          = 0;
    }
}

// Control log output at intervals; when log_intvl is called repeatedly within the interval time, only one log entry is output
// When log_intvl is called frequently in succession, the log output frequency will decrease (intervals will lengthen) until the maximum interval, MAX_INTERVAL, is reached
template <typename... Args>
void log_intvl(std::shared_ptr<ObLogIntvlRecord> record, uint64_t minIntvlMsec, spdlog::source_loc src_loc, spdlog::level::level_enum level, std::string fmt,
               Args &&...args) {
    if(minIntvlMsec == 0) {
        spdlog::default_logger_raw()->log(src_loc, level, fmt, std::forward<Args>(args)...);
    }
    else {
        std::unique_lock<std::mutex> lock(record->mtx);
        auto                         nowTime = std::chrono::system_clock::now();
        record->lastLogTime                  = nowTime;
        record->count++;
        uint64_t duration           = std::chrono::duration_cast<std::chrono::milliseconds>(nowTime - record->lastInvokeTime).count();
        bool     zeroLastInvokeTime = record->lastInvokeTime.time_since_epoch() == std::chrono::system_clock::duration::zero();
        if(duration > record->interval || zeroLastInvokeTime) {
            if(!zeroLastInvokeTime) {
                uint64_t avgInterval = duration / record->count;
                if(avgInterval < record->interval) {  // Reduce the log output frequency
                    record->interval *= 2;
                    if(record->interval > MAX_LOG_INTERVAL) {
                        record->interval = MAX_LOG_INTERVAL;
                    }
                }
                else {
                    record->interval = minIntvlMsec;  // Restore the log output frequency
                }

                if(record->count > 1) {
                    fmt = fmt + " [**" + std::to_string(record->count) + " logs in " + std::to_string(duration) + "ms**]";
                }
            }

            spdlog::default_logger_raw()->log(src_loc, level, fmt, std::forward<Args>(args)...);
            record->count          = 0;
            record->lastInvokeTime = nowTime;
            lock.unlock();
            if(record->invokeThread.joinable())  // Notify the thread to exit (resources will be automatically released after the thread exits)
            {
                record->cv.notify_all();
                record->invokeThread.join();
            }
        }
        else if(record->count == 1) {
            lock.unlock();
            if(record->invokeThread.joinable()) {
                record->invokeThread.join();
            }
            auto func            = std::bind(std::move(log_intvl_invoke<Args &...>), record, minIntvlMsec, src_loc, level, fmt, std::forward<Args>(args)...);
            record->invokeThread = std::thread(func);
        }
    }
}

// Control the log output interval in milliseconds; 0 means no control
#define LOG_INTVL(tag, minIntvlMsec, level, ...)                                                                                                            \
    do {                                                                                                                                                    \
        std::unique_lock<std::mutex> logIntvlRecordMapLock(logIntvlRecordMapMtx);                                                                           \
        if(logIntvlRecordMapDestroyed) {                                                                                                                    \
            break;                                                                                                                                          \
        }                                                                                                                                                   \
        if(logIntvlRecordMap.size() > LOG_RECORD_MAP_SIZE_LIMIT) {                                                                                          \
            LOG_WARN("logIntvlRecordMap size {} > {}, clear it!", logIntvlRecordMap.size(), LOG_RECORD_MAP_SIZE_LIMIT);                                     \
            auto nowTime = std::chrono::system_clock::now();                                                                                                \
            for(auto logIntvlRecordMapIter = logIntvlRecordMap.begin(); logIntvlRecordMapIter != logIntvlRecordMap.end();) {                                \
                uint64_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(nowTime - logIntvlRecordMapIter->second->lastInvokeTime).count(); \
                if(logIntvlRecordMapIter->second->count == 0 && duration > logIntvlRecordMapIter->second->interval) {                                       \
                    logIntvlRecordMapIter = logIntvlRecordMap.erase(logIntvlRecordMapIter);                                                                 \
                }                                                                                                                                           \
                else {                                                                                                                                      \
                    logIntvlRecordMapIter++;                                                                                                                \
                }                                                                                                                                           \
            }                                                                                                                                               \
        }                                                                                                                                                   \
        auto logIntvlRecordMapIter = logIntvlRecordMap.find(tag);                                                                                           \
        if(logIntvlRecordMapIter == logIntvlRecordMap.end()) {                                                                                              \
            auto record      = std::make_shared<ObLogIntvlRecord>();                                                                                        \
            record->count    = 0;                                                                                                                           \
            record->interval = minIntvlMsec;                                                                                                                \
            logIntvlRecordMap.insert(std::make_pair(tag, record));                                                                                          \
            logIntvlRecordMapIter = logIntvlRecordMap.find(tag);                                                                                            \
        }                                                                                                                                                   \
        log_intvl(logIntvlRecordMapIter->second, minIntvlMsec, spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION }, level, __VA_ARGS__);              \
    } while(0)

#define LOG_INTVL_ON_THREAD(minIntvlMsec, level, ...)                                  \
    do {                                                                               \
        std::stringstream ss;                                                          \
        ss << std::this_thread::get_id();                                              \
        std::string tag = std::string(__FILE__) + std::to_string(__LINE__) + ss.str(); \
        LOG_INTVL(tag, minIntvlMsec, level, __VA_ARGS__);                              \
    } while(0)

// The LOG_XXX_INTVL macro can only be used within class member functions because it uses the `this` pointer as a tag (log output interval control is bound to a specific object)
// The LOG_XXX_INTVL_THREAD macro uses the current thread ID as a tag and can be used in class member functions and regular functions (log output interval control is bound to a specific thread)
// It is recommended to prioritize the use of the LOG_XXX_INTVL macro, since log outputs from the same place where the same object exists are called in different threads (e.g., VideoSensor's raw frame data callback)
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
#define LOG_TRACE_INTVL(...) LOG_INTVL(LOG_INTVL_OBJECT_TAG, DEF_MIN_LOG_INTVL, spdlog::level::trace, __VA_ARGS__)
#define LOG_TRACE_INTVL_THREAD(...) LOG_INTVL_ON_THREAD(DEF_MIN_LOG_INTVL, spdlog::level::trace, __VA_ARGS__)
#define LOG_TRACE_INTVL_MS(minIntvlMsec, ...) LOG_INTVL(LOG_INTVL_OBJECT_TAG, minIntvlMsec, spdlog::level::trace, __VA_ARGS__)
#define LOG_TRACE_INTVL_THREAD_MS(minIntvlMsec, ...) LOG_INTVL_ON_THREAD(minIntvlMsec, spdlog::level::trace, __VA_ARGS__)
#else
#define LOG_TRACE_INTVL(...)
#define LOG_TRACE_INTVL_THREAD(...)
#define LOG_TRACE_INTVL_MS(...)
#define LOG_TRACE_INTVL_THREAD_MS(...)
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG
#define LOG_DEBUG_INTVL(...) LOG_INTVL(LOG_INTVL_OBJECT_TAG, DEF_MIN_LOG_INTVL, spdlog::level::debug, __VA_ARGS__)
#define LOG_DEBUG_INTVL_THREAD(...) LOG_INTVL_ON_THREAD(DEF_MIN_LOG_INTVL, spdlog::level::debug, __VA_ARGS__)
#define LOG_DEBUG_INTVL_MS(minIntvlMsec, ...) LOG_INTVL(LOG_INTVL_OBJECT_TAG, minIntvlMsec, spdlog::level::debug, __VA_ARGS__)
#define LOG_DEBUG_INTVL_THREAD_MS(minIntvlMsec, ...) LOG_INTVL_ON_THREAD(minIntvlMsec, spdlog::level::debug, __VA_ARGS__)
#else
#define LOG_DEBUG_INTVL(...)
#define LOG_DEBUG_INTVL_THREAD(...)
#define LOG_DEBUG_INTVL_MS(...)
#define LOG_DEBUG_INTVL_THREAD_MS(...)
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_INFO
#define LOG_INFO_INTVL(...) LOG_INTVL(LOG_INTVL_OBJECT_TAG, DEF_MIN_LOG_INTVL, spdlog::level::info, __VA_ARGS__)
#define LOG_INFO_INTVL_THREAD(...) LOG_INTVL_ON_THREAD(DEF_MIN_LOG_INTVL, spdlog::level::info, __VA_ARGS__)
#define LOG_INFO_INTVL_MS(minIntvlMsec, ...) LOG_INTVL(LOG_INTVL_OBJECT_TAG, minIntvlMsec, spdlog::level::info, __VA_ARGS__)
#define LOG_INFO_INTVL_THREAD_MS(minIntvlMsec, ...) LOG_INTVL_ON_THREAD(minIntvlMsec, spdlog::level::info, __VA_ARGS__)
#else
#define LOG_INFO_INTVL(...)
#define LOG_INFO_INTVL_THREAD(...)
#define LOG_INFO_INTVL_MS(...)
#define LOG_INFO_INTVL_THREAD_MS(...)
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_WARN
#define LOG_WARN_INTVL(...) LOG_INTVL(LOG_INTVL_OBJECT_TAG, DEF_MIN_LOG_INTVL, spdlog::level::warn, __VA_ARGS__)
#define LOG_WARN_INTVL_THREAD(...) LOG_INTVL_ON_THREAD(DEF_MIN_LOG_INTVL, spdlog::level::warn, __VA_ARGS__)
#define LOG_WARN_INTVL_MS(minIntvlMsec, ...) LOG_INTVL(LOG_INTVL_OBJECT_TAG, minIntvlMsec, spdlog::level::warn, __VA_ARGS__)
#define LOG_WARN_INTVL_THREAD_MS(minIntvlMsec, ...) LOG_INTVL_ON_THREAD(minIntvlMsec, spdlog::level::warn, __VA_ARGS__)
#else
#define LOG_WARN_INTVL(...)
#define LOG_WARN_INTVL_THREAD(...)
#define LOG_WARN_INTVL_MS(...)
#define LOG_WARN_INTVL_THREAD_MS(...)
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_ERROR
#define LOG_ERROR_INTVL(...) LOG_INTVL(LOG_INTVL_OBJECT_TAG, DEF_MIN_LOG_INTVL, spdlog::level::err, __VA_ARGS__)
#define LOG_ERROR_INTVL_THREAD(...) LOG_INTVL_ON_THREAD(DEF_MIN_LOG_INTVL, spdlog::level::err, __VA_ARGS__)
#define LOG_ERROR_INTVL_MS(minIntvlMsec, ...) LOG_INTVL(LOG_INTVL_OBJECT_TAG, minIntvlMsec, spdlog::level::err, __VA_ARGS__)
#define LOG_ERROR_INTVL_THREAD_MS(minIntvlMsec, ...) LOG_INTVL_ON_THREAD(minIntvlMsec, spdlog::level::err, __VA_ARGS__)
#else
#define LOG_ERROR_INTVL(...)
#define LOG_ERROR_INTVL_THREAD(...)
#define LOG_ERROR_INTVL_MS(...)
#define LOG_ERROR_INTVL_THREAD_MS(...)
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_CRITICAL
#define LOG_FATAL_INTVL(...) LOG_INTVL(LOG_INTVL_OBJECT_TAG, DEF_MIN_LOG_INTVL, spdlog::level::critical, __VA_ARGS__)
#define LOG_FATAL_INTVL_THREAD(...) LOG_INTVL_ON_THREAD(DEF_MIN_LOG_INTVL, spdlog::level::critical, __VA_ARGS__)
#define LOG_FATAL_INTVL_MS(minIntvlMsec, ...) LOG_INTVL(LOG_INTVL_OBJECT_TAG, minIntvlMsec, spdlog::level::critical, __VA_ARGS__)
#define LOG_FATAL_INTVL_THREAD_MS(minIntvlMsec, ...) LOG_INTVL_ON_THREAD(minIntvlMsec, spdlog::level::critical, __VA_ARGS__)
#else
#define LOG_FATAL_INTVL(...)
#define LOG_FATAL_INTVL_THREAD(...)
#define LOG_FATAL_INTVL_MS(minIntvlMsec, ...)
#define LOG_FATAL_INTVL_THREAD_MS(minIntvlMsec, ...)
#endif
