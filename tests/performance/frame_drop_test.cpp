// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#endif

struct PerfConfig {
    int durationSec = 60;
    int cpuThreads  = 0;
    int ioThreads   = 2;

    static PerfConfig &instance() {
        static PerfConfig cfg;
        return cfg;
    }

private:
    PerfConfig() {
        durationSec = ENV().perfDurationSec();
        cpuThreads  = ENV().perfCpuThreads();
        ioThreads   = ENV().perfIoThreads();
        std::cout << "[PerfConfig] duration=" << durationSec << "s"
                  << "  cpu-threads=" << (cpuThreads == 0 ? "auto" : std::to_string(cpuThreads)) << "  io-threads=" << ioThreads << "\n";
    }
};

static std::string streamLabel(OBFrameType type) {
    switch(type) {
    case OB_FRAME_DEPTH:
        return "depth";
    case OB_FRAME_COLOR:
        return "color";
    case OB_FRAME_IR:
    case OB_FRAME_IR_LEFT:
    case OB_FRAME_IR_RIGHT:
        return "ir";
    default:
        return "other_" + std::to_string(static_cast<int>(type));
    }
}

static OBFrameType normalizeFrameType(OBFrameType type) {
    if(type == OB_FRAME_IR_LEFT || type == OB_FRAME_IR_RIGHT)
        return OB_FRAME_IR;
    return type;
}

struct FrameRecord {
    uint64_t sdkIndex;
    int64_t  metaFrameNumber;
    uint64_t hwTimestampUs;
    uint64_t sysTimestampUs;
    uint64_t globalTimestampUs;
    int      fps;
};

struct StreamStats {
    std::string streamName;
    uint64_t    totalFrames        = 0;
    int         fps                = 0;
    double      expectedIntervalUs = 0;

    int64_t  prevMetaFrameNumber = -1;
    uint64_t prevSdkIndex        = 0;
    uint64_t metaIndexDrops      = 0;
    uint64_t sdkIndexDrops       = 0;
    bool     metaObserved        = false;

    uint64_t prevHwTs          = 0;
    uint64_t prevSysTs         = 0;
    uint64_t prevGlobalTs      = 0;
    uint64_t hwTsAnomalies     = 0;
    uint64_t sysTsAnomalies    = 0;
    uint64_t globalTsAnomalies = 0;
};

struct ResourceSample {
    double elapsedSec;
    float  cpuPercent;
    float  memoryMB;
};

#ifdef _WIN32

static float getProcessCpuUsage() {
    static int64_t sLastTime       = 0;
    static int64_t sLastSystemTime = 0;

    FILETIME now, creation, exitT, kernel, user;
    GetSystemTimeAsFileTime(&now);

    if(!GetProcessTimes(GetCurrentProcess(), &creation, &exitT, &kernel, &user))
        return -1.0f;

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int cpuNum = si.dwNumberOfProcessors;

    auto toU64 = [](const FILETIME &ft) -> int64_t {
        LARGE_INTEGER li;
        li.LowPart  = ft.dwLowDateTime;
        li.HighPart = ft.dwHighDateTime;
        return li.QuadPart;
    };

    int64_t systemTime = (toU64(kernel) + toU64(user)) / cpuNum;
    int64_t timeNow    = toU64(now);

    if(sLastTime == 0) {
        sLastSystemTime = systemTime;
        sLastTime       = timeNow;
        return 0.0f;
    }

    int64_t sysDelta  = systemTime - sLastSystemTime;
    int64_t timeDelta = timeNow - sLastTime;
    sLastSystemTime   = systemTime;
    sLastTime         = timeNow;

    if(timeDelta == 0)
        return 0.0f;
    return static_cast<float>(static_cast<double>(sysDelta) * 100.0 / static_cast<double>(timeDelta));
}

static float getProcessMemoryMB() {
    PROCESS_MEMORY_COUNTERS mc;
    if(GetProcessMemoryInfo(GetCurrentProcess(), &mc, sizeof(mc)))
        return static_cast<float>(mc.WorkingSetSize) / (1024.0f * 1024.0f);
    return -1.0f;
}

#elif defined(__linux__)

