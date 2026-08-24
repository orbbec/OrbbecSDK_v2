// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/hpp/Utils.hpp>

#include <cstring>
#include <memory>
#include <string>

class TC_CPP_01_Context : public ContextTest {};

TEST_F(TC_CPP_01_Context, TC_CPP_01_01_context_default_create_destroy) {
    ASSERT_NE(ctx_, nullptr);
    auto devList = ctx_->queryDeviceList();
    ASSERT_NE(devList, nullptr);
}

TEST_F(TC_CPP_01_Context, TC_CPP_01_02_context_config_path) {
    {
        ob::Context ctxDefault("");
        auto        devList = ctxDefault.queryDeviceList();
        ASSERT_NE(devList, nullptr);
    }
    {
        ob::Context ctxInvalid("/non_existent_path/config.json");
        auto        devList = ctxInvalid.queryDeviceList();
        ASSERT_NE(devList, nullptr);
    }
}

TEST_F(TC_CPP_01_Context, TC_CPP_01_03_context_repeated_create_destroy) {
    for(int i = 0; i < 10; i++) {
        auto c = std::make_shared<ob::Context>();
        ASSERT_NE(c, nullptr) << "Iteration " << i;
        auto devList = c->queryDeviceList();
        ASSERT_NE(devList, nullptr) << "Iteration " << i;
        c.reset();
    }
}

TEST_F(TC_CPP_01_Context, TC_CPP_01_04_free_idle_memory) {
    ASSERT_NO_THROW(ctx_->freeIdleMemory());

    auto devList1 = ctx_->queryDeviceList();
    ASSERT_NE(devList1, nullptr);

    ASSERT_NO_THROW(ctx_->freeIdleMemory());

    auto devList2 = ctx_->queryDeviceList();
    ASSERT_NE(devList2, nullptr);
}

TEST_F(TC_CPP_01_Context, TC_CPP_01_05_uvc_backend) {
#if !defined(BUILD_USB_PAL)
    GTEST_SKIP() << "SDK built without USB PAL (BUILD_USB_PAL=OFF)";
#else
    bool        usbPalMissing = false;
    bool        hasUnexpected = false;
    std::string errMsg;

    auto testBackendType = [&](OBUvcBackendType backendType, const std::string &typeName) {
        if(usbPalMissing || hasUnexpected) {
            return;
        }

        try {
            ctx_->setUvcBackendType(backendType);
        }
        catch(const ob::Error &e) {
            errMsg = e.what() ? e.what() : "";
            if(errMsg.find("Usb pal is not exist") != std::string::npos || errMsg.find("BUILD_USB_PAL") != std::string::npos) {
                usbPalMissing = true;
                GTEST_SKIP() << "USB PAL backend unavailable at runtime (" << typeName << "): " << errMsg;
            }
            hasUnexpected = true;
        }
        catch(const std::exception &e) {
            errMsg        = e.what() ? e.what() : "std::exception";
            hasUnexpected = true;
        }
        catch(...) {
            errMsg        = "unknown exception";
            hasUnexpected = true;
        }

        ASSERT_FALSE(hasUnexpected) << "setUvcBackendType(" << typeName << ") failed: " << errMsg;
    };

    testBackendType(OB_UVC_BACKEND_TYPE_AUTO, "AUTO");

#ifdef __linux__
    testBackendType(OB_UVC_BACKEND_TYPE_LIBUVC, "LIBUVC");
    testBackendType(OB_UVC_BACKEND_TYPE_V4L2, "V4L2");
#endif
    testBackendType(OB_UVC_BACKEND_TYPE_AUTO, "AUTO reset");
#endif
}

TEST_F(TC_CPP_01_Context, TC_CPP_01_06_extension_plugin_directory) {
    ASSERT_NO_THROW(ob::Context::setExtensionsDirectory("."));

    ASSERT_NO_THROW(ob::Context::setExtensionsDirectory(""));

    ASSERT_NO_THROW(ob::Context::setExtensionsDirectory("/non_existent_path"));
}
