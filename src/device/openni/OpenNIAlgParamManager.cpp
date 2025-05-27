// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "OpenNIAlgParamManager.hpp"

#include "InternalTypes.hpp"
#include "property/InternalProperty.hpp"
#include "publicfilters/IMUCorrector.hpp"
#include "stream/StreamIntrinsicsManager.hpp"
#include "stream/StreamExtrinsicsManager.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "exception/ObException.hpp"
#include "DevicePids.hpp"
#include <vector>

namespace libobsensor {
OpenNIAlgParamManager::OpenNIAlgParamManager(IDevice *owner) : DisparityAlgParamManagerBase(owner) {
    fetchParamFromDevice();
    registerBasicExtrinsics();
}

void OpenNIAlgParamManager::fetchParamFromDevice() {
    try {
        auto owner      = getOwner();
        auto deviceInfo = owner->getInfo();
        int  pid        = deviceInfo->pid_;
        auto iter       = std::find(OpenNIDualPids.begin(), OpenNIDualPids.end(), pid);
        if(iter != OpenNIDualPids.end()) {
            OBInternalPropertyID groupIndex = OB_RAW_DATA_DUAL_CAMERA_PARAMS_0;
            if(pid == OB_DEVICE_DABAI_DW2_PID || pid == OB_DEVICE_DABAI_DCW2_PID || pid == OB_DEVICE_GEMINI_EW_PID || pid == OB_DEVICE_GEMINI_EW_LITE_PID) {
                groupIndex                   = OB_RAW_DATA_DUAL_CAMERA_PARAMS_0;
                disparityParam_.minDisparity = 48.0f;
            }

            if(pid == OB_DEVICE_MAX_PRO_PID || pid == OB_DEVICE_GEMINI_UW_PID) {
                groupIndex                   = OB_RAW_DATA_DUAL_CAMERA_PARAMS_2;
                disparityParam_.minDisparity = 80.0f;
            }

            if(pid == OB_DEVICE_DABAI_MAX_PID) {
                disparityParam_.minDisparity = 80.0f;
            }

            OBCalibrationParamContent content = {};

            std::vector<uint8_t> data;
            uint32_t             size       = 0;
            auto                 propServer = owner->getPropertyServer();
            propServer->getRawData(
                groupIndex,
                [&](OBDataTranState state, OBDataChunk *dataChunk) {
                    if(state == DATA_TRAN_STAT_TRANSFERRING) {
                        if(size == 0) {
                            size = dataChunk->fullDataSize;
                            data.resize(size);
                        }
                        memcpy(data.data() + dataChunk->offset, dataChunk->data, dataChunk->size);
                    }
                },
                PROP_ACCESS_INTERNAL);
            if(size > 0) {
                memcpy(&content, data.data(), size);
            }
            depthCalibParamList_.push_back(content);
            disparityParam_.baseline     = content.HOST.virCam.bl;
            disparityParam_.zpd          = 0;
            disparityParam_.fx           = content.HOST.virCam.fx;
            disparityParam_.zpps         = 0;
            disparityParam_.bitSize      = 12;
            disparityParam_.dispIntPlace = 8;
            disparityParam_.unit         = 1;
            disparityParam_.dispOffset   = 0;
            disparityParam_.invalidDisp  = 0;
            disparityParam_.packMode     = OB_DISP_PACK_OPENNI;
            disparityParam_.isDualCamera = true;

            // CameraParam
            OBCameraParam param = {};
            memcpy(&param.depthIntrinsic, content.HOST.soft_d2c.d_intr_p, sizeof(content.HOST.soft_d2c.d_intr_p));
            param.depthIntrinsic.width  = 640;
            param.depthIntrinsic.height = 400;
            param.depthDistortion.k1    = content.HOST.soft_d2c.d_k[0];
            param.depthDistortion.k2    = content.HOST.soft_d2c.d_k[1];
            param.depthDistortion.k3    = content.HOST.soft_d2c.d_k[2];
            param.depthDistortion.p1    = content.HOST.soft_d2c.d_k[3];
            param.depthDistortion.p2    = content.HOST.soft_d2c.d_k[4];
            param.depthDistortion.model = OB_DISTORTION_KANNALA_BRANDT4;

            memcpy(&param.rgbIntrinsic, content.HOST.soft_d2c.c_intr_p, sizeof(content.HOST.soft_d2c.c_intr_p));
            param.rgbIntrinsic.width  = 640;
            param.rgbIntrinsic.height = 480;
            param.rgbDistortion.k1    = content.HOST.soft_d2c.c_k[0];
            param.rgbDistortion.k2    = content.HOST.soft_d2c.c_k[1];
            param.rgbDistortion.k3    = content.HOST.soft_d2c.c_k[2];
            param.rgbDistortion.p1    = content.HOST.soft_d2c.c_k[3];
            param.rgbDistortion.p2    = content.HOST.soft_d2c.c_k[4];
            param.rgbDistortion.model = OB_DISTORTION_KANNALA_BRANDT4;
            memcpy(&param.transform.rot, content.HOST.soft_d2c.d2c_r, sizeof(content.HOST.soft_d2c.d2c_r));
            memcpy(&param.transform.trans, content.HOST.soft_d2c.d2c_t, sizeof(content.HOST.soft_d2c.d2c_t));
            param.isMirrored = false;

            switch(pid) {
            case OB_DEVICE_MAX_PRO_PID:
            case OB_DEVICE_GEMINI_UW_PID: {
                // 640x480 640x400
                int16_t colorWidth  = 640;
                int16_t colorHeight = 480;
                int16_t depthWidth  = 640;
                int16_t depthHeight = 400;
                for(int i = 0; i < 2; i++) {
                    OBCameraParam realParam       = {};
                    int16_t       paramScaleValue = (int16_t)pow(2, i);
                    memcpy(&realParam, &param, sizeof(param));
                    realParam.depthIntrinsic.fx     = static_cast<float>(realParam.depthIntrinsic.fx * 1.1204);
                    realParam.depthIntrinsic.fy     = static_cast<float>(realParam.depthIntrinsic.fy * 1.1204);

                    realParam.depthIntrinsic.fx     = realParam.depthIntrinsic.fx / paramScaleValue;
                    realParam.depthIntrinsic.fy     = realParam.depthIntrinsic.fy / paramScaleValue;
                    realParam.depthIntrinsic.cx     = realParam.depthIntrinsic.cx / paramScaleValue;
                    realParam.depthIntrinsic.cy     = realParam.depthIntrinsic.cy / paramScaleValue;
                    realParam.rgbIntrinsic.cy       = realParam.rgbIntrinsic.cy + 40;
                    realParam.depthIntrinsic.width  = depthWidth / paramScaleValue;
                    realParam.depthIntrinsic.height = depthHeight / paramScaleValue;
                    realParam.rgbIntrinsic.width    = colorWidth;
                    realParam.rgbIntrinsic.height   = colorHeight;
                    calibrationCameraParamList_.push_back(realParam);
                }

                // 640x480 640x320
                depthWidth  = 640;
                depthHeight = 320;
                for(int i = 0; i < 2; i++) {
                    OBCameraParam realParam       = {};
                    int16_t       paramScaleValue = (int16_t)pow(2, i);
                    memcpy(&realParam, &param, sizeof(param));
                    realParam.depthIntrinsic.fx = static_cast<float>(realParam.depthIntrinsic.fx * 1.1204);
                    realParam.depthIntrinsic.fy = static_cast<float>(realParam.depthIntrinsic.fy * 1.1204);
                    realParam.depthIntrinsic.cy = realParam.depthIntrinsic.cy - 40;

                    realParam.depthIntrinsic.fx     = realParam.depthIntrinsic.fx / paramScaleValue;
                    realParam.depthIntrinsic.fy     = realParam.depthIntrinsic.fy / paramScaleValue;
                    realParam.depthIntrinsic.cx     = realParam.depthIntrinsic.cx / paramScaleValue;
                    realParam.depthIntrinsic.cy     = realParam.depthIntrinsic.cy / paramScaleValue;

                    memcpy(&realParam.rgbIntrinsic, &realParam.depthIntrinsic, 4 * sizeof(float));

                    realParam.depthIntrinsic.width  = depthWidth / paramScaleValue;
                    realParam.depthIntrinsic.height = depthHeight / paramScaleValue;
                    realParam.rgbIntrinsic.width    = colorWidth;
                    realParam.rgbIntrinsic.height   = colorHeight;
                    calibrationCameraParamList_.push_back(realParam);
                }

                // 1280x960 640x400
                colorWidth  = 1280;
                colorHeight = 960;
                depthWidth  = 640;
                depthHeight = 400;
                for(int i = 0; i < 2; i++) {
                    OBCameraParam realParam       = {};
                    int16_t       paramScaleValue = (int16_t)pow(2, i);
                    memcpy(&realParam, &param, sizeof(param));
                    realParam.depthIntrinsic.fx = static_cast<float>(realParam.depthIntrinsic.fx * 1.1204);
                    realParam.depthIntrinsic.fy = static_cast<float>(realParam.depthIntrinsic.fy * 1.1204);

                    realParam.depthIntrinsic.fx = realParam.depthIntrinsic.fx / paramScaleValue;
                    realParam.depthIntrinsic.fy = realParam.depthIntrinsic.fy / paramScaleValue;
                    realParam.depthIntrinsic.cx = realParam.depthIntrinsic.cx / paramScaleValue;
                    realParam.depthIntrinsic.cy = realParam.depthIntrinsic.cy / paramScaleValue;
                    realParam.rgbIntrinsic.fx   = realParam.rgbIntrinsic.fx * 2;
                    realParam.rgbIntrinsic.fy   = realParam.rgbIntrinsic.fy * 2;
                    realParam.rgbIntrinsic.cx   = realParam.rgbIntrinsic.cx * 2;
                    realParam.rgbIntrinsic.cy   = (realParam.rgbIntrinsic.cy + 40) * 2;

                    realParam.depthIntrinsic.width  = depthWidth / paramScaleValue;
                    realParam.depthIntrinsic.height = depthHeight / paramScaleValue;
                    realParam.rgbIntrinsic.width    = colorWidth;
                    realParam.rgbIntrinsic.height   = colorHeight;
                    calibrationCameraParamList_.push_back(realParam);
                }

                // 1280x960 640x400
                colorWidth  = 1600;
                colorHeight = 1200;
                depthWidth  = 640;
                depthHeight = 400;
                for(int i = 0; i < 2; i++) {
                    OBCameraParam realParam       = {};
                    int16_t       paramScaleValue = (int16_t)pow(2, i);
                    memcpy(&realParam, &param, sizeof(param));
                    realParam.depthIntrinsic.fx = static_cast<float>(realParam.depthIntrinsic.fx * 1.1204);
                    realParam.depthIntrinsic.fy = static_cast<float>(realParam.depthIntrinsic.fy * 1.1204);

                    realParam.depthIntrinsic.fx = realParam.depthIntrinsic.fx / paramScaleValue;
                    realParam.depthIntrinsic.fy = realParam.depthIntrinsic.fy / paramScaleValue;
                    realParam.depthIntrinsic.cx = realParam.depthIntrinsic.cx / paramScaleValue;
                    realParam.depthIntrinsic.cy = realParam.depthIntrinsic.cy / paramScaleValue;
                    realParam.rgbIntrinsic.fx   = static_cast<float>(realParam.rgbIntrinsic.fx * 2.5);
                    realParam.rgbIntrinsic.fy   = static_cast<float>(realParam.rgbIntrinsic.fy * 2.5);
                    realParam.rgbIntrinsic.cx   = static_cast<float>(realParam.rgbIntrinsic.cx * 2.5);
                    realParam.rgbIntrinsic.cy   = static_cast<float>((realParam.rgbIntrinsic.cy + 40) * 2.5);

                    realParam.depthIntrinsic.width  = depthWidth / paramScaleValue;
                    realParam.depthIntrinsic.height = depthHeight / paramScaleValue;
                    realParam.rgbIntrinsic.width    = colorWidth;
                    realParam.rgbIntrinsic.height   = colorHeight;
                    calibrationCameraParamList_.push_back(realParam);
                }

                d2cProfileList_ = {
                    { 640, 480, 640, 400, ALIGN_D2C_HW, 0, { 1.0f, 0, 40, 0, 40 } }, { 640, 480, 640, 400, ALIGN_D2C_SW, 0, { 1.0f, 0, 0, 0, 0 } },
                    { 640, 480, 320, 200, ALIGN_D2C_SW, 1, { 1.0f, 0, 0, 0, 0 } },   { 640, 480, 640, 320, ALIGN_UNSUPPORTED, 2, { 1.0f, 0, 0, 0, 0 } },
                    { 640, 480, 320, 160, ALIGN_UNSUPPORTED, 3, { 1.0f, 0, 0, 0, 0 } }, { 1280, 960, 640, 400, ALIGN_D2C_SW, 4, { 1.0f, 0, 0, 0, 0 } },
                    { 1280, 960, 320, 200, ALIGN_D2C_SW, 5, { 1.0f, 0, 0, 0, 0 } },  { 1600, 1200, 640, 400, ALIGN_D2C_SW, 6, { 1.0f, 0, 0, 0, 0 } },
                    { 1600, 1200, 320, 200, ALIGN_D2C_SW, 7, { 1.0f, 0, 0, 0, 0 } },
                };
            } break;
            case OB_DEVICE_DABAI_MAX_PID: {
                // 640x320 640x320
                int16_t colorWidth  = 640;
                int16_t colorHeight = 320;
                int16_t depthWidth  = 640;
                int16_t depthHeight = 320;
                for(int i = 0; i < 2; i++) {
                    OBCameraParam realParam       = {};
                    int16_t       paramScaleValue = (int16_t)pow(2, i);
                    memcpy(&realParam, &param, sizeof(param));

                    realParam.depthIntrinsic.fx = realParam.rgbIntrinsic.fx / paramScaleValue;
                    realParam.depthIntrinsic.fy = realParam.rgbIntrinsic.fy / paramScaleValue;
                    realParam.depthIntrinsic.cx = realParam.rgbIntrinsic.cx / paramScaleValue;
                    realParam.depthIntrinsic.cy = realParam.rgbIntrinsic.cy / paramScaleValue;
                    realParam.rgbIntrinsic.fx   = realParam.rgbIntrinsic.fx / paramScaleValue;
                    realParam.rgbIntrinsic.fy   = realParam.rgbIntrinsic.fy / paramScaleValue;
                    realParam.rgbIntrinsic.cx   = realParam.rgbIntrinsic.cx / paramScaleValue;
                    realParam.rgbIntrinsic.cy   = realParam.rgbIntrinsic.cy / paramScaleValue;

                    realParam.depthIntrinsic.width  = depthWidth / paramScaleValue;
                    realParam.depthIntrinsic.height = depthHeight / paramScaleValue;
                    realParam.rgbIntrinsic.width    = colorWidth / paramScaleValue;
                    realParam.rgbIntrinsic.height   = colorHeight / paramScaleValue;
                    calibrationCameraParamList_.push_back(realParam);
                }

                // 640x400 640x400
                colorWidth  = 640;
                colorHeight = 400;
                depthWidth  = 640;
                depthHeight = 400;
                for(int i = 0; i < 2; i++) {
                    int16_t       paramScaleValue = (int16_t)pow(2, i);
                    OBCameraParam realParam       = {};
                    memcpy(&realParam, &param, sizeof(param));
                    realParam.rgbIntrinsic.cy   = realParam.rgbIntrinsic.cy + 40;

                    realParam.depthIntrinsic.fx = realParam.rgbIntrinsic.fx / paramScaleValue;
                    realParam.depthIntrinsic.fy = realParam.rgbIntrinsic.fy / paramScaleValue;
                    realParam.depthIntrinsic.cx = realParam.rgbIntrinsic.cx / paramScaleValue;
                    realParam.depthIntrinsic.cy = realParam.rgbIntrinsic.cy / paramScaleValue;
                    realParam.rgbIntrinsic.fx   = realParam.rgbIntrinsic.fx / paramScaleValue;
                    realParam.rgbIntrinsic.fy   = realParam.rgbIntrinsic.fy / paramScaleValue;
                    realParam.rgbIntrinsic.cx   = realParam.rgbIntrinsic.cx / paramScaleValue;
                    realParam.rgbIntrinsic.cy   = realParam.rgbIntrinsic.cy / paramScaleValue;

                    realParam.depthIntrinsic.width  = depthWidth / paramScaleValue;
                    realParam.depthIntrinsic.height = depthHeight / paramScaleValue;
                    realParam.rgbIntrinsic.width    = colorWidth / paramScaleValue;
                    realParam.rgbIntrinsic.height   = colorHeight / paramScaleValue;
                    calibrationCameraParamList_.push_back(realParam);
                }

                d2cProfileList_ = {
                    { 640, 320, 640, 320, ALIGN_UNSUPPORTED, 0, { 1.0f, 0, 0, 0, 0 } },
                    { 640, 320, 320, 160, ALIGN_UNSUPPORTED, 1, { 2.0f, 0, 0, 0, 0 } },
                    { 640, 400, 640, 400, ALIGN_UNSUPPORTED, 2, { 1.0f, 0, 0, 0, 0 } },
                    { 320, 200, 320, 200, ALIGN_UNSUPPORTED, 3, { 2.0f, 0, 0, 0, 0 } },
                };
            } break;
            case OB_DEVICE_DABAI_DC1_PID:{
                int16_t colorWidth  = 640;
                int16_t colorHeight = 480;
                int16_t depthWidth  = 640;
                int16_t depthHeight = 400;
                for(int i = 0; i < 3; i++) {
                    OBCameraParam realParam       = {};
                    int16_t       paramScaleValue = (int16_t)pow(2, i);
                    memcpy(&realParam, &param, sizeof(param));

                    realParam.depthIntrinsic.fx     = realParam.depthIntrinsic.fx / paramScaleValue;
                    realParam.depthIntrinsic.fy     = realParam.depthIntrinsic.fy / paramScaleValue;
                    realParam.depthIntrinsic.cx     = realParam.depthIntrinsic.cx / paramScaleValue;
                    realParam.depthIntrinsic.cy     = realParam.depthIntrinsic.cy / paramScaleValue;
                    realParam.depthIntrinsic.width  = depthWidth / paramScaleValue;
                    realParam.depthIntrinsic.height = depthHeight / paramScaleValue;
                    realParam.rgbIntrinsic.width    = colorWidth;
                    realParam.rgbIntrinsic.height   = colorHeight;
                    calibrationCameraParamList_.push_back(realParam);
                }

                OBCameraParam realParam = {};
                memcpy(&realParam, &param, sizeof(param));
                realParam.depthIntrinsic.fx     = realParam.depthIntrinsic.fx * 2;
                realParam.depthIntrinsic.fy     = realParam.depthIntrinsic.fy * 2;
                realParam.depthIntrinsic.cx     = realParam.depthIntrinsic.cx * 2;
                realParam.depthIntrinsic.cy     = realParam.depthIntrinsic.cy * 2;
                realParam.depthIntrinsic.width  = depthWidth * 2;
                realParam.depthIntrinsic.height = depthHeight * 2;
                realParam.rgbIntrinsic.width    = colorWidth;
                realParam.rgbIntrinsic.height   = colorHeight;
                calibrationCameraParamList_.push_back(realParam);

                OBCameraParam realParam2 = {};
                memcpy(&realParam2, &param, sizeof(param));
                realParam2.depthIntrinsic.fx     = realParam2.depthIntrinsic.fx;
                realParam2.depthIntrinsic.fy     = realParam2.depthIntrinsic.fy;
                realParam2.depthIntrinsic.cx     = realParam2.depthIntrinsic.cx;
                realParam2.depthIntrinsic.cy     = realParam2.depthIntrinsic.cy;
                realParam2.rgbIntrinsic.fx       = static_cast<float>(realParam2.rgbIntrinsic.fx * 2.125);
                realParam2.rgbIntrinsic.fy       = static_cast<float>(realParam2.rgbIntrinsic.fy * 2.125);
                realParam2.rgbIntrinsic.cx       = static_cast<float>(realParam2.rgbIntrinsic.cx * 2.125) - 35;
                realParam2.rgbIntrinsic.cy       = static_cast<float>(realParam2.rgbIntrinsic.cy * 2.125) - 150;
                realParam2.depthIntrinsic.width  = depthWidth;
                realParam2.depthIntrinsic.height = depthHeight;
                realParam2.rgbIntrinsic.width    = 1280;
                realParam2.rgbIntrinsic.height   = 720;
                calibrationCameraParamList_.push_back(realParam2);

                d2cProfileList_ = {
                    { 640, 480, 640, 400, ALIGN_D2C_HW, 0, { 1.0f, 0, 0, 0, 80 } }, { 640, 480, 640, 400, ALIGN_D2C_SW, 0, { 1.0f, 0, 0, 0, 0 } },
                    { 640, 480, 320, 200, ALIGN_D2C_HW, 1, { 2.0f, 0, 0, 0, 80 } }, { 640, 480, 320, 200, ALIGN_D2C_SW, 1, { 1.0f, 0, 0, 0, 0 } },
                    { 640, 480, 160, 100, ALIGN_D2C_HW, 2, { 4.0f, 0, 0, 0, 80 } }, { 640, 480, 160, 100, ALIGN_D2C_SW, 2, { 1.0f, 0, 0, 0, 0 } },
                    { 1280, 960, 640, 480, ALIGN_D2C_SW, 3, { 1.0f, 0, 0, 0, 0 } }, { 1280, 720, 640, 400, ALIGN_D2C_SW, 4, { 1.0f, 0, 0, 0, 0 } },
                };
            
            } break;
            case OB_DEVICE_DABAI_DW2_PID:
            case OB_DEVICE_DABAI_DCW2_PID:
            case OB_DEVICE_GEMINI_EW_PID:
            case OB_DEVICE_GEMINI_EW_LITE_PID: {
                //16:9
                int16_t colorWidth  = 640;
                int16_t colorHeight = 360;
                int16_t depthWidth  = 640;
                int16_t depthHeight = 400;
                for(int i = 0; i < 3; i++) {
                    OBCameraParam realParam       = {};
                    int16_t       paramScaleValue = (int16_t)pow(2, i);
                    memcpy(&realParam, &param, sizeof(param));

                    realParam.depthIntrinsic.fx     = realParam.depthIntrinsic.fx / paramScaleValue;
                    realParam.depthIntrinsic.fy     = realParam.depthIntrinsic.fy / paramScaleValue;
                    realParam.depthIntrinsic.cx     = realParam.depthIntrinsic.cx / paramScaleValue;
                    realParam.depthIntrinsic.cy     = realParam.depthIntrinsic.cy / paramScaleValue;
                    realParam.rgbIntrinsic.fx       = realParam.rgbIntrinsic.fx;
                    realParam.rgbIntrinsic.fy       = realParam.rgbIntrinsic.fy;
                    realParam.rgbIntrinsic.cx       = realParam.rgbIntrinsic.cx;
                    realParam.rgbIntrinsic.cy       = realParam.rgbIntrinsic.cy;
                    realParam.depthIntrinsic.width  = depthWidth / paramScaleValue;
                    realParam.depthIntrinsic.height = depthHeight / paramScaleValue;
                    realParam.rgbIntrinsic.width    = colorWidth;
                    realParam.rgbIntrinsic.height   = colorHeight;
                    calibrationCameraParamList_.push_back(realParam);
                }

                colorWidth  = 640;
                colorHeight = 480;
                depthWidth  = 640;
                depthHeight = 400;
                for(int i = 0; i < 2; i++) {
                    OBCameraParam realParam       = {};
                    int16_t       paramScaleValue = (int16_t)pow(2, i);
                    memcpy(&realParam, &param, sizeof(param));

                    realParam.depthIntrinsic.fx = realParam.depthIntrinsic.fx / paramScaleValue;
                    realParam.depthIntrinsic.fy = realParam.depthIntrinsic.fy / paramScaleValue;
                    realParam.depthIntrinsic.cx = realParam.depthIntrinsic.cx / paramScaleValue;
                    realParam.depthIntrinsic.cy = realParam.depthIntrinsic.cy / paramScaleValue;
                    realParam.rgbIntrinsic.fx   = static_cast<float>(realParam.rgbIntrinsic.fx * 3 / 2.25);
                    realParam.rgbIntrinsic.fy   = static_cast<float>(realParam.rgbIntrinsic.fy * 3 / 2.25);
                    realParam.rgbIntrinsic.cx   = static_cast<float>((realParam.rgbIntrinsic.cx * 3 - 240) / 2.25);
                    realParam.rgbIntrinsic.cy   = static_cast<float>(realParam.rgbIntrinsic.cy * 3 / 2.25);

                    realParam.depthIntrinsic.width  = depthWidth / paramScaleValue;
                    realParam.depthIntrinsic.height = depthHeight / paramScaleValue;
                    realParam.rgbIntrinsic.width    = colorWidth;
                    realParam.rgbIntrinsic.height   = colorHeight;
                    calibrationCameraParamList_.push_back(realParam);
                }

                colorWidth  = 320;
                colorHeight = 240;
                depthWidth  = 640;
                depthHeight = 400;
                for(int i = 0; i < 2; i++) {
                    OBCameraParam realParam       = {};
                    int16_t       paramScaleValue = (int16_t)pow(2, i);
                    memcpy(&realParam, &param, sizeof(param));

                    realParam.depthIntrinsic.fx = realParam.depthIntrinsic.fx / paramScaleValue;
                    realParam.depthIntrinsic.fy = realParam.depthIntrinsic.fy / paramScaleValue;
                    realParam.depthIntrinsic.cx = realParam.depthIntrinsic.cx / paramScaleValue;
                    realParam.depthIntrinsic.cy = realParam.depthIntrinsic.cy / paramScaleValue;
                    realParam.rgbIntrinsic.fx   = static_cast<float>(realParam.rgbIntrinsic.fx * 3 / 2.25) / 2;
                    realParam.rgbIntrinsic.fy   = static_cast<float>(realParam.rgbIntrinsic.fy * 3 / 2.25) / 2;
                    realParam.rgbIntrinsic.cx   = static_cast<float>((realParam.rgbIntrinsic.cx * 3 - 240) / 2.25) / 2;
                    realParam.rgbIntrinsic.cy   = static_cast<float>(realParam.rgbIntrinsic.cy * 3 / 2.25) / 2;

                    realParam.depthIntrinsic.width  = depthWidth / paramScaleValue;
                    realParam.depthIntrinsic.height = depthHeight / paramScaleValue;
                    realParam.rgbIntrinsic.width    = colorWidth;
                    realParam.rgbIntrinsic.height   = colorHeight;
                    calibrationCameraParamList_.push_back(realParam);
                }


                //16:9
                colorWidth  = 640;
                colorHeight = 360;
                depthWidth  = 540;
                depthHeight = 400;
                for(int i = 0; i < 2; i++) {
                    OBCameraParam realParam       = {};
                    int16_t       paramScaleValue = (int16_t)pow(2, i);
                    memcpy(&realParam, &param, sizeof(param));

                    realParam.depthIntrinsic.fx = realParam.depthIntrinsic.fx / paramScaleValue;
                    realParam.depthIntrinsic.fy = realParam.depthIntrinsic.fy / paramScaleValue;
                    realParam.depthIntrinsic.cx = realParam.depthIntrinsic.cx / paramScaleValue;
                    realParam.depthIntrinsic.cx = realParam.depthIntrinsic.cx - (50 / paramScaleValue);
                    realParam.depthIntrinsic.cy = realParam.depthIntrinsic.cy / paramScaleValue;
                    realParam.rgbIntrinsic.fx   = realParam.rgbIntrinsic.fx / paramScaleValue;
                    realParam.rgbIntrinsic.fy   = realParam.rgbIntrinsic.fy / paramScaleValue;
                    realParam.rgbIntrinsic.cx   = realParam.rgbIntrinsic.cx / paramScaleValue;
                    realParam.rgbIntrinsic.cy   = realParam.rgbIntrinsic.cy / paramScaleValue;

                    realParam.depthIntrinsic.width  = depthWidth / paramScaleValue;
                    realParam.depthIntrinsic.height = depthHeight / paramScaleValue;
                    realParam.rgbIntrinsic.width    = colorWidth / paramScaleValue;
                    realParam.rgbIntrinsic.height   = colorHeight / paramScaleValue;
                    calibrationCameraParamList_.push_back(realParam);
                }

                // 4:3
                colorWidth  = 640;
                colorHeight = 480;
                depthWidth  = 540;
                depthHeight = 400;
                for(int i = 0; i < 2; i++) {
                    OBCameraParam realParam       = {};
                    int16_t       paramScaleValue = (int16_t)pow(2, i);
                    memcpy(&realParam, &param, sizeof(param));

                    realParam.depthIntrinsic.fx = realParam.depthIntrinsic.fx / paramScaleValue;
                    realParam.depthIntrinsic.fy = realParam.depthIntrinsic.fy / paramScaleValue;
                    realParam.depthIntrinsic.cx = realParam.depthIntrinsic.cx / paramScaleValue;
                    realParam.depthIntrinsic.cx = realParam.depthIntrinsic.cx - (50 / paramScaleValue);
                    realParam.depthIntrinsic.cy = realParam.depthIntrinsic.cy / paramScaleValue;
                    realParam.rgbIntrinsic.fx   = static_cast<float>(realParam.rgbIntrinsic.fx * 3 / 2.25);
                    realParam.rgbIntrinsic.fy   = static_cast<float>(realParam.rgbIntrinsic.fy * 3 / 2.25);
                    realParam.rgbIntrinsic.cx   = static_cast<float>((realParam.rgbIntrinsic.cx * 3 - 240) / 2.25);
                    realParam.rgbIntrinsic.cy   = static_cast<float>(realParam.rgbIntrinsic.cy * 3 / 2.25);

                    realParam.depthIntrinsic.width  = depthWidth / paramScaleValue;
                    realParam.depthIntrinsic.height = depthHeight / paramScaleValue;
                    realParam.rgbIntrinsic.width    = colorWidth;
                    realParam.rgbIntrinsic.height   = colorHeight;
                    calibrationCameraParamList_.push_back(realParam);
                }

                colorWidth  = 1280;
                colorHeight = 720;
                depthWidth  = 640;
                depthHeight = 400;
                for(int i = 0; i < 2; i++) {
                    OBCameraParam realParam       = {};
                    int16_t       paramScaleValue = (int16_t)pow(2, i);
                    memcpy(&realParam, &param, sizeof(param));

                    realParam.depthIntrinsic.fx = realParam.depthIntrinsic.fx / paramScaleValue;
                    realParam.depthIntrinsic.fy = realParam.depthIntrinsic.fy / paramScaleValue;
                    realParam.depthIntrinsic.cx = realParam.depthIntrinsic.cx / paramScaleValue;
                    realParam.depthIntrinsic.cy = realParam.depthIntrinsic.cy / paramScaleValue;
                    realParam.rgbIntrinsic.fx   = realParam.rgbIntrinsic.fx * 2;
                    realParam.rgbIntrinsic.fy   = realParam.rgbIntrinsic.fy * 2;
                    realParam.rgbIntrinsic.cx   = realParam.rgbIntrinsic.cx * 2;
                    realParam.rgbIntrinsic.cy   = realParam.rgbIntrinsic.cy * 2;

                    realParam.depthIntrinsic.width  = depthWidth / paramScaleValue;
                    realParam.depthIntrinsic.height = depthHeight / paramScaleValue;
                    realParam.rgbIntrinsic.width    = colorWidth;
                    realParam.rgbIntrinsic.height   = colorHeight;
                    calibrationCameraParamList_.push_back(realParam);
                }

                colorWidth  = 1280;
                colorHeight = 960;
                depthWidth  = 640;
                depthHeight = 400;
                for(int i = 0; i < 2; i++) {
                    OBCameraParam realParam       = {};
                    int16_t       paramScaleValue = (int16_t)pow(2, i);
                    memcpy(&realParam, &param, sizeof(param));

                    realParam.depthIntrinsic.fx = realParam.depthIntrinsic.fx / paramScaleValue;
                    realParam.depthIntrinsic.fy = realParam.depthIntrinsic.fy / paramScaleValue;
                    realParam.depthIntrinsic.cx = realParam.depthIntrinsic.cx / paramScaleValue;
                    realParam.depthIntrinsic.cy = realParam.depthIntrinsic.cy / paramScaleValue;
                    realParam.rgbIntrinsic.fx   = static_cast<float>(realParam.rgbIntrinsic.fx * 3 / 2.25) * 2;
                    realParam.rgbIntrinsic.fy   = static_cast<float>(realParam.rgbIntrinsic.fy * 3 / 2.25) * 2;
                    realParam.rgbIntrinsic.cx   = static_cast<float>((realParam.rgbIntrinsic.cx * 3 - 240) / 2.25) * 2;
                    realParam.rgbIntrinsic.cy   = static_cast<float>(realParam.rgbIntrinsic.cy * 3 / 2.25) * 2;

                    realParam.depthIntrinsic.width  = depthWidth / paramScaleValue;
                    realParam.depthIntrinsic.height = depthHeight / paramScaleValue;
                    realParam.rgbIntrinsic.width    = colorWidth;
                    realParam.rgbIntrinsic.height   = colorHeight;
                    calibrationCameraParamList_.push_back(realParam);
                }

                colorWidth  = 1920;
                colorHeight = 1080;
                depthWidth  = 640;
                depthHeight = 400;
                for(int i = 0; i < 2; i++) {
                    OBCameraParam realParam       = {};
                    int16_t       paramScaleValue = (int16_t)pow(2, i);
                    memcpy(&realParam, &param, sizeof(param));

                    realParam.depthIntrinsic.fx     = realParam.depthIntrinsic.fx / paramScaleValue;
                    realParam.depthIntrinsic.fy     = realParam.depthIntrinsic.fy / paramScaleValue;
                    realParam.depthIntrinsic.cx     = realParam.depthIntrinsic.cx / paramScaleValue;
                    realParam.depthIntrinsic.cy     = realParam.depthIntrinsic.cy / paramScaleValue;
                    realParam.rgbIntrinsic.fx       = realParam.rgbIntrinsic.fx * 3;
                    realParam.rgbIntrinsic.fy       = realParam.rgbIntrinsic.fy * 3;
                    realParam.rgbIntrinsic.cx       = realParam.rgbIntrinsic.cx * 3;
                    realParam.rgbIntrinsic.cy       = realParam.rgbIntrinsic.cy * 3;
                    realParam.depthIntrinsic.width  = depthWidth / paramScaleValue;
                    realParam.depthIntrinsic.height = depthHeight / paramScaleValue;
                    realParam.rgbIntrinsic.width    = colorWidth;
                    realParam.rgbIntrinsic.height   = colorHeight;
                    calibrationCameraParamList_.push_back(realParam);
                }

                if(pid == OB_DEVICE_DABAI_DW2_PID || pid == OB_DEVICE_GEMINI_EW_LITE_PID) {
                    d2cProfileList_ = {
                        { 640, 360, 640, 400, ALIGN_D2C_HW, 0, { 1.0f, 0, 0, 0, -40 } },   { 640, 360, 320, 200, ALIGN_D2C_HW, 1, { 2.0f, 0, 0, 0, -40 } },
                        { 640, 360, 160, 100, ALIGN_D2C_HW, 2, { 4.0f, 0, -20, 0, -40 } }, { 640, 360, 540, 400, ALIGN_D2C_HW, 7, { 1.0f, 50, 0, 50, -40 } },
                        { 640, 360, 270, 200, ALIGN_D2C_HW, 8, { 1.0f, 50, 0, 50, -40 } },

                    };
                }

                if(pid == OB_DEVICE_DABAI_DCW2_PID || pid == OB_DEVICE_GEMINI_EW_PID) {
                    d2cProfileList_ = {
                        { 640, 360, 640, 400, ALIGN_D2C_HW, 0, { 1.0f, 0, 0, 0, -40 } },
                        { 640, 360, 640, 400, ALIGN_D2C_SW, 0, { 1.0f, 0, 0, 0, 0 } },
                        { 640, 480, 640, 400, ALIGN_D2C_HW, 3, { 1.333f, -107, 0, -106, -53 } },
                        { 640, 480, 640, 400, ALIGN_D2C_SW, 3, { 1.0f, 0, 0, 0, 0 } },
                        { 320, 240, 320, 200, ALIGN_D2C_HW, 6, { 1.333f, -53, 0, -53, -26 } },
                        { 320, 240, 320, 200, ALIGN_D2C_SW, 6, { 1.0f, 0, 0, 0, 0 } },
                        { 640, 360, 320, 200, ALIGN_D2C_SW, 1, { 1.0f, 0, 0, 0, 0 } },
                        { 640, 480, 320, 200, ALIGN_D2C_SW, 4, { 1.0f, 0, 0, 0, 0 } },
                        { 320, 240, 640, 400, ALIGN_D2C_SW, 5, { 1.0f, 0, 0, 0, 0 } },
                        { 1280, 720, 640, 400, ALIGN_D2C_SW, 11, { 1.0f, 0, 0, 0, 0 } },
                        { 1280, 720, 320, 200, ALIGN_D2C_SW, 12, { 1.0f, 0, 0, 0, 0 } },
                        { 640, 360, 540, 400, ALIGN_UNSUPPORTED, 7, { 1.0f, 0, 0, 0, -40 } },
                        { 320, 180, 270, 200, ALIGN_UNSUPPORTED, 8, { 1.0f, 0, 0, 0, -40 } },
                        { 1280, 720, 320, 200, ALIGN_UNSUPPORTED, 13, { 1.0f, 0, 0, 0, 0 } },
                    };
                }
            } break;
            default:
                break;
            }

        }
        else {
            auto propServer              = owner->getPropertyServer();
            auto extParam                = propServer->getStructureDataT<OBExtensionParam>(OB_STRUCT_EXTENSION_PARAM);
            disparityParam_.baseline     = extParam.fDCmosEmitterDistance;
            disparityParam_.zpd          = extParam.fReferenceDistance;
            disparityParam_.fx           = extParam.fReferenceDistance / extParam.fReferencePixelSize;
            disparityParam_.zpps         = extParam.fReferencePixelSize;
            disparityParam_.bitSize      = 12;
            disparityParam_.dispIntPlace = 8;
            disparityParam_.unit         = 10;
            disparityParam_.dispOffset   = 0;
            disparityParam_.invalidDisp  = 0;
            disparityParam_.packMode     = OB_DISP_PACK_OPENNI;
            disparityParam_.isDualCamera = false;
            disparityParam_.minDisparity = 0.0f;

            OBCameraParam param    = {};
            auto          camParam = propServer->getStructureDataT<OBInternalCameraParam>(OB_STRUCT_INTERNAL_CAMERA_PARAM);
            memcpy(&param.depthIntrinsic, camParam.d_intr_p, sizeof(camParam.d_intr_p));
            param.depthIntrinsic.width  = 640;
            param.depthIntrinsic.height = 480;
            param.depthDistortion.k1    = camParam.d_k[0];
            param.depthDistortion.k2    = camParam.d_k[1];
            param.depthDistortion.k3    = camParam.d_k[2];
            param.depthDistortion.p1    = camParam.d_k[3];
            param.depthDistortion.p2    = camParam.d_k[4];
            param.depthDistortion.model = OB_DISTORTION_KANNALA_BRANDT4;

            memcpy(&param.rgbIntrinsic, camParam.c_intr_p, sizeof(camParam.c_intr_p));
            param.rgbIntrinsic.width  = 640;
            param.rgbIntrinsic.height = 480;
            param.rgbDistortion.k1    = camParam.c_k[0];
            param.rgbDistortion.k2    = camParam.c_k[1];
            param.rgbDistortion.k3    = camParam.c_k[2];
            param.rgbDistortion.p1    = camParam.c_k[3];
            param.rgbDistortion.p2    = camParam.c_k[4];
            param.rgbDistortion.model = OB_DISTORTION_KANNALA_BRANDT4;
            memcpy(&param.transform.rot, camParam.d2c_r, sizeof(camParam.d2c_r));
            memcpy(&param.transform.trans, camParam.d2c_t, sizeof(camParam.d2c_t));
            param.isMirrored = false;

            for(int i = 0; i < 3; i++) {
                OBCameraParam realParam       = {};
                int16_t       paramScaleValue = (int16_t)pow(2, i);
                memcpy(&realParam, &param, sizeof(param));

                realParam.depthIntrinsic.fx     = realParam.depthIntrinsic.fx / paramScaleValue;
                realParam.depthIntrinsic.fy     = realParam.depthIntrinsic.fy / paramScaleValue;
                realParam.depthIntrinsic.cx     = realParam.depthIntrinsic.cx / paramScaleValue;
                realParam.depthIntrinsic.cy     = realParam.depthIntrinsic.cy / paramScaleValue;
                realParam.depthIntrinsic.width  = realParam.depthIntrinsic.width / paramScaleValue;
                realParam.depthIntrinsic.height = realParam.depthIntrinsic.height / paramScaleValue;

                realParam.rgbIntrinsic.fx       = realParam.rgbIntrinsic.fx;
                realParam.rgbIntrinsic.fy       = realParam.rgbIntrinsic.fy;
                realParam.rgbIntrinsic.cx       = realParam.rgbIntrinsic.cx;
                realParam.rgbIntrinsic.cy       = realParam.rgbIntrinsic.cy;
                realParam.rgbIntrinsic.width    = realParam.rgbIntrinsic.width;
                realParam.rgbIntrinsic.height   = realParam.rgbIntrinsic.height;
                calibrationCameraParamList_.push_back(realParam);
            }

            OBCameraParam realParam2       = {};
            memcpy(&realParam2, &param, sizeof(param));
            realParam2.depthIntrinsic.fx     = realParam2.depthIntrinsic.fx * 2;
            realParam2.depthIntrinsic.fy     = realParam2.depthIntrinsic.fy * 2;
            realParam2.depthIntrinsic.cx     = realParam2.depthIntrinsic.cx * 2;
            realParam2.depthIntrinsic.cy     = realParam2.depthIntrinsic.cy * 2;
            realParam2.depthIntrinsic.width  = realParam2.depthIntrinsic.width * 2;
            realParam2.depthIntrinsic.height = realParam2.depthIntrinsic.height * 2;

            realParam2.rgbIntrinsic.fx     = realParam2.rgbIntrinsic.fx;
            realParam2.rgbIntrinsic.fy     = realParam2.rgbIntrinsic.fy;
            realParam2.rgbIntrinsic.cx     = realParam2.rgbIntrinsic.cx;
            realParam2.rgbIntrinsic.cy     = realParam2.rgbIntrinsic.cy;
            realParam2.rgbIntrinsic.width  = realParam2.rgbIntrinsic.width;
            realParam2.rgbIntrinsic.height = realParam2.rgbIntrinsic.height;
            calibrationCameraParamList_.push_back(realParam2);

            OBCameraParam realParam = {};
            memcpy(&realParam, &param, sizeof(param));
            realParam.depthIntrinsic.fx     = realParam.depthIntrinsic.fx * 2;
            realParam.depthIntrinsic.fy     = realParam.depthIntrinsic.fy * 2;
            realParam.depthIntrinsic.cx     = realParam.depthIntrinsic.cx * 2;
            realParam.depthIntrinsic.cy     = realParam.depthIntrinsic.cy * 2 + 32;
            realParam.depthIntrinsic.width  = 1280;
            realParam.depthIntrinsic.height = 1024;

            realParam.rgbIntrinsic.fx     = realParam.rgbIntrinsic.fx;
            realParam.rgbIntrinsic.fy     = realParam.rgbIntrinsic.fy;
            realParam.rgbIntrinsic.cx     = realParam.rgbIntrinsic.cx;
            realParam.rgbIntrinsic.cy     = realParam.rgbIntrinsic.cy;
            realParam.rgbIntrinsic.width  = realParam.rgbIntrinsic.width;
            realParam.rgbIntrinsic.height = realParam.rgbIntrinsic.height;
            calibrationCameraParamList_.push_back(realParam);

            d2cProfileList_ = {
                { 640, 480, 640, 480, ALIGN_D2C_HW, 0, { 1.0f, 0, 0, 0, 0 } },  { 640, 480, 640, 480, ALIGN_D2C_SW, 0, { 1.0f, 0, 0, 0, 0 } },
                { 640, 480, 320, 240, ALIGN_D2C_HW, 1, { 2.0f, 0, 0, 0, 0 } },  { 640, 480, 320, 240, ALIGN_D2C_SW, 1, { 2.0f, 0, 0, 0, 0 } },
                { 640, 480, 160, 120, ALIGN_D2C_HW, 2, { 4.0f, 0, 0, 0, 0 } },  { 640, 480, 160, 120, ALIGN_D2C_SW, 2, { 4.0f, 0, 0, 0, 0 } },
                { 640, 480, 1280, 960, ALIGN_D2C_SW, 3, { 1.0f, 0, 0, 0, 0 } }, { 640, 480, 1280, 1024, ALIGN_UNSUPPORTED, 4, { 1.0f, 0, 0, 0, 0 } },
            };
        }
    }
    catch(const std::exception &e) {
        LOG_ERROR("Get depth calibration params failed! {}", e.what());
    }
}

void OpenNIAlgParamManager::registerBasicExtrinsics() {
    auto owner      = getOwner();
    auto deviceInfo = owner->getInfo();
    int  pid        = deviceInfo->pid_;
    auto iter       = std::find(OpenniAstraPids.begin(), OpenniAstraPids.end(), pid);
    if(iter == OpenniAstraPids.end()) {
        auto extrinsicMgr            = StreamExtrinsicsManager::getInstance();
        auto depthBasicStreamProfile = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_DEPTH, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
        auto colorBasicStreamProfile = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_COLOR, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
        auto irBasicStreamProfile    = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_IR, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);

        if(!calibrationCameraParamList_.empty()) {
            auto d2cExtrinsic = calibrationCameraParamList_.front().transform;
            extrinsicMgr->registerExtrinsics(depthBasicStreamProfile, colorBasicStreamProfile, d2cExtrinsic);
        }
        extrinsicMgr->registerSameExtrinsics(irBasicStreamProfile, depthBasicStreamProfile);
        basicStreamProfileList_.emplace_back(depthBasicStreamProfile);
        basicStreamProfileList_.emplace_back(colorBasicStreamProfile);
        basicStreamProfileList_.emplace_back(irBasicStreamProfile);
    }
    else {
        auto monIter = std::find(OpenniMonocularPids.begin(), OpenniMonocularPids.end(), pid);
        if(monIter != OpenniMonocularPids.end()) {
            auto extrinsicMgr = StreamExtrinsicsManager::getInstance();
            auto depthBasicStreamProfile =
                StreamProfileFactory::createVideoStreamProfile(OB_STREAM_DEPTH, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
            auto colorBasicStreamProfile =
                StreamProfileFactory::createVideoStreamProfile(OB_STREAM_COLOR, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
            auto irBasicStreamProfile = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_IR, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);

            if(!calibrationCameraParamList_.empty()) {
                auto d2cExtrinsic = calibrationCameraParamList_.front().transform;
                extrinsicMgr->registerExtrinsics(depthBasicStreamProfile, colorBasicStreamProfile, d2cExtrinsic);
            }
            extrinsicMgr->registerSameExtrinsics(irBasicStreamProfile, depthBasicStreamProfile);
            basicStreamProfileList_.emplace_back(depthBasicStreamProfile);
            basicStreamProfileList_.emplace_back(colorBasicStreamProfile);
            basicStreamProfileList_.emplace_back(irBasicStreamProfile);
        }
        else {
            auto extrinsicMgr = StreamExtrinsicsManager::getInstance();
            auto depthBasicStreamProfile =
                StreamProfileFactory::createVideoStreamProfile(OB_STREAM_DEPTH, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
            auto irBasicStreamProfile = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_IR, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
            extrinsicMgr->registerSameExtrinsics(irBasicStreamProfile, depthBasicStreamProfile);
            basicStreamProfileList_.emplace_back(depthBasicStreamProfile);
            basicStreamProfileList_.emplace_back(irBasicStreamProfile);
        }
    }
}

void OpenNIAlgParamManager::bindDisparityParam(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) {
    auto dispParam    = getDisparityParam();
    auto intrinsicMgr = StreamIntrinsicsManager::getInstance();
    for(const auto &sp: streamProfileList) {
        if(!sp->is<DisparityBasedStreamProfile>()) {
            continue;
        }

        OBDisparityParam disparityParam = {0};
        memcpy(&disparityParam, &dispParam, sizeof(OBDisparityParam));
        auto vsp = sp->as<VideoStreamProfile>();
        if(vsp->getType() == OB_STREAM_DEPTH) {
            if(vsp->getFormat() == OB_FORMAT_Y11) {
                disparityParam.bitSize = 11;
            }
        }
        intrinsicMgr->registerDisparityBasedStreamDisparityParam(sp, disparityParam);
    }
}

}  // namespace libobsensor
