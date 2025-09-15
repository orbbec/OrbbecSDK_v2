#pragma once
#include "PipelineHolder.hpp"
#include <libobsensor/ObSensor.hpp>
#include <iostream>
#include <vector>
#include <mutex>
#include <condition_variable>


class FramePairingManager {
public:
    FramePairingManager();
    ~FramePairingManager();

private:
    bool pipelineHoldersFrameNotEmpty();
    void sortFrameMap(std::vector<std::shared_ptr<PipelineHolder>> &pipelineHolders, std::vector<std::shared_ptr<PipelineHolder>> &pipelineHolderVector);

public:
    void setPipelineHolderList(std::vector<std::shared_ptr<PipelineHolder>> pipelineHolderList);

    std::vector<std::pair<std::shared_ptr<ob::Frame>, std::shared_ptr<ob::Frame>>> getFramePairs();

    void release();

private:
    bool     destroy_;
    bool     timestampPairingEnable_;
    uint64_t timestampPairingRange_;

    std::vector<std::shared_ptr<PipelineHolder>> pipelineHolderList_;
    std::map<int, std::shared_ptr<PipelineHolder>> depthPipelineHolderList_;
    std::map<int, std::shared_ptr<PipelineHolder>> colorPipelineHolderList_;
};
