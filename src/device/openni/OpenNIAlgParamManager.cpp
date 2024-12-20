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
            if(pid == OB_DEVICE_DW_PID || pid == OB_DEVICE_DCW_PID || pid == OB_DEVICE_GEMINI_E_PID || pid == OB_DEVICE_GEMINI_E_LITE_PID) {
                groupIndex = OB_RAW_DATA_DUAL_CAMERA_PARAMS_2;
            }

            if(pid == OB_DEVICE_MAX_PRO_PID || pid == OB_DEVICE_GEMINI_UW_PID) {
                groupIndex = OB_RAW_DATA_DUAL_CAMERA_PARAMS_1;
            }

            OBCalibrationParamContent content = {};

            std::vector<uint8_t> data;
            uint32_t             size       = 0;
            auto                 propServer = owner->getPropertyServer();
            propServer->getRawData(
                OB_RAW_DATA_DUAL_CAMERA_PARAMS_0,
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

            switch(pid) {
            case OB_DEVICE_MAX_PRO_PID:
            case OB_DEVICE_GEMINI_UW_PID: {
                content.HOST.soft_d2c;
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
                param.rgbIntrinsic.width    = 640;
                param.rgbIntrinsic.height   = 480;
                param.rgbDistortion.k1      = content.HOST.soft_d2c.c_k[0];
                param.rgbDistortion.k2      = content.HOST.soft_d2c.c_k[1];
                param.rgbDistortion.k3      = content.HOST.soft_d2c.c_k[2];
                param.rgbDistortion.p1      = content.HOST.soft_d2c.c_k[3];
                param.rgbDistortion.p2      = content.HOST.soft_d2c.c_k[4];
                param.rgbDistortion.model   = OB_DISTORTION_KANNALA_BRANDT4;
                memcpy(&param.transform.rot, content.HOST.soft_d2c.d2c_r, sizeof(content.HOST.soft_d2c.d2c_r));
                memcpy(&param.transform.trans, content.HOST.soft_d2c.d2c_t, sizeof(content.HOST.soft_d2c.d2c_t));
                param.isMirrored            = false;
                
                // 640x480 640x400
                int16_t colorWidth  = 640;
                int16_t colorHeight = 480;
                int16_t depthWidth  = 640;
                int16_t depthHeight = 400;
                for(int i = 0; i < 2; i++) {
                    OBCameraParam realParam       = {};
                    int16_t       paramScaleValue = (int16_t)pow(2, i);
                    memcpy(&realParam, &param, sizeof(param));
                    realParam.depthIntrinsic.fx      = realParam.depthIntrinsic.fx / paramScaleValue;
                    realParam.depthIntrinsic.fy      = realParam.depthIntrinsic.fy / paramScaleValue;
                    realParam.depthIntrinsic.cx      = realParam.depthIntrinsic.cx / paramScaleValue;
                    realParam.depthIntrinsic.cy      = realParam.depthIntrinsic.cy / paramScaleValue;
                    realParam.rgbIntrinsic.cy        = realParam.rgbIntrinsic.cy + 40;
                    realParam.depthIntrinsic.width   = depthWidth / paramScaleValue;
                    realParam.depthIntrinsic.height  = depthHeight / paramScaleValue;
                    realParam.rgbIntrinsic.width     = colorWidth;
                    realParam.rgbIntrinsic.height    = colorHeight;
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
                    realParam.rgbIntrinsic.cy       = realParam.rgbIntrinsic.cy + 40;
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
                    { 640, 480, 320, 200, ALIGN_D2C_SW, 1, { 1.0f, 0, 0, 0, 0 } },   { 1280, 960, 640, 400, ALIGN_D2C_SW, 4, { 1.0f, 0, 0, 0, 0 } },
                    { 1280, 960, 320, 200, ALIGN_D2C_SW, 5, { 1.0f, 0, 0, 0, 0 } },  { 1600, 1200, 640, 400, ALIGN_D2C_SW, 6, { 1.0f, 0, 0, 0, 0 } },
                    { 1600, 1200, 320, 200, ALIGN_D2C_SW, 7, { 1.0f, 0, 0, 0, 0 } },
                };
            } break;
            default:
                break;
            }

        }
        else {
            
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
}

}  // namespace libobsensor
