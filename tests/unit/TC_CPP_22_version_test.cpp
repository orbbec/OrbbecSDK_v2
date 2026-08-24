// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/hpp/Utils.hpp>

#include <cstring>
#include <memory>
#include <string>

class TC_CPP_22_Version : public SDKTestBase {};

TEST_F(TC_CPP_22_Version, TC_CPP_22_01_full_version) {
    int ver   = ob::Version::getVersion();
    int major = ob::Version::getMajor();
    int minor = ob::Version::getMinor();
    int patch = ob::Version::getPatch();

    EXPECT_GT(ver, 0) << "Full version should be > 0";
    EXPECT_GT(major, 0) << "Major version should be > 0";
    EXPECT_GE(minor, 0);
    EXPECT_GE(patch, 0);
    int computed = major * 10000 + minor * 100 + patch;
    EXPECT_EQ(ver, computed) << "Version mismatch: getVersion()=" << ver << " but major*10000+minor*100+patch=" << computed;
}

TEST_F(TC_CPP_22_Version, TC_CPP_22_02_stage_version) {
    const char *stage = ob::Version::getStageVersion();
    if(!stage || std::strlen(stage) == 0) {
        SUCCEED() << "No stage version (typical for release builds)";
        return;
    }
    std::string s(stage);
    bool known = (s == "alpha" || s == "beta" || s == "rc" || s == "release" || s.find("alpha") != std::string::npos || s.find("beta") != std::string::npos
                  || s.find("rc") != std::string::npos);
    EXPECT_TRUE(known) << "Unexpected stage version: " << s;
}
