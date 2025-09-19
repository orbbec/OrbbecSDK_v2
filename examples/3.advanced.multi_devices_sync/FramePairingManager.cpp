#include "FramePairingManager.hpp"
#include <map>
#include <algorithm>


uint64_t getFrameTimestampMsec(const std::shared_ptr<const ob::Frame> frame) {
    return frame->getTimeStampUs() / 1000;
}


FramePairingManager::FramePairingManager()
    : destroy_(false) {

}

FramePairingManager::~FramePairingManager() {
    release();
}

bool FramePairingManager::pipelineHoldersFrameNotEmpty() {
    if(pipelineHolderList_.size() == 0) {
        return false;
    }

    for(const auto &holder: pipelineHolderList_) {
        if(!holder->isFrameReady()) {
            return false;
        }
    }
    return true;
}

void FramePairingManager::setPipelineHolderList(std::vector<std::shared_ptr<PipelineHolder>> pipelineHolderList) {
	this->pipelineHolderList_ = pipelineHolderList;
    for(auto &&pipelineHolder: pipelineHolderList) {
        int deviceIndex = pipelineHolder->getDeviceIndex();
        if(pipelineHolder->getSensorType() == OB_SENSOR_DEPTH) {
            depthPipelineHolderList_[deviceIndex] = pipelineHolder;
        }
        if(pipelineHolder->getSensorType() == OB_SENSOR_COLOR) {
            colorPipelineHolderList_[deviceIndex] = pipelineHolder;
        }
    }
}

std::vector<std::pair<std::shared_ptr<ob::Frame>, std::shared_ptr<ob::Frame>>> FramePairingManager::getFramePairs() {
    std::vector<std::pair<std::shared_ptr<ob::Frame>, std::shared_ptr<ob::Frame>>> framePairs;
    if(pipelineHolderList_.size() > 0) {
        int depthPipelineHolderSize = static_cast<int>(depthPipelineHolderList_.size());
        auto start = std::chrono::steady_clock::now();
        // Timestamp Matching Mode.
        while(!pipelineHoldersFrameNotEmpty() && !destroy_) {
            // Wait for frames if not yet available (optional: add sleep for simulation)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            if(elapsed > 200) {
                return framePairs;
            }
        }

        if(destroy_) {
            return framePairs;
        }

        bool discardFrame = false;

        std::map<int, std::shared_ptr<ob::Frame>> depthFramesMap;
        std::map<int, std::shared_ptr<ob::Frame>> colorFramesMap;

        std::vector<std::shared_ptr<PipelineHolder>> pipelineHolderVector;
        sortFrameMap(pipelineHolderList_, pipelineHolderVector);

        auto        refIter       = pipelineHolderVector.begin();
        const auto &refHolder     = *refIter;
        auto        refTsp        = getFrameTimestampMsec(refHolder->frontFrame());
        auto        refHalfTspGap = refHolder->halfTspGap;
        for(const auto &item: pipelineHolderVector) {
            auto     tarFrame      = item->frontFrame();
            auto     tarHalfTspGap = item->halfTspGap;
            int      index         = item->getDeviceIndex();
            auto     frameType     = item->getFrameType();
            uint32_t tspHalfGap    = tarHalfTspGap > refHalfTspGap ? tarHalfTspGap : refHalfTspGap;

            // std::cout << "tspHalfGap : " << tspHalfGap << std::endl;

            auto tarTsp  = getFrameTimestampMsec(tarFrame);
            auto diffTsp = tarTsp - refTsp;
            if(diffTsp > tspHalfGap) {
                discardFrame = true;
                //std::cout << "index = " << index << " frame type = " << frameType << " diff tsp = " << diffTsp << std::endl;
                break;
            }

            refHalfTspGap = tarHalfTspGap;

            if(frameType == OB_FRAME_DEPTH) {
                depthFramesMap[index] = item->getFrame();
            }
            if(frameType == OB_FRAME_COLOR) {
                colorFramesMap[index] = item->getFrame();
            }
        }

        if(discardFrame) {
            depthFramesMap.clear();
            colorFramesMap.clear();
            return framePairs;
        }

        std::cout << "=================================================" << std::endl;

        for(int i = 0; i < depthPipelineHolderSize; i++) {
            auto depthFrame = depthFramesMap[i];
            auto colorFrame = colorFramesMap[i];
            std::cout << "Device#" << i << ", "
                      << " depth(us) "
                      << ", frame timestamp=" << depthFrame->timeStampUs() << ","
                      << "global timestamp = " << depthFrame->globalTimeStampUs() << ","
                      << "system timestamp = " << depthFrame->systemTimeStampUs() << std::endl;

            std::cout << "Device#" << i << ", "
                      << " color(us) "
                      << ", frame timestamp=" << colorFrame->timeStampUs() << ","
                      << "global timestamp = " << colorFrame->globalTimeStampUs() << ","
                      << "system timestamp = " << colorFrame->systemTimeStampUs() << std::endl;

            framePairs.emplace_back(depthFrame, colorFrame);
        }
        return framePairs;
    }

    return framePairs;
}

void FramePairingManager::sortFrameMap(std::vector<std::shared_ptr<PipelineHolder>> &pipelineHolders,
                                       std::vector<std::shared_ptr<PipelineHolder>> &pipelineHolderVector) {
    for(const auto &holder: pipelineHolders) {
        pipelineHolderVector.push_back(holder);
    }

    std::sort(pipelineHolderVector.begin(), pipelineHolderVector.end(), [](const std::shared_ptr<PipelineHolder> &x, const std::shared_ptr<PipelineHolder> &y) {
        auto xTsp = getFrameTimestampMsec(x->frontFrame());
        auto yTsp = getFrameTimestampMsec(y->frontFrame());
        return xTsp < yTsp;
    });
}

void FramePairingManager::release() {
    destroy_ = true;
}
