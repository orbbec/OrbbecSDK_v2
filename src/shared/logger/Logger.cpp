// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "Logger.hpp"
#include "LoggerHelper.hpp"
#include "LoggerInterval.hpp"

#include "exception/ObException.hpp"
#include "environment/EnvConfig.hpp"

#include "LogCallbackSink.hpp"
#ifdef __ANDROID__
#include <spdlog/sinks/android_sink.h>
#else
#include <spdlog/sinks/stdout_color_sinks.h>
#endif
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/async_logger.h>
#include <spdlog/async.h>
#include "spdlog/pattern_formatter.h"

#include <map>

std::map<std::string, std::shared_ptr<ObLogIntvlRecord>> logIntvlRecordMap;
std::mutex                                               logIntvlRecordMapMtx;
std::atomic<bool>                                        logIntvlRecordMapDestroyed(false);

namespace libobsensor {

const std::map<OBLogSeverity, spdlog::level::level_enum> OBLogSeverityToSpdlogLevel = {
#ifdef _DEBUG
    { OB_LOG_SEVERITY_DEBUG, spdlog::level::level_enum::trace },
#else
    { OB_LOG_SEVERITY_DEBUG, spdlog::level::level_enum::debug },
#endif
    { OB_LOG_SEVERITY_INFO, spdlog::level::level_enum::info },   { OB_LOG_SEVERITY_WARN, spdlog::level::level_enum::warn },
    { OB_LOG_SEVERITY_ERROR, spdlog::level::level_enum::err },   { OB_LOG_SEVERITY_FATAL, spdlog::level::level_enum::critical },
    { OB_LOG_SEVERITY_OFF, spdlog::level::level_enum::off },
};

const std::map<spdlog::level::level_enum, OBLogSeverity> SpdlogLevelToOBLogSeverity = {
    { spdlog::level::level_enum::trace, OB_LOG_SEVERITY_DEBUG }, { spdlog::level::level_enum::debug, OB_LOG_SEVERITY_DEBUG },
    { spdlog::level::level_enum::info, OB_LOG_SEVERITY_INFO },   { spdlog::level::level_enum::warn, OB_LOG_SEVERITY_WARN },
    { spdlog::level::level_enum::err, OB_LOG_SEVERITY_ERROR },   { spdlog::level::level_enum::critical, OB_LOG_SEVERITY_FATAL },
    { spdlog::level::level_enum::off, OB_LOG_SEVERITY_OFF },
};

#ifdef __ANDROID__
const char *OB_DEFAULT_LOG_FILE_PATH = "/sdcard/Orbbec/Log/";
#else
const char *OB_DEFAULT_LOG_FILE_PATH = "Log/";
#endif

const OBLogSeverity OB_DEFAULT_LOG_SEVERITY  = OB_LOG_SEVERITY_INFO;
const std::string   OB_DEFAULT_LOG_FMT       = "[%m/%d %H:%M:%S.%f][%l][%t][%s:%#] %v";
const uint64_t      OB_DEFAULT_MAX_FILE_SIZE = 1024 * 1024 * 100;
const uint16_t      OB_DEFAULT_MAX_FILE_NUM  = 3;
const std::string   OB_DEFAULT_LOG_FILE_NAME = "OrbbecSDK.log.txt";

struct Logger::LoggerConfig {
    bool          loadFileLogSeverityFromEnvConfig = true;
    OBLogSeverity fileLogSeverity                  = OB_LOG_SEVERITY_DEBUG;

    bool        loadFileLogPathFromEnvConfig = true;
    std::string fileLogOutputDir             = OB_DEFAULT_LOG_FILE_PATH;
    std::string fileLogFileName              = OB_DEFAULT_LOG_FILE_NAME;
    uint64_t    fileLogMaxFileSize           = OB_DEFAULT_MAX_FILE_SIZE;
    uint64_t    fileLogMaxFileNum            = OB_DEFAULT_MAX_FILE_NUM;

