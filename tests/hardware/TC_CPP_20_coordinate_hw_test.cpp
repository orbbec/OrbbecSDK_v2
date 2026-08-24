// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/h/Utils.h>
#include "hw_test_helpers.hpp"

#include <cstring>
#include <fstream>
#include <string>

using namespace hw_test;

class TC_CPP_20_Coord_HW : public PipelineTest {};

TEST_F(TC_CPP_20_Coord_HW, TC_CPP_20_05_save_ply) {
    pipeline_.reset();
    ASSERT_TRUE(rebootAndReconnect(ctx_, cfg_.deviceSerial(), device_, devInfo_)) << "Device did not reconnect after reboot";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    pipeline_ = std::make_shared<ob::Pipeline>(device_);

    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    config->enableStream(OB_STREAM_COLOR);
    config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);
    pipeline_->start(config);

    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::shared_ptr<ob::FrameSet> fs;
    for(int i = 0; i < 5; ++i) {
        fs = waitForFramesetWithRetry(pipeline_, 10);
        if(fs && fs->getDepthFrame())
            break;
    }
    ASSERT_NE(fs, nullptr) << "No frameset received within retry limit";
    auto depth = fs->getDepthFrame();
    ASSERT_NE(depth, nullptr) << "No depth frame in frameset";
    pipeline_->stop();

#ifdef _WIN32
    std::string plyFile = "ob_test_points.ply";
#else
    std::string plyFile = "/tmp/ob_test_points.ply";
#endif
    std::remove(plyFile.c_str());

    std::shared_ptr<ob::PointCloudFilter> pcFilter;
    try {
        pcFilter = std::make_shared<ob::PointCloudFilter>();
    }
    catch(const ob::Error &) {
        GTEST_SKIP() << "No PointCloudFilter";
    }

    auto result = pcFilter->process(depth);
    if(result) {
        ob_error *error = nullptr;
        ob_save_pointcloud_to_ply(plyFile.c_str(), const_cast<ob_frame *>(result->getImpl()), false, false, 0.0f, &error);
        if(error)
            ob_delete_error(error);
        {
            std::ifstream ifs(plyFile);
            EXPECT_TRUE(ifs.good()) << "ASCII PLY file not created";
        }
        std::remove(plyFile.c_str());

        error = nullptr;
        ob_save_pointcloud_to_ply(plyFile.c_str(), const_cast<ob_frame *>(result->getImpl()), true, false, 0.0f, &error);
        if(error)
            ob_delete_error(error);
        {
            std::ifstream ifs(plyFile);
            EXPECT_TRUE(ifs.good()) << "Binary PLY file not created";
        }
        std::remove(plyFile.c_str());
    }

    auto resultFs = pcFilter->process(fs);
    if(resultFs) {
        ob_error *error = nullptr;
        ob_save_pointcloud_to_ply(plyFile.c_str(), const_cast<ob_frame *>(resultFs->getImpl()), false, false, 0.0f, &error);
        if(error)
            ob_delete_error(error);
        {
            std::ifstream ifs(plyFile);
            EXPECT_TRUE(ifs.good()) << "RGB ASCII PLY file not created";
        }
        std::remove(plyFile.c_str());
    }

    std::remove(plyFile.c_str());
}
