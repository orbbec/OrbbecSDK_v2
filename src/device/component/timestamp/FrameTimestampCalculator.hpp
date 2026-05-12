// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IFrame.hpp"
#include "IFrameTimestamp.hpp"
#include "IFrameTimestamp.hpp"
#include "DeviceComponentBase.hpp"

#include <atomic>
#include <cstdint>

namespace libobsensor {
class GlobalTimestampCalculator : public IFrameTimestampCalculator, public DeviceComponentBase {
public:
    GlobalTimestampCalculator(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq);

    virtual ~GlobalTimestampCalculator() noexcept override = default;

    void calculate(std::shared_ptr<Frame> frame) override;
    void clear() override;

private:
    void resetStateImpl();
    
private:
    uint64_t                                deviceTimeFreq_;
    uint64_t                                frameTimeFreq_;
    std::shared_ptr<IGlobalTimestampFitter> globalTimestampFitter_;

    // EMA-based residual correction for slow bias drift, with a locked startup baseline.
    // Refit changes shift ema_ by the prediction delta to keep output continuous.
    // Large residual spikes are rejected from EMA/MAD updates so they do not pollute state.
    const double   EMA_TAU_US         = 5.0e6;                      // 5s time constant
    const uint64_t BASELINE_LOCK_US   = 10ULL * 1000ULL * 1000ULL;  // ~2*tau settling
    const double   OUTLIER_K          = 6.0;                        // reject if |dev| > K*MAD
    const double   MAD_FLOOR_US       = 200.0;                      // avoid over-strict gate when noise is tiny
    double         ema_               = 0.0;
    double         emaBaseline_       = 0.0;
    double         madEma_            = 0.0;
    uint64_t       startSteadyUs_     = 0;
    uint64_t       lastFrameSteadyUs_ = 0;
    bool           emaInited_         = false;
    bool           baselineReady_     = false;
    // Cached previous fit params for refit detection.
    double prevCoeffA_      = 0.0;
    double prevAnchorDevMs_ = 0.0;
    double prevAnchorSysUs_ = 0.0;
    bool   fitCached_       = false;

    // Set by clear() (any thread); drained by calculate() (frame thread).
    std::atomic<bool> pendingReset_{ false };
};

class FrameTimestampCalculatorDirectly : public IFrameTimestampCalculator, public DeviceComponentBase {
public:
    FrameTimestampCalculatorDirectly(IDevice *device, uint64_t clockFreq);

    virtual ~FrameTimestampCalculatorDirectly() noexcept override = default;

    void calculate(std::shared_ptr<Frame> frame) override;
    void clear() override {}

private:
    uint64_t clockFreq_;
};

class FrameTimestampCalculatorBaseDeviceTime : public IFrameTimestampCalculator, public DeviceComponentBase {
public:
    FrameTimestampCalculatorBaseDeviceTime(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq);

    virtual ~FrameTimestampCalculatorBaseDeviceTime() noexcept override = default;

    void calculate(std::shared_ptr<Frame> frame) override;
    void clear() override;

    uint64_t calculate(uint64_t srcTimestamp);

private:
    uint64_t deviceTimeFreq_;
    uint64_t frameTimeFreq_;

    uint64_t prevSrcTsp_;
    uint64_t prevHostTsp_;
    uint64_t baseDevTime_;
};

class FrameTimestampCalculatorOverMetadata : public IFrameTimestampCalculator, public DeviceComponentBase {
public:
    FrameTimestampCalculatorOverMetadata(IDevice *device, OBFrameMetadataType metadataType, uint64_t clockFreq);

    virtual ~FrameTimestampCalculatorOverMetadata() noexcept override = default;

    void calculate(std::shared_ptr<Frame> frame) override;
    void clear() override;

private:
    OBFrameMetadataType metadataType_;
    uint64_t            clockFreq_;
};

class FrameTimestampCalculatorOverUvcSCR : public IFrameTimestampCalculator, public DeviceComponentBase {
public:
    FrameTimestampCalculatorOverUvcSCR(IDevice *device, uint64_t clockFreq);

    virtual ~FrameTimestampCalculatorOverUvcSCR() noexcept override = default;

    void calculate(std::shared_ptr<Frame> frame) override;
    void clear() override;

private:
    uint64_t clockFreq_;
};

class G435LeFrameTimestampCalculatorDeviceTime : public IFrameTimestampCalculator, public DeviceComponentBase{
public:
    G435LeFrameTimestampCalculatorDeviceTime(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq, uint64_t clockFreq);

    virtual ~G435LeFrameTimestampCalculatorDeviceTime() override = default;

    void calculate(std::shared_ptr<Frame> frame) override;

    void clear() override;

private:
    std::shared_ptr<FrameTimestampCalculatorBaseDeviceTime> baseCalculator_;
    std::shared_ptr<FrameTimestampCalculatorDirectly>       directCalculator_;
};

}  // namespace libobsensor