    bool          loadConsoleLogSeverityFromEnvConfig = true;
    OBLogSeverity consoleLogSeverity                  = OB_DEFAULT_LOG_SEVERITY;

    bool          loadCallbackLogSeverityFromEnvConfig = true;
    OBLogSeverity callbackLogSeverity                  = OB_DEFAULT_LOG_SEVERITY;
    LogCallback   logCallback                          = nullptr;

    bool async = false;
};

Logger::LoggerConfig    Logger::config_;
std::mutex              Logger::instanceMutex_;
std::weak_ptr<Logger>   Logger::instanceWeakPtr_;
std::shared_ptr<Logger> Logger::getInstance() {
    std::lock_guard<std::mutex> lock(instanceMutex_);
    auto                        instance = instanceWeakPtr_.lock();
    if(!instance) {
        instance         = std::shared_ptr<Logger>(new Logger());
        instanceWeakPtr_ = instance;
    }
    return instance;
}

Logger::Logger() : spdlogRegistry_(spdlog::details::registry::instance_ptr()) {
    spdlog::set_pattern(OB_DEFAULT_LOG_FMT);

    std::lock_guard<std::mutex> lock(logIntvlRecordMapMtx);
    logIntvlRecordMapDestroyed = false;
    loadEnvConfig();
    createConsoleSink();
    createFileSink();
    createCallbackSink();
    updateDefaultSpdLogger();
}

Logger::~Logger() noexcept {
    {
        std::lock_guard<std::mutex> lock(logIntvlRecordMapMtx);
        logIntvlRecordMapDestroyed = true;
        for(auto &it: logIntvlRecordMap) {
            it.second->flush();
            it.second.reset();
        }
        logIntvlRecordMap.clear();
    }

    spdlog::set_default_logger(std::make_shared<spdlog::logger>("EmptySinksLogger"));

    if(consoleSink_) {
        consoleSink_->flush();
        consoleSink_.reset();
    }
    if(fileSink_) {
        fileSink_->flush();
        fileSink_.reset();
    }

    if(callbackSink_) {
        callbackSink_->flush();
        callbackSink_.reset();
    }

    spdlogRegistry_.reset();
}

void Logger::createFileSink() {
    if(fileSink_) {
        fileSink_->flush();
        fileSink_.reset();
    }
    if(config_.fileLogSeverity != OB_LOG_SEVERITY_OFF) {
        auto  path     = config_.fileLogOutputDir + "/" + config_.fileLogFileName;
        auto &fileSize = config_.fileLogMaxFileSize;
        auto &fileNum  = config_.fileLogMaxFileNum;
        try {
            fileSink_ = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(path, fileSize, fileNum);
        }
        catch(const std::exception &e) {
            LOG_ERROR("Error creating file sink for logger! {}", e.what());
            fileSink_ = nullptr;
        }
        catch(...) {
            LOG_ERROR("Unknown error creating file sink for logger! path:{}", path);
            fileSink_ = nullptr;
        }
        if(fileSink_) {
            auto fileLogLevel = OBLogSeverityToSpdlogLevel.find(config_.fileLogSeverity)->second;
            fileSink_->set_level(fileLogLevel);
        }
    }
}

void Logger::createConsoleSink() {
    if(consoleSink_) {
        consoleSink_->flush();
        consoleSink_.reset();
    }
    if(config_.consoleLogSeverity != OB_LOG_SEVERITY_OFF) {
        if(!consoleSink_) {
#ifdef __ANDROID__
            consoleSink_ = std::make_shared<spdlog::sinks::android_sink_mt>("OrbbecSDK");
#else
            consoleSink_ = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
#endif
        }
        auto consoleLogLevel = OBLogSeverityToSpdlogLevel.find(config_.consoleLogSeverity)->second;
        consoleSink_->set_level(consoleLogLevel);
    }
}

void Logger::createCallbackSink() {
    if(callbackSink_) {
        callbackSink_->flush();
        callbackSink_.reset();
    }
    if(config_.logCallback != nullptr && config_.callbackLogSeverity != OB_LOG_SEVERITY_OFF) {
        if(!callbackSink_) {
            callbackSink_ = std::make_shared<spdlog::sinks::callback_sink_mt>([](spdlog::level::level_enum logLevel, std::string msg) {
                if(config_.logCallback) {
                    config_.logCallback(SpdlogLevelToOBLogSeverity.find(logLevel)->second, msg);
                }
            });
        }
        auto callbackLogLevel = OBLogSeverityToSpdlogLevel.find(config_.callbackLogSeverity)->second;
        callbackSink_->set_level(callbackLogLevel);
    }
}

void Logger::updateDefaultSpdLogger() {
    std::vector<spdlog::sink_ptr> sinks;
    if(consoleSink_) {
        sinks.push_back(consoleSink_);
    }
    if(fileSink_) {
        sinks.push_back(fileSink_);
    }
    if(callbackSink_) {
        sinks.push_back(callbackSink_);
    }

    std::shared_ptr<spdlog::logger> spdLogger;
    if(config_.async) {
        spdlog::init_thread_pool(1024, 1);  // queue with 1k items and 1 threads, multiple threads will cause the log output order to be disordered

        // Asynchronous logger
        spdLogger = std::make_shared<spdlog::async_logger>("OrbbecSDK", sinks.begin(), sinks.end(),  //
                                                           spdlog::thread_pool(), spdlog::async_overflow_policy::block);

        spdlog::flush_every(std::chrono::seconds(1));  // Set to flush the log every 1 second
    }
    else {
        // Synchronize logger
        spdLogger = std::make_shared<spdlog::logger>("OrbbecSDK", sinks.begin(), sinks.end());
    }

    spdlog::set_default_logger(spdLogger);
    spdlog::set_level(spdlog::level::trace);  // Set the logger log level (here set to trace, the actual output will be output according to the sink's log
                                              // level)
    spdlog::flush_on(spdlog::level::trace);   // Set the flush log level (immediately output logs when receiving logs greater than or equal to this level)
    spdlog::set_pattern(OB_DEFAULT_LOG_FMT);
}

void Logger::loadEnvConfig() {
    auto envConfig       = EnvConfig::getInstance();
    int  globalLogLevel  = -1;
    int  fileLogLevel    = -1;
    int  consoleLogLevel = -1;
    if(!envConfig->getIntValue("Log.LogLevel", globalLogLevel)) {
        globalLogLevel = OB_DEFAULT_LOG_SEVERITY;
    }
    if(!envConfig->getIntValue("Log.FileLogLevel", fileLogLevel) && globalLogLevel >= OB_LOG_SEVERITY_DEBUG) {
        fileLogLevel = globalLogLevel;
    }
    if(!envConfig->getIntValue("Log.ConsoleLogLevel", consoleLogLevel) && globalLogLevel >= OB_LOG_SEVERITY_DEBUG) {
        consoleLogLevel = globalLogLevel;
    }

    if(config_.loadFileLogSeverityFromEnvConfig && fileLogLevel >= OB_LOG_SEVERITY_DEBUG) {
        config_.fileLogSeverity = (OBLogSeverity)fileLogLevel;
    }

    if(config_.loadConsoleLogSeverityFromEnvConfig && consoleLogLevel >= OB_LOG_SEVERITY_DEBUG) {
        config_.consoleLogSeverity = (OBLogSeverity)consoleLogLevel;
    }

    std::string dir;
    envConfig->getStringValue("Log.OutputDir", dir);
    if(dir.empty()) {
        dir = OB_DEFAULT_LOG_FILE_PATH;
    }

    std::string fileName;
    envConfig->getStringValue("Log.FileName", fileName);
    if(fileName.empty()) {
        fileName = OB_DEFAULT_LOG_FILE_NAME;
    }

    int maxFileSize = 0;
    envConfig->getIntValue("Log.MaxFileSize", maxFileSize);
    maxFileSize = maxFileSize * 1024 * 1024;  // MB to Byte
    if(maxFileSize == 0) {
        maxFileSize = OB_DEFAULT_MAX_FILE_SIZE;
    }

    int maxFileNum = 0;
    envConfig->getIntValue("Log.MaxFileNum", maxFileNum);
    if(maxFileNum == 0) {
        maxFileNum = OB_DEFAULT_MAX_FILE_NUM;
    }

    if(config_.loadFileLogPathFromEnvConfig) {
        config_.fileLogOutputDir = dir;

        config_.fileLogFileName = fileName;

        config_.fileLogMaxFileSize = maxFileSize;

        config_.fileLogMaxFileNum = maxFileNum;
    }
    bool async = false;
    if(envConfig->getBooleanValue("Log.Async", async)) {
        config_.async = async;
    }
}

void Logger::setLogSeverity(OBLogSeverity severity) {
    config_.loadFileLogSeverityFromEnvConfig     = false;
    config_.fileLogSeverity                      = severity;
    config_.loadConsoleLogSeverityFromEnvConfig  = false;
    config_.consoleLogSeverity                   = severity;
    config_.loadCallbackLogSeverityFromEnvConfig = false;
    config_.callbackLogSeverity                  = severity;

    std::lock_guard<std::mutex> lock(instanceMutex_);
    auto                        instance = instanceWeakPtr_.lock();
    if(instance) {
        instance->createFileSink();
        instance->createConsoleSink();
        instance->createCallbackSink();
        instance->updateDefaultSpdLogger();
    }
}

void Logger::setConsoleLogSeverity(OBLogSeverity severity) {
    config_.loadConsoleLogSeverityFromEnvConfig = false;
    config_.consoleLogSeverity                  = severity;

    std::lock_guard<std::mutex> lock(instanceMutex_);
    auto                        instance = instanceWeakPtr_.lock();
    if(instance) {
        instance->createConsoleSink();
        instance->updateDefaultSpdLogger();
    }
}

void Logger::setFileLogConfig(OBLogSeverity severity, const std::string &directory, uint32_t maxFileSize, uint32_t maxFileNum) {
    config_.loadFileLogSeverityFromEnvConfig = false;
    config_.fileLogSeverity                  = severity;
    config_.loadFileLogPathFromEnvConfig     = false;
    config_.fileLogOutputDir                 = directory;
    config_.fileLogMaxFileSize               = maxFileSize * 1024 * 1024;  // MB to Byte
    config_.fileLogMaxFileNum                = maxFileNum;
    if(directory.empty()) {
        config_.fileLogOutputDir = OB_DEFAULT_LOG_FILE_PATH;
    }
    if(maxFileSize == 0) {
        config_.fileLogMaxFileSize = OB_DEFAULT_MAX_FILE_SIZE;
    }
    if(maxFileNum == 0) {
        config_.fileLogMaxFileNum = OB_DEFAULT_MAX_FILE_NUM;
    }

    std::lock_guard<std::mutex> lock(instanceMutex_);
    auto                        instance = instanceWeakPtr_.lock();
    if(instance) {
        instance->createFileSink();
        instance->updateDefaultSpdLogger();
    }
}

void Logger::setLogCallback(OBLogSeverity severity, LogCallback logCallback) {
    config_.loadCallbackLogSeverityFromEnvConfig = false;
    config_.callbackLogSeverity                  = severity;
    config_.logCallback                          = logCallback;

    std::lock_guard<std::mutex> lock(instanceMutex_);
    auto                        instance = instanceWeakPtr_.lock();
    if(instance) {
        instance->createCallbackSink();
        instance->updateDefaultSpdLogger();
    }
}

}  // namespace libobsensor
