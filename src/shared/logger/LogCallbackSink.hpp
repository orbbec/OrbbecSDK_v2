// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

// Purpose: Defines a custom sink for spdlog that allows for a callback to be called when a log is received.
#pragma once

#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/synchronous_factory.h>

#include <mutex>
#include <functional>
#include <string>

namespace spdlog {
typedef std::function<void(level::level_enum logLevel, std::string msg)> log_callback_t;

namespace sinks {
template <typename Mutex> class callback_sink : public base_sink<Mutex> {
public:
    explicit callback_sink(log_callback_t callback) : callback_(callback) {}
    callback_sink(const callback_sink &)            = delete;
    callback_sink &operator=(const callback_sink &) = delete;

protected:
    void sink_it_(const details::log_msg &msg) override {
        if(callback_) {
            memory_buf_t formatted;
            base_sink<Mutex>::formatter_->format(msg, formatted);
            callback_(msg.level, std::string(formatted.data(), formatted.size()));
        }
    }
    void flush_() override {
        // noting to do
    }
    log_callback_t callback_;
};

using callback_sink_mt = callback_sink<std::mutex>;
using callback_sink_st = callback_sink<details::null_mutex>;

}  // namespace sinks

template <typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<logger> callback_logger_mt(const std::string &logger_name, log_callback_t callback) {
    auto callback_logger = Factory::template create<sinks::callback_sink_mt>(logger_name, callback);
    callback_logger->set_level(level::off);
    return callback_logger;
}

template <typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<logger> callback_logger_st(const std::string &logger_name, log_callback_t callback) {
    auto callback_logger = Factory::template create<sinks::callback_sink_st>(logger_name, callback);
    callback_logger->set_level(level::off);
    return callback_logger;
}

}  // namespace spdlog

