// #pragma once
#include "FrameAggregator.hpp"
#include "frame/FrameFactory.hpp"
#include "logger/Logger.hpp"
#include "utils/PublicTypeHelper.hpp"

#include <thread>

namespace libobsensor {

#define MAX_FRAME_DELAY 0.5f  // 0.3s max delay diff + 0.1s max frame gap
#define MAX_NORMAL_MODE_QUEUE_SIZE 3

typedef std::map<OBFrameType, SourceFrameQueue>::iterator FrameQueuePair;

const std::map<OBStreamType, OBFrameType> STREAM_FRAME_TYPE_MAP = {
    { OB_STREAM_COLOR, OB_FRAME_COLOR },     { OB_STREAM_DEPTH, OB_FRAME_DEPTH },        { OB_STREAM_IR, OB_FRAME_IR },
    { OB_STREAM_IR_LEFT, OB_FRAME_IR_LEFT }, { OB_STREAM_IR_RIGHT, OB_FRAME_IR_RIGHT },  { OB_STREAM_ACCEL, OB_FRAME_ACCEL },
    { OB_STREAM_GYRO, OB_FRAME_GYRO },       { OB_STREAM_RAW_PHASE, OB_FRAME_RAW_PHASE }
};

uint64_t getFrameTimestampMsec(const std::shared_ptr<const Frame> &frame, FrameSyncMode syncMode) {
    if(syncMode == FrameSyncModeSyncAccordingSystemTimestamp) {
        return frame->getSystemTimeStampUsec() / 1000;
    }
    return frame->getTimeStampUsec() / 1000;
}

FrameAggregator::FrameAggregator()
    : frameSyncMode_(FrameSyncModeDisable),
      miniTimeStamp_(0),
      frameAggregateOutputMode_(OB_FRAME_AGGREGATE_OUTPUT_ANY_SITUATION),
      frameCnt_(0),
      withColorFrame_(false),
      matchingRateFirst_(true) {}

FrameAggregator::~FrameAggregator() noexcept {
    reset();
}

void FrameAggregator::updateConfig(std::shared_ptr<const Config> config, const bool matchingRateFirst) {
    std::unique_lock<std::recursive_mutex> lk(srcFrameQueueMutex_);
    frameAggregateOutputMode_ = config->getFrameAggregateOutputMode();
    matchingRateFirst_        = matchingRateFirst;
    reset();
    auto profiles = config->getEnabledStreamProfileList();
    for(auto &profile: profiles) {
        float fps = 0;
        if(profile->is<const VideoStreamProfile>()) {
            auto videoProfile = profile->as<const VideoStreamProfile>();
            fps               = (float)videoProfile->getFps();
        }
        else if(profile->is<const AccelStreamProfile>()) {
            auto accelStreamProfile = profile->as<const AccelStreamProfile>();
            fps                     = utils::mapIMUSampleRateToValue(accelStreamProfile->getSampleRate());
        }
        else if(profile->is<const GyroStreamProfile>()) {
            auto gyroStreamProfile = profile->as<const GyroStreamProfile>();
            fps                    = utils::mapIMUSampleRateToValue(gyroStreamProfile->getSampleRate());
        }

        float maxSyncQueueSize = fps * MAX_FRAME_DELAY + 1;
        maxSyncQueueSize += ((maxSyncQueueSize - (int)maxSyncQueueSize) > 0 ? 1 : 0);
        auto halfTspGap = static_cast<uint32_t>(500.0f / fps + 0.5);  // +0.5以完成四舍五入
        srcFrameQueueMap_.insert(
            { STREAM_FRAME_TYPE_MAP.find(profile->getType())->second, { std::queue<std::shared_ptr<const Frame>>(), (uint32_t)maxSyncQueueSize, halfTspGap } });
    }
}

void FrameAggregator::pushFrame(std::shared_ptr<const Frame> frame) {
    std::unique_lock<std::recursive_mutex> lk(srcFrameQueueMutex_);
    withEmptyQueue_ = false;
    // auto tmp = frame->getSystemTimeStamp();
    // LOG_INFO("Type: {}, AggFrameTimeStamp: {}", frame->getType(), frame->getSystemTimeStamp());
    auto frameType = frame->getType();
    for(auto &item: srcFrameQueueMap_) {
        if(item.first == frameType) {
            item.second.queue.push(frame);
            uint32_t maxQueueSize_ = frameSyncMode_ == FrameSyncModeDisable ? MAX_NORMAL_MODE_QUEUE_SIZE : item.second.maxSyncQueueSize_;
            // auto size = item.second.queue.size();
            // LOG_INFO("Type: {}, queueSize: {}", frame->getType(), size);
            if(item.second.queue.size() >= maxQueueSize_) {
                withOverflowQueue_          = true;
                withOverflowQueueFrameType_ = frameType;
            }

            auto timestamp = getFrameTimestampMsec(frame, frameSyncMode_);
            if(srcFrameQueueMap_.size() > 1 && frameSyncMode_ && (srcFrameQueueMap_.size() == 2 || !matchingRateFirst_)
               && (miniTimeStamp_ == 0 || timestamp < miniTimeStamp_)) {
                miniTimeStamp_          = timestamp;
                miniTimeStampFrameType_ = frameType;
            }
        }
        else if(item.second.queue.empty()) {
            withEmptyQueue_ = true;
        }
    }
    tryAggregator();
}

void sortFrameMap(std::map<OBFrameType, SourceFrameQueue> &frameMap, std::vector<FrameQueuePair> &frameVec, FrameSyncMode frameSyncMode) {
    // srcFrameQueueMap_排序，队头时间戳小的队列的pair放前面
    auto iter = frameMap.begin();
    while(iter != frameMap.end()) {
        if(!iter->second.queue.empty()) {
            frameVec.push_back(iter);
        }
        iter++;
    }
    std::sort(frameVec.begin(), frameVec.end(), [frameSyncMode](const FrameQueuePair &x, const FrameQueuePair &y) {
        auto xTsp = getFrameTimestampMsec(x->second.queue.front(), frameSyncMode);
        auto yTsp = getFrameTimestampMsec(y->second.queue.front(), frameSyncMode);
        return xTsp < yTsp;
    });
}

void FrameAggregator::tryAggregator() {
    std::unique_lock<std::recursive_mutex> lk(srcFrameQueueMutex_);
    while(!withEmptyQueue_ || withOverflowQueue_) {
        frameCnt_       = 0;
        withColorFrame_ = false;
        auto frameSet   = FrameFactory::createFrameSet();
        if(!frameSet) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // 等100ms后再次尝试是否能申请成功
            continue;
        }
        if(srcFrameQueueMap_.size() > 1 && frameSyncMode_) {
            if(matchingRateFirst_ && srcFrameQueueMap_.size() != 2) {  // 匹配率优先
                std::vector<FrameQueuePair> frameVector;
                sortFrameMap(srcFrameQueueMap_, frameVector, frameSyncMode_);  // srcFrameQueueMap_排序
                auto refIter       = frameVector.begin();
                auto refTsp        = getFrameTimestampMsec((*refIter)->second.queue.front(), frameSyncMode_);
                auto refHalfTspGap = (*refIter)->second.halfTspGap;
                for(auto &item: frameVector) {
                    auto    &tarFrame      = item->second.queue.front();
                    auto     tarHalfTspGap = item->second.halfTspGap;
                    uint32_t tspHalfGap    = tarHalfTspGap < refHalfTspGap ? tarHalfTspGap : refHalfTspGap;

                    auto tarTsp = getFrameTimestampMsec(tarFrame, frameSyncMode_);
                    if(tarTsp - refTsp > tspHalfGap) {
                        break;
                    }
                    refTsp        = tarTsp;  // 出队后，将本次参考时间戳保存下来，作为下一次循环的参考
                    refHalfTspGap = tarHalfTspGap;
                    frameSet->pushFrame(std::move(tarFrame));
                    frameCnt_++;
                    if(item->first == OB_FRAME_COLOR) {
                        withColorFrame_ = true;
                    }
                    item->second.queue.pop();
                    if(withOverflowQueue_ && item->first == withOverflowQueueFrameType_) {
                        withOverflowQueue_ = false;
                    }
                    if(!withEmptyQueue_ && item->second.queue.empty()) {
                        withEmptyQueue_ = true;
                    }
                }
            }
            else {  // 匹配精度优先
                uint64_t    newMiniTimeStamp_          = 0;
                OBFrameType newMiniTimeStampFrameType_ = miniTimeStampFrameType_;
                // 同步匹配
                auto refIter       = srcFrameQueueMap_.find(miniTimeStampFrameType_);
                auto refTsp        = getFrameTimestampMsec(refIter->second.queue.front(), frameSyncMode_);
                auto refHalfTspGap = refIter->second.halfTspGap;
                for(auto &item: srcFrameQueueMap_) {
                    if(!item.second.queue.empty()) {
                        auto    &tarFrame      = item.second.queue.front();
                        auto     tarHalfTspGap = item.second.halfTspGap;
                        uint32_t tspHalfGap    = tarHalfTspGap < refHalfTspGap ? tarHalfTspGap : refHalfTspGap;
                        auto     tarTsp        = getFrameTimestampMsec(tarFrame, frameSyncMode_);
                        if(tarTsp - refTsp <= tspHalfGap) {
                            frameSet->pushFrame(std::move(tarFrame));
                            frameCnt_++;
                            if(item.first == OB_FRAME_COLOR) {
                                withColorFrame_ = true;
                            }
                            item.second.queue.pop();
                            if(withOverflowQueue_ && item.first == withOverflowQueueFrameType_) {
                                withOverflowQueue_ = false;
                            }
                        }
                    }

                    if(item.second.queue.empty()) {
                        withEmptyQueue_ = true;
                        continue;
                    }

                    auto tsp = getFrameTimestampMsec(item.second.queue.front(), frameSyncMode_);
                    if(newMiniTimeStamp_ == 0 || newMiniTimeStamp_ > tsp) {
                        newMiniTimeStamp_          = tsp;
                        newMiniTimeStampFrameType_ = item.second.queue.front()->getType();
                    }
                }
                miniTimeStamp_          = newMiniTimeStamp_;
                miniTimeStampFrameType_ = newMiniTimeStampFrameType_;
            }
        }
        else {
            // 非同步匹配
            for(auto &item: srcFrameQueueMap_) {
                if(!withEmptyQueue_ || item.second.queue.size() >= MAX_NORMAL_MODE_QUEUE_SIZE - 1) {
                    auto &srcFrame = item.second.queue.front();
                    frameSet->pushFrame(std::move(srcFrame));
                    item.second.queue.pop();
                    frameCnt_++;
                    if(item.first == OB_FRAME_COLOR) {
                        withColorFrame_ = true;
                    }
                }
            }
            withEmptyQueue_    = true;
            withOverflowQueue_ = false;
        }
        // test code:
        // auto        depthFrame = frameSet->getFrame(OB_FRAME_DEPTH);
        // auto        colorFrame = frameSet->getFrame(OB_FRAME_COLOR);
        // auto        irFrame    = frameSet->getFrame(OB_FRAME_IR);
        // std::string msg;
        // if(depthFrame) {
        //     msg = msg + " | depthFrame tsp=" + std::to_string(depthFrame->getTimeStamp());
        // }
        // if(irFrame) {
        //     msg = msg + " | irFrame tsp=" + std::to_string(irFrame->getTimeStamp());
        // }
        // if(colorFrame) {
        //     msg = msg + " | colorFrame tsp=" + std::to_string(colorFrame->getTimeStamp());
        // }
        // if(depthFrame && irFrame) {
        //     msg = msg + " | depth-ir diff=" + std::to_string((int64_t)depthFrame->getTimeStamp() - (int64_t)irFrame->getTimeStamp());
        // }
        // if(depthFrame && colorFrame) {
        //     msg = msg + " | depth-color diff=" + std::to_string((int64_t)depthFrame->getTimeStamp() - (int64_t)colorFrame->getTimeStamp());
        // }
        // if(frameSyncMode_){
        //     LOG_WARN(msg;
        // }
        outputFrameset(frameSet);
    }
}

