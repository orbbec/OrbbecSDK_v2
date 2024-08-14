#include "FrameTimestampCalculator.hpp"
#include "frame/Frame.hpp"
#include "usb/uvc/UvcTypes.hpp"
#include "utils/Utils.hpp"
#include "logger/LoggerInterval.hpp"
#include "InternalTypes.hpp"

namespace libobsensor {

#define BASE_DEV_TIME_MASK 0xffffffff00000000
#define TSP_OVERFLOW_32BIT 0x100000000

GlobalTimestampCalculator::GlobalTimestampCalculator(IDevice *owner, uint64_t deviceTimeFreq, uint64_t frameTimeFreq)
    : DeviceComponentBase(owner), deviceTimeFreq_(deviceTimeFreq), frameTimeFreq_(frameTimeFreq) {
    globalTimestampFitter_ = owner->getComponentT<GlobalTimestampFitter>(OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER).get();
}

void GlobalTimestampCalculator::calculate(std::shared_ptr<Frame> frame) {
    auto srcTimestamp    = static_cast<uint32_t>(frame->getTimeStampUsec());  // get timestamp from frame and keep low 32 bits
    auto linearFuncParam = globalTimestampFitter_->getLinearFuncParam();

    // Convert to a timestamp with the same frequency as the device clock frequency
    double   transformedTsp              = static_cast<double>(srcTimestamp) * deviceTimeFreq_ / 1000000.0;
    uint64_t transformedTspOverflowValue = static_cast<uint64_t>(static_cast<double>(TSP_OVERFLOW_32BIT) * deviceTimeFreq_ / 1000000.0);

    // Calculate the approximate number of overflows based on the check data. The number of overflows of the current data frame should be within the range of
    // this number plus or minus 1.
    uint32_t numOfOverflows = static_cast<uint32_t>(linearFuncParam.checkDataX / transformedTspOverflowValue);
    if(numOfOverflows > 0) {
        numOfOverflows--;
    }
    while(true) {
        double value1 = (double)numOfOverflows * transformedTspOverflowValue + transformedTsp;
        double value2 = (double)(numOfOverflows + 1) * transformedTspOverflowValue + transformedTsp;
        if(value1 >= linearFuncParam.checkDataX) {
            break;
        }
        if(value1 <= linearFuncParam.checkDataX && value2 >= linearFuncParam.checkDataX) {
            if(linearFuncParam.checkDataX - value1 > value2 - linearFuncParam.checkDataX) {
                numOfOverflows++;
            }
            break;
        }
        numOfOverflows++;
    }
    transformedTsp = (static_cast<double>(TSP_OVERFLOW_32BIT) * numOfOverflows + srcTimestamp) * deviceTimeFreq_ / 1000000;
    auto globalTsp = static_cast<uint64_t>(linearFuncParam.coefficientA * transformedTsp + linearFuncParam.constantB);
    frame->setGlobalTimeStampUsec(globalTsp);
}

void GlobalTimestampCalculator::clear() {}

FrameTimestampCalculatorDirectly::FrameTimestampCalculatorDirectly(IDevice *device, uint64_t clockFreq) : DeviceComponentBase(device), clockFreq_(clockFreq) {}

void FrameTimestampCalculatorDirectly::calculate(std::shared_ptr<Frame> frame) {
    auto srcTimestamp    = frame->getTimeStampUsec();
    auto outputTimestamp = static_cast<double>(srcTimestamp) * clockFreq_ / 1000000;
    frame->setTimeStampUsec(static_cast<uint64_t>(outputTimestamp));
}

FrameTimestampCalculatorBaseDeviceTime::FrameTimestampCalculatorBaseDeviceTime(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq)
    : DeviceComponentBase(device), deviceTimeFreq_(deviceTimeFreq), frameTimeFreq_(frameTimeFreq) {
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
    auto outputTsp     = (baseDevTime_ & BASE_DEV_TIME_MASK) + srcTimestamp;
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

}  // namespace libobsensor