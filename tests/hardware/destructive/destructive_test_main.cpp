// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "../hw_test_main_common.hpp"

int main(int argc, char **argv) {
    std::string configPath = test_utils::extractConfigPath(argc, argv);
    TestConfig::loadFromFile(configPath);
    test_utils::setupTestLogger();
    hw_test_main::enableDestructiveMode();
    ::testing::InitGoogleTest(&argc, argv);
    hw_test_main::setDefaultFilterIfUnset(hw_test_main::kDestructiveTestFilter);
    return RUN_ALL_TESTS();
}
