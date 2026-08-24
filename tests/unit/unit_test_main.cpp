// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"

int main(int argc, char **argv) {
    std::string configPath = test_utils::extractConfigPath(argc, argv);
    TestConfig::loadFromFile(configPath);
    test_utils::setupTestLogger();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
