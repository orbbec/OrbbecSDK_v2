// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "LiDARPointFilter.hpp"
#include "exception/ObException.hpp"
#include "logger/LoggerInterval.hpp"
#include "frame/FrameFactory.hpp"
#include "stream/StreamProfile.hpp"
#include "InternalTypes.hpp"

namespace libobsensor {

struct LiDARFilterThreshold {
    uint16_t neighbors;
    uint16_t pointNumThreshold;
    float    tanAngleThreshold;
};

#define EPS 0.00001f

static std::unordered_map<uint32_t, LiDARFilterThreshold> LIDAR_FILTER_THRESHOLD_MAP = {
    { 1, { 0, 2, 0.0044f } },  // filterLevel_ = 1
    { 2, { 0, 2, 0.0087f } },  // filterLevel_ = 2
    { 3, { 1, 2, 0.0087f } },  // filterLevel_ = 3
    { 4, { 0, 2, 0.0175f } },  // filterLevel_ = 4
    { 5, { 1, 2, 0.0175f } },  // filterLevel_ = 5
};

LiDARPointFilter::LiDARPointFilter() : filterLevel_(0), kernelSize_(5) {}

LiDARPointFilter::~LiDARPointFilter() {
    cacheDistanceTrans_.clear();
}

void LiDARPointFilter::updateConfig(std::vector<std::string> &params) {
    // FilterLevel
    std::lock_guard<std::recursive_mutex> lock(paramsMutex_);
    if(params.size() != 1) {
        throw invalid_value_exception("LiDARPointFilter config error: params size not match");
    }
    try {
        filterLevel_ = std::stoi(params[0]);
    }
    catch(const std::exception &e) {
        throw invalid_value_exception("LiDARPointFilter config error: " + std::string(e.what()));
    }
}

const std::string &LiDARPointFilter::getConfigSchema() const {
    // csv format: name，type，min，max，step，default，description
    static const std::string schema = "FilterLevel, integer, 0, 5, 1, 0, filter level of lidar point cloud";
    return schema;
}

std::shared_ptr<Frame> LiDARPointFilter::process(std::shared_ptr<const Frame> frame) {
    if(!frame) {
        return nullptr;
    }

    auto lidarProfile = frame->getStreamProfile()->as<LiDARStreamProfile>()->getInfo();
    auto outFrame     = FrameFactory::createFrameFromOtherFrame(frame, true);

    uint32_t pointCount       = static_cast<uint32_t>(outFrame->getDataSize() / sizeof(OBLiDARSpherePoint));
    auto     spherePointPtr   = reinterpret_cast<OBLiDARSpherePoint *>(outFrame->getDataMutable());
    uint32_t totalPointNumber = lidarProfile.maxDataBlockNum * lidarProfile.pointsNum;

    // no filter
    if(filterLevel_ <= 0) {
        return outFrame;
    }

    frameDataFilter(spherePointPtr, pointCount, totalPointNumber);
    return outFrame;
}

void LiDARPointFilter::frameDataFilter(OBLiDARSpherePoint *pointData, uint32_t pointCount, uint32_t totalPointNumber) {

    constexpr float MY_PI     = 3.14159265358979323846f;
    constexpr float DEG_2_RAD = MY_PI / 180.0f;

    // TODO: obtain from the lidar device
    const constexpr float       startAngle = 45;
    const constexpr float       stopAngle  = 315;
    const constexpr float       angleRange = stopAngle - startAngle;
    const LiDARFilterThreshold &threshold  = LIDAR_FILTER_THRESHOLD_MAP[filterLevel_];

    const float angleResolution = angleRange / totalPointNumber * DEG_2_RAD;
    const float angleResSin     = std::sin(angleResolution);
    const float angleResCos     = std::cos(angleResolution);

    if(cacheDistanceTrans_.size() < pointCount) {
        cacheDistanceTrans_.resize(pointCount);
    }

    for(uint32_t i = 0; i < pointCount; i++) {
        const auto &centerCosPhi = std::cos(pointData[i].phi * DEG_2_RAD);
        cacheDistanceTrans_[i]   = pointData[i].distance * centerCosPhi;
    }

    uint32_t sumLeftAbnormal  = 0;
    uint32_t sumRightAbnormal = 0;
    uint32_t halfKernelSize   = kernelSize_ >> 1;

    float pointDistanceCenterCos = 0;
    float pointDistanceCenterSin = 0;
    float pointDistanceCmp       = 0;

    for(uint32_t i = halfKernelSize; i < (pointCount - halfKernelSize); i++) {
        sumLeftAbnormal  = 0;
        sumRightAbnormal = 0;

        pointDistanceCenterCos = cacheDistanceTrans_[i] * angleResCos;
        pointDistanceCenterSin = cacheDistanceTrans_[i] * angleResSin;

        for(uint32_t j = i - halfKernelSize; j < (i + halfKernelSize); j++) {

            // To avoid comparing the point cloud to be detected with itself, no filtering is performed
            // when the distance of the current point cloud to be detected is 0.
            if((j == i) || (std::abs(cacheDistanceTrans_[i]) < EPS)) {
                continue;
            }

            pointDistanceCmp      = cacheDistanceTrans_[j];
            const float &diff     = pointDistanceCmp - pointDistanceCenterCos;
            auto         tanAngle = pointDistanceCenterSin / diff;

            if((tanAngle < threshold.tanAngleThreshold) && (tanAngle > -threshold.tanAngleThreshold)) {
                if(j < i)
                    sumLeftAbnormal++;
                else
                    sumRightAbnormal++;

                if((sumLeftAbnormal >= threshold.pointNumThreshold) || (sumRightAbnormal >= threshold.pointNumThreshold)) {
                    for(int num = (-threshold.neighbors); num <= threshold.neighbors; num++) {
                        pointData[i + num].distance = 0.f;
                    }
                }
            }
        }
    }
}

}  // namespace libobsensor
