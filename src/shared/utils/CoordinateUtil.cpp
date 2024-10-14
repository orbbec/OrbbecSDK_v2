// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "CoordinateUtil.hpp"
#include "logger/Logger.hpp"
#include "logger/LoggerInterval.hpp"

#include <cmath>

namespace libobsensor {
static bool judgeTransformValid(OBD2CTransform cameraRotParam) {
    // Orthogonality of rotation matrix
    // r1 .*r2 = 0 ;
    float r1r2 = cameraRotParam.rot[0] * cameraRotParam.rot[3] + cameraRotParam.rot[1] * cameraRotParam.rot[4] + cameraRotParam.rot[2] * cameraRotParam.rot[5];

    float r1r3 =
        cameraRotParam.rot[0] * cameraRotParam.rot[6] + cameraRotParam.rot[1] * cameraRotParam.rot[7] + cameraRotParam.rot[2] * cameraRotParam.rot[8];

    if(fabsf(r1r2) < EPS && fabs(r1r3) < EPS) {
        return true;
    }
    return false;
}

static bool judgeIntrinsicValid(OBCameraIntrinsic param) {
    if(param.width == 0 || param.height == 0) {
        return false;
    }

    if((param.fx < 1.0f) || (param.fy < 1.0f )||(param.cx < 1.0f) || (param.cy <1.0f)) {
        return false;
    }
    if((param.fx > FMAX) || (param.fy > FMAX) | (param.cx > FMAX) || (param.cy > FMAX)) {
        return false;
    }

    return true;
}

static bool undistortIterativeUnproject(const OBCameraIntrinsic intrinsic, const OBCameraDistortion disto, const OBPoint2f pixel, OBPoint2f *xy) {
    double r2, r4, r6;
    double x = 0;
    double y = 0;
    double kr_inv, xd, yd, dx, dy;
    xd = (pixel.x - intrinsic.cx) / intrinsic.fx;
    yd = (pixel.y - intrinsic.cy) / intrinsic.fy;

    x               = xd;
    y               = yd;
    double bestErr = 99999;
    int valid          = 1;

    // if(disto.model == OB_DISTORTION_BROWN_CONRADY) {
    {

        for(int i = 0; i < 20; i++) {
            // Iterate to remove distortion
            r2     = x * x + y * y;
            r4     = r2 * r2;
            r6     = r4 * r2;
            kr_inv = (1 + disto.k4 * r2 + disto.k5 * r4 + disto.k6 * r6) / (1 + disto.k1 * r2 + disto.k2 * r4 + disto.k3 * r6);
            dx     = disto.p1 * 2 * x * y + disto.p2 * (r2 + 2 * x * x);
            dy     = disto.p2 * 2 * x * y + disto.p1 * (r2 + 2 * y * y);
            x      = (xd - dx) * kr_inv;
            y      = (yd - dy) * kr_inv;

            // Add distortion to distortion-free points,
            // double a1, a2, a3, cdist, icdist2;
            double a1, a2, a3, cdist;
            double xd0, yd0;
            r2            = x * x + y * y;
            r4            = r2 * r2;
            r6            = r4 * r2;
            a1            = 2 * x * y;
            a2            = r2 + 2 * x * x;
            a3            = r2 + 2 * y * y;
            cdist         = (1 + disto.k1 * r2 + disto.k2 * r4 + disto.k3 * r6) / (1 + disto.k4 * r2 + disto.k5 * r4 + disto.k6 * r6);
            xd0           = x * cdist + disto.p1 * a1 + disto.p2 * a2;
            yd0           = y * cdist + disto.p1 * a3 + disto.p2 * a1;
            double x_proj = xd0 * intrinsic.fx + intrinsic.cx;
            double y_proj = yd0 * intrinsic.fy + intrinsic.cy;
            double error  = sqrt(pow(x_proj - pixel.x, 2) + pow(y_proj - pixel.y, 2));

            if(error > bestErr)
                break;

            bestErr = error;

            if(error < 0.01) {
                break;
            }
        }
    }

    if(bestErr > 0.5) {
        valid = 0;
        xy->x     = (float)xd;
        xy->y     = (float)yd;
        return valid;
    }

     xy->x = (float)x, xy->y = (float)y;
    return valid;
}


void calculateInverseMatrix(OBD2CTransform srcTrans, OBD2CTransform *dstTrans) {

    dstTrans->rot[0] = srcTrans.rot[0];
    dstTrans->rot[1] = srcTrans.rot[3];
    dstTrans->rot[2] = srcTrans.rot[6];
    dstTrans->rot[3] = srcTrans.rot[1];
    dstTrans->rot[4] = srcTrans.rot[4];
    dstTrans->rot[5] = srcTrans.rot[7];
    dstTrans->rot[6] = srcTrans.rot[2];
    dstTrans->rot[7] = srcTrans.rot[5];
    dstTrans->rot[8] = srcTrans.rot[8];

    dstTrans->trans[0] = -(dstTrans->rot[0] * srcTrans.trans[0] + dstTrans->rot[1] * srcTrans.trans[1] + dstTrans->rot[2] * srcTrans.trans[2]);
    dstTrans->trans[1] = -(dstTrans->rot[3] * srcTrans.trans[0] + dstTrans->rot[4] * srcTrans.trans[1] + dstTrans->rot[5] * srcTrans.trans[2]);
    dstTrans->trans[2] = -(dstTrans->rot[6] * srcTrans.trans[0] + dstTrans->rot[7] * srcTrans.trans[1] + dstTrans->rot[8] * srcTrans.trans[2]);
}

bool isPixelValid(const OBPoint2f curr, const OBPoint2f start, const OBPoint2f end) {
    bool valid1 = end.x >= start.x && end.x >= curr.x && curr.x >= start.x;
    bool valid2 = end.x <= start.x && end.x <= curr.x && curr.x <= start.x;
    bool valid3 = end.y >= start.y && end.y >= curr.y && curr.y >= start.y;
    bool valid4 = end.y <= start.y && end.y <= curr.y && curr.y <= start.y;
    return (valid1 || valid2) && (valid3 || valid4);
}

void nextPixel(OBPoint2f &curr, const OBPoint2f start, const OBPoint2f end) {
    if(fabsf(end.x - start.x) < EPS) {
        curr.y = end.y > curr.y ? curr.y + 1 : curr.y - 1;
    }
    else {
        float slope = (end.y - start.y) / (end.x - start.x);

        if(fabs(end.x - curr.x) > fabs(end.y - curr.y)) {
            curr.x = end.x > curr.x ? curr.x + 1 : curr.x - 1;
            curr.y = end.y - slope * (end.x - curr.x);
        }
        else {
            curr.y = end.y > curr.y ? curr.y + 1 : curr.y - 1;
            curr.x = end.x - ((end.y - curr.y) / slope);
        }
    }
}

static void project3dPointToPixelWithDistortion(const OBCameraIntrinsic intrinsic, const OBCameraDistortion distortion, OBPoint3f point, OBPoint2f *pixel, int &valid) {
    double x = point.x / point.z;
    double y = point.y / point.z;

    valid     = 1;
    double xd = x, yd = y;
    //if k1 >0, we need to take image distortion into accout when projecting onto an image
    if(fabsf(distortion.k1) > EPS || fabsf(distortion.k2) > EPS) {
        if(distortion.model == OB_DISTORTION_BROWN_CONRADY){
            double a1, a2, a3, cdist;
            double r2, r4, r6;
            r2    = x * x + y * y;
            r4    = r2 * r2;
            r6    = r4 * r2;
            a1    = 2 * x * y;
            a2    = r2 + 2 * x * x;
            a3    = r2 + 2 * y * y;
            cdist = (1 + distortion.k1 * r2 + distortion.k2 * r4 + distortion.k3 * r6) / (1 + distortion.k4 * r2 + distortion.k5 * r4 + distortion.k6 * r6);
            xd    = x * cdist + distortion.p1 * a1 + distortion.p2 * a2;
            yd    = y * cdist + distortion.p1 * a3 + distortion.p2 * a1;
        }
    }

    pixel->x = (float)xd * intrinsic.fx + intrinsic.cx;
    pixel->y = (float)yd * intrinsic.fy + intrinsic.cy;

    if(pixel->x > intrinsic.width - 1 || pixel->y > intrinsic.height - 1) {
        valid = 0;
    }
    if(pixel->x < 0 || pixel->y < 0) {
        valid = 0;
    }

}

bool CoordinateUtil::transformation3dTo3d(const OBPoint3f sourcePoint3f, OBD2CTransform transSourceToTarget, OBPoint3f *targetPoint3f) {
    // step 1: parameter validity judgment
    if(!judgeTransformValid(transSourceToTarget)) {
        return false;
    }

    if(fabsf(sourcePoint3f.z) < EPS) {
        return false;
    }

    // step 2: Calculate conversion relationship
   // R *X + t
    float rx =
        transSourceToTarget.rot[0] * sourcePoint3f.x + transSourceToTarget.rot[1] * sourcePoint3f.y + transSourceToTarget.rot[2] * sourcePoint3f.z;
    float ry =
        transSourceToTarget.rot[3] * sourcePoint3f.x + transSourceToTarget.rot[4] * sourcePoint3f.y + transSourceToTarget.rot[5] * sourcePoint3f.z;
    float rz =
        transSourceToTarget.rot[6] * sourcePoint3f.x + transSourceToTarget.rot[7] * sourcePoint3f.y + transSourceToTarget.rot[8] * sourcePoint3f.z;

    (*targetPoint3f).x = rx + transSourceToTarget.trans[0];
    (*targetPoint3f).y = ry + transSourceToTarget.trans[1];
    (*targetPoint3f).z = rz + transSourceToTarget.trans[2];

    return true;
}

bool CoordinateUtil::transformation2dTo3d(const OBCameraIntrinsic sourceIntrinsic, const OBPoint2f sourcePoint2f, const float sourceDepthPixelValue,
                                          OBD2CTransform transSourceToTarget, OBPoint3f *targetPoint3f) {
    // step 1: parameter validity judgment
    if(!judgeIntrinsicValid(sourceIntrinsic)) {
        return false;
    }

    if(!judgeTransformValid(transSourceToTarget)) {
        return false;
    }

    // sourcePoint2f is the pixel coordinates of the image, or sub-pixel coordinates
    if(sourcePoint2f.x < 0 || sourcePoint2f.y < 0) {
        return false;
    }

    if(sourcePoint2f.x > (sourceIntrinsic.width - 1) || sourcePoint2f.y > (sourceIntrinsic.height - 1)) {
        return false;
    }

    // step 2: Convert 2D to 3D point
    OBPoint3f source_3f;
    source_3f.z = sourceDepthPixelValue;  // Assignment in z direction
    // Convert 2d to 3d (same as converting point cloud)
    source_3f.x = sourceDepthPixelValue * (sourcePoint2f.x - sourceIntrinsic.cx) / sourceIntrinsic.fx;
    source_3f.y = sourceDepthPixelValue * (sourcePoint2f.y - sourceIntrinsic.cy) / sourceIntrinsic.fy;

    // step 3: Convert the 3D point under the source coordinates to the target camera coordinates
    bool ret = transformation3dTo3d(source_3f, transSourceToTarget, targetPoint3f);

    return ret;
}

bool CoordinateUtil::transformation2dTo3d(const OBCameraIntrinsic sourceIntrinsic, const OBCameraDistortion sourceDistortion, const OBPoint2f sourcePoint2f,
                                                      const float sourceDepthPixelValue, OBD2CTransform transSourceToTarget,
                                                      OBPoint3f *targetPoint3f) {
  // step 1: parameter validity judgment
    if(!judgeIntrinsicValid(sourceIntrinsic)) {
        return false;
    }

    if(!judgeTransformValid(transSourceToTarget)) {
        return false;
    }

    // sourcePoint2f is the pixel coordinates of the image, or sub-pixel coordinates
    if(sourcePoint2f.x < 0 || sourcePoint2f.y < 0) {
        return false;
    }

    if(sourcePoint2f.x > (sourceIntrinsic.width - 1) || sourcePoint2f.y > (sourceIntrinsic.height - 1)) {
        return false;
    }

    // step 2: Convert 2D to 3D point
    OBPoint3f source_3f;
    source_3f.z = sourceDepthPixelValue;  // Assignment in z direction
    // Convert 2d to 3d (same as converting point cloud)
    OBPoint2f pointUnitFocalPlane;
    bool      valid = undistortIterativeUnproject(sourceIntrinsic, sourceDistortion, sourcePoint2f, &pointUnitFocalPlane);

    if(valid == false) {
        return false;
    }

    source_3f.x = pointUnitFocalPlane.x * source_3f.z;
    source_3f.y = pointUnitFocalPlane.y * source_3f.z;

    // step 3: Convert the 3D point under the source to the target camera coordinates
    bool ret = transformation3dTo3d(source_3f, transSourceToTarget, targetPoint3f);

    return ret;
}

bool CoordinateUtil::transformation3dTo2d(const OBPoint3f sourcePoint3f, const OBCameraIntrinsic targetIntrinsic, const OBCameraDistortion targetDistortion,
                                          OBD2CTransform transSourceToTarget, OBPoint2f *targetPoint2f) {

    // step 1: Convert the 3D point under the source to the target camera coordinates
    OBPoint3f targetPoint3f;
    bool ret = transformation3dTo3d(sourcePoint3f, transSourceToTarget, &targetPoint3f);
    if(!ret) {
        return ret;
    }

    // step 2: Convert the 3D point under the target camera coordinates to a 2D point
    int valid = 1;
    project3dPointToPixelWithDistortion(targetIntrinsic, targetDistortion, targetPoint3f, targetPoint2f, valid);

    return valid;
}

bool CoordinateUtil::transformation2dTo2d(const OBCameraIntrinsic sourceIntrinsic, const OBCameraDistortion sourceDistortion, const OBPoint2f sourcePoint2f,
                                          const float sourceDepthPixelValue, const OBCameraIntrinsic targetIntrinsic,
                                          const OBCameraDistortion targetDistortion,OBD2CTransform transSourceToTarget, OBPoint2f *targetPoint2f) {

    // step 1: First convert the 2d of the source into the 3d of the target.
    OBPoint3f targetPoint3f;
    bool      ret = transformation2dTo3d(sourceIntrinsic, sourceDistortion, sourcePoint2f, sourceDepthPixelValue, transSourceToTarget, &targetPoint3f);
    if(!ret) {
        return ret;
    }

    // step 2: Convert the 3D point under the target camera coordinates to the 2D point under the image
    int valid = 1;
    project3dPointToPixelWithDistortion(targetIntrinsic, targetDistortion, targetPoint3f, targetPoint2f, valid);

    return valid;
}

bool CoordinateUtil::transformationColor2dToDepth2d(const OBCameraIntrinsic colorIntrinsic, const OBCameraDistortion colorDistortion,
                                                    const OBPoint2f colorPixel, uint16_t *depthMap, float depthScaleMm, const OBCameraIntrinsic depthIntrinsic,
                                                    const OBCameraDistortion depthDistortion, OBD2CTransform transDepthToColor,
                                                    OBD2CTransform transColorToDepth, OBPoint2f *depthPixel) {

    float depthRangeMm[2] = { 60.f, 16000.f };
    OBPoint2f nearPixelDepth, farthestPixelDepth;
    bool   nearValid = 0, farthestValid = 0;

    //color pixel to depth pixel when z= depthRangeMm[0]
    nearValid = transformation2dTo2d(colorIntrinsic, colorDistortion, colorPixel, depthRangeMm[0], depthIntrinsic, depthDistortion, transColorToDepth,
                                  &nearPixelDepth);
    if(!nearValid) {
        if(nearPixelDepth.x< 0) {
            nearPixelDepth.x = 0;
        }
        if(nearPixelDepth.y <0) {
            nearPixelDepth.y = 0;
        }
        if(nearPixelDepth.x > depthIntrinsic.width -1) {
            nearPixelDepth.x = (float)depthIntrinsic.width - 1;
        }
        if(nearPixelDepth.y > depthIntrinsic.height - 1) {
            nearPixelDepth.y = (float)depthIntrinsic.height - 1;
        }
    }

    // color pixel to depth pixel when z= depthRangeMm[1]
    farthestValid = transformation2dTo2d(colorIntrinsic, colorDistortion, colorPixel, depthRangeMm[1], depthIntrinsic, depthDistortion, transColorToDepth,
                                  &farthestPixelDepth);
    if(!farthestValid) {
        if(farthestPixelDepth.x < 0) {
            farthestPixelDepth.x = 0;
        }
        if(farthestPixelDepth.y < 0) {
            farthestPixelDepth.y = 0;
        }
        if(farthestPixelDepth.x > depthIntrinsic.width - 1) {
            farthestPixelDepth.x = (float)depthIntrinsic.width - 1;
        }
        if(farthestPixelDepth.y > depthIntrinsic.height - 1) {
            farthestPixelDepth.y = (float)depthIntrinsic.height - 1;
        }
    }

    // search along line for the depth pixel that it's projected pixel is the closest to the input pixel
    float minDist = std::numeric_limits<float>::max();
    for(OBPoint2f curPixel = nearPixelDepth; isPixelValid(curPixel, nearPixelDepth, farthestPixelDepth);
        nextPixel(curPixel, nearPixelDepth, farthestPixelDepth)) {

        int x     = (int)curPixel.x;
        int y     = (int)curPixel.y;
        int index =  y* depthIntrinsic.width + x;

        if(depthMap[index] == 0)
            continue;

        float depth = depthScaleMm * depthMap[index];

        // depth pixel to color pixel
        OBPoint2f curColorPixel{};
        transformation2dTo2d(depthIntrinsic, depthDistortion, curPixel, depth, colorIntrinsic, colorDistortion, transDepthToColor, &curColorPixel);
        float errDist = (float)(pow((curColorPixel.x - colorPixel.x), 2) + pow((curColorPixel.y - colorPixel.y), 2));
        if(errDist < minDist) {
            minDist     = errDist;
            depthPixel->x = curPixel.x;
            depthPixel->y = curPixel.y;
        }
    }

    return true;
}
    // std::shared_ptr<Frame> CoordinateUtil::transformationDepthFrameToColorCamera(std::shared_ptr<IDevice> device, std::shared_ptr<Frame> depthFrame,
// uint32_t targetColorCameraWidth, uint32_t targetColorCameraHeight) {
// auto absDevice = std::dynamic_pointer_cast<AbstractDevice>(device);
// if(absDevice == nullptr) {
// throw std::runtime_error("Device is invalid!");
// }
//
// auto cameraParamList = absDevice->getCalibrationCameraParamList();
// if(cameraParamList.empty()) {
// LOG_WARN("Get calibration param failed,cameraParamList is empty!");
// return nullptr;
// }
//
// auto profileInfo =
// absDevice->getSupportedProfileInfo(targetColorCameraWidth, targetColorCameraHeight, depthFrame->getWidth(), depthFrame->getHeight(), ALIGN_D2C_SW_MODE);
// if(!profileInfo.valid()) {
// LOG_ERROR("Input invalid");
// return nullptr;
// }
//
// auto cameraParam = cameraParamList[profileInfo.paramIndex];
// auto depthWidth = depthFrame->getWidth();
// auto depthHeight = depthFrame->getHeight();
////Since the default parameters stored in the module may not match the current image state, for example, the MX6600 project parameters are calibrated in the
///mirrored state, but the output image is non-mirrored, so the parameters need to be processed.
// cameraParam = absDevice->preProcessCameraParam(cameraParam);
////soft D2C filter
// auto d2cFilter = std::make_shared<D2CFilter>();
// d2cFilter->setCameraParam(cameraParam);
// OBD2CAlignParam alignParam = absDevice->getD2CAlignParam();
// d2cFilter->setD2CAlignParam(&alignParam);
// d2cFilter->setColorSize(targetColorCameraWidth, targetColorCameraHeight);
//
////post process filter
// auto postProcessFilter = std::make_shared<PostProcessFilter>();
// postProcessFilter->setPostProcessParam(profileInfo.postProcessParam);
// postProcessFilter->setDepthScaleRequire(true);
// postProcessFilter->setDepthSize(depthWidth, depthHeight);
//
// depthFrame = d2cFilter->process(depthFrame);
// if(depthFrame == nullptr) {
// LOG_ERROR("D2C process failed.");
// return nullptr;
// }
//
// bool isParamTransform = absDevice->getCameraParamTransformState();
// cameraParam = absDevice->transformCameraParamToD2CDstParam(cameraParam, profileInfo.postProcessParam, targetColorCameraWidth, targetColorCameraHeight,
// depthWidth, depthHeight); cameraParam = CameraParamProcessor::postProcessCameraParam(profileInfo.postProcessParam, cameraParam, isParamTransform,
// ALIGN_D2C_SW_MODE,targetColorCameraWidth, targetColorCameraHeight, depthWidth, depthHeight, true);
// postProcessFilter->setTargetIntrinsic(cameraParam.depthIntrinsic);
//
// depthFrame = postProcessFilter->process(depthFrame);
// if(depthFrame == nullptr) {
// LOG_ERROR("Post process failed.");
// return nullptr;
// }
//
// return depthFrame;
// }

bool CoordinateUtil::transformationInitXYTables(const OBCameraIntrinsic intrinsic, const OBCameraDistortion distortion, float *data, uint32_t *dataSize,
                                                OBXYTables *xyTables) {
    // step 1: parameter validity judgment
    if(!judgeIntrinsicValid(intrinsic)) {
        return false;
    }

    int width  = intrinsic.width;
    int height = intrinsic.height;

    size_t tableSize = (size_t)(width * height);
    if(data == NULL)  // If no external memory is requested, an error will be reported
    {
        (*dataSize) = 2 * (uint32_t)tableSize;
        return false;
    }
    else {
        // If the externally requested memory size is not enough, an error will be reported.
        if(*dataSize < 2 * tableSize) {
            LOG_ERROR("Unexpected xy table size {}, should be larger or equal than {}.", int(*dataSize), int(2 * tableSize));
            return false;
        }

        xyTables->width  = width;
        xyTables->height = height;
        xyTables->xTable = data;
        xyTables->yTable = data + tableSize;

        OBPoint2f point2d;
        OBPoint2f outPoint2d;
        for(int y = 0, idx = 0; y < height; y++) {
            point2d.y = (float)y;
            for(int x = 0; x < width; x++, idx++) {
                point2d.x = (float)x;

                // pixel to point without distortion on the unit focal plane
                if(!undistortIterativeUnproject(intrinsic, distortion, point2d, &outPoint2d)) {
                    // x table value of NAN marks invalid
                    xyTables->xTable[idx] = NAN;
                    // set y table value to 0 to speed up SSE implementation
                    xyTables->yTable[idx] = 0.f;
                }
                else {
                    xyTables->xTable[idx] = outPoint2d.x;
                    xyTables->yTable[idx] = outPoint2d.y;
                }
            }
        }
        (*dataSize) = 2 * (uint32_t)tableSize;
    }
    return true;
}

bool CoordinateUtil::transformationInitAddDistortionUVTables(const OBCameraIntrinsic intrinsic, const OBCameraDistortion distortion, float *data,
                                                             uint32_t *dataSize, OBXYTables *uvTables) {
    int                width     = intrinsic.width;
    int                height    = intrinsic.height;

    size_t tableSize = (size_t)(width * height);
    if(data == NULL)  // If no external memory is requested, an error will be reported
    {
        (*dataSize) = 2 * (uint32_t)tableSize;
        return false;
    }
    else {
        // If the externally requested memory size is not enough, an error will be reported.
        if(*dataSize < 2 * tableSize) {
            LOG_ERROR("Unexpected xy table size {}, should be larger or equal than {}.", int(*dataSize), int(2 * tableSize));
            return false;
        }

        // data and ob_xy_tables_t share memory
        uvTables->width  = width;
        uvTables->height = height;
        uvTables->xTable = data;
        uvTables->yTable = data + tableSize;

        for(int row = 0, idx = 0; row < height; row++) {
            double y = ((double)row - intrinsic.cy) / intrinsic.fy;
            for(int col = 0; col < width; col++, idx++) {
                double x = ((double)col - intrinsic.cx) / intrinsic.fx;

                double xd = x, yd = y;
                if(distortion.model == OB_DISTORTION_BROWN_CONRADY) {
                     // Add distortion, only supports Brown model, k2, k3, k6 model, KB is not supported
                     // double a1, a2, a3, cdist, icdist2;
                     double a1, a2, a3, cdist;
                     double r2, r4, r6;
                     r2    = x * x + y * y;
                     r4    = r2 * r2;
                     r6    = r4 * r2;
                     a1    = 2 * x * y;
                     a2    = r2 + 2 * x * x;
                     a3    = r2 + 2 * y * y;
                     cdist = (1 + distortion.k1 * r2 + distortion.k2 * r4 + distortion.k3 * r6)
                             / (1 + distortion.k4 * r2 + distortion.k5 * r4 + distortion.k6 * r6);
                     xd = x * cdist + distortion.p1 * a1 + distortion.p2 * a2;
                     yd = y * cdist + distortion.p1 * a3 + distortion.p2 * a1;
                }

                float x_proj = (float)xd * intrinsic.fx + intrinsic.cx;
                float y_proj = (float)yd * intrinsic.fy + intrinsic.cy;

                // Determine whether the point after adding distortion exceeds the image range. If it exceeds, the point is invalid.
                if(x_proj < 0 || x_proj > (width - 1) || y_proj < 0 || y_proj > (height - 1)) {
                    // Invalid data output
                    // x table value of NAN marks invalid
                    uvTables->xTable[idx] = NAN;
                    // set y table value to 0 to speed up SSE implementation
                    uvTables->yTable[idx] = 0.f;
                }
                else  // Valid data output
                {
                    uvTables->xTable[idx] = x_proj;
                    uvTables->yTable[idx] = y_proj;
                }
            }
        }
        (*dataSize) = 2 * (uint32_t)tableSize;
    }
    return true;
}

void CoordinateUtil::transformationDepthToPointCloud(OBXYTables *xyTables, const void *depthImageData, void *pointCloudData, float positionDataScale,
                                                     OBCoordinateSystemType type) {
    const uint16_t *imageData = (const uint16_t *)depthImageData;
    float          *xyzData   = (float *)pointCloudData;
    float           x, y, z;
    int             coordinateSystemCoefficient = type == OB_LEFT_HAND_COORDINATE_SYSTEM ? -1 : 1;

    for(int i = 0; i < xyTables->width * xyTables->height; i++) {
        float x_tab = xyTables->xTable[i];

        uint16_t depthValue = imageData[i];
        if(!std::isnan(x_tab) && depthValue != 65535) {
            z = (float)depthValue;
            x = x_tab * (float)z;
            y = xyTables->yTable[i] * (float)z * coordinateSystemCoefficient;

            z *= positionDataScale;
            x *= positionDataScale;
            y *= positionDataScale;
        }
        else {
            x = 0.0;
            y = 0.0;
            z = 0.0;
        }

        xyzData[3 * i + 0] = x;
        xyzData[3 * i + 1] = y;
        xyzData[3 * i + 2] = z;
    }
}

void CoordinateUtil::transformationDepthToRGBDPointCloud(OBXYTables *xyTables, const void *depthImageData, const void *colorImageData, void *pointCloudData,
                                                         float positionDataScale, OBCoordinateSystemType type, bool colorDataNormalization) {
    const uint16_t *dImageData = (const uint16_t *)depthImageData;
    const uint8_t  *cImageData = (const uint8_t *)colorImageData;
    float          *xyzrgbData = (float *)pointCloudData;
    float           x, y, z;
    float           r, g, b;
    int             coordinateSystemCoefficient = type == OB_LEFT_HAND_COORDINATE_SYSTEM ? -1 : 1;
    float           colorDivCoeff               = colorDataNormalization ? 255.0f : 1.0f;

    for(int i = 0; i < xyTables->width * xyTables->height; i++) {
        float x_tab = xyTables->xTable[i];

        uint16_t depthValue = dImageData[i];
        if(!std::isnan(x_tab) && depthValue != 65535) {
            z = (float)depthValue;
            x = x_tab * (float)z;
            y = xyTables->yTable[i] * (float)z * coordinateSystemCoefficient;

            z *= positionDataScale;
            x *= positionDataScale;
            y *= positionDataScale;

            r = cImageData[3 * i + 0] / colorDivCoeff;
            g = cImageData[3 * i + 1] / colorDivCoeff;
            b = cImageData[3 * i + 2] / colorDivCoeff;
        }
        else {
            x = 0.0;
            y = 0.0;
            z = 0.0;
            r = 0.0;
            g = 0.0;
            b = 0.0;
        }

        xyzrgbData[6 * i + 0] = x;
        xyzrgbData[6 * i + 1] = y;
        xyzrgbData[6 * i + 2] = z;
        xyzrgbData[6 * i + 3] = r;
        xyzrgbData[6 * i + 4] = g;
        xyzrgbData[6 * i + 5] = b;
    }
}

void CoordinateUtil::transformationDepthToRGBDPointCloudByUVTables(const OBCameraIntrinsic rgbIntrinsic, OBXYTables *uvTables, const void *depthImageData,
                                                                   const void *colorImageData, void *pointCloudData, float positionDataScale,
                                                                   OBCoordinateSystemType type, bool colorDataNormalization) {
    const uint16_t *dImageData = (const uint16_t *)depthImageData;
    const uint8_t  *cImageData = (const uint8_t *)colorImageData;
    float          *xyzrgbData = (float *)pointCloudData;
    float           x, y, z;
    float           r, g, b;
    int             coordinateSystemCoefficient = type == OB_LEFT_HAND_COORDINATE_SYSTEM ? -1 : 1;
    float           colorDivCoeff               = colorDataNormalization ? 255.0f : 1.0f;

    for(int i = 0; i < uvTables->width * uvTables->height; i++) {
        // int u_tab = (int)uvTables->xTable[i];
        // int v_tab = (int)uvTables->yTable[i];

        int xValue = i % uvTables->width;
        int yValue = i / uvTables->width;

        uint16_t depthValue = dImageData[i];
        if(!std::isnan(uvTables->xTable[i]) && depthValue != 65535) {
            z = (float)depthValue;
            x = ((xValue - rgbIntrinsic.cx) / rgbIntrinsic.fx) * (float)z;
            y = ((yValue - rgbIntrinsic.cy) / rgbIntrinsic.fy) * (float)z * coordinateSystemCoefficient;

            z *= positionDataScale;
            x *= positionDataScale;
            y *= positionDataScale;

            int u_rgb   = (int)round(uvTables->xTable[i]);
            int v_rgb   = (int)round(uvTables->yTable[i]);
            int idx_rgb = v_rgb * uvTables->width + u_rgb;

            r = cImageData[3 * idx_rgb + 0] / colorDivCoeff;
            g = cImageData[3 * idx_rgb + 1] / colorDivCoeff;
            b = cImageData[3 * idx_rgb + 2] / colorDivCoeff;
        }
        else {
            x = (int)0.0;
            y = (int)0.0;
            z = 0.0;
            r = 0.0;
            g = 0.0;
            b = 0.0;
        }

        xyzrgbData[6 * i + 0] = (float)x;
        xyzrgbData[6 * i + 1] = (float)y;
        xyzrgbData[6 * i + 2] = z;
        xyzrgbData[6 * i + 3] = r;
        xyzrgbData[6 * i + 4] = g;
        xyzrgbData[6 * i + 5] = b;
    }
}

}  // namespace libobsensor