static float getProcessCpuUsage() {
    auto               pid = getpid();
    std::ostringstream cmd;
    cmd << "top -bn1 -p " << pid << " | grep " << pid << " | awk '{print $9}'";
    FILE *fp = popen(cmd.str().c_str(), "r");
    if(!fp)
        return -1.0f;
    char  buf[128];
    float usage = -1.0f;
    if(fgets(buf, sizeof(buf), fp))
        usage = std::strtof(buf, nullptr);
    pclose(fp);
    return usage;
}

static float getProcessMemoryMB() {
    std::ifstream sf("/proc/self/status");
    std::string   line;
    while(std::getline(sf, line)) {
        if(line.compare(0, 6, "VmRSS:") == 0) {
            long kb = 0;
            std::sscanf(line.c_str(), "VmRSS: %ld", &kb);
            return static_cast<float>(kb) / 1024.0f;
        }
    }
    return -1.0f;
}

#else

static float getProcessCpuUsage() {
    return -1.0f;
}
static float getProcessMemoryMB() {
    return -1.0f;
}

#endif

class CpuStressor {
public:
    ~CpuStressor() {
        stop();
    }

    void start(unsigned int numThreads = 0) {
        if(numThreads == 0) {
            numThreads = std::thread::hardware_concurrency();
            if(numThreads == 0)
                numThreads = 4;
        }
        running_ = true;
        for(unsigned int i = 0; i < numThreads; ++i) {
            threads_.emplace_back([this]() {
                volatile double val = 1.0;
                while(running_.load(std::memory_order_relaxed)) {
                    for(int j = 0; j < 100000; ++j) {
                        val = val * 1.000001 + 0.000001;
                        if(val > 1e10)
                            val = 1.0;
                    }
                }
            });
        }
        std::cout << "[CpuStressor] Started " << numThreads << " stress threads\n";
    }

    void stop() {
        running_ = false;
        for(auto &t: threads_) {
            if(t.joinable())
                t.join();
        }
        threads_.clear();
    }

private:
    std::atomic<bool>        running_{ false };
    std::vector<std::thread> threads_;
};

class IoStressor {
public:
    ~IoStressor() {
        stop();
    }

    void start(int numThreads = 2) {
        running_ = true;
        for(int i = 0; i < numThreads; ++i) {
            threads_.emplace_back([this, i]() {
                std::string       filename = "perf_io_stress_" + std::to_string(i) + ".tmp";
                std::vector<char> buf(1024 * 1024, 'X');

                while(running_.load(std::memory_order_relaxed)) {
                    {
                        std::ofstream f(filename, std::ios::binary);
                        for(int j = 0; j < 10 && running_.load(std::memory_order_relaxed); ++j) {
                            f.write(buf.data(), static_cast<std::streamsize>(buf.size()));
                        }
                    }
                    {
                        std::ifstream f(filename, std::ios::binary);
                        while(f.read(buf.data(), static_cast<std::streamsize>(buf.size())) && running_.load(std::memory_order_relaxed)) {
                        }
                    }
                }
                std::remove(filename.c_str());
            });
        }
        std::cout << "[IoStressor] Started " << numThreads << " IO stress threads\n";
    }

    void stop() {
        running_ = false;
        for(auto &t: threads_) {
            if(t.joinable())
                t.join();
        }
        threads_.clear();
    }

private:
    std::atomic<bool>        running_{ false };
    std::vector<std::thread> threads_;
};

class PerfStreamTest : public DeviceTest {
protected:
    std::shared_ptr<ob::Pipeline> pipeline_;
    int                           durationSec_ = 60;

    std::mutex                                            mu_;
    std::map<OBFrameType, StreamStats>                    statsMap_;
    std::map<OBFrameType, std::unique_ptr<std::ofstream>> csvMap_;

    std::atomic<bool>           resourceRunning_{ false };
    std::thread                 resourceThread_;
    std::vector<ResourceSample> resourceSamples_;

    std::string csvPrefix_;

    void SetUp() override {
        DeviceTest::SetUp();
        if(!deviceSetupCompleted_)
            return;
        pipeline_ = std::make_shared<ob::Pipeline>(device_);
        ASSERT_NE(pipeline_, nullptr);
        durationSec_ = PerfConfig::instance().durationSec;
    }

    void TearDown() override {
        stopResourceMonitor();
        if(pipeline_) {
            try {
                pipeline_->stop();
            }
            catch(...) {
            }
            pipeline_.reset();
        }
        closeCsvFiles();
        DeviceTest::TearDown();
    }

