// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/hpp/Utils.hpp>

#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

class TC_CPP_23_Logger : public SDKTestBase {};

TEST_F(TC_CPP_23_Logger, TC_CPP_23_01_log_severity_set) {
    const OBLogSeverity levels[] = {
        OB_LOG_SEVERITY_DEBUG, OB_LOG_SEVERITY_INFO, OB_LOG_SEVERITY_WARN, OB_LOG_SEVERITY_ERROR, OB_LOG_SEVERITY_FATAL, OB_LOG_SEVERITY_OFF,
    };
    for(auto level: levels) {
        EXPECT_NO_THROW(ob::Context::setLoggerSeverity(level)) << "Failed to set log level: " << (int)level;
    }
    ob::Context::setLoggerSeverity(OB_LOG_SEVERITY_WARN);
}

TEST_F(TC_CPP_23_Logger, TC_CPP_23_02_log_to_file) {
#ifdef _WIN32
    std::string logDir  = ".";
    std::string logFile = ".\\OrbbecSDK.log.txt";
#else
    std::string logDir  = "/tmp";
    std::string logFile = "/tmp/OrbbecSDK.log.txt";
#endif

    std::remove(logFile.c_str());

    ob::Context::setLoggerToFile(OB_LOG_SEVERITY_DEBUG, logDir.c_str());

    {
        ob::Context ctx;
        ctx.queryDeviceList();
    }

    bool exists = false;
    for(int elapsed = 0; elapsed < 2000; elapsed += 50) {
        std::ifstream ifs(logFile);
        if(ifs.good()) {
            exists = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    EXPECT_TRUE(exists) << "Log file not created within 2 s: " << logFile;
    if(exists) {
        std::ifstream ifs(logFile);
        std::string   content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        EXPECT_FALSE(content.empty()) << "Log file is empty";
    }

    ob::Context::setLoggerToFile(OB_LOG_SEVERITY_OFF, "");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::remove(logFile.c_str());

    ob::Context::setLoggerSeverity(OB_LOG_SEVERITY_WARN);
}

TEST_F(TC_CPP_23_Logger, TC_CPP_23_03_log_to_console) {
    EXPECT_NO_THROW(ob::Context::setLoggerToConsole(OB_LOG_SEVERITY_INFO));

    ob::Context::setLoggerSeverity(OB_LOG_SEVERITY_WARN);
}

TEST_F(TC_CPP_23_Logger, TC_CPP_23_04_log_callback) {
    auto logCount = std::make_shared<std::atomic<int>>(0);
    ob::Context::setLoggerToCallback(OB_LOG_SEVERITY_DEBUG, [logCount](OBLogSeverity, const char *) { (*logCount)++; });

    {
        ob::Context ctx;
        ctx.queryDeviceList();
    }

    for(int elapsed = 0; logCount->load() == 0 && elapsed < 1000; elapsed += 50) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    EXPECT_GT(logCount->load(), 0) << "No log callback received";

    ob::Context::setLoggerToCallback(OB_LOG_SEVERITY_OFF, nullptr);
    ob::Context::setLoggerSeverity(OB_LOG_SEVERITY_WARN);
}

TEST_F(TC_CPP_23_Logger, TC_CPP_23_05_external_message) {
    auto found = std::make_shared<std::atomic<bool>>(false);
    ob::Context::setLoggerToCallback(OB_LOG_SEVERITY_INFO, [found](OBLogSeverity, const char *msg) {
        if(msg && std::string(msg).find("NOHW_TEST_MARKER") != std::string::npos) {
            *found = true;
        }
    });

    {
        ob::Context ctx;
        ob::Context::logExternalMessage(OB_LOG_SEVERITY_INFO, "NOHW", "NOHW_TEST_MARKER", __FILE__, __func__, __LINE__);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(found->load()) << "External message not captured";

    ob::Context::setLoggerSeverity(OB_LOG_SEVERITY_WARN);
    ob::Context::setLoggerToCallback(OB_LOG_SEVERITY_OFF, nullptr);
}
