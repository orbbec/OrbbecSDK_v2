// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "Logger.hpp"

/**
 * @brief Temporarily redefine the log macros to automatically
 *        prepend the device serial number as a log prefix.
 *
 * This allows existing LOG_INFO/LOG_ERROR/LOG_DEBUG calls
 * to include the device SN without modifying their usage.
 *
 * @note The caller must implement the `GetCurrentSN()` function
 *       to return the current device serial number as a string.
 */

// Debug trace information, only valid when compiled into debug version
#undef LOG_TRACE
#define LOG_TRACE(...) SPDLOG_LOGGER_TRACE(spdlog::default_logger(), "[{}] {}", GetCurrentSN(), fmt::format(__VA_ARGS__).c_str())

// Debugging information, for SDK developers, provides SDK internal running process and status information
#undef LOG_DEBUG
#define LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(spdlog::default_logger(), "[{}] {}", GetCurrentSN(), fmt::format(__VA_ARGS__).c_str())

// General information, facing users, providing SDK running status and call result information
#undef LOG_INFO
#define LOG_INFO(...) SPDLOG_LOGGER_INFO(spdlog::default_logger(), "[{}] {}", GetCurrentSN(), fmt::format(__VA_ARGS__).c_str())

// Warning information (such as: memory usage reaches the maximum limit, cache queue is full, system memory is about to be exhausted, etc.)
#undef LOG_WARN
#define LOG_WARN(...) SPDLOG_LOGGER_WARN(spdlog::default_logger(), "[{}] {}", GetCurrentSN(), fmt::format(__VA_ARGS__).c_str())

// Error information (such as: user parameter error, device error calling sequence, device offline and unresponsive, etc.)
#undef LOG_ERROR
#define LOG_ERROR(...) SPDLOG_LOGGER_ERROR(spdlog::default_logger(), "[{}] {}", GetCurrentSN(), fmt::format(__VA_ARGS__).c_str())

// Fatal error message (for example: insufficient system memory prevents normal operation)
#undef LOG_FATAL
#define LOG_FATAL(...) SPDLOG_LOGGER_CRITICAL(spdlog::default_logger(), "[{}] {}", GetCurrentSN(), fmt::format(__VA_ARGS__).c_str())
