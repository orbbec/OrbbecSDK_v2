// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G330FrameTimestampCalculator.hpp"
#include "logger/LoggerInterval.hpp"
#include "InternalTypes.hpp"
#include "frame/Frame.hpp"

namespace libobsensor {
G330FrameTimestampCalculatorBaseDeviceTime::G330FrameTimestampCalculatorBaseDeviceTime(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq)
    : device_(device), deviceTimeFreq_(deviceTimeFreq), frameTimeFreq_(frameTimeFreq) {
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

void G330FrameTimestampCalculatorBaseDeviceTime::calculate(std::shared_ptr<Frame> frame) {
    auto srcTimestamp = frame->getTimeStampUsec();
    auto rstTsp       = calculate(srcTimestamp);
    frame->setTimeStampUsec(rstTsp);
}

uint64_t G330FrameTimestampCalculatorBaseDeviceTime::calculate(uint64_t srcTimestamp) {
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
    bool tspDecrease = (srcTimestamp < prevSrcTsp_) && (prevSrcTsp_ - srcTimestamp > 0.5 * frameTimeFreq_);  // 0.5 second

    // Determine whether the data frame timestamp difference is similar to the system timestamp difference
    uint64_t curHostTsp      = utils::getNowTimesMs();
    int64_t  srcTspDiffMs    = static_cast<int64_t>((static_cast<double>(srcTimestamp) - prevSrcTsp_) / frameTimeFreq_ * 1000);  // Convert unit to milliseconds
    int64_t  hostTspDiffMs   = curHostTsp - prevHostTsp_;
    uint64_t prevSrcTspMs    = static_cast<uint64_t>(static_cast<double>(prevSrcTsp_) / frameTimeFreq_ * 1000);
    bool     tspDiffAbnormal = ((static_cast<double>(hostTspDiffMs) - srcTspDiffMs) >= prevSrcTspMs / 2);

    if(baseDevTime_ == 0 || ((tspDecrease || tspDiffAbnormal) && (srcTimestamp != 0 || prevSrcTsp_ != 0)) || prevSrcTspMs <= 50) {
        LOG_DEBUG_INTVL_MS(1000, "updateBaseTimeStamp:");
        LOG_DEBUG_INTVL_MS(1000, "\tsrcTimestamp={0}, prevSrcTsp_={1}, tspDecrease={2}", srcTimestamp, prevSrcTsp_, tspDecrease);
        LOG_DEBUG_INTVL_MS(1000, "\tsrcTspDiffMs={0}, hostTspDiffMs={1}, tspDiffAbnormal={2}", srcTspDiffMs, hostTspDiffMs, tspDiffAbnormal);

        {
            auto propertyServer = device_->getPropertyServer();
            auto devTime        = propertyServer->getStructureDataT<OBDeviceTime>(OB_STRUCT_DEVICE_TIME);
            baseDevTime_        = static_cast<uint64_t>((static_cast<double>(devTime.time) + devTime.rtt / 2) / deviceTimeFreq_ * frameTimeFreq_);
        }

        uint64_t overFlowTimes              = baseDevTime_ / (256 * frameTimeFreq_);
        uint64_t calculateLeftoverTimestamp = baseDevTime_ - overFlowTimes * (256 * frameTimeFreq_);
        if(calculateLeftoverTimestamp < srcTimestamp) {
            baseDevTime_ = (overFlowTimes - 1) * (256 * frameTimeFreq_);
        }
        else {
            baseDevTime_ -= calculateLeftoverTimestamp;
        }
    }

    prevHostTsp_       = curHostTsp;
    prevSrcTsp_        = srcTimestamp;
    auto outputTsp     = baseDevTime_ + srcTimestamp;
    auto timestampUsec = static_cast<double>(outputTsp) / frameTimeFreq_ * 1000000;
    return static_cast<uint64_t>(timestampUsec);
}

void G330FrameTimestampCalculatorBaseDeviceTime::clear() {
    prevSrcTsp_  = 0;
    prevHostTsp_ = 0;
    baseDevTime_ = 0;
}

}  // namespace libobsensor

