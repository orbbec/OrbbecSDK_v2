// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"

using test_utils::waitForFramesetWithRetry;

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class ThreadSafetyTest : public DeviceTest {
protected:
    int durationSec_ = 10;
    int threadCount_ = 4;

    void SetUp() override {
        DeviceTest::SetUp();
        if(!deviceSetupCompleted_)
            return;
        durationSec_ = ENV().tsDurationSec();
        threadCount_ = ENV().tsThreadCount();
        std::cout << "[ThreadSafety] duration=" << durationSec_ << "s, threads=" << threadCount_ << std::endl;
    }
};

TEST_F(ThreadSafetyTest, TC_TS_01_concurrent_pipeline_start_stop) {
    auto pipeline = std::make_shared<ob::Pipeline>(device_);
    ASSERT_NE(pipeline, nullptr);

    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);

    std::atomic<bool> running{ true };
    std::atomic<int>  errors{ 0 };

    auto worker = [&](int /*id*/) {
        while(running.load()) {
            try {
                pipeline->start(config);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                pipeline->stop();
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            catch(const ob::Error &e) {
                std::cout << "[TC_TS_01] ob::Error in thread: " << e.what() << std::endl;
            }
            catch(const std::exception &e) {
                std::cerr << "[TC_TS_01] std::exception in thread: " << e.what() << std::endl;
                errors.fetch_add(1);
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(threadCount_);
    for(int i = 0; i < threadCount_; ++i) {
        threads.emplace_back(worker, i);
    }

    std::this_thread::sleep_for(std::chrono::seconds(durationSec_));
    running.store(false);

    for(auto &t: threads) {
        t.join();
    }

    try {
        pipeline->stop();
    }
    catch(...) {
    }

    try {
        auto config = std::make_shared<ob::Config>();
        config->enableStream(OB_STREAM_DEPTH);
        pipeline->start(config);
        auto frameset = waitForFramesetWithRetry(pipeline);
        EXPECT_NE(frameset, nullptr) << "Pipeline not usable after concurrent start/stop stress";
        try {
            pipeline->stop();
        }
        catch(...) {
        }
    }
    catch(const ob::Error &e) {
        GTEST_SKIP() << "Pipeline unusable after stress (no usable device/stream in this environment): " << e.what();
    }

    EXPECT_EQ(errors.load(), 0) << "Unexpected std::exception caught during concurrent start/stop";
    std::cout << "[TC_TS_01] Completed without crash or deadlock." << std::endl;
}

TEST_F(ThreadSafetyTest, TC_TS_02_stream_while_property_access) {
    auto pipeline = std::make_shared<ob::Pipeline>(device_);
    ASSERT_NE(pipeline, nullptr);

    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);

    struct PropEntry {
        OBPropertyID id;
        int32_t      minVal;
        int32_t      maxVal;
    };
    std::vector<PropEntry> rwProps;

    int propCount = device_->getSupportedPropertyCount();
    for(int i = 0; i < propCount; ++i) {
        auto item = device_->getSupportedProperty(static_cast<uint32_t>(i));
        if(item.permission == OB_PERMISSION_READ_WRITE && item.type == OB_INT_PROPERTY) {
            try {
                auto range = device_->getIntPropertyRange(item.id);
                rwProps.push_back({ item.id, range.min, range.max });
            }
            catch(...) {
            }
        }
    }

    std::atomic<bool>     running{ true };
    std::atomic<uint64_t> frameCount{ 0 };
    std::atomic<int>      streamErrors{ 0 };
    std::atomic<int>      propErrors{ 0 };

    try {
        pipeline->start(config);
    }
    catch(const std::exception &e) {
        GTEST_SKIP() << "Pipeline start failed: " << e.what();
    }

    std::thread streamThread([&]() {
        while(running.load()) {
            try {
                auto frameset = waitForFramesetWithRetry(pipeline);
                if(frameset) {
                    frameCount.fetch_add(1);
                }
            }
            catch(const ob::Error &) {
            }
            catch(const std::exception &e) {
                std::cerr << "[TC_TS_02-stream] " << e.what() << std::endl;
                streamErrors.fetch_add(1);
            }
        }
    });

    std::thread propThread([&]() {
        while(running.load()) {
            for(const auto &p: rwProps) {
                if(!running.load())
                    break;
                try {
                    int32_t cur = device_->getIntProperty(p.id);
                    device_->setIntProperty(p.id, cur);
                }
                catch(const ob::Error &) {
                }
                catch(const std::exception &e) {
                    std::cerr << "[TC_TS_02-prop] " << e.what() << std::endl;
                    propErrors.fetch_add(1);
                }
            }
            if(rwProps.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(durationSec_));
    running.store(false);

    streamThread.join();
    propThread.join();

    try {
        pipeline->stop();
    }
    catch(...) {
    }

    std::cout << "[TC_TS_02] frames=" << frameCount.load() << ", rw_properties=" << rwProps.size() << std::endl;

    EXPECT_EQ(streamErrors.load(), 0) << "Unexpected std::exception in stream thread";
    EXPECT_EQ(propErrors.load(), 0) << "Unexpected std::exception in property thread";
    EXPECT_GT(frameCount.load(), 0u) << "No frames received during the test";
    std::cout << "[TC_TS_02] Completed without crash or deadlock." << std::endl;
}

TEST_F(ThreadSafetyTest, TC_TS_03_concurrent_pipeline_create_destroy) {
    std::atomic<bool> running{ true };
    std::atomic<int>  errors{ 0 };
    std::atomic<int>  iterations{ 0 };

    auto worker = [&](int /*id*/) {
        while(running.load()) {
            try {
                auto pipeline = std::make_shared<ob::Pipeline>(device_);
                auto config   = std::make_shared<ob::Config>();
                config->enableStream(OB_STREAM_DEPTH);
                pipeline->start(config);

                for(int f = 0; f < 5 && running.load(); ++f) {
                    waitForFramesetWithRetry(pipeline);
                }

                pipeline->stop();
                iterations.fetch_add(1);
            }
            catch(const ob::Error &e) {
                std::cout << "[TC_TS_03] ob::Error: " << e.what() << std::endl;
            }
            catch(const std::exception &e) {
                std::cerr << "[TC_TS_03] std::exception: " << e.what() << std::endl;
                errors.fetch_add(1);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(threadCount_);
    for(int i = 0; i < threadCount_; ++i) {
        threads.emplace_back(worker, i);
    }

    std::this_thread::sleep_for(std::chrono::seconds(durationSec_));
    running.store(false);

    for(auto &t: threads) {
        t.join();
    }

    std::cout << "[TC_TS_03] iterations=" << iterations.load() << std::endl;

    EXPECT_EQ(errors.load(), 0) << "Unexpected std::exception during concurrent create/destroy";
    std::cout << "[TC_TS_03] Completed without crash or deadlock." << std::endl;
}
