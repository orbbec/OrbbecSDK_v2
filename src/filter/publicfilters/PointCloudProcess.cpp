// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "PointCloudProcess.hpp"
#include "exception/ObException.hpp"
#include "logger/LoggerInterval.hpp"
#include "frame/FrameFactory.hpp"
#include "stream/StreamProfile.hpp"
#include "libobsensor/h/ObTypes.h"
#include "utils/CoordinateUtil.hpp"
#include "utils/Utils.hpp"

namespace libobsensor {

PointCloudFilter::PointCloudFilter()
    : pointFormat_(OB_FORMAT_POINT),
      positionDataScale_(1.0f),
      coordinateSystemType_(OB_RIGHT_HAND_COORDINATE_SYSTEM),
      isColorDataNormalization_(false),
      tablesDataSize_(0),
      tablesData_(nullptr) {}

PointCloudFilter::~PointCloudFilter() noexcept {
    reset();
}

void PointCloudFilter::reset() {
    if(formatConverter_) {
        formatConverter_.reset();
    }
    if(tablesData_) {
        tablesData_.reset();
        tablesDataSize_ = 0;
    }
}

void PointCloudFilter::updateConfig(std::vector<std::string> &params) {
    if(params.size() != 4) {
        throw invalid_value_exception("PointCloudFilter config error: params size not match");
    }
    try {
        OBFormat type = (OBFormat)std::stoi(params[0]);
        if(type != OB_FORMAT_POINT && type != OB_FORMAT_RGB_POINT) {
            LOG_ERROR("Invalid type, the pointType must be OB_FORMAT_POINT or OB_FORMAT_RGB_POINT");
        }
        else {
            pointFormat_ = type;
        }

        float scale = std::stof(params[1]);
        if(scale >= 0.000009 && scale <= 100) {
            positionDataScale_ = scale;
        }

        int colorDataNormalizationState = std::stoi(params[2]);
        isColorDataNormalization_       = colorDataNormalizationState == 0 ? false : true;

        int csType            = std::stoi(params[3]);
        coordinateSystemType_ = static_cast<OBCoordinateSystemType>(csType);
    }
    catch(const std::exception &e) {
        throw invalid_value_exception("PointCloudFilter config error: " + std::string(e.what()));
    }
}

const std::string &PointCloudFilter::getConfigSchema() const {
    // csv format: name，type， min，max，step，default，description
    static const std::string schema = "pointFormat, integer, 19, 20, 1, 19, create point type: 19 is OB_FORMAT_POINT; 20 is OB_FORMAT_RGB_POINT\n"
                                      "coordinateDataScale, float, 0.00000001, 100, 0.00001, 1.0, coordinate data scale\n"
                                      "colorDataNormalization, integer, 0, 1, 1, 0, color data normal state\n"
                                      "coordinateSystemType, integer, 0, 1, 1, 1, Coordinate system representation type: 0 is left hand; 1 is right hand\n";
    return schema;
}

std::shared_ptr<Frame> PointCloudFilter::createDepthPointCloud(std::shared_ptr<const Frame> frame) {
    std::shared_ptr<const Frame> depthFrame;
    if(frame->is<FrameSet>()) {
        auto frameSet = frame->as<FrameSet>();
        depthFrame    = frameSet->getFrame(OB_FRAME_DEPTH);
    }
    else {
        depthFrame = frame;
    }

    if(depthFrame == nullptr) {
        LOG_ERROR_INTVL("No depth frame found!");
        return nullptr;
    }

    auto depthVideoFrame         = depthFrame->as<VideoFrame>();
    auto depthVideoStreamProfile = depthVideoFrame->getStreamProfile()->as<VideoStreamProfile>();
    auto depthWidth              = depthVideoFrame->getWidth();
    auto depthHeight             = depthVideoFrame->getHeight();
    auto pointDataSize           = depthWidth * depthHeight * sizeof(OBPoint);

    auto pointFrame = FrameFactory::createFrame(OB_FRAME_POINTS, OB_FORMAT_POINT, pointDataSize);
    if(pointFrame == nullptr) {
        LOG_ERROR_INTVL("Acquire point cloud frame failed!");
        return nullptr;
    }

    auto frameSize = depthWidth * depthHeight * 2;
    if(tablesData_ && tablesDataSize_ != frameSize) {
        tablesData_.reset();
        tablesData_ = nullptr;
    }

    if(tablesData_ == nullptr) {
        tablesDataSize_                   = frameSize;
        tablesData_                       = std::shared_ptr<float>(new float[tablesDataSize_], std::default_delete<float[]>());
        OBCameraIntrinsic  depthIntrinsic = depthVideoStreamProfile->getIntrinsic();
        OBCameraDistortion depthDisto     = depthVideoStreamProfile->getDistortion();
        if(!CoordinateUtil::transformationInitXYTables(depthIntrinsic, depthDisto, reinterpret_cast<float *>(tablesData_.get()), &tablesDataSize_,
                                                       &xyTables_)) {
            LOG_ERROR_INTVL("Init transformation coordinate tables failed!");
            tablesData_.reset();
            return nullptr;
        }
    }

    CoordinateUtil::transformationDepthToPointCloud(&xyTables_, depthFrame->getData(), (void *)pointFrame->getData(), positionDataScale_,
                                                    coordinateSystemType_);

    float depthValueScale = depthFrame->as<DepthFrame>()->getValueScale();
    pointFrame->copyInfoFromOther(depthFrame);
    // Actual coordinate scaling = Depth scaling factor / Set coordinate scaling factor.
    pointFrame->as<PointsFrame>()->setCoordinateValueScale(depthValueScale / positionDataScale_);

    return pointFrame;
}

std::shared_ptr<Frame> PointCloudFilter::createRGBDPointCloud(std::shared_ptr<const Frame> frame) {
    if(!frame->is<FrameSet>()) {
        LOG_ERROR_INTVL("Input frame is not a frameset, can not convert to pointcloud!");
        return nullptr;
    }

    auto frameSet   = frame->as<FrameSet>();
    auto depthFrame = frameSet->getFrame(OB_FRAME_DEPTH);
    auto colorFrame = frameSet->getFrame(OB_FRAME_COLOR);

    if(depthFrame == nullptr || colorFrame == nullptr) {
        LOG_ERROR_INTVL("depth frame or color frame not found in frameset!");
        return nullptr;
    }

    auto                       depthVideoFrame         = depthFrame->as<VideoFrame>();
    auto                       colorVideoFrame         = colorFrame->as<VideoFrame>();
    auto                       depthVideoStreamProfile = depthVideoFrame->getStreamProfile()->as<VideoStreamProfile>();
    auto                       colorVideoStreamProfile = colorVideoFrame->getStreamProfile()->as<VideoStreamProfile>();
    OBPointCloudDistortionType distortionType          = getDistortionType(colorVideoStreamProfile->getDistortion(), depthVideoStreamProfile->getDistortion());

    // How C2D can recognize
    uint32_t           dstHeight             = depthVideoFrame->getHeight();
    uint32_t           dstWidth              = depthVideoFrame->getWidth();
    auto               dstVideoStreamProfile = depthVideoStreamProfile;
    OBCameraIntrinsic  dstIntrinsic          = dstVideoStreamProfile->getIntrinsic();
    OBCameraDistortion dstDistortion         = dstVideoStreamProfile->getDistortion();

    // Create an RGBD point cloud frame
    auto pointFrame = FrameFactory::createFrame(OB_FRAME_POINTS, OB_FORMAT_RGB_POINT, dstWidth * dstHeight * sizeof(OBColorPoint));
    if(pointFrame == nullptr) {
        LOG_WARN_INTVL("Acquire point cloud frame failed!");
        return nullptr;
    }

    // decode rgb frame
    if(formatConverter_ == nullptr) {
        formatConverter_ = std::make_shared<FormatConverter>();
    }

    std::shared_ptr<const Frame> tarFrame;
    std::vector<std::string>     params;
    switch(colorFrame->getFormat()) {
    case OB_FORMAT_YUYV:
        // FORMAT_YUYV_TO_RGB
        params.push_back("0");
        formatConverter_->updateConfig(params);
        break;
    case OB_FORMAT_UYVY:
        // FORMAT_UYVY_TO_RGB888
        params.push_back("10");
        formatConverter_->updateConfig(params);
        break;
    case OB_FORMAT_I420:
        // FORMAT_I420_TO_RGB888
        params.push_back("1");
        formatConverter_->updateConfig(params);
        break;
    case OB_FORMAT_MJPG:
        // FORMAT_MJPG_TO_RGB888
        params.push_back("7");
        formatConverter_->updateConfig(params);
        break;
    case OB_FORMAT_NV21:
        // FORMAT_NV21_TO_RGB888
        params.push_back("2");
        formatConverter_->updateConfig(params);
        break;
    case OB_FORMAT_NV12:
        // FORMAT_NV12_TO_RGB888
        params.push_back("3");
        formatConverter_->updateConfig(params);
        break;
    case OB_FORMAT_RGB:
        tarFrame = colorFrame;
        break;
    default:
        throw unsupported_operation_exception("unsupported color format for RgbDepth pointCloud convert!");
    }

    uint8_t *colorData = nullptr;
    if(colorFrame->getFormat() != OB_FORMAT_RGB) {
        tarFrame = formatConverter_->process(colorFrame);
    }

    if(tarFrame) {
        colorData = (uint8_t *)tarFrame->getData();
    }
    else {
        LOG_ERROR_INTVL("get rgb data failed!");
        return nullptr;
    }

    auto frameSize = dstWidth * dstHeight * 2;
    if(tablesData_ && tablesDataSize_ != frameSize) {
        tablesData_.reset();
        tablesData_ = nullptr;
    }

    if(tablesData_ == nullptr) {
        tablesDataSize_ = frameSize;
        tablesData_     = std::shared_ptr<float>(new float[tablesDataSize_], std::default_delete<float[]>());
        if(distortionType == OBPointCloudDistortionType::OB_POINT_CLOUD_ZERO_DISTORTION_TYPE) {
            memset(&dstDistortion, 0, sizeof(OBCameraDistortion));
        }

        if(distortionType == OBPointCloudDistortionType::OB_POINT_CLOUD_ADD_DISTORTION_TYPE) {
            if(!CoordinateUtil::transformationInitAddDistortionUVTables(dstIntrinsic, dstDistortion, reinterpret_cast<float *>(tablesData_.get()),
                                                                        &tablesDataSize_, &xyTables_)) {
                LOG_ERROR_INTVL("Init add distortion transformation coordinate tables failed!");
                return nullptr;
            }
        }
        else {
            if(!CoordinateUtil::transformationInitXYTables(dstIntrinsic, dstDistortion, reinterpret_cast<float *>(tablesData_.get()), &tablesDataSize_,
                                                           &xyTables_)) {
                LOG_ERROR_INTVL("Init transformation coordinate tables failed!");
                return nullptr;
            }
        }
    }

    if(distortionType == OBPointCloudDistortionType::OB_POINT_CLOUD_ADD_DISTORTION_TYPE) {
        CoordinateUtil::transformationDepthToRGBDPointCloudByUVTables(dstIntrinsic, &xyTables_, depthFrame->getData(), colorData, (void *)pointFrame->getData(),
                                                                      positionDataScale_, coordinateSystemType_, isColorDataNormalization_);
    }
    else {
        CoordinateUtil::transformationDepthToRGBDPointCloud(&xyTables_, depthFrame->getData(), colorData, (void *)pointFrame->getData(), positionDataScale_,
                                                            coordinateSystemType_, isColorDataNormalization_, colorVideoFrame->getWidth(), colorVideoFrame->getHeight());
    }

    float depthValueScale = depthVideoFrame->as<DepthFrame>()->getValueScale();
    pointFrame->copyInfoFromOther(depthFrame);
    // Actual coordinate scaling = Depth scaling factor / Set coordinate scaling factor.
    pointFrame->as<PointsFrame>()->setCoordinateValueScale(depthValueScale / positionDataScale_);
    return pointFrame;
}

std::shared_ptr<Frame> PointCloudFilter::process(std::shared_ptr<const Frame> frame) {
    if(!frame) {
        return nullptr;
    }

    std::shared_ptr<Frame> pointsFrame = nullptr;
    if(pointFormat_ == OB_FORMAT_POINT) {
        pointsFrame = createDepthPointCloud(frame);
    }
    else {
        pointsFrame = createRGBDPointCloud(frame);
    }
    return pointsFrame;
}

PointCloudFilter::OBPointCloudDistortionType PointCloudFilter::getDistortionType(OBCameraDistortion colorDistortion, OBCameraDistortion depthDistortion) {
    OBPointCloudDistortionType type;
    OBCameraDistortion         zeroDistortion;
    memset(&zeroDistortion, 0, sizeof(OBCameraDistortion));

    if(memcmp(&colorDistortion, &depthDistortion, sizeof(OBCameraDistortion)) == 0) {
        type = OBPointCloudDistortionType::OB_POINT_CLOUD_UN_DISTORTION_TYPE;
    }
    else if((memcmp(&depthDistortion, &zeroDistortion, sizeof(OBCameraDistortion)) == 0)
            && (memcmp(&colorDistortion, &zeroDistortion, sizeof(OBCameraDistortion)) != 0)) {
        type = OBPointCloudDistortionType::OB_POINT_CLOUD_ADD_DISTORTION_TYPE;
    }
    else {
        type = OBPointCloudDistortionType::OB_POINT_CLOUD_ZERO_DISTORTION_TYPE;
    }
    return type;
}

}  // namespace libobsensor

