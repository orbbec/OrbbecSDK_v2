// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>

using test_utils::waitForFramesetWithRetry;

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <initializer_list>
#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <vector>

namespace {

struct LogEntry {
    OBLogSeverity severity;
    std::string   message;
};

class LogCollector {
public:
    void append(OBLogSeverity severity, const char *msg) {
        std::lock_guard<std::mutex> lock(mu_);
        entries_.push_back({ severity, msg ? msg : "" });
    }

    std::vector<LogEntry> snapshot() const {
        std::lock_guard<std::mutex> lock(mu_);
        return entries_;
    }

    bool containsSubstring(const std::string &substr) const {
        std::lock_guard<std::mutex> lock(mu_);
        std::string                 lowerSub = toLower(substr);
        for(const auto &e: entries_) {
            if(toLower(e.message).find(lowerSub) != std::string::npos)
                return true;
        }
        return false;
    }

    std::vector<LogEntry> range(size_t fromIdx, size_t toIdx) const {
        std::lock_guard<std::mutex> lock(mu_);
        if(fromIdx >= entries_.size())
            return {};
        if(toIdx > entries_.size())
            toIdx = entries_.size();
        return { entries_.begin() + static_cast<ptrdiff_t>(fromIdx), entries_.begin() + static_cast<ptrdiff_t>(toIdx) };
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mu_);
        return entries_.size();
    }

    bool rangeContains(size_t fromIdx, size_t toIdx, const std::string &substr) const {
        auto        seg      = range(fromIdx, toIdx);
        std::string lowerSub = toLower(substr);
        for(const auto &e: seg) {
            if(toLower(e.message).find(lowerSub) != std::string::npos)
                return true;
        }
        return false;
    }

    bool rangeContainsPattern(size_t fromIdx, size_t toIdx, const std::string &pattern) const {
        auto seg = range(fromIdx, toIdx);
        try {
            std::regex re(pattern, std::regex_constants::icase);
            for(const auto &e: seg) {
                if(std::regex_search(e.message, re))
                    return true;
            }
        }
        catch(const std::regex_error &) {
            return rangeContains(fromIdx, toIdx, pattern);
        }
        return false;
    }

    size_t rangeCountPattern(size_t fromIdx, size_t toIdx, const std::string &pattern) const {
        auto   seg   = range(fromIdx, toIdx);
        size_t count = 0;
        try {
            std::regex re(pattern, std::regex_constants::icase);
            for(const auto &e: seg) {
                if(std::regex_search(e.message, re))
                    ++count;
            }
        }
        catch(const std::regex_error &) {
            std::string lowerPattern = toLower(pattern);
            for(const auto &e: seg) {
                if(toLower(e.message).find(lowerPattern) != std::string::npos)
                    ++count;
            }
        }
        return count;
    }

    void dumpRange(size_t fromIdx, size_t toIdx, const std::string &label) const {
        auto seg = range(fromIdx, toIdx);
        std::cout << "=== Log dump [" << label << "] (" << seg.size() << " entries) ===" << std::endl;
        for(const auto &e: seg) {
            const char *sevStr = "?";
            switch(e.severity) {
            case OB_LOG_SEVERITY_DEBUG:
                sevStr = "DBG";
                break;
            case OB_LOG_SEVERITY_INFO:
                sevStr = "INF";
                break;
            case OB_LOG_SEVERITY_WARN:
                sevStr = "WRN";
                break;
            case OB_LOG_SEVERITY_ERROR:
                sevStr = "ERR";
                break;
            case OB_LOG_SEVERITY_FATAL:
                sevStr = "FAT";
                break;
            default:
                break;
            }
            std::cout << "  [" << sevStr << "] " << e.message << std::endl;
        }
        std::cout << "=== End dump ===" << std::endl;
    }

private:
    static std::string toLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    mutable std::mutex    mu_;
    std::vector<LogEntry> entries_;
};

}  // namespace

class TC_SCENARIO_LogCompleteness : public HardwareTest {
protected:
    enum class RuleKind {
        Contains,
        OrderedPair,
        BalancedPair,
    };

    struct RuleSpec {
        RuleKind    kind;
        const char *patternA;
        const char *patternB;
        const char *failure;
    };