void FrameAggregator::outputFrameset(std::shared_ptr<const FrameSet> frameSet) {
    if(frameSet != nullptr) {
        if(srcFrameQueueMap_.size() == 1 || frameAggregateOutputMode_ == OB_FRAME_AGGREGATE_OUTPUT_ANY_SITUATION) {
            FrameSetCallbackFunc_(frameSet);
        }
        else if(frameAggregateOutputMode_ == OB_FRAME_AGGREGATE_OUTPUT_COLOR_FRAME_REQUIRE && withColorFrame_) {
            FrameSetCallbackFunc_(frameSet);
        }
        else if(frameAggregateOutputMode_ == OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE && frameCnt_ == srcFrameQueueMap_.size()) {
            FrameSetCallbackFunc_(frameSet);
        }
    }
}

void FrameAggregator::enableFrameSync(FrameSyncMode mode) {
    if(frameSyncMode_ != mode) {
        std::unique_lock<std::recursive_mutex> lk(srcFrameQueueMutex_);
        frameSyncMode_ = mode;
        clearAllFrameQueue();
    }
}

void FrameAggregator::setCallback(FrameCallback callback) {
    FrameSetCallbackFunc_ = callback;
}

void FrameAggregator::clearAllFrameQueue() {
    std::unique_lock<std::recursive_mutex> lk(srcFrameQueueMutex_);
    for(auto &item: srcFrameQueueMap_) {
        auto &queue = item.second.queue;
        while(!queue.empty()) {
            queue.pop();
        }
    }
    miniTimeStamp_     = 0;
    withOverflowQueue_ = false;
}

void FrameAggregator::reset() {
    std::unique_lock<std::recursive_mutex> lk(srcFrameQueueMutex_);
    clearAllFrameQueue();
    srcFrameQueueMap_.clear();
}

void FrameAggregator::clearFrameQueue(OBFrameType frameType) {
    std::unique_lock<std::recursive_mutex> lk(srcFrameQueueMutex_);
    for(auto &item: srcFrameQueueMap_) {
        if(frameType == item.first) {
            auto &queue = item.second.queue;
            while(!queue.empty()) {
                queue.pop();
            }
            break;
        }
    }
}

}  // namespace libobsensor