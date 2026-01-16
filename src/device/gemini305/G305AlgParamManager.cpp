// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G305AlgParamManager.hpp"
#include "stream/StreamIntrinsicsManager.hpp"
#include "stream/StreamExtrinsicsManager.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "property/InternalProperty.hpp"
#include "DevicePids.hpp"
#include "exception/ObException.hpp"

#include <vector>
#include <sstream>
namespace libobsensor {

bool G305AlgParamManager::findBestMatchedCameraParam(const std::vector<OBCameraParam>                &cameraParamList,
                                                     const std::shared_ptr<const VideoStreamProfile> &profile, OBCameraParam &result) {
    bool found            = false;
    auto decimationConfig = profile->getDecimationConfig();
    auto width            = decimationConfig.originWidth == 0 ? profile->getWidth() : decimationConfig.originWidth;
    auto height           = decimationConfig.originHeight == 0 ? profile->getHeight() : decimationConfig.originHeight;

    // match same resolution
    for(auto &param: cameraParamList) {
        auto streamType = profile->getType();
        if((streamType == OB_STREAM_DEPTH || streamType == OB_STREAM_IR || streamType == OB_STREAM_IR_LEFT || streamType == OB_STREAM_IR_RIGHT)
           && static_cast<uint32_t>(param.depthIntrinsic.width) == width && static_cast<uint32_t>(param.depthIntrinsic.height) == height) {
            found  = true;
            result = param;
            break;
        }
        else if((streamType == OB_STREAM_COLOR || streamType == OB_STREAM_COLOR_RIGHT || streamType == OB_STREAM_COLOR_LEFT)
                && static_cast<uint32_t>(param.rgbIntrinsic.width) == width && static_cast<uint32_t>(param.rgbIntrinsic.height) == height) {
            found  = true;
            result = param;
            break;
        }
    }

    if(!found) {
        // match same ratio
        float ratio = (float)profile->getWidth() / profile->getHeight();
        for(auto &param: cameraParamList) {
            auto streamType = profile->getType();
            if((streamType == OB_STREAM_DEPTH || streamType == OB_STREAM_IR || streamType == OB_STREAM_IR_LEFT || streamType == OB_STREAM_IR_RIGHT)
               && (float)param.depthIntrinsic.width / param.depthIntrinsic.height == ratio) {
                found  = true;
                result = param;
                break;
            }
            else if((streamType == OB_STREAM_COLOR || streamType == OB_STREAM_COLOR_RIGHT || streamType == OB_STREAM_COLOR_LEFT)
                    && (float)param.rgbIntrinsic.width / param.rgbIntrinsic.height == ratio) {
                found  = true;
                result = param;
                break;
            }
        }
    }

    return found;
}

G305AlgParamManager::G305AlgParamManager(IDevice *owner) : DisparityAlgParamManagerBase(owner) {
    fetchParamFromDevice();
    fetchPresetResolutionConfig();
    fixD2CParmaList();
    registerBasicExtrinsics();
}

void G305AlgParamManager::fetchParamFromDevice() {

    try {
        auto owner           = getOwner();
        auto propServer      = owner->getPropertyServer();
        depthCalibParamList_ = propServer->getStructureDataListProtoV1_1_T<OBDepthCalibrationParam, 1>(OB_RAW_DATA_DEPTH_CALIB_PARAM);

        const auto &depthCalib       = depthCalibParamList_.front();
        disparityParam_.baseline     = depthCalib.baseline;
        disparityParam_.zpd          = depthCalib.z0;
        disparityParam_.fx           = depthCalib.focalPix;
        disparityParam_.zpps         = depthCalib.z0 / depthCalib.focalPix;
        disparityParam_.bitSize      = 14;  // low 14 bit
        disparityParam_.dispIntPlace = 8;
        disparityParam_.unit         = depthCalib.unit;
        disparityParam_.dispOffset   = depthCalib.dispOffset;
        disparityParam_.invalidDisp  = depthCalib.invalidDisp;
        disparityParam_.packMode     = OB_DISP_PACK_ORIGINAL_NEW;
        disparityParam_.isDualCamera = true;
    }
    catch(const std::exception &e) {
        LOG_ERROR("Get depth calibration params failed! {}", e.what());
    }

    uint8_t retry                  = 2;
    bool    readCalibParamsSuccess = false;
    while(retry > 0 && !readCalibParamsSuccess) {
        try {
            auto owner             = getOwner();
            auto propServer        = owner->getPropertyServer();
            auto cameraParamList   = propServer->getStructureDataListProtoV1_1_T<OBCameraParam_Internal_V0, 0>(OB_RAW_DATA_ALIGN_CALIB_PARAM);
            readCalibParamsSuccess = true;
            for(auto &cameraParam: cameraParamList) {
                OBCameraParam param;
                param.depthIntrinsic = cameraParam.depthIntrinsic;
                param.rgbIntrinsic   = cameraParam.rgbIntrinsic;
                memcpy(&param.depthDistortion, &cameraParam.depthDistortion, sizeof(param.depthDistortion));
                param.depthDistortion.model = OB_DISTORTION_BROWN_CONRADY;
                memcpy(&param.rgbDistortion, &cameraParam.rgbDistortion, sizeof(param.rgbDistortion));
                param.rgbDistortion.model = OB_DISTORTION_BROWN_CONRADY_K6;
                param.transform           = cameraParam.transform;
                param.isMirrored          = false;
                originCalibrationCameraParamList_.emplace_back(param);

                std::stringstream ss;
                ss << param;
                LOG_DEBUG("-{}", ss.str());
            }
        }
        catch(const std::exception &e) {
            LOG_ERROR("Get depth calibration params failed! {}", e.what());
            retry--;
            if(retry > 0) {
                LOG_DEBUG("Retrying to depth calibration params.");
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    }

    retry                   = 2;
    bool readD2CListSuccess = false;
    while(retry > 0 && !readD2CListSuccess) {
        try {
            auto owner            = getOwner();
            auto propServer       = owner->getPropertyServer();
            originD2cProfileList_ = propServer->getStructureDataListProtoV1_1_T<OBD2CProfile, 0>(OB_RAW_DATA_D2C_ALIGN_SUPPORT_PROFILE_LIST);
            readD2CListSuccess    = true;
            LOG_DEBUG("Read origin D2C profile list success,size:{}!", originD2cProfileList_.size());
        }
        catch(const std::exception &e) {
            LOG_ERROR("Get depth to color profile list failed! {}", e.what());
            retry--;
            if(retry > 0) {
                LOG_DEBUG("Retrying to read origin D2C profile list.");
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    }
}

void G305AlgParamManager::reFetchDisparityParams() {
    try {
        auto owner           = getOwner();
        auto propServer      = owner->getPropertyServer();
        depthCalibParamList_ = propServer->getStructureDataListProtoV1_1_T<OBDepthCalibrationParam, 1>(OB_RAW_DATA_DEPTH_CALIB_PARAM);

        const auto &depthCalib       = depthCalibParamList_.front();
        disparityParam_.baseline     = depthCalib.baseline;
        disparityParam_.zpd          = depthCalib.z0;
        disparityParam_.fx           = depthCalib.focalPix;
        disparityParam_.zpps         = depthCalib.z0 / depthCalib.focalPix;
        disparityParam_.bitSize      = 14;  // low 14 bit
        disparityParam_.dispIntPlace = 8;
        disparityParam_.unit         = depthCalib.unit;
        disparityParam_.dispOffset   = depthCalib.dispOffset;
        disparityParam_.invalidDisp  = depthCalib.invalidDisp;
        disparityParam_.packMode     = OB_DISP_PACK_ORIGINAL_NEW;
        disparityParam_.isDualCamera = true;
    }
    catch(const std::exception &e) {
        LOG_ERROR("Get depth calibration params failed! {}", e.what());
    }
}

void G305AlgParamManager::registerBasicExtrinsics() {
    auto extrinsicMgr            = StreamExtrinsicsManager::getInstance();
    auto depthBasicStreamProfile = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_DEPTH, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
    auto colorBasicStreamProfile = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_COLOR, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
    auto leftcolorBasicStreamProfile =
        StreamProfileFactory::createVideoStreamProfile(OB_STREAM_COLOR_LEFT, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
    auto rightcolorBasicStreamProfile =
        StreamProfileFactory::createVideoStreamProfile(OB_STREAM_COLOR_RIGHT, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
    auto leftIrBasicStreamProfile  = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_IR_LEFT, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
    auto rightIrBasicStreamProfile = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_IR_RIGHT, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
    if(!originCalibrationCameraParamList_.empty()) {
        auto d2cExtrinsic = originCalibrationCameraParamList_.front().transform;
        extrinsicMgr->registerExtrinsics(depthBasicStreamProfile, colorBasicStreamProfile, d2cExtrinsic);
    }
    extrinsicMgr->registerSameExtrinsics(leftIrBasicStreamProfile, depthBasicStreamProfile);

    if(!depthCalibParamList_.empty()) {
        auto left_to_right     = IdentityExtrinsics;
        left_to_right.trans[0] = -1.0f * depthCalibParamList_.front().baseline * depthCalibParamList_.front().unit;
        extrinsicMgr->registerExtrinsics(leftIrBasicStreamProfile, rightIrBasicStreamProfile, left_to_right);
    }

    basicStreamProfileList_.emplace_back(depthBasicStreamProfile);
    basicStreamProfileList_.emplace_back(colorBasicStreamProfile);
    basicStreamProfileList_.emplace_back(leftcolorBasicStreamProfile);
    basicStreamProfileList_.emplace_back(rightcolorBasicStreamProfile);
    basicStreamProfileList_.emplace_back(leftIrBasicStreamProfile);
    basicStreamProfileList_.emplace_back(rightIrBasicStreamProfile);
}

void G305AlgParamManager::fixD2CParmaList() {
    std::vector<Resolution> appendColorResolutions;
    std::vector<Resolution> depthResolutions;

    auto owner      = getOwner();
    auto deviceInfo = owner->getInfo();

    std::vector<Resolution> otherDeviceColorResolutions = { { 1280, 800 }, { 1280, 720 }, { 848, 480 }, { 848, 530 }, { 640, 480 } };

    std::map<std::pair<Resolution, int>, Resolution> downDacimationToOrigMap = {
        { { { 1280, 800 }, 1 }, { 1280, 800 } }, { { { 640, 400 }, 2 }, { 1280, 800 } },  { { { 424, 266 }, 3 }, { 1280, 800 } },
        { { { 320, 200 }, 4 }, { 1280, 800 } },  { { { 1280, 720 }, 1 }, { 1280, 720 } }, { { { 640, 360 }, 2 }, { 1280, 720 } },
        { { { 424, 240 }, 3 }, { 1280, 720 } },  { { { 320, 180 }, 4 }, { 1280, 720 } },  { { { 848, 530 }, 1 }, { 848, 530 } },
        { { { 424, 266 }, 2 }, { 848, 530 } },   { { { 282, 176 }, 3 }, { 848, 530 } },   { { { 212, 132 }, 4 }, { 848, 530 } },
        { { { 848, 480 }, 1 }, { 848, 480 } },   { { { 424, 240 }, 2 }, { 848, 480 } },   { { { 282, 160 }, 3 }, { 848, 480 } },
        { { { 212, 120 }, 4 }, { 848, 480 } },   { { { 640, 480 }, 1 }, { 640, 480 } },   { { { 320, 240 }, 2 }, { 640, 480 } },
        { { { 160, 120 }, 4 }, { 640, 480 } }

    };

    appendColorResolutions.assign(otherDeviceColorResolutions.begin(), otherDeviceColorResolutions.end());

    if(originD2cProfileList_.empty()) {
        return;
    }

    d2cProfileList_ = originD2cProfileList_;
    for(auto &profile: d2cProfileList_) {
        profile.alignType = ALIGN_D2C_HW;
    }

    calibrationCameraParamList_ = originCalibrationCameraParamList_;
    for(const auto &colorRes: appendColorResolutions) {
        auto          colorProfile = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_COLOR, OB_FORMAT_UNKNOWN, colorRes.width, colorRes.height, 30);
        OBCameraParam colorAlignParam;
        if(!findBestMatchedCameraParam(originCalibrationCameraParamList_, colorProfile, colorAlignParam)) {
            continue;
        }
        OBCameraIntrinsic colorIntrinsic = colorAlignParam.rgbIntrinsic;
        int               colorScale     = colorIntrinsic.width / colorProfile->getWidth();
        auto              colorRatio     = 1.f / colorScale;
        float             colorDx        = (((int)colorIntrinsic.width - colorScale * (int)colorProfile->getWidth()) / 2.f) + colorScale - 1;  // delta_w/2
        float             colorBase      = ((int)colorIntrinsic.height - colorScale * (int)colorProfile->getHeight()) / 2.f;
        colorBase                        = colorBase < 0.f ? 0.f : colorBase;
        float colorDy                    = colorBase + colorScale - 1.f;
        colorIntrinsic.fx *= colorRatio;
        colorIntrinsic.fy *= colorRatio;
        colorIntrinsic.cx     = (colorIntrinsic.cx - colorDx) * colorRatio;
        colorIntrinsic.cy     = (colorIntrinsic.cy - colorDy) * colorRatio;
        colorIntrinsic.width  = static_cast<int16_t>(colorProfile->getWidth());
        colorIntrinsic.height = static_cast<int16_t>(colorProfile->getHeight());
        for(const auto &downDepthResolution: downDacimationToOrigMap) {
            auto depthRes     = downDepthResolution.first.first;
            auto origDepthRes = downDepthResolution.second;
            auto depthProfile = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_DEPTH, OB_FORMAT_UNKNOWN, origDepthRes.width, origDepthRes.height, 30);
            OBCameraParam depthAlignParam;
            if(!findBestMatchedCameraParam(originCalibrationCameraParamList_, depthProfile, depthAlignParam)) {
                continue;
            }
            OBCameraIntrinsic depthIntrinsic = depthAlignParam.depthIntrinsic;
            int               depthScale     = depthIntrinsic.width / depthRes.width;
            auto              depthRatio     = 1.f / depthScale;
            float             depthDx        = (((int)depthIntrinsic.width - depthScale * (int)depthRes.width) / 2.f) + depthScale - 1;  // delta_w/2
            float             depthBase      = ((int)depthIntrinsic.height - depthScale * (int)depthRes.height) / 2.f;
            depthBase                        = depthBase < 0.f ? 0.f : depthBase;
            float depthDy                    = depthBase + depthScale - 1.f;
            depthIntrinsic.fx *= depthRatio;
            depthIntrinsic.fy *= depthRatio;
            depthIntrinsic.cx     = (depthIntrinsic.cx - depthDx) * depthRatio;
            depthIntrinsic.cy     = (depthIntrinsic.cy - depthDy) * depthRatio;
            depthIntrinsic.width  = static_cast<int16_t>(depthRes.width);
            depthIntrinsic.height = static_cast<int16_t>(depthRes.height);

            auto index = calibrationCameraParamList_.size();
            calibrationCameraParamList_.push_back({ depthIntrinsic, colorIntrinsic, depthAlignParam.depthDistortion, depthAlignParam.rgbDistortion,
                                                    depthAlignParam.transform, depthAlignParam.isMirrored });

            OBD2CProfile d2cProfile;
            d2cProfile.alignType        = ALIGN_D2C_SW;
            d2cProfile.postProcessParam = { 1.0f, 0, 0, 0, 0 };
            d2cProfile.colorWidth       = static_cast<int16_t>(colorProfile->getWidth());
            d2cProfile.colorHeight      = static_cast<int16_t>(colorProfile->getHeight());
            d2cProfile.depthWidth       = static_cast<int16_t>(depthRes.width);
            d2cProfile.depthHeight      = static_cast<int16_t>(depthRes.height);
            d2cProfile.paramIndex       = (uint8_t)index;
            d2cProfileList_.push_back(d2cProfile);
        }
    }

    // fix depth intrinsic according to post process param and rgb intrinsic
    for(auto &profile: d2cProfileList_) {
        if(profile.alignType != ALIGN_D2C_HW || profile.paramIndex >= calibrationCameraParamList_.size()) {
            continue;
        }
        const auto &cameraParam = calibrationCameraParamList_.at(profile.paramIndex);

        auto colorProfile = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_COLOR, OB_FORMAT_UNKNOWN, profile.colorWidth, profile.colorHeight, 30);

        OBCameraParam fixedCameraParam = cameraParam;
        OBCameraParam matchedCameraParam;
        if(!findBestMatchedCameraParam(originCalibrationCameraParamList_, colorProfile, matchedCameraParam)) {
            continue;
        }
        fixedCameraParam.rgbIntrinsic = matchedCameraParam.rgbIntrinsic;

        // scale the rgb(target) intrinsic to match the profile
        auto ratio = (float)profile.colorWidth / fixedCameraParam.rgbIntrinsic.width;
        fixedCameraParam.rgbIntrinsic.fx *= ratio;
        fixedCameraParam.rgbIntrinsic.fy *= ratio;
        fixedCameraParam.rgbIntrinsic.cx *= ratio;
        fixedCameraParam.rgbIntrinsic.cy *= ratio;
        fixedCameraParam.rgbIntrinsic.width  = static_cast<int16_t>((float)fixedCameraParam.rgbIntrinsic.width * ratio);
        fixedCameraParam.rgbIntrinsic.height = static_cast<int16_t>((float)fixedCameraParam.rgbIntrinsic.height * ratio);

        // Scale the rgb(target) intrinsic parameters by the same factor as the depth intrinsic scale.
        auto depthRatio = (float)fixedCameraParam.depthIntrinsic.width / profile.depthWidth;
        fixedCameraParam.rgbIntrinsic.fx *= depthRatio;
        fixedCameraParam.rgbIntrinsic.fy *= depthRatio;
        fixedCameraParam.rgbIntrinsic.cx *= depthRatio;
        fixedCameraParam.rgbIntrinsic.cy *= depthRatio;
        fixedCameraParam.rgbIntrinsic.width  = static_cast<int16_t>((float)fixedCameraParam.rgbIntrinsic.width * depthRatio);
        fixedCameraParam.rgbIntrinsic.height = static_cast<int16_t>((float)fixedCameraParam.rgbIntrinsic.height * depthRatio);

        // according to post process param to reverse process the rgb(target) intrinsic
        fixedCameraParam.rgbIntrinsic.fx     = fixedCameraParam.rgbIntrinsic.fx / profile.postProcessParam.depthScale;
        fixedCameraParam.rgbIntrinsic.fy     = fixedCameraParam.rgbIntrinsic.fy / profile.postProcessParam.depthScale;
        fixedCameraParam.rgbIntrinsic.cx     = (fixedCameraParam.rgbIntrinsic.cx - profile.postProcessParam.alignLeft) / profile.postProcessParam.depthScale;
        fixedCameraParam.rgbIntrinsic.cy     = (fixedCameraParam.rgbIntrinsic.cy - profile.postProcessParam.alignTop) / profile.postProcessParam.depthScale;
        fixedCameraParam.rgbIntrinsic.width  = profile.depthWidth;
        fixedCameraParam.rgbIntrinsic.height = profile.depthHeight;

        auto index = calibrationCameraParamList_.size();
        calibrationCameraParamList_.push_back(fixedCameraParam);
        // auto oldProfile    = profile;
        profile.paramIndex = (uint8_t)index;

        // std::stringstream ss;
        // ss << "Fix align calibration camera params:" << std::endl;
        // ss << oldProfile << std::endl;
        // ss << cameraParam.rgbIntrinsic << std::endl;
        // ss << "to:" << std::endl;
        // ss << profile << std::endl;
        // ss << fixedCameraParam.rgbIntrinsic << std::endl;
        // LOG_INFO("- {}", ss.str());
    }

    for(auto &d2cProfile: d2cProfileList_) {
        if(d2cProfile.alignType != 1) {
            continue;
        }
        auto colorWidth  = d2cProfile.colorWidth;
        auto colorHeight = d2cProfile.colorHeight;
        auto depthWidth  = d2cProfile.depthWidth;
        auto depthHeight = d2cProfile.depthHeight;
        for(uint8_t i = 0; i < calibrationCameraParamList_.size(); i++) {
            if(calibrationCameraParamList_[i].rgbIntrinsic.width != colorWidth || calibrationCameraParamList_[i].rgbIntrinsic.height != colorHeight
               || calibrationCameraParamList_[i].depthIntrinsic.width != depthWidth || calibrationCameraParamList_[i].depthIntrinsic.height != depthHeight) {
                continue;
            }
            d2cProfile.paramIndex = i;
            break;
        }
    }

    // LOG_DEBUG("Fixed align calibration camera params success! num={}", calibrationCameraParamList_.size());
    // for(auto &&profile: d2cProfileList_) {
    //     if(profile.paramIndex >= calibrationCameraParamList_.size()) {
    //         continue;
    //     }

    //     std::stringstream ss;
    //     ss << profile;
    //     ss << std::endl;
    //     ss << calibrationCameraParamList_[profile.paramIndex];
    //     LOG_DEBUG("- {}", ss.str());
    // }
}

void G305AlgParamManager::bindIntrinsic(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) {
    auto intrinsicMgr = StreamIntrinsicsManager::getInstance();
    for(const auto &sp: streamProfileList) {
        OBCameraIntrinsic  intrinsic  = { 0 };
        OBCameraDistortion distortion = { 0 };
        OBCameraParam      param{};
        auto               vsp = sp->as<VideoStreamProfile>();
        if(!findBestMatchedCameraParam(calibrationCameraParamList_, vsp, param)) {
            // throw libobsensor::unsupported_operation_exception("Can not find matched camera param!");
            continue;
        }
        switch(sp->getType()) {
        case OB_STREAM_COLOR:
        case OB_STREAM_COLOR_LEFT:
        case OB_STREAM_COLOR_RIGHT:
            intrinsic  = param.rgbIntrinsic;
            distortion = param.rgbDistortion;
            break;
        case OB_STREAM_DEPTH:
        case OB_STREAM_IR:
        case OB_STREAM_IR_LEFT:
        case OB_STREAM_IR_RIGHT:
            intrinsic  = param.depthIntrinsic;
            distortion = param.depthDistortion;
            break;
        default:
            break;
        }
        auto  decimationConfig = vsp->getDecimationConfig();
        auto  originWidth      = decimationConfig.factor == 0 ? vsp->getWidth() : decimationConfig.originWidth;
        auto  originHeight     = decimationConfig.factor == 0 ? vsp->getHeight() : decimationConfig.originHeight;
        int   scale            = originWidth / vsp->getWidth();
        auto  ratio            = 1.f / scale;
        float dx               = (((int)originWidth - scale * (int)vsp->getWidth()) / 2.f) + scale - 1;  // delta_w/2
        float dyBase           = (originHeight - scale * vsp->getHeight()) * 0.5f;
        dyBase                 = dyBase < 0.f ? 0.f : dyBase;
        float dy               = dyBase + scale - 1.f;
        intrinsic.fx *= ratio;
        intrinsic.fy *= ratio;
        intrinsic.cx     = (intrinsic.cx - dx) * ratio;
        intrinsic.cy     = (intrinsic.cy - dy) * ratio;
        intrinsic.width  = static_cast<int16_t>(vsp->getWidth());
        intrinsic.height = static_cast<int16_t>(vsp->getHeight());
        intrinsicMgr->registerVideoStreamIntrinsics(sp, intrinsic);
        intrinsicMgr->registerVideoStreamDistortion(sp, distortion);
    }
}

void G305AlgParamManager::fetchPresetResolutionConfig() {
    auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();  // Auto-lock when getting propertyServer

    if(!propServer->isPropertySupported(OB_RAW_DATA_PRESET_RESOLUTION_MASK_LIST, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
        return;
    }

    try {
        auto presetResolutionMaskList = propServer->getStructureDataListProtoV1_1_T<OBPresetResolutionMask, 1>(OB_RAW_DATA_PRESET_RESOLUTION_MASK_LIST);
        for(auto &ratio: presetResolutionMaskList) {
            int depthDecimation[16] = { 0 };
            int irDecimation[16]    = { 0 };
            for(int bit = 0; bit < 16; ++bit) {
                bool depthBitSet = (ratio.depthDecimationFlag & (1 << bit)) != 0;
                bool irBitSet    = (ratio.irDecimationFlag & (1 << bit)) != 0;
                if(irBitSet) {
                    irDecimation[bit] = 1;
                }
                if(depthBitSet) {
                    depthDecimation[bit] = 1;
                }
            }

            for(int depthBit = 0; depthBit < 16; ++depthBit) {
                if(depthDecimation[depthBit] == 0) {
                    continue;
                }
                for(int irBit = 0; irBit < 16; ++irBit) {
                    if(irDecimation[irBit] == 0) {
                        continue;
                    }
                    OBPresetResolutionConfig config{};
                    config.width                 = ratio.width;
                    config.height                = ratio.height;
                    config.depthDecimationFactor = depthBit + 1;
                    config.irDecimationFactor    = irBit + 1;
                    presetResolutionConfigList_.push_back(config);
                }
            }
        }
    }
    catch(const std::exception &e) {
        LOG_ERROR("Get preset resolution mask failed! {}", e.what());
        return;
    }
}

std::vector<OBPresetResolutionConfig> G305AlgParamManager::getPresetResolutionConfigList() const {
    return presetResolutionConfigList_;
}

}  // namespace libobsensor
