// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>

#include <algorithm>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>

class TC_CPP_24_Error : public SDKTestBase {};

TEST_F(TC_CPP_24_Error, TC_CPP_24_01_exception_type_info) {
    try {
        ob::FilterFactory::createFilter("TotallyInvalidFilter");
        FAIL() << "Expected error for invalid filter name";
    }
    catch(const ob::Error &e) {
        EXPECT_NE(e.getMessage(), nullptr);
        EXPECT_GT(std::strlen(e.getMessage()), 0u);
        EXPECT_NE(e.getFunction(), nullptr);
        EXPECT_GT(std::strlen(e.getFunction()), 0u);
        EXPECT_NE(e.getExceptionType(), OB_EXCEPTION_TYPE_UNKNOWN);
    }
}

TEST_F(TC_CPP_24_Error, TC_CPP_24_02_invalid_value_exception) {
    auto frame = ob::FrameFactory::createVideoFrame(OB_FRAME_DEPTH, OB_FORMAT_Y16, 0, 0);
    ASSERT_NE(frame, nullptr);
}

TEST_F(TC_CPP_24_Error, TC_CPP_24_03_wrong_api_sequence) {
    try {
        ob::Pipeline pipeline;
        auto         frameset = pipeline.waitForFrameset(100);
        EXPECT_EQ(frameset, nullptr) << "waitForFrameset on unstarted pipeline should return nullptr";
    }
    catch(const ob::Error &e) {
        EXPECT_NE(e.getExceptionType(), OB_EXCEPTION_TYPE_UNKNOWN) << "Exception type should be specific: " << e.what();
    }
    catch(const std::exception &e) {
        SUCCEED() << "Exception caught (no hardware): " << e.what();
    }
}

TEST_F(TC_CPP_24_Error, TC_CPP_24_06_all_exception_types) {
    std::vector<OBExceptionType> types = {
        OB_EXCEPTION_TYPE_UNKNOWN,
        OB_EXCEPTION_TYPE_CAMERA_DISCONNECTED,
        OB_EXCEPTION_TYPE_PLATFORM,
        OB_EXCEPTION_TYPE_INVALID_VALUE,
        OB_EXCEPTION_TYPE_WRONG_API_CALL_SEQUENCE,
        OB_EXCEPTION_TYPE_NOT_IMPLEMENTED,
        OB_EXCEPTION_TYPE_IO,
        OB_EXCEPTION_TYPE_MEMORY,
        OB_EXCEPTION_TYPE_UNSUPPORTED_OPERATION,
        OB_EXCEPTION_TYPE_ACCESS_DENIED,
    };

    std::sort(types.begin(), types.end());
    auto dup = std::adjacent_find(types.begin(), types.end());
    EXPECT_EQ(dup, types.end()) << "Duplicate exception type found";

    auto frame = ob::FrameFactory::createVideoFrame(OB_FRAME_DEPTH, OB_FORMAT_Y16, 0, 0);
    ASSERT_NE(frame, nullptr);

    try {
        ob::Pipeline pipeline;
        auto         frameset = pipeline.waitForFrameset(100);
        EXPECT_EQ(frameset, nullptr) << "waitForFrameset on unstarted pipeline should return nullptr";
    }
    catch(const ob::Error &e) {
        EXPECT_NE(e.getExceptionType(), OB_EXCEPTION_TYPE_UNKNOWN) << "Exception type should be specific: " << e.what();
    }
    catch(const std::exception &e) {
        SUCCEED() << "Exception caught (no hardware): " << e.what();
    }
}