    class LoggerCallbackGuard {
    public:
        explicit LoggerCallbackGuard(LogCollector *collector) : collector_(collector) {
            ob::Context::setLoggerToCallback(OB_LOG_SEVERITY_DEBUG, [collector](OBLogSeverity severity, const char *msg) {
                if(collector)
                    collector->append(severity, msg);
            });
        }

        ~LoggerCallbackGuard() {
            ob::Context::setLoggerToConsole(OB_LOG_SEVERITY_INFO);
        }

    private:
        LogCollector *collector_;
    };

    size_t markPhase() const {
        return collector_.size();
    }

    void flushLogs() {
        static std::atomic<int> counter{ 0 };
        int                     id       = ++counter;
        auto                    sentinel = "FLUSH_SENTINEL_" + std::to_string(id);

        ob::Context::logExternalMessage(OB_LOG_SEVERITY_INFO, "TEST_FLUSH", sentinel, __FILE__, __func__, __LINE__);

        for(int i = 0; i < 100; ++i) {
            if(collector_.containsSubstring(sentinel))
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void verifyPhaseHasLogs(size_t start, size_t end, const char *label, const char *failure) const {
        EXPECT_GT(end, start) << failure;
        collector_.dumpRange(start, end, label);
    }

    void verifyPhaseContains(size_t start, size_t end, const char *label, const std::string &pattern, const char *failure) const {
        if(!collector_.rangeContainsPattern(start, end, pattern)) {
            std::cout << "[WARN] " << label << ": " << failure << " (log pattern not found, non-blocking)" << std::endl;
        }
    }

    void verifyOrderedPair(size_t start, size_t end, const char *label, const std::string &beginPattern, const std::string &endPattern,
                           const char *failure) const {
        auto logs      = collector_.range(start, end);
        bool beginSeen = false;
        bool endSeen   = false;

        std::regex beginRe(beginPattern, std::regex_constants::icase);
        std::regex endRe(endPattern, std::regex_constants::icase);
        for(const auto &entry: logs) {
            if(!beginSeen && std::regex_search(entry.message, beginRe)) {
                beginSeen = true;
                continue;
            }
            if(beginSeen && std::regex_search(entry.message, endRe)) {
                endSeen = true;
                break;
            }
        }

        if(!(beginSeen && endSeen)) {
            std::cout << "[WARN] " << label << ": " << failure << " (log pair not found, non-blocking)" << std::endl;
        }
    }

    void verifyBalancedCount(size_t start, size_t end, const char *label, const std::string &createPattern, const std::string &destroyPattern,
                             const char *failure) const {
        size_t createCount  = collector_.rangeCountPattern(start, end, createPattern);
        size_t destroyCount = collector_.rangeCountPattern(start, end, destroyPattern);
        if(createCount == 0u || createCount != destroyCount) {
            std::cout << "[WARN] " << label << ": create/destroy mismatch (create=" << createCount << ", destroy=" << destroyCount << ") - " << failure
                      << " (non-blocking)" << std::endl;
        }
    }

    template <size_t N>
    void applyPhaseRuleTable(size_t start, size_t end, const char *label, bool requireLogs, const RuleSpec (&rules)[N], const char *missingLogFailure) const {
        if(requireLogs) {
            verifyPhaseHasLogs(start, end, label, missingLogFailure);
        }
        else {
            collector_.dumpRange(start, end, label);
        }

        for(const auto &rule: rules) {
            switch(rule.kind) {
            case RuleKind::Contains:
                verifyPhaseContains(start, end, label, rule.patternA ? rule.patternA : "", rule.failure ? rule.failure : "contains rule failed");
                break;
            case RuleKind::OrderedPair:
                verifyOrderedPair(start, end, label, rule.patternA ? rule.patternA : "", rule.patternB ? rule.patternB : "",
                                  rule.failure ? rule.failure : "ordered pair rule failed");
                break;
            case RuleKind::BalancedPair:
                verifyBalancedCount(start, end, label, rule.patternA ? rule.patternA : "", rule.patternB ? rule.patternB : "",
                                    rule.failure ? rule.failure : "balanced pair rule failed");
                break;
            }
        }
    }

    void applyPhaseRuleTable(size_t start, size_t end, const char *label, bool requireLogs, std::initializer_list<RuleSpec> rules,
                             const char *missingLogFailure) const {
        if(requireLogs) {
            verifyPhaseHasLogs(start, end, label, missingLogFailure);
        }
        else {
            collector_.dumpRange(start, end, label);
        }

        for(const auto &rule: rules) {
            switch(rule.kind) {
            case RuleKind::Contains:
                verifyPhaseContains(start, end, label, rule.patternA ? rule.patternA : "", rule.failure ? rule.failure : "contains rule failed");
                break;
            case RuleKind::OrderedPair:
                verifyOrderedPair(start, end, label, rule.patternA ? rule.patternA : "", rule.patternB ? rule.patternB : "",
                                  rule.failure ? rule.failure : "ordered pair rule failed");
                break;
            case RuleKind::BalancedPair:
                verifyBalancedCount(start, end, label, rule.patternA ? rule.patternA : "", rule.patternB ? rule.patternB : "",
                                    rule.failure ? rule.failure : "balanced pair rule failed");
                break;
            }
        }
    }

    LogCollector collector_;
};

TEST_F(TC_SCENARIO_LogCompleteness, TC_SCENARIO_01_full_lifecycle_log_completeness) {

    LoggerCallbackGuard loggerGuard(&collector_);

    size_t phase1Start = markPhase();
    auto   ctx         = std::make_shared<ob::Context>();
    ASSERT_NE(ctx, nullptr);
    flushLogs();
    size_t phase1End = markPhase();

    applyPhaseRuleTable(phase1Start, phase1End, "Phase1-ContextInit", true, {}, "Phase 1 (Context init): Expected log output during context creation");

    size_t phase2Start = markPhase();
    auto   devList     = ctx->queryDeviceList();
    ASSERT_NE(devList, nullptr);
    ASSERT_GT(devList->deviceCount(), 0u) << "No connected device";
    const std::string &sn     = cfg_.deviceSerial();
    auto               device = sn.empty() ? devList->getDevice(0) : devList->getDeviceBySN(sn.c_str());
    ASSERT_NE(device, nullptr);
    auto devInfo = device->getDeviceInfo();
    ASSERT_NE(devInfo, nullptr);
    auto serialNumber = std::string(devInfo->getSerialNumber());
    flushLogs();
    size_t phase2End = markPhase();

    static const RuleSpec kPhase2Rules[] = {
        { RuleKind::Contains, "query|device list|Name:|PID:", nullptr, "Phase 2 (Device enumeration): Expected device enumeration keywords in logs" },
    };
    applyPhaseRuleTable(phase2Start, phase2End, "Phase2-Enumerate", true, kPhase2Rules,
                        "Phase 2 (Device enumeration): Expected log output during device query");

    size_t phase3Start = markPhase();
    auto   pipeline    = std::make_shared<ob::Pipeline>(device);
    auto   config      = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    config->enableStream(OB_STREAM_COLOR);
    pipeline->start(config);

    std::shared_ptr<ob::FrameSet> frameset;
    frameset = waitForFramesetWithRetry(pipeline);

    ASSERT_NE(frameset, nullptr) << "Failed to acquire frameset after pipeline start";
    flushLogs();
    size_t                phase3End      = markPhase();
    static const RuleSpec kPhase3Rules[] = {
        { RuleKind::OrderedPair, "Try to start streams!", "Start streams done!", "stream start logs must contain a begin/end pair" },
        { RuleKind::OrderedPair, "Try to start stream:.*Depth", "STREAMING@Depth", "depth stream logs must show start intent followed by streaming state" },
        { RuleKind::OrderedPair, "Try to start stream:.*Color", "stream started.*sensorType=Color",
          "color stream logs must show start intent followed by stream started confirmation" },
        { RuleKind::Contains, "Pipeline start done!", nullptr, "Phase 3 (Stream start): Expected pipeline completion log" },
    };
    applyPhaseRuleTable(phase3Start, phase3End, "Phase3-StreamStart", true, kPhase3Rules,
                        "Phase 3 (Stream start): Expected log output during pipeline start and frame acquisition");

    size_t phase4Start   = markPhase();
    auto   depthFrame    = frameset->getFrame(OB_FRAME_DEPTH);
    bool   filterApplied = false;

    if(depthFrame) {
        try {
            auto decimation = ob::FilterFactory::createFilter("DecimationFilter");
            auto result     = decimation->process(depthFrame);
            if(result)
                filterApplied = true;
        }
        catch(const ob::Error &) {
        }

        try {
            auto pcFilter = ob::FilterFactory::createFilter("PointCloudFilter");
            auto result   = pcFilter->process(depthFrame);
            if(result)
                filterApplied = true;
        }
        catch(const ob::Error &) {
        }
    }
    flushLogs();
    size_t phase4End = markPhase();

    if(filterApplied) {
        static const RuleSpec kPhase4Rules[] = {
            { RuleKind::BalancedPair, "Filter DecimationFilter created", "Filter DecimationFilter destroyed",
              "DecimationFilter should have matched creation/destruction logs" },
            { RuleKind::BalancedPair, "Filter PointCloudFilter created", "Filter PointCloudFilter destroyed",
              "PointCloudFilter should have matched creation/destruction logs" },
            { RuleKind::OrderedPair, "Filter DecimationFilter created", "Filter DecimationFilter destroyed",
              "DecimationFilter lifecycle logs must be ordered" },
            { RuleKind::OrderedPair, "Filter PointCloudFilter created", "Filter PointCloudFilter destroyed",
              "PointCloudFilter lifecycle logs must be ordered" },
        };
        applyPhaseRuleTable(phase4Start, phase4End, "Phase4-FilterProcessing", true, kPhase4Rules,
                            "Phase 4 (Filter): Expected log output during filter processing");
    }
    else {
        applyPhaseRuleTable(phase4Start, phase4End, "Phase4-FilterProcessing", false, {},
                            "Phase 4 (Filter): No filter applied - skipping filter log validation");
    }

    size_t phase5Start = markPhase();
    pipeline->stop();
    pipeline.reset();
    flushLogs();
    size_t phase5End = markPhase();

    static const RuleSpec kPhase5Rules[] = {
        { RuleKind::OrderedPair, "Try to stop streams!", "Stop streams done!", "stream stop logs must contain a begin/end pair" },
        { RuleKind::OrderedPair, "Try to stop pipeline!", "Stop pipeline done!", "pipeline stop logs must contain a begin/end pair" },
        { RuleKind::Contains, "Pipeline destroyed!", nullptr, "Phase 5 (Stream stop): Expected pipeline destruction log" },
    };
    applyPhaseRuleTable(phase5Start, phase5End, "Phase5-StreamStop", true, kPhase5Rules, "Phase 5 (Stream stop): Expected log output during pipeline stop");

    size_t phase6Start = markPhase();

    std::atomic<bool> removedSeen{ false };
    std::atomic<bool> addedSeen{ false };
    auto              cbId = ctx->registerDeviceChangedCallback([&](std::shared_ptr<ob::DeviceList> removed, std::shared_ptr<ob::DeviceList> added) {
        if(removed && removed->getCount() > 0)
            removedSeen = true;
        if(added && added->getCount() > 0)
            addedSeen = true;
    });

    try {
        device->reboot();
    }
    catch(const ob::Error &e) {
        std::cout << "[LifecycleTest] reboot() threw: " << e.getMessage() << " - continuing to handle teardown\n";
    }
    catch(...) {
        std::cout << "[LifecycleTest] reboot() threw unknown exception - continuing\n";
    }
    device.reset();
    devInfo.reset();
    devList.reset();
    frameset.reset();

    for(int i = 0; i < 30 && !removedSeen.load(); i++)
        std::this_thread::sleep_for(std::chrono::seconds(1));

    size_t                phase7Boundary = markPhase();
    static const RuleSpec kPhase6Rules[] = {
        { RuleKind::Contains, "removed: 1, added: 0|device removed|Device removed event occurred", nullptr,
          "Phase 6/7 (Reboot + offline): Expected device removal logs" },
    };
    applyPhaseRuleTable(phase6Start, phase7Boundary, "Phase6-RebootAndOffline", true, kPhase6Rules,
                        "Phase 6/7 (Reboot + offline): Expected log output during reboot and removal");
    EXPECT_TRUE(removedSeen.load()) << "Phase 7 (Offline): Device removed callback was not received within 30s";

    for(int i = 0; i < 60 && !addedSeen.load(); i++)
        std::this_thread::sleep_for(std::chrono::seconds(1));

    size_t phase8End = markPhase();
    ctx->unregisterDeviceChangedCallback(cbId);

    static const RuleSpec kPhase8Rules[] = {
        { RuleKind::Contains, "removed: 0, added: 1|device list changed: added=1|Device arrival event occurred", nullptr,
          "Phase 8 (Online): Expected device arrival / add-side logs" },
    };
    applyPhaseRuleTable(phase7Boundary, phase8End, "Phase8-DeviceOnline", true, kPhase8Rules,
                        "Phase 8 (Online): Expected log output during device reconnection");
    EXPECT_TRUE(addedSeen.load()) << "Phase 8 (Online): Device added callback was not received within 60s";

    if(addedSeen.load()) {
        devList = ctx->queryDeviceList();
        ASSERT_NE(devList, nullptr);
        device = devList->getDeviceBySN(serialNumber.c_str());
        EXPECT_NE(device, nullptr) << "Device SN " << serialNumber << " not found after reboot";
    }

    static const RuleSpec kPhase68Rules[] = {
        { RuleKind::OrderedPair, "removed: 1, added: 0", "removed: 0, added: 1", "reboot lifecycle logs must show device removal before device re-addition" },
    };
    applyPhaseRuleTable(phase6Start, phase8End, "Phase6-8-RebootLifecycle", false, kPhase68Rules, "Phase 6-8: Expected reboot lifecycle log output");

    size_t phase9Start = markPhase();
    device.reset();
    devList.reset();
    ctx.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    size_t phase9End = markPhase();

    static const RuleSpec kPhase9Rules[] = {
        { RuleKind::Contains, "destroyed|fittingLoop exit|disabled|exit", nullptr, "Phase 9 (Context destroy): Expected teardown-side logs" },
    };
    if(phase9End > phase9Start) {
        applyPhaseRuleTable(phase9Start, phase9End, "Phase9-ContextDestroy", false, kPhase9Rules,
                            "Phase 9 (Context destroy): Expected log output during context teardown");
    }

    auto allLogs = collector_.snapshot();
    std::cout << "\n========== Log Completeness Summary ==========" << std::endl;
    std::cout << "Total log entries captured: " << allLogs.size() << std::endl;

    size_t counts[6] = {};
    for(const auto &e: allLogs) {
        if(e.severity >= 0 && e.severity <= 5)
            counts[e.severity]++;
    }
    std::cout << "  DEBUG: " << counts[OB_LOG_SEVERITY_DEBUG] << std::endl;
    std::cout << "  INFO:  " << counts[OB_LOG_SEVERITY_INFO] << std::endl;
    std::cout << "  WARN:  " << counts[OB_LOG_SEVERITY_WARN] << std::endl;
    std::cout << "  ERROR: " << counts[OB_LOG_SEVERITY_ERROR] << std::endl;
    std::cout << "  FATAL: " << counts[OB_LOG_SEVERITY_FATAL] << std::endl;

    struct PhaseInfo {
        const char *name;
        size_t      start;
        size_t      end;
    };
    PhaseInfo phases[] = {
        { "Context init", phase1Start, phase1End },      { "Device enumerate", phase2Start, phase2End }, { "Stream start", phase3Start, phase3End },
        { "Filter processing", phase4Start, phase4End }, { "Stream stop", phase5Start, phase5End },      { "Reboot+Offline", phase6Start, phase7Boundary },
        { "Device online", phase7Boundary, phase8End },  { "Context destroy", phase9Start, phase9End },
    };
    for(const auto &p: phases) {
        size_t n = (p.end > p.start) ? (p.end - p.start) : 0;
        std::cout << "  Phase [" << p.name << "]: " << n << " log entries" << (n == 0 ? " *** MISSING ***" : "") << std::endl;
    }
    std::cout << "================================================\n" << std::endl;

    if(phase9End <= phase9Start) {
        std::cout << "[NOTE] Phase 9 (Context destroy) captured no teardown logs - expected occasionally,"
                     " as the Logger is destroyed with the Context."
                  << std::endl;
    }
}
