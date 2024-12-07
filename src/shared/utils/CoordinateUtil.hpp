// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "libobsensor/h/ObTypes.h"
// #include "core/device/IDevice.hpp"
// #include "core/frame/Frame.hpp"
#include <memory>
#include <cmath>

namespace libobsensor {
#define EPS 1e-4
#define FMAX 1e4

class CoordinateUtil {
public:
    static bool transformation3dTo3d(const OBPoint3f sourcePoint3f, OBD2CTransform transSourceToTarget, OBPoint3f *targetPoint3f);

    static bool transformation2dTo3d(const OBCameraIntrinsic sourceIntrinsic, const OBPoint2f sourcePoint2f, const float sourceDepthPixelValue,
                                     OBD2CTransform transSourceToTarget, OBPoint3f *targetPoint3f);

    static bool transformation2dTo3d(const OBCameraIntrinsic sourceIntrinsic, const OBCameraDistortion sourceDistortion, const OBPoint2f sourcePoint2f,
                                     const float sourceDepthPixelValue, const OBD2CTransform transSourceToTarget, OBPoint3f *targetPoint3f);

    static bool transformation3dTo2d(const OBPoint3f sourcePoint3f, const OBCameraIntrinsic targetIntrinsic, const OBCameraDistortion targetDistortion,
                                     OBD2CTransform transSourceToTarget, OBPoint2f *targetPoint2f);

    static bool transformation2dTo2d(const OBCameraIntrinsic sourceIntrinsic, const OBCameraDistortion sourceDistortion, const OBPoint2f sourcePoint2f,
                                     const float sourceDepthPixelValue, const OBCameraIntrinsic targetIntrinsic, const OBCameraDistortion targetDistortion,
                                     OBD2CTransform transSourceToTarget, OBPoint2f *targetPoint2f);

    static bool transformationColor2dToDepth2d(const OBCameraIntrinsic colorIntrinsic, const OBCameraDistortion colorDistortion, const OBPoint2f colorPixel,
                                               uint16_t *depthMap, float depthScaleMm, const OBCameraIntrinsic depthIntrinsic,
                                               const OBCameraDistortion depthDistortion, OBD2CTransform transDepthToColor, OBD2CTransform transColorToDepth,
                                               OBPoint2f *depthPixel);

    /*static std::shared_ptr<Frame> transformationDepthFrameToColorCamera(std::shared_ptr<IDevice> device, std::shared_ptr<Frame> depthFrame,
    uint32_t targetColorCameraWidth, uint32_t targetColorCameraHeight);*/

    // The memory of data needs to be applied externally. During initialization, the size of the external application is data_size, for example, datasize =
    // 1920*1080 *2 (1920*1080 means image resolution, 2 means there are two luts, The coordinates of x and y respectively)
    static bool transformationInitXYTables(const OBCameraIntrinsic intrinsic, const OBCameraDistortion distortion, float *data, uint32_t *dataSize,
                                           OBXYTables *xyTables);

    static bool transformationInitAddDistortionUVTables(const OBCameraIntrinsic intrinsic, const OBCameraDistortion distortion, float *data, uint32_t *dataSize,
                                                        OBXYTables *uvTables);

    static void transformationDepthToPointCloud(OBXYTables *xyTables, const void *depthImageData, void *pointCloudData, float positionDataScale = 1.0f,
                                                OBCoordinateSystemType type = OB_RIGHT_HAND_COORDINATE_SYSTEM);

    // colorResolution = colorScale * depthResolution
    static void transformationDepthToRGBDPointCloud(OBXYTables *xyTables, const void *depthImageData, const void *colorImageData, void *pointCloudData,
                                                    float positionDataScale = 1.0f, OBCoordinateSystemType type = OB_RIGHT_HAND_COORDINATE_SYSTEM,
                                                    bool colorDataNormalization = false, uint32_t colorWidth = 0, uint32_t colorHeight = 0);

    // colorResolution = colorScale * depthResolution
    static void transformationDepthToRGBDPointCloudByUVTables(const OBCameraIntrinsic rgbIntrinsic, OBXYTables *uvTables, const void *depthImageData,
                                                              const void *colorImageData, void *pointCloudData, float positionDataScale = 1.0f,
                                                              OBCoordinateSystemType type = OB_RIGHT_HAND_COORDINATE_SYSTEM, bool colorDataNormalization = false);
};
}  // namespace libobsensor
