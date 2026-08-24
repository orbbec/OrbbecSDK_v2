// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/hpp/Utils.hpp>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

class TC_CPP_13_Filter : public SDKTestBase {};

TEST_F(TC_CPP_13_Filter, TC_CPP_13_01_create_all_builtin_filters) {
    const std::vector<std::string> requiredFilterNames = {
        "DecimationFilter", "ThresholdFilter", "Align", "FormatConverter", "HDRMerge", "PointCloudFilter", "SequenceIdFilter",
    };

    const std::vector<std::string> optionalPrivateFilterNames = {
        "SpatialAdvancedFilter", "SpatialFastFilter",  "SpatialModerateFilter", "TemporalFilter",
        "HoleFillingFilter",     "NoiseRemovalFilter", "DisparityTransform",    "FalsePositiveFilter",
    };

    auto createFilterAndCheck = [](const std::string &name, bool required) {
        try {
            auto filter = ob::FilterFactory::createFilter(name);
            EXPECT_NE(filter, nullptr) << "Filter is null: " << name;
        }
        catch(const ob::Error &e) {
            std::string errMsg = e.what() ? e.what() : "";
            if(required) {
                FAIL() << "Failed to create required filter: " << name << " error: " << errMsg;
                return;
            }
            if(errMsg.find("Private filter library not activated") != std::string::npos || errMsg.find("Invalid filter name") != std::string::npos) {
                GTEST_LOG_(INFO) << "Skip optional filter in current environment: " << name << " error: " << errMsg;
                return;
            }
            FAIL() << "Failed to create optional filter: " << name << " error: " << errMsg;
        }
    };

    for(const auto &name: requiredFilterNames) {
        createFilterAndCheck(name, true);
    }

    for(const auto &name: optionalPrivateFilterNames) {
        createFilterAndCheck(name, false);
    }
}

TEST_F(TC_CPP_13_Filter, TC_CPP_13_02_create_invalid_filter) {
    EXPECT_THROW(ob::FilterFactory::createFilter("TotallyBogusFilter"), ob::Error) << "Expected error for invalid filter name";
}

TEST_F(TC_CPP_13_Filter, TC_CPP_13_05_filter_enable_disable) {
    auto filter = ob::FilterFactory::createFilter("DecimationFilter");
    ASSERT_NE(filter, nullptr);

    filter->enable(true);
    EXPECT_TRUE(filter->isEnabled());

    filter->enable(false);
    EXPECT_FALSE(filter->isEnabled());

    filter->enable(true);
    EXPECT_TRUE(filter->isEnabled());
}

TEST_F(TC_CPP_13_Filter, TC_CPP_13_06_filter_reset_and_config) {
    auto filter = ob::FilterFactory::createFilter("DecimationFilter");
    ASSERT_NE(filter, nullptr);

    auto schema = filter->getConfigSchema();
    EXPECT_GT(schema.size(), 0u);

    auto schemaVec = filter->getConfigSchemaVec();
    ASSERT_FALSE(schemaVec.empty());

    ASSERT_NO_THROW(filter->getConfigValue(schemaVec[0].name));
    ASSERT_NO_THROW(filter->setConfigValue(schemaVec[0].name, schemaVec[0].def));

    ASSERT_NO_THROW(filter->reset());
}

TEST_F(TC_CPP_13_Filter, TC_CPP_13_07_filter_type_check) {
    std::shared_ptr<ob::Filter> filter;
    try {
        filter = ob::FilterFactory::createFilter("PointCloudFilter");
    }
    catch(const ob::Error &) {
        GTEST_SKIP() << "PointCloudFilter not available on this build";
    }
    ASSERT_NE(filter, nullptr);

    EXPECT_EQ(filter->getName(), "PointCloudFilter");

    EXPECT_TRUE(filter->is<ob::PointCloudFilter>());
    auto pcf = filter->as<ob::PointCloudFilter>();
    ASSERT_NE(pcf, nullptr);
}

TEST_F(TC_CPP_13_Filter, TC_CPP_13_17_private_filter) {
    EXPECT_THROW(ob::FilterFactory::createPrivateFilter("SomePrivateFilter", "invalid_key"), ob::Error) << "Invalid private filter should throw ob::Error";
}
