// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IFilter.hpp"
#include "FormatConverterProcess.hpp"
#include "stream/StreamProfile.hpp"
#include <mutex>
#include <map>

namespace libobsensor {

class PointCloudFilter : public IFilterBase {
    enum class OBPointCloudDistortionType {
        // The depth camera already includes the distortion from the color camera.
        // When converting to a point cloud, distortion correction is needed.
        OB_POINT_CLOUD_UN_DISTORTION_TYPE = 0,

        // The depth camera does not include the distortion from the color camera.
        // When converting to a point cloud, the distortion from the color camera should be added,
        // and then distortion correction should be performed.
        OB_POINT_CLOUD_ADD_DISTORTION_TYPE,

        // The depth camera has non-zero distortion parameters, and the color camera also has non-zero distortion parameters.
        // This might indicate an abnormality in the module parameters. By default, it is assumed that the distortion is zero.
        OB_POINT_CLOUD_ZERO_DISTORTION_TYPE,
    };

public:
    PointCloudFilter();
    virtual ~PointCloudFilter() noexcept override;

    void reset() override;

    void               updateConfig(std::vector<std::string> &params) override;
    void               setConfigData(void *data, uint32_t size) override;
    const std::string &getConfigSchema() const override;

private:
    bool                   hasIntrinsicChanged(const OBCameraIntrinsic &cached, const OBCameraIntrinsic &target);
    std::shared_ptr<Frame> createDepthPointCloud(std::shared_ptr<const Frame> frame);
    std::shared_ptr<Frame> createRGBDPointCloud(std::shared_ptr<const Frame> frame);

    std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;

    PointCloudFilter::OBPointCloudDistortionType getDistortionType(OBCameraDistortion colorDistortion, OBCameraDistortion depthDistortion);

    void updateOutputProfile(const std::shared_ptr<const Frame> frame);

protected:
    OBFormat               pointFormat_;
    float                  positionDataScale_;
    OBCoordinateSystemType coordinateSystemType_;
    bool                   isColorDataNormalization_;
    bool                   isOutputZeroPoint_;

    std::shared_ptr<FormatConverter> formatConverter_;

    // data for depth
    OBCameraIntrinsic      depthDstIntrinsic_{};
    uint32_t               depthTablesDataSize_;
    std::shared_ptr<float> depthTablesData_;
    OBXYTables             depthXyTables_;
    // data for rgbd
    OBCameraIntrinsic          rgbdDstIntrinsic_{};
    OBPointCloudDistortionType rgbdDistortionType_{ OBPointCloudDistortionType::OB_POINT_CLOUD_UN_DISTORTION_TYPE };
    uint32_t                   rgbdTablesDataSize_;
    std::shared_ptr<float>     rgbdTablesData_;
    OBXYTables                 rgbdXyTables_;

    std::map<std::tuple<std::string, uint8_t>, std::shared_ptr<VideoStreamProfile>> registeredProfiles_;
    std::shared_ptr<const VideoStreamProfile>                                       sourceStreamProfile_;
    std::shared_ptr<VideoStreamProfile>                                             targetStreamProfile_;

    uint8_t decimationFactor_;
    uint8_t patchSize_;
    bool    optionsChanged_;
    bool    recalcProfile_;
};

}  // namespace libobsensor
