#pragma once

#include <string.h>
#include <vector>
#include <memory>
#include "libobsensor/h/ObTypes.h"

namespace libobsensor {

const OBExtrinsic IdentityExtrinsics = { {
                                             1,
                                             0,
                                             0,
                                             0,
                                             1,
                                             0,
                                             0,
                                             0,
                                             1,
                                         },
                                         { 0, 0, 0 } };

class CameraParamProcessor {
public:
    static void cameraIntrinsicParamsMirror(OBCameraIntrinsic *intrinsic);
    static void distortionParamMirror(OBCameraDistortion *distort);
    static void d2cTransformParamsMirror(OBD2CTransform *transform);

    static void cameraIntrinsicParamsFlip(OBCameraIntrinsic *intrinsic);
    static void distortionParamFlip(OBCameraDistortion *distort);
    static void d2cTransformParamsFlip(OBD2CTransform *transform);

    static void cameraIntrinsicParamsRotate90(OBCameraIntrinsic *intrinsic);
    static void distortionParamRotate90(OBCameraDistortion *distort);
    static void d2cTransformParamsRotate90(OBD2CTransform *transform);

    static void cameraIntrinsicParamsRotate180(OBCameraIntrinsic *intrinsic);
    static void distortionParamRotate180(OBCameraDistortion *distort);
    static void d2cTransformParamsRotate180(OBD2CTransform *transform);

    static void cameraIntrinsicParamsRotate270(OBCameraIntrinsic *intrinsic);
    static void distortionParamRotate270(OBCameraDistortion *distort);
    static void d2cTransformParamsRotate270(OBD2CTransform *transform);

    static void mirrorCameraParam(OBCameraParam *cameraParam);
    static void flipCameraParam(OBCameraParam *cameraParam);
    static void rotateCameraParam(OBCameraParam *cameraParam, int rotateAngle);

    static OBExtrinsic multiplyExtrinsic(const OBExtrinsic &extrinsic1, const OBExtrinsic &extrinsic2);
    static OBExtrinsic inverseExtrinsic(const OBExtrinsic &extrinsic);
};
}  // namespace libobsensor