    void openCsv(OBFrameType normalizedType) {
        std::string name = csvPrefix_ + "_" + streamLabel(normalizedType) + "_frames.csv";
        auto        ofs  = std::unique_ptr<std::ofstream>(new std::ofstream(name));
        if(ofs->is_open()) {
            *ofs << "SdkIndex,MetaFrameNumber,"
                    "HwTimestamp_us,SysTimestamp_us,GlobalTimestamp_us,FPS\n";
        }
        csvMap_[normalizedType] = std::move(ofs);
    }

    void writeCsvRow(OBFrameType normalizedType, const FrameRecord &r) {
        auto it = csvMap_.find(normalizedType);
        if(it == csvMap_.end() || !it->second || !it->second->is_open())
            return;
        *(it->second) << r.sdkIndex << "," << r.metaFrameNumber << "," << r.hwTimestampUs << "," << r.sysTimestampUs << "," << r.globalTimestampUs << ","
                      << r.fps << "\n";
    }

    void writeResourceCsv() {
        std::string   name = csvPrefix_ + "_resource.csv";
        std::ofstream ofs(name);
        if(!ofs.is_open())
            return;
        ofs << "ElapsedSec,CPU_Percent,Memory_MB\n";
        for(auto &s: resourceSamples_) {
            ofs << std::fixed << std::setprecision(2) << s.elapsedSec << "," << s.cpuPercent << "," << s.memoryMB << "\n";
        }
        ofs.close();
    }

    void closeCsvFiles() {
        csvMap_.clear();
    }

    void processFrame(std::shared_ptr<ob::Frame> frame) {
        if(!frame)
            return;

        OBFrameType ntype = normalizeFrameType(frame->getType());
        if(ntype != OB_FRAME_DEPTH && ntype != OB_FRAME_COLOR && ntype != OB_FRAME_IR)
            return;

        FrameRecord rec;
        rec.sdkIndex          = frame->getIndex();
        rec.hwTimestampUs     = frame->getTimeStampUs();
        rec.sysTimestampUs    = frame->getSystemTimeStampUs();
        rec.globalTimestampUs = frame->getGlobalTimeStampUs();
        rec.metaFrameNumber   = frame->hasMetadata(OB_FRAME_METADATA_TYPE_FRAME_NUMBER) ? frame->getMetadataValue(OB_FRAME_METADATA_TYPE_FRAME_NUMBER) : -1;

        rec.fps = 0;
        try {
            auto profile = frame->getStreamProfile();
            if(profile) {
                auto vp = profile->as<ob::VideoStreamProfile>();
                if(vp)
                    rec.fps = static_cast<int>(vp->getFps());
            }
        }
        catch(...) {
        }

        std::lock_guard<std::mutex> lock(mu_);
        auto                       &st = statsMap_[ntype];

        if(st.totalFrames == 0) {
            st.streamName         = streamLabel(ntype);
            st.fps                = rec.fps;
            st.expectedIntervalUs = (rec.fps > 0) ? (1e6 / static_cast<double>(rec.fps)) : 0;
        }

        if(st.totalFrames > 0) {
            if(rec.sdkIndex > st.prevSdkIndex) {
                st.sdkIndexDrops += (rec.sdkIndex - st.prevSdkIndex - 1);
            }
            else {
                st.sdkIndexDrops++;
            }

            if(rec.metaFrameNumber >= 0 && st.prevMetaFrameNumber >= 0) {
                st.metaObserved = true;
                if(rec.metaFrameNumber > st.prevMetaFrameNumber) {
                    st.metaIndexDrops += static_cast<uint64_t>(rec.metaFrameNumber - st.prevMetaFrameNumber - 1);
                }
                else if(rec.metaFrameNumber != st.prevMetaFrameNumber) {
                    st.metaIndexDrops++;
                }
            }

            if(st.expectedIntervalUs > 0) {
                double tolerance = st.expectedIntervalUs * 0.5;

                if(st.prevHwTs > 0 && rec.hwTimestampUs > 0) {
                    double diff = static_cast<double>(rec.hwTimestampUs) - static_cast<double>(st.prevHwTs);
                    if(std::fabs(diff - st.expectedIntervalUs) > tolerance)
                        st.hwTsAnomalies++;
                }
                if(st.prevSysTs > 0 && rec.sysTimestampUs > 0) {
                    double diff = static_cast<double>(rec.sysTimestampUs) - static_cast<double>(st.prevSysTs);
                    if(std::fabs(diff - st.expectedIntervalUs) > tolerance)
                        st.sysTsAnomalies++;
                }
                if(st.prevGlobalTs > 0 && rec.globalTimestampUs > 0) {
                    double diff = static_cast<double>(rec.globalTimestampUs) - static_cast<double>(st.prevGlobalTs);
                    if(std::fabs(diff - st.expectedIntervalUs) > tolerance)
                        st.globalTsAnomalies++;
                }
            }
        }

        st.prevSdkIndex        = rec.sdkIndex;
        st.prevMetaFrameNumber = rec.metaFrameNumber;
        st.prevHwTs            = rec.hwTimestampUs;
        st.prevSysTs           = rec.sysTimestampUs;
        st.prevGlobalTs        = rec.globalTimestampUs;
        st.totalFrames++;

        writeCsvRow(ntype, rec);
    }

