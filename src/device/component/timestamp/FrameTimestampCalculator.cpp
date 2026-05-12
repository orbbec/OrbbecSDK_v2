// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FrameTimestampCalculator.hpp"
#include "frame/Frame.hpp"
#include "usb/uvc/UvcTypes.hpp"
#include "utils/Utils.hpp"
#include "logger/LoggerInterval.hpp"
#include "InternalTypes.hpp"
#include <cmath>

namespace libobsensor {

#define BASE_DEV_TIME_MASK 0xffffffff00000000
static constexpr uint64_t TSP_OVERFLOW_32BIT = 0x100000000ULL;

GlobalTimestampCalculator::GlobalTimestampCalculator(IDevice *owner, uint64_t deviceTimeFreq, uint64_t frameTimeFreq)
    : DeviceComponentBase(owner), deviceTimeFreq_(deviceTimeFreq), frameTimeFreq_(frameTimeFreq) {
    (void)frameTimeFreq_;
    globalTimestampFitter_ = owner->getComponentT<IGlobalTimestampFitter>(OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER).get();

    // Reset EMA/refit state on device-clock reset events to avoid a 25s wrong-output tail.
    auto                  propServer = owner->getPropertyServer();
    std::vector<uint32_t> resetProps;
    if(propServer->isPropertySupported(OB_PROP_TIMER_RESET_SIGNAL_BOOL, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        resetProps.push_back(OB_PROP_TIMER_RESET_SIGNAL_BOOL);
    }
    if(propServer->isPropertySupported(OB_STRUCT_DEVICE_TIME, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        resetProps.push_back(OB_STRUCT_DEVICE_TIME);
    }
    if(!resetProps.empty()) {
        propServer->registerAccessCallback(resetProps, [this](uint32_t, const uint8_t *, size_t, PropertyOperationType operationType) {
            if(operationType == PROP_OP_WRITE) {
                clear();
            }
        });
    }
}

void GlobalTimestampCalculator::calculate(std::shared_ptr<Frame> frame) {
    if(pendingReset_.exchange(false, std::memory_order_acquire)) {
        resetStateImpl();
    }

    const auto rawTsUs = frame->getTimeStampUsec();
    if(rawTsUs == 0) {
        // If the device timestamp is invalid (0), keep global timestamp as 0.
        frame->setGlobalTimeStampUsec(0);
        return;
    }

    auto linearFuncParam = globalTimestampFitter_->getLinearFuncParam();
    if(linearFuncParam.coefficientA <= 0) {
        // No valid fit yet.
        frame->setGlobalTimeStampUsec(0);
        return;
    }

    // Convert frame device timestamp (us) to the same unit as fit anchors (ms),
    // assuming the full uint64_t timestamp - no 32-bit rollover handling needed.
    double frameDevMs = static_cast<double>(rawTsUs) * deviceTimeFreq_ / 1000000.0;

    // Always anchor at the fit center: it is a weighted mean of N samples (sqrt(N) lower
    // noise than any single raw point); drift is handled by the fitter, not by switching anchor.
    double anchorDevMs = static_cast<double>(linearFuncParam.refDevTime);
    double anchorSysUs = static_cast<double>(linearFuncParam.refSysTime);

    double   diff           = frameDevMs - anchorDevMs;
    double   incrementalUs  = linearFuncParam.coefficientA * diff;
    double   predSteadyUs   = anchorSysUs + incrementalUs;
    uint64_t frameSteadyTs  = frame->getSteadyTimeStampUsec();
    int64_t  realtimeOffset = static_cast<int64_t>(frame->getSystemTimeStampUsec()) - static_cast<int64_t>(frameSteadyTs);

    // Refit detection: if fit params changed, predSteadyUs jumps by Δ for this same frame.
    // Shift ema_ by -Δ so output stays continuous and EMA continues tracking new residuals.
    if(fitCached_ && (linearFuncParam.coefficientA != prevCoeffA_ || anchorDevMs != prevAnchorDevMs_ || anchorSysUs != prevAnchorSysUs_)) {
        double prevPredUs = prevAnchorSysUs_ + prevCoeffA_ * (frameDevMs - prevAnchorDevMs_);
        double delta      = predSteadyUs - prevPredUs;
        ema_ -= delta;
    }
    prevCoeffA_      = linearFuncParam.coefficientA;
    prevAnchorDevMs_ = anchorDevMs;
    prevAnchorSysUs_ = anchorSysUs;
    fitCached_       = true;

    // EMA correction: tracks slow bias drift continuously. See header for math.
    double  residualUs   = static_cast<double>(frameSteadyTs) - predSteadyUs;
    int64_t correctionUs = 0;
    if(!emaInited_) {
        ema_               = residualUs;
        madEma_            = 0.0;
        startSteadyUs_     = frameSteadyTs;
        lastFrameSteadyUs_ = frameSteadyTs;
        emaInited_         = true;
    }
    else {
        double dtUs = static_cast<double>(frameSteadyTs - lastFrameSteadyUs_);
        if(dtUs <= 0.0) {
            dtUs = 1.0;
        }
        double alpha = 1.0 - std::exp(-dtUs / EMA_TAU_US);
        double devUs = residualUs - ema_;
        // Outlier gate: only active after baseline lock (MAD has settled by then).
        double madThr   = std::max(MAD_FLOOR_US, OUTLIER_K * madEma_);
        bool   isOutlier = baselineReady_ && (std::fabs(devUs) > madThr);
        if(!isOutlier) {
            ema_    = ema_ + alpha * devUs;
            madEma_ = madEma_ + alpha * (std::fabs(devUs) - madEma_);
        }
        lastFrameSteadyUs_ = frameSteadyTs;
    }
    if(!baselineReady_ && (frameSteadyTs - startSteadyUs_) >= BASELINE_LOCK_US) {
        emaBaseline_   = ema_;
        baselineReady_ = true;
    }
    if(baselineReady_) {
        correctionUs = static_cast<int64_t>(ema_ - emaBaseline_);
    }

    int64_t globalTsp = static_cast<int64_t>(predSteadyUs + 0.5) + realtimeOffset + correctionUs;

    frame->setGlobalTimeStampUsec(static_cast<uint64_t>(globalTsp));
}

void GlobalTimestampCalculator::clear() {
    // Thread-safe: actual reset runs at the top of the next calculate() on the frame thread.
    pendingReset_.store(true, std::memory_order_release);
}

void GlobalTimestampCalculator::resetStateImpl() {
    ema_               = 0.0;
    emaBaseline_       = 0.0;
    madEma_            = 0.0;
    startSteadyUs_     = 0;
    lastFrameSteadyUs_ = 0;
    emaInited_         = false;
    baselineReady_     = false;
    prevCoeffA_        = 0.0;
    prevAnchorDevMs_   = 0.0;
    prevAnchorSysUs_   = 0.0;
    fitCached_         = false;
}

FrameTimestampCalculatorDirectly::FrameTimestampCalculatorDirectly(IDevice *device, uint64_t clockFreq) : DeviceComponentBase(device), clockFreq_(clockFreq) {}

void FrameTimestampCalculatorDirectly::calculate(std::shared_ptr<Frame> frame) {
    auto srcTimestamp    = frame->getTimeStampUsec();
    auto outputTimestamp = static_cast<double>(srcTimestamp) * 1000000 / clockFreq_;
    frame->setTimeStampUsec(static_cast<uint64_t>(outputTimestamp));
}

FrameTimestampCalculatorBaseDeviceTime::FrameTimestampCalculatorBaseDeviceTime(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq)
    : DeviceComponentBase(device), deviceTimeFreq_(deviceTimeFreq), frameTimeFreq_(frameTimeFreq), prevSrcTsp_(0), prevHostTsp_(0), baseDevTime_(0) {
    auto                  propServer = device->getPropertyServer();
    std::vector<uint32_t> supportedProps;
    if(propServer->isPropertySupported(OB_PROP_TIMER_RESET_SIGNAL_BOOL, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        supportedProps.push_back(OB_PROP_TIMER_RESET_SIGNAL_BOOL);
    }
    if(propServer->isPropertySupported(OB_STRUCT_DEVICE_TIME, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        supportedProps.push_back(OB_STRUCT_DEVICE_TIME);
    }
    if(!supportedProps.empty()) {
        propServer->registerAccessCallback(supportedProps, [&](uint32_t, const uint8_t *, size_t, PropertyOperationType operationType) {
            if(operationType == PROP_OP_WRITE) {
                clear();
            }
        });
    }
}

void FrameTimestampCalculatorBaseDeviceTime::calculate(std::shared_ptr<Frame> frame) {
    auto srcTimestamp = frame->getTimeStampUsec();
    auto rstTsp       = calculate(srcTimestamp);
    frame->setTimeStampUsec(rstTsp);
}

uint64_t FrameTimestampCalculatorBaseDeviceTime::calculate(uint64_t srcTimestamp) {
    // Conditions that need to update baseDevTime_:
    // 1. The first frame after opening the stream
    // 2. The timestamp becomes smaller (the size of the timestamps of the previous and later data frames is reversed), indicating that a timestamp overflow has
    // occurred or the device clock has been cleared (a small probability may also be caused by abnormal data transmission)
    // 3. The difference in system time between the arrival of two frames of data is greater than the difference in device timestamps between two frames of
    // data, indicating that an overflow or clearing occurred during the triggering interval of the previous frame. However, the timestamps of the preceding and
    // following data frames The size is not reversed, Try to actively update and obtain the device timestamp (but in order to avoid frequent acquisition of
    // device timestamps due to misjudgments caused by data transmission fluctuations, we set the difference to be greater than prevSrcTspMs /2)
    // 4. When the last timestamp is less than 50ms, avoid missing judgments
    // LOG(INFO) << " >>> srcTimeStamp: " << srcTimestamp << " <<< ";
    // Determine whether the timestamp becomes smaller
    bool tspDecrease = (srcTimestamp < prevSrcTsp_);

    // Determine whether the data frame timestamp difference is similar to the system timestamp difference
    uint64_t curHostTsp      = utils::getSteadyTimeMs();
    int64_t  srcTspDiffMs    = static_cast<int64_t>((static_cast<double>(srcTimestamp) - prevSrcTsp_) / frameTimeFreq_ * 1000);  // Convert unit to milliseconds
    int64_t  hostTspDiffMs   = curHostTsp - prevHostTsp_;
    uint64_t prevSrcTspMs    = static_cast<uint64_t>(static_cast<double>(prevSrcTsp_) / frameTimeFreq_ * 1000);
    bool     tspDiffAbnormal = ((static_cast<double>(hostTspDiffMs) - srcTspDiffMs) >= prevSrcTspMs / 2);

    if(baseDevTime_ == 0 || ((tspDecrease || tspDiffAbnormal) && (srcTimestamp != 0 || prevSrcTsp_ != 0)) || prevSrcTspMs <= 50) {
        LOG_DEBUG_INTVL_MS(1000, "updateBaseTimeStamp:");
        LOG_DEBUG_INTVL_MS(1000, "\tsrcTimestamp={0}, prevSrcTsp_={1}, tspDecrease={2}", srcTimestamp, prevSrcTsp_, tspDecrease);
        LOG_DEBUG_INTVL_MS(1000, "\tsrcTspDiffMs={0}, hostTspDiffMs={1}, tspDiffAbnormal={2}", srcTspDiffMs, hostTspDiffMs, tspDiffAbnormal);

        {
            auto owner          = getOwner();
            auto propertyServer = owner->getPropertyServer();
            auto devTime        = propertyServer->getStructureDataT<OBDeviceTime>(OB_STRUCT_DEVICE_TIME);
            baseDevTime_        = static_cast<uint64_t>((static_cast<double>(devTime.time) + devTime.rtt / 2) / deviceTimeFreq_ * frameTimeFreq_);
        }

        if((baseDevTime_ & ~BASE_DEV_TIME_MASK) <= srcTimestamp && baseDevTime_ > TSP_OVERFLOW_32BIT) {
            // An overflow occurred between the timestamp of the data frame and updateBaseTimeStamp.
            baseDevTime_ -= TSP_OVERFLOW_32BIT;
        }
    }

    prevHostTsp_       = curHostTsp;
    prevSrcTsp_        = srcTimestamp;
    auto outputTsp     = (baseDevTime_ & BASE_DEV_TIME_MASK) + (srcTimestamp & (~BASE_DEV_TIME_MASK));
    auto timestampUsec = static_cast<double>(outputTsp) / frameTimeFreq_ * 1000000;
    return static_cast<uint64_t>(timestampUsec);
}

void FrameTimestampCalculatorBaseDeviceTime::clear() {
    prevSrcTsp_  = 0;
    prevHostTsp_ = 0;
    baseDevTime_ = 0;
}

FrameTimestampCalculatorOverMetadata::FrameTimestampCalculatorOverMetadata(IDevice *owner, OBFrameMetadataType metadataType, uint64_t clockFreq)
    : DeviceComponentBase(owner), metadataType_(metadataType), clockFreq_(clockFreq) {}

void FrameTimestampCalculatorOverMetadata::calculate(std::shared_ptr<Frame> frame) {
    auto timestamp     = frame->getMetadataValue(metadataType_);
    auto timestampUsec = static_cast<double>(timestamp) / clockFreq_ * 1000000;
    frame->setTimeStampUsec(static_cast<uint64_t>(timestampUsec));
}

void FrameTimestampCalculatorOverMetadata::clear() {}

FrameTimestampCalculatorOverUvcSCR::FrameTimestampCalculatorOverUvcSCR(IDevice *owner, uint64_t clockFreq)
    : DeviceComponentBase(owner), clockFreq_(clockFreq) {}

void FrameTimestampCalculatorOverUvcSCR::calculate(std::shared_ptr<Frame> frame) {
    auto     metadata         = frame->getMetadata();
    auto     uvcPayloadHeader = reinterpret_cast<const StandardUvcFramePayloadHeader *>(metadata);
    uint32_t low4Bytes        = uvcPayloadHeader->dwPresentationTime;
    // using the low 4 bytes of scrSourceClock as high 4 bytes of timestamp
    uint32_t high4Bytes    = *reinterpret_cast<const uint32_t *>(uvcPayloadHeader->scrSourceClock);
    uint64_t timestamp     = static_cast<uint64_t>(high4Bytes) << 32 | low4Bytes;
    auto     timestampUsec = static_cast<double>(timestamp) / clockFreq_ * 1000000;
    frame->setTimeStampUsec(static_cast<uint64_t>(timestampUsec));
}

void FrameTimestampCalculatorOverUvcSCR::clear() {}

G435LeFrameTimestampCalculatorDeviceTime::G435LeFrameTimestampCalculatorDeviceTime(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq,
                                                                                   uint64_t clockFreq)
    : DeviceComponentBase(device) {
    baseCalculator_   = std::make_shared<FrameTimestampCalculatorBaseDeviceTime>(device, deviceTimeFreq, frameTimeFreq);
    directCalculator_ = std::make_shared<FrameTimestampCalculatorDirectly>(device, clockFreq);
}

void G435LeFrameTimestampCalculatorDeviceTime::calculate(std::shared_ptr<Frame> frame) {
    if(frame->getFormat() == OB_FORMAT_YUYV || frame->getFormat() == OB_FORMAT_I420 || frame->getFormat() == OB_FORMAT_Y8
       || frame->getFormat() == OB_FORMAT_Y10) {
        directCalculator_->calculate(frame);
    }
    else {
        if(frame->hasMetadata(OB_FRAME_METADATA_TYPE_TIMESTAMP)) {
            frame->setTimeStampUsec(frame->getMetadataValue(OB_FRAME_METADATA_TYPE_TIMESTAMP));
        }
        else {
            baseCalculator_->calculate(frame);
        }
    }
}

void G435LeFrameTimestampCalculatorDeviceTime::clear() {
    baseCalculator_->clear();
    directCalculator_->clear();
}

}  // namespace libobsensor
