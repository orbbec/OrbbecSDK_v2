#pragma once
#include "IDevice.hpp"
#include "DeviceComponentBase.hpp"

#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace libobsensor {

// First degree function coefficient y=ax+b
typedef struct {
    double   coefficientA;
    double   constantB;
    uint64_t checkDataX;
    uint64_t checkDataY;
} LinearFuncParam;

class GlobalTimestampFitter : public DeviceComponentBase {
public:
    GlobalTimestampFitter(IDevice *owner);
    ~GlobalTimestampFitter();
    LinearFuncParam getLinearFuncParam();
    void            reFitting();
    void            pause();
    void            resume();

    void enable(bool en);
    bool isEnabled() const;

private:
    void fittingLoop();

private:
    bool enable_;
    std::thread             sampleThread_;
    std::mutex              sampleMutex_;
    std::condition_variable sampleCondVar_;
    bool                    sampleLoopExit_;

    typedef struct {
        uint64_t systemTimestamp;
        uint64_t deviceTimestamp;
    } TimestampPair;

    std::deque<TimestampPair> samplingQueue_;
    uint32_t                  maxQueueSize_ = 100;

    // The refresh interval needs to be less than half the interval of the data frame, that is, it needs to be sampled at least twice within an overflow period.
    uint32_t refreshIntervalMsec_ = 1000;

    std::mutex              linearFuncParamMutex_;
    std::condition_variable linearFuncParamCondVar_;
    LinearFuncParam         linearFuncParam_;
};
}  // namespace libobsensor