    void startResourceMonitor() {
        resourceSamples_.clear();
        getProcessCpuUsage();

        resourceRunning_ = true;
        resourceThread_  = std::thread([this]() {
            auto start = std::chrono::steady_clock::now();
            while(resourceRunning_.load(std::memory_order_relaxed)) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if(!resourceRunning_.load(std::memory_order_relaxed))
                    break;

                auto   now     = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(now - start).count();
                float  cpu     = getProcessCpuUsage();
                float  mem     = getProcessMemoryMB();
                resourceSamples_.push_back({ elapsed, cpu, mem });
            }
        });
    }

    void stopResourceMonitor() {
        resourceRunning_ = false;
        if(resourceThread_.joinable())
            resourceThread_.join();
    }

    void printSummary() {
        std::lock_guard<std::mutex> lock(mu_);
        std::cout << "\n========== Performance Test Summary [" << csvPrefix_ << "] ==========\n";
        std::cout << "Duration: " << durationSec_ << " seconds\n\n";

        for(auto &kv: statsMap_) {
            auto &st = kv.second;
            std::cout << "--- " << st.streamName << " (fps=" << st.fps << ") ---\n"
                      << "  Total frames:              " << st.totalFrames << "\n"
                      << "  SDK index discontinuities: " << st.sdkIndexDrops << "\n"
                      << "  Meta HW index drops:       " << st.metaIndexDrops << "\n"
                      << "  HW timestamp anomalies:    " << st.hwTsAnomalies << "\n"
                      << "  Sys timestamp anomalies:   " << st.sysTsAnomalies << "\n"
                      << "  Global ts anomalies:       " << st.globalTsAnomalies << "\n"
                      << "  Expected interval (us):    " << std::fixed << std::setprecision(1) << st.expectedIntervalUs << "\n\n";
        }
        std::cout.unsetf(std::ios::fixed);

        if(!resourceSamples_.empty()) {
            float avgCpu = 0, avgMem = 0, maxCpu = 0, maxMem = 0;
            for(auto &s: resourceSamples_) {
                avgCpu += s.cpuPercent;
                avgMem += s.memoryMB;
                if(s.cpuPercent > maxCpu)
                    maxCpu = s.cpuPercent;
                if(s.memoryMB > maxMem)
                    maxMem = s.memoryMB;
            }
            float n = static_cast<float>(resourceSamples_.size());
            avgCpu /= n;
            avgMem /= n;

            std::cout << "--- Resource Usage ---\n"
                      << "  Avg CPU: " << std::fixed << std::setprecision(2) << avgCpu << " %\n"
                      << "  Max CPU: " << maxCpu << " %\n"
                      << "  Avg Mem: " << avgMem << " MB\n"
                      << "  Max Mem: " << maxMem << " MB\n";
            std::cout.unsetf(std::ios::fixed);
        }
        std::cout << "====================================================\n";
    }

    void assertDropRate(double maxRate) {
        std::lock_guard<std::mutex> lock(mu_);
        ASSERT_FALSE(statsMap_.empty()) << "No frames received during the " << durationSec_ << " s test window";
        for(auto &kv: statsMap_) {
            auto &st = kv.second;
            EXPECT_GT(st.totalFrames, 0u) << st.streamName << ": received 0 frames in " << durationSec_ << " s";

            if(st.totalFrames == 0)
                continue;

            double sdkDropRate = static_cast<double>(st.sdkIndexDrops) / static_cast<double>(st.totalFrames);
            EXPECT_LT(sdkDropRate, maxRate) << st.streamName << " sdk-index drop rate: " << std::fixed << std::setprecision(2) << (sdkDropRate * 100.0) << " %"
                                            << " (" << st.sdkIndexDrops << " / " << st.totalFrames << ")";

            if(st.metaObserved) {
                double metaDropRate = static_cast<double>(st.metaIndexDrops) / static_cast<double>(st.totalFrames);
                EXPECT_LT(metaDropRate, maxRate) << st.streamName << " meta-index drop rate: " << std::fixed << std::setprecision(2) << (metaDropRate * 100.0)
                                                 << " %"
                                                 << " (" << st.metaIndexDrops << " / " << st.totalFrames << ")";
            }
        }
    }

    bool runStreamingTest(const std::string &prefix) {
        csvPrefix_ = prefix;
        statsMap_.clear();

        try {
            if(device_->isGlobalTimestampSupported()) {
                device_->enableGlobalTimestamp(true);
            }
        }
        catch(...) {
        }

        auto config = std::make_shared<ob::Config>();
        config->enableStream(OB_STREAM_DEPTH);
        config->enableStream(OB_STREAM_COLOR);
        config->enableStream(OB_STREAM_IR);
        config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_DISABLE);

        openCsv(OB_FRAME_DEPTH);
        openCsv(OB_FRAME_COLOR);
        openCsv(OB_FRAME_IR);

        startResourceMonitor();

        auto callback = [this](std::shared_ptr<ob::FrameSet> frameset) {
            if(!frameset)
                return;
            uint32_t count = frameset->getCount();
            for(uint32_t i = 0; i < count; ++i) {
                auto frame = frameset->getFrameByIndex(i);
                if(frame)
                    processFrame(frame);
            }
        };

        bool started = false;
        try {
            pipeline_->start(config, callback);
            started = true;
        }
        catch(const ob::Error &e) {
            std::cout << "[PerfTest] Start with depth+color+IR failed: " << e.what() << "\n";
        }

        if(!started) {
            std::cout << "[PerfTest] Retrying with depth + color only ...\n";
            config = std::make_shared<ob::Config>();
            config->enableStream(OB_STREAM_DEPTH);
            config->enableStream(OB_STREAM_COLOR);
            config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_DISABLE);
            try {
                pipeline_->start(config, callback);
                started = true;
            }
            catch(const ob::Error &e) {
                stopResourceMonitor();
                std::cout << "[PerfTest] Pipeline could not start: " << e.what() << "\n";
                return false;
            }
        }

        std::cout << "[PerfTest] Streaming (" << prefix << ") for " << durationSec_ << " s ...\n";
        std::this_thread::sleep_for(std::chrono::seconds(durationSec_));

        try {
            pipeline_->stop();
        }
        catch(...) {
        }
        stopResourceMonitor();

        writeResourceCsv();
        for(auto &kv: csvMap_) {
            if(kv.second && kv.second->is_open())
                kv.second->flush();
        }
        std::cout << "[PerfTest] Reports saved: " << csvPrefix_ << "_*_frames.csv, " << csvPrefix_ << "_resource.csv\n";
        printSummary();
        return true;
    }
};

TEST_F(PerfStreamTest, TC_PERF_01_frame_drop_detection) {
    if(!runStreamingTest("baseline")) {
        GTEST_SKIP() << "Pipeline could not start (no usable device/stream in this environment)";
    }
    assertDropRate(0.15);
}

TEST_F(PerfStreamTest, TC_PERF_02_frame_drop_under_cpu_stress) {
    unsigned int cpuThreads = static_cast<unsigned int>(PerfConfig::instance().cpuThreads);

    CpuStressor stressor;
    stressor.start(cpuThreads);

    if(!runStreamingTest("cpu_stress")) {
        GTEST_SKIP() << "Pipeline could not start (no usable device/stream in this environment)";
    }
    assertDropRate(0.20);
}

TEST_F(PerfStreamTest, TC_PERF_03_frame_drop_under_io_stress) {
    int ioThreads = PerfConfig::instance().ioThreads;

    IoStressor stressor;
    stressor.start(ioThreads);

    if(!runStreamingTest("io_stress")) {
        GTEST_SKIP() << "Pipeline could not start (no usable device/stream in this environment)";
    }
    assertDropRate(0.20);
}
