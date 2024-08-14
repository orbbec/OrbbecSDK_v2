#pragma once
#include "FilterBase.hpp"
#include "FormatConverterProcess.hpp"
#include <mutex>

namespace libobsensor {

class PointCloudFilter : public FilterBase {
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
    PointCloudFilter(const std::string &name);
    virtual ~PointCloudFilter() noexcept;

    void reset() override;

    void               updateConfig(std::vector<std::string> &params) override;
    const std::string &getConfigSchema() const override;

private:
    std::shared_ptr<Frame> createDepthPointCloud(std::shared_ptr<const Frame> frame);
    std::shared_ptr<Frame> createRGBDPointCloud(std::shared_ptr<const Frame> frame);

    std::shared_ptr<Frame> processFunc(std::shared_ptr<const Frame> frame) override;

    PointCloudFilter::OBPointCloudDistortionType getDistortionType(OBCameraDistortion colorDistortion, OBCameraDistortion depthDistortion);

protected:
    OBFormat               pointFormat_;
    float                  positionDataScale_;
    OBCoordinateSystemType coordinateSystemType_;
    bool                   isColorDataNormalization_;

    std::shared_ptr<FormatConverter> formatConverter_;

    uint32_t               tablesDataSize_;
    std::shared_ptr<float> tablesData_;
    OBXYTables             xyTables_;
};

}  // namespace libobsensor
