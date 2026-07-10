// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "PresetHandler.hpp"
#include "IDepthWorkModeManager.hpp"
#include "PresetDefinitions.hpp"
#include "IDeviceMonitor.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "PresetDefinitions.hpp"

namespace libobsensor {

namespace {
// On a playback device a non-writable property must be skipped silently (no throw, no log).
// On a real device this returns false, so the caller keeps its original behavior (write, or warn/throw).
bool skipWriteOnPlayback(IDevice *owner, uint32_t propertyId) {
    if(owner == nullptr || !owner->isPlaybackDevice()) {
        return false;
    }
    auto propServer = owner->getPropertyServer();
    return propServer && !propServer->isPropertySupported(propertyId, PROP_OP_WRITE, PROP_ACCESS_INTERNAL);
}
}  // namespace

// FrameInterleaveHandler
FrameInterleaveHandler::FrameInterleaveHandler(IDevice *owner)
    : owner_(owner),
      paramsKeyList_{ { kInterleaveDepthExposureKey, &FrameInterleaveParam::depthExposureTime },
                      { kInterleaveDepthGainKey, &FrameInterleaveParam::depthGain },
                      { kInterleaveDepthBrightnessKey, &FrameInterleaveParam::depthBrightness },
                      { kInterleaveDepthAeMaxExposureKey, &FrameInterleaveParam::depthMaxExposure },
                      { kInterleaveLaserControlKey, &FrameInterleaveParam::laserSwitch } } {
    auto manager = owner_->getComponentT<IFrameInterleaveManager>(OB_DEV_COMPONENT_FRAME_INTERLEAVE_MANAGER, false);
    if(manager) {
        frameInterleaveManager_ = manager.get();
    }
}

bool FrameInterleaveHandler::onPreChildrenSet(const Json::Value &value) {
    if(skipWriteOnPlayback(owner_, OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL)) {
        return false;
    }
    // clear data
    enable_          = false;
    currentIndex_    = -1;
    currentModeName_ = "";
    params_.clear();

    if(frameInterleaveManager_ == nullptr) {
        LOG_WARN("The current device doesn't contain the frame interleave manager component");
        return false;
    }
    if(!value.isMember(kEnableKey)) {
        THROW_INVALID_PARAM_EXCEPTION(utils::string::to_string() << "Missing required field in frame interleave: " << kEnableKey);
    }
    enable_ = jsonmodel::JsonTraits<bool>::from(value[kEnableKey]);
    return enable_;
}

void FrameInterleaveHandler::onSetChild(const std::string &k, const Json::Value &v, const Json::Value &parent) {
    utils::unusedVar(parent);
    if(skipWriteOnPlayback(owner_, OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL)) {
        return;
    }
    // Save all values
    if(k == kEnableKey) {
        enable_ = jsonmodel::JsonTraits<bool>::from(v);
    }
    else if(k == kInterleaveModeKey) {
        currentModeName_ = jsonmodel::JsonTraits<std::string>::from(v);
        if(currentModeName_.empty()) {
            THROW_INVALID_PARAM_EXCEPTION("The value is empty in frame interleave: " + k);
        }
    }
    else if(k == kInterleaveConfigIndexKey) {
        currentIndex_ = jsonmodel::JsonTraits<int>::from(v);
        if(currentIndex_ < 0) {
            THROW_INVALID_PARAM_EXCEPTION("The value is invalid in frame interleave: " + k);
        }
    }
    else if(k == kInterleaveParmas) {
        if(!v.isArray()) {
            THROW_INVALID_PARAM_EXCEPTION("The value is not array in frame interleave: " + k);
        }
        params_.clear();
        for(Json::ArrayIndex i = 0; i < v.size(); ++i) {
            const Json::Value   &item = v[i];
            FrameInterleaveParam param{};
            for(auto &keyMap: paramsKeyList_) {
                const auto &key                   = keyMap.first;
                int FrameInterleaveParam::*member = keyMap.second;
                if(!item.isMember(key)) {
                    THROW_INVALID_PARAM_EXCEPTION("Missing required field in frame interleave params: " + key);
                }
                param.*member = jsonmodel::JsonTraits<int>::from(item[key]);
            }
            params_.push_back(std::move(param));
        }
        if(params_.empty()) {
            THROW_INVALID_PARAM_EXCEPTION("The child node is empty in frame interleave: " + k);
        }
    }
    else {
        THROW_INVALID_PARAM_EXCEPTION("Invalid key of frame interleave: '" + k + "'");
    }
}

void FrameInterleaveHandler::onPostChildrenSet() {
    if(frameInterleaveManager_ == nullptr) {
        LOG_WARN("The current device doesn't contain the frame interleave manager component");
        return;
    }
    if(skipWriteOnPlayback(owner_, OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL)) {
        return;
    }
    // Set all values
    auto propServer = owner_->getPropertyServer();
    propServer->setPropertyValueT<bool>(OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL, enable_);
    if(enable_) {
        // update params
        int32_t index = 0;
        for(const auto &param: params_) {
            frameInterleaveManager_->updateParam(currentModeName_, param, index++);
        }
        // load
        frameInterleaveManager_->loadFrameInterleave(currentModeName_);
        // update index
        propServer->setPropertyValueT<int>(OB_PROP_FRAME_INTERLEAVE_CONFIG_INDEX_INT, currentIndex_);
    }
}

std::vector<std::string> FrameInterleaveHandler::onPreChildrenGet() {
    // clear data
    enable_          = false;
    currentIndex_    = -1;
    currentModeName_ = "";
    params_.clear();

    if(frameInterleaveManager_ == nullptr) {
        LOG_WARN("The current device doesn't contain the frame interleave manager component");
        return {};
    }
    // Get all values
    auto propServer = owner_->getPropertyServer();
    enable_         = propServer->getPropertyValueT<bool>(OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL);
    if(enable_) {
        currentIndex_    = propServer->getPropertyValueT<int>(OB_PROP_FRAME_INTERLEAVE_CONFIG_INDEX_INT);
        currentModeName_ = frameInterleaveManager_->getCurrentFrameInterleaveName();
        if(!currentModeName_.empty()) {
            auto count = frameInterleaveManager_->getParamCount();
            for(int32_t i = 0; i < count; i++) {
                auto param = frameInterleaveManager_->getParam(currentModeName_, i);
                params_.push_back(param);
            }
        }
    }
    // No any extra child nodes
    return {};
}

jsonmodel::ExportValue FrameInterleaveHandler::exportChildValue(const std::string &k) {
    if(k == kEnableKey) {
        return jsonmodel::makeScalar(enable_);
    }
    else if(k == kInterleaveModeKey) {
        return jsonmodel::makeScalar(currentModeName_);
    }
    else if(k == kInterleaveConfigIndexKey) {
        return jsonmodel::makeScalar(currentIndex_);
    }
    else if(k == kInterleaveParmas) {
        std::vector<jsonmodel::ExportValue> paramItems;
        paramItems.reserve(params_.size());
        for(const auto &param: params_) {
            std::vector<jsonmodel::ExportField> fields;
            fields.reserve(paramsKeyList_.size());
            for(const auto &entry: paramsKeyList_) {
                fields.emplace_back(jsonmodel::makeField(entry.first, param.*(entry.second)));
            }
            paramItems.emplace_back(jsonmodel::ExportValue::object(std::move(fields)));
        }
        return jsonmodel::ExportValue::array(std::move(paramItems));
    }
    else {
        THROW_INVALID_PARAM_EXCEPTION("Invalid key of frame interleave: '" + k + "'");
    }
}

// RoiHandler
RoiHandler::RoiHandler(IDevice *owner, uint32_t propertyId, OBSensorType sensorType)
    : owner_(owner),
      propertyId_(propertyId),
      sensorType_(sensorType),
      valueMap_{ { kLeftKey, &roi_.x0_left }, { kRightKey, &roi_.x1_right }, { kTopKey, &roi_.y0_top }, { kBottomKey, &roi_.y1_bottom } } {}

RoiHandler::~RoiHandler() {
    stopSetting();
}

void RoiHandler::onImportReset() {
    // Cancel any pending deferred write left over from a previous import, even when this import
    // does not carry the ROI node at all.
    stopSetting();
}

bool RoiHandler::onPreChildrenSet(const Json::Value &value) {
    utils::unusedVar(value);
    stopSetting();
    writeSupported_ = true;
    auto propServer = owner_->getPropertyServer();
    if(!propServer->isPropertySupported(propertyId_, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        writeSupported_ = false;
        return false;
    }
    return true;
}

void RoiHandler::onSetChild(const std::string &k, const Json::Value &v, const Json::Value &parent) {
    utils::unusedVar(parent);
    if(v.isNull()) {
        writeSupported_ = false;
        return;
    }

    // Save all values
    auto it = valueMap_.find(k);
    if(it == valueMap_.end()) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid key of ROI: '" + k + "'");
    }
    auto data = jsonmodel::JsonTraits<int>::from(v);
    if(data < std::numeric_limits<int16_t>::min() || data > std::numeric_limits<int16_t>::max()) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid value of ROI. '" + k + "': " + std::to_string(data));
    }
    *(it->second) = static_cast<int16_t>(data);
}

void RoiHandler::onPostChildrenSet() {
    if(!writeSupported_) {
        if(!owner_->isPlaybackDevice()) {
            LOG_WARN("Unsupported ROI properties, skipping setting");
        }
        return;
    }

    auto sensor = owner_->getSensor(sensorType_);
    if(!sensor) {
        LOG_WARN("Sensor not found for ROI property '{}', skipping setting", propertyId_);
        return;
    }

    setRoiProperties();

    if(owner_->isPlaybackDevice()) {
        return;
    }

    // Always register a stream callback to write again once streaming starts, ensuring the
    // value takes effect even when the firmware ignores writes made before the stream is active.
    *needSetting_   = true;
    needUnregister_ = true;
    registerUserOverrideGuard();
    callbackToken_ = sensor->registerStreamStateChangedCallback([this](OBStreamState state, const std::shared_ptr<const StreamProfile> &profile) {
        utils::unusedVar(profile);
        if(state != STREAM_STATE_STREAMING) {
            return;
        }
        // Claim this deferred write exactly once: only the invocation that flips true->false starts
        // the thread, so a repeated STREAMING event never reassigns a still-joinable settingThread_.
        bool expected = true;
        if(!needSetting_->compare_exchange_strong(expected, false)) {
            return;
        }
        settingThread_ = std::thread([this]() {
            setRoiProperties();
            unregisterStreamCallback();
        });
    });
}

void RoiHandler::stopSetting() {
    unregisterStreamCallback();
    unregisterUserOverrideGuard();
    if(settingThread_.joinable()) {
        settingThread_.join();
    }
}

void RoiHandler::registerUserOverrideGuard() {
    if(overrideGuardToken_ != 0) {
        return;
    }
    auto propServer = owner_->getPropertyServer();
    // Capture the flag by shared_ptr rather than this: an access callback may still run from a
    // snapshot taken before unregister, when the handler could already have been destroyed.
    auto needSetting    = needSetting_;
    overrideGuardToken_ = propServer->registerAccessCallback(propertyId_, [needSetting](uint32_t, const uint8_t *, size_t, PropertyOperationType op) {
        if(op == PROP_OP_WRITE) {
            needSetting->store(false);
        }
    });
}

void RoiHandler::unregisterUserOverrideGuard() {
    if(overrideGuardToken_ != 0) {
        TRY_EXECUTE({
            auto propServer = owner_->getPropertyServer();
            propServer->unregisterAccessCallback(overrideGuardToken_);
        });
        overrideGuardToken_ = 0;
    }
}

void RoiHandler::unregisterStreamCallback() {
    if(needUnregister_) {
        TRY_EXECUTE({
            auto sensor = owner_->getSensor(sensorType_);
            sensor->unregisterStreamStateChangedCallback(callbackToken_);
            needUnregister_ = false;
        });
    }
}

void RoiHandler::setRoiProperties() {
    auto propServer = owner_->getPropertyServer();
    TRY_EXECUTE({ propServer->setStructureDataT<AE_ROI>(propertyId_, roi_); });
}

std::vector<std::string> RoiHandler::onPreChildrenGet() {
    auto propServer = owner_->getPropertyServer();
    if(propServer->isPropertySupported(propertyId_, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
        roi_           = propServer->getStructureDataT<AE_ROI>(propertyId_);
        readSupported_ = true;
        auto sensor    = owner_->getSensor(sensorType_);
        if(sensor && !sensor->isStreamActivated()) {
            LOG_WARN("Exporting ROI property '{}' while stream is not active; the value may differ from what will be applied after streaming starts",
                     propertyId_);
        }
    }
    else {
        readSupported_ = false;
        roi_.x0_left = roi_.x1_right = roi_.y0_top = roi_.y1_bottom = 0;
        LOG_WARN("Unsupported property '{}', skipping getting, return nullvalue", propertyId_);
    }
    // No any extra child nodes
    return {};
}

jsonmodel::ExportValue RoiHandler::exportChildValue(const std::string &k) {
    if(!readSupported_) {
        return jsonmodel::ExportValue::nullValue();
    }

    auto it = valueMap_.find(k);
    if(it == valueMap_.end()) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid key of ROI: '" + k + "'");
    }
    return jsonmodel::makeScalar(static_cast<int>(*(it->second)));
}

// D2DHandler
constexpr const char kDisable[]  = "disable";
constexpr const char kHardware[] = "hardware";
constexpr const char kSoftware[] = "software";
D2DHandler::D2DHandler(IDevice *owner)
    : owner_(owner), valueMap_{ { kDisable, Mode::Disable }, { kHardware, Mode::Hardware }, { kSoftware, Mode::Software } } {}

void D2DHandler::set(const std::string &k, const Json::Value &v) {
    utils::unusedVar(k);
    auto mode = toString(v);
    auto it   = valueMap_.find(mode);
    if(it == valueMap_.end()) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid value of '" + k + "': " + mode);
    }
    auto propServer = owner_->getPropertyServer();
    // Per-property guard: on playback the hardware disparity property is read-only and is skipped,
    // while the software disparity property is writable and is still restored.
    auto setDisparity = [&](uint32_t propertyId, bool value) {
        if(skipWriteOnPlayback(owner_, propertyId)) {
            return;
        }
        propServer->setPropertyValueT<bool>(propertyId, value);
    };
    switch(it->second) {
    case Mode::Hardware:
        setDisparity(OB_PROP_DISPARITY_TO_DEPTH_BOOL, true);
        setDisparity(OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL, false);
        break;
    case Mode::Software:
        setDisparity(OB_PROP_DISPARITY_TO_DEPTH_BOOL, false);
        setDisparity(OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL, true);
        break;
    case Mode::Disable:
    default:
        setDisparity(OB_PROP_DISPARITY_TO_DEPTH_BOOL, false);
        setDisparity(OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL, false);
        break;
    }
}

jsonmodel::ExportValue D2DHandler::exportValue(const std::string &k) {
    utils::unusedVar(k);
    auto        propServer = owner_->getPropertyServer();
    auto        hardware   = propServer->getPropertyValueT<bool>(OB_PROP_DISPARITY_TO_DEPTH_BOOL);
    std::string mode;
    if(hardware) {
        // hardware first
        mode = kHardware;
    }
    else {
        auto software = propServer->getPropertyValueT<bool>(OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL);
        if(software) {
            mode = kSoftware;
        }
        else {
            mode = kDisable;
        }
    }

    return jsonmodel::makeScalar(mode);
}

// SoftwareNoiseRemovalFilterHandler
bool SoftwareNoiseRemovalFilterHandler::onPreChildrenSet(const Json::Value &value) {
    enable_ = jsonmodel::JsonTraits<bool>::from(value[kEnableKey]);
    return enable_;
}

void SoftwareNoiseRemovalFilterHandler::onSetChild(const std::string &k, const Json::Value &v, const Json::Value &parent) {
    utils::unusedVar(parent);
    // Save all values
    if(k == kEnableKey) {
        enable_ = jsonmodel::JsonTraits<bool>::from(v);
    }
    else if(k == kNoiseRemovalFilterMinDifferenceKey) {
        minDiff_ = jsonmodel::JsonTraits<int>::from(v);
    }
    else if(k == kNoiseRemovalFilterMaxSizeKey) {
        maxSize_ = jsonmodel::JsonTraits<int>::from(v);
    }
    else {
        THROW_INVALID_PARAM_EXCEPTION("Invalid key of software noise removal filter: '" + k + "'");
    }
}

void SoftwareNoiseRemovalFilterHandler::onPostChildrenSet() {
    // Set all values
    auto propServer = owner_->getPropertyServer();
    propServer->setPropertyValueT<bool>(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_BOOL, enable_);
    if(enable_) {
        propServer->setPropertyValueT<int>(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_DIFF_INT, minDiff_);
        propServer->setPropertyValueT<int>(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_SPECKLE_SIZE_INT, maxSize_);
    }
}

std::vector<std::string> SoftwareNoiseRemovalFilterHandler::onPreChildrenGet() {
    // Get all values
    auto propServer = owner_->getPropertyServer();
    enable_         = propServer->getPropertyValueT<bool>(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_BOOL);
    minDiff_        = propServer->getPropertyValueT<int>(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_DIFF_INT);
    maxSize_        = propServer->getPropertyValueT<int>(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_SPECKLE_SIZE_INT);

    // No any extra child nodes
    return {};
}

jsonmodel::ExportValue SoftwareNoiseRemovalFilterHandler::exportChildValue(const std::string &k) {
    if(k == kEnableKey) {
        return jsonmodel::makeScalar(enable_);
    }
    else if(k == kNoiseRemovalFilterMinDifferenceKey) {
        return jsonmodel::makeScalar(minDiff_);
    }
    else if(k == kNoiseRemovalFilterMaxSizeKey) {
        return jsonmodel::makeScalar(maxSize_);
    }
    else {
        THROW_INVALID_PARAM_EXCEPTION("Invalid key of software noise removal filter: '" + k + "'");
    }
}

// HardwareNoiseRemovalFilterHandler
bool HardwareNoiseRemovalFilterHandler::onPreChildrenSet(const Json::Value &value) {
    enable_ = jsonmodel::JsonTraits<bool>::from(value[kEnableKey]);
    return enable_;
}

void HardwareNoiseRemovalFilterHandler::onSetChild(const std::string &k, const Json::Value &v, const Json::Value &parent) {
    utils::unusedVar(parent);
    // Save all values
    if(k == kEnableKey) {
        enable_ = jsonmodel::JsonTraits<bool>::from(v);
    }
    else if(k == kNoiseRemovalFilterThresholdKey) {
        threshold_ = jsonmodel::JsonTraits<float>::from(v);
    }
    else {
        THROW_INVALID_PARAM_EXCEPTION("Invalid key of hardware noise removal filter: '" + k + "'");
    }
}

void HardwareNoiseRemovalFilterHandler::onPostChildrenSet() {
    if(skipWriteOnPlayback(owner_, OB_PROP_HW_NOISE_REMOVE_FILTER_ENABLE_BOOL)) {
        return;
    }
    // Set all values
    auto propServer = owner_->getPropertyServer();
    propServer->setPropertyValueT<bool>(OB_PROP_HW_NOISE_REMOVE_FILTER_ENABLE_BOOL, enable_);
    if(enable_) {
        propServer->setPropertyValueT<float>(OB_PROP_HW_NOISE_REMOVE_FILTER_THRESHOLD_FLOAT, threshold_);
    }
}

std::vector<std::string> HardwareNoiseRemovalFilterHandler::onPreChildrenGet() {
    // Get all values
    auto propServer = owner_->getPropertyServer();
    enable_         = propServer->getPropertyValueT<bool>(OB_PROP_HW_NOISE_REMOVE_FILTER_ENABLE_BOOL);
    threshold_      = propServer->getPropertyValueT<float>(OB_PROP_HW_NOISE_REMOVE_FILTER_THRESHOLD_FLOAT);

    // No any extra child nodes
    return {};
}

jsonmodel::ExportValue HardwareNoiseRemovalFilterHandler::exportChildValue(const std::string &k) {
    if(k == kEnableKey) {
        return jsonmodel::makeScalar(enable_);
    }
    else if(k == kNoiseRemovalFilterThresholdKey) {
        return jsonmodel::makeScalar(threshold_);
    }
    else {
        THROW_INVALID_PARAM_EXCEPTION("Invalid key of hardware noise removal filter: '" + k + "'");
    }
}

// DisparitySearchHandler
DisparitySearchHandler::~DisparitySearchHandler() {
    stopSetting();
}

void DisparitySearchHandler::onImportReset() {
    // Cancel any pending deferred write left over from a previous import, even when this import
    // does not carry the disparity search node at all.
    stopSetting();
}

void DisparitySearchHandler::stopSetting() {
    unregisterStreamCallback();
    unregisterUserOverrideGuard();
    if(settingThread_.joinable()) {
        settingThread_.join();
    }
}

void DisparitySearchHandler::registerUserOverrideGuard() {
    if(overrideGuardToken_ != 0) {
        return;
    }
    auto propServer = owner_->getPropertyServer();
    // Capture the flag by shared_ptr rather than this: an access callback may still run from a
    // snapshot taken before unregister, when the handler could already have been destroyed.
    auto needSetting    = needSetting_;
    overrideGuardToken_ = propServer->registerAccessCallback({ OB_PROP_DISP_SEARCH_RANGE_MODE_INT, OB_PROP_DISP_SEARCH_OFFSET_INT },
                                                             [needSetting](uint32_t, const uint8_t *, size_t, PropertyOperationType op) {
                                                                 if(op == PROP_OP_WRITE) {
                                                                     needSetting->store(false);
                                                                 }
                                                             });
}

void DisparitySearchHandler::unregisterUserOverrideGuard() {
    if(overrideGuardToken_ != 0) {
        TRY_EXECUTE({
            auto propServer = owner_->getPropertyServer();
            propServer->unregisterAccessCallback(overrideGuardToken_);
        });
        overrideGuardToken_ = 0;
    }
}

void DisparitySearchHandler::unregisterStreamCallback() {
    if(needUnregister_) {
        TRY_EXECUTE({
            auto sensor = owner_->getSensor(OB_SENSOR_DEPTH);
            sensor->unregisterStreamStateChangedCallback(callbackToken_);
            needUnregister_ = false;
        });
    }
}

void DisparitySearchHandler::setDisparitySearchProperties() {
    TRY_EXECUTE({
        auto propServer = owner_->getPropertyServer();
        propServer->setPropertyValueT<int>(OB_PROP_DISP_SEARCH_RANGE_MODE_INT, rangeMode_);
        propServer->setPropertyValueT<int>(OB_PROP_DISP_SEARCH_OFFSET_INT, searchOffset_);
    });
}

bool DisparitySearchHandler::onPreChildrenSet(const Json::Value &value) {
    utils::unusedVar(value);
    stopSetting();
    // clear data
    rangeMode_    = 0;
    searchOffset_ = 0;
    // check permission
    writeSupported_ = true;
    auto propServer = owner_->getPropertyServer();
    if(!propServer->isPropertySupported(OB_PROP_DISP_SEARCH_RANGE_MODE_INT, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)
       || !propServer->isPropertySupported(OB_PROP_DISP_SEARCH_OFFSET_INT, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        writeSupported_ = false;
        return false;
    }
    return true;
}

void DisparitySearchHandler::onSetChild(const std::string &k, const Json::Value &v, const Json::Value &parent) {
    utils::unusedVar(parent);
    if(v.isNull()) {
        writeSupported_ = false;
        return;
    }

    // Save all values
    if(k == kDisparitySearchRangeModeKey) {
        rangeMode_ = jsonmodel::JsonTraits<int>::from(v);
        for(const auto &entry: rangeModeMapping_) {
            if(entry.second == rangeMode_) {
                rangeMode_ = entry.first;
                break;
            }
        }
    }
    else if(k == kDisparitySearchOffsetKey) {
        searchOffset_ = jsonmodel::JsonTraits<int>::from(v);
    }
    else {
        THROW_INVALID_PARAM_EXCEPTION("Invalid key of disparity search: '" + k + "'");
    }
}

void DisparitySearchHandler::onPostChildrenSet() {
    if(owner_->isPlaybackDevice()) {
        return;
    }
    if(!writeSupported_) {
        LOG_WARN("Unsupported disparity search properties, skipping setting");
        return;
    }

    auto sensor = owner_->getSensor(OB_SENSOR_DEPTH);

    setDisparitySearchProperties();

    // Always register a stream callback to write again once streaming starts, ensuring the
    // value takes effect even when the firmware ignores writes made before the stream is active.
    *needSetting_   = true;
    needUnregister_ = true;
    registerUserOverrideGuard();
    callbackToken_ = sensor->registerStreamStateChangedCallback([this](OBStreamState state, const std::shared_ptr<const StreamProfile> &profile) {
        utils::unusedVar(profile);
        if(state != STREAM_STATE_STREAMING) {
            return;
        }
        // Claim this deferred write exactly once: only the invocation that flips true->false starts
        // the thread, so a repeated STREAMING event never reassigns a still-joinable settingThread_.
        bool expected = true;
        if(!needSetting_->compare_exchange_strong(expected, false)) {
            return;
        }
        settingThread_ = std::thread([this]() {
            setDisparitySearchProperties();
            unregisterStreamCallback();
        });
    });
}

std::vector<std::string> DisparitySearchHandler::onPreChildrenGet() {
    // clear data
    rangeMode_    = 0;
    searchOffset_ = 0;
    // check permission
    readSupported_  = true;
    auto propServer = owner_->getPropertyServer();

    // Prefer the read-only CURRENT_* property (the value in effect), fall back to base.
    // Returns false if neither is readable.
    auto pick = [&](uint32_t cur, uint32_t base, uint32_t &id) -> bool {
        if(propServer->isPropertySupported(cur, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
            id = cur;
            return true;
        }
        if(propServer->isPropertySupported(base, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
            id = base;
            return true;
        }
        return false;
    };

    uint32_t rangeModeId = 0, searchOffsetId = 0;
    if(!pick(OB_PROP_CURRENT_DISP_SEARCH_RANGE_MODE_INT, OB_PROP_DISP_SEARCH_RANGE_MODE_INT, rangeModeId)
       || !pick(OB_PROP_CURRENT_DISP_SEARCH_OFFSET_INT, OB_PROP_DISP_SEARCH_OFFSET_INT, searchOffsetId)) {
        readSupported_ = false;
        return {};
    }
    // Get all values
    rangeMode_    = propServer->getPropertyValueT<int>(rangeModeId);
    searchOffset_ = propServer->getPropertyValueT<int>(searchOffsetId);
    auto sensor   = owner_->getSensor(OB_SENSOR_DEPTH);
    if(sensor && !sensor->isStreamActivated()) {
        LOG_WARN("Exporting disparity search properties while stream is not active; the values may differ from what will be applied after streaming starts");
    }
    // No any extra child nodes
    return {};
}

jsonmodel::ExportValue DisparitySearchHandler::exportChildValue(const std::string &k) {  // Save all values
    if(!readSupported_) {
        return jsonmodel::ExportValue::nullValue();
    }

    if(k == kDisparitySearchRangeModeKey) {
        auto it = rangeModeMapping_.find(rangeMode_);
        return jsonmodel::makeScalar(it != rangeModeMapping_.end() ? it->second : rangeMode_);
    }
    else if(k == kDisparitySearchOffsetKey) {
        return jsonmodel::makeScalar(searchOffset_);
    }
    else {
        THROW_INVALID_PARAM_EXCEPTION("Invalid key of disparity search: '" + k + "'");
    }
}

// DepthWorkModeHandler
void DepthWorkModeHandler::set(const std::string &k, const Json::Value &v) {
    utils::unusedVar(k);

    if(owner_->isPlaybackDevice()) {
        // DepthWorkMode are real-hardware only; skip silently on playback.
        return;
    }

    auto value                = jsonmodel::JsonTraits<std::string>::from(v);
    auto depthWorkModeManager = owner_->getComponentT<IDepthWorkModeManager>(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER);
    depthWorkModeManager->switchDepthWorkMode(value);
}

jsonmodel::ExportValue DepthWorkModeHandler::exportValue(const std::string &k) {
    utils::unusedVar(k);
    auto depthWorkModeManager = owner_->getComponentT<IDepthWorkModeManager>(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER);
    auto workMode             = depthWorkModeManager->getCurrentDepthWorkMode();
    return jsonmodel::makeScalar(workMode.name);
}

// HeartbeatHandler
void HeartbeatHandler::set(const std::string &k, const Json::Value &v) {
    utils::unusedVar(k);
    if(owner_->isPlaybackDevice()) {
        // Heartbeat / firmware log are real-hardware only; skip silently on playback.
        return;
    }
    auto monitor = owner_->getComponentT<IDeviceMonitor>(OB_DEV_COMPONENT_DEVICE_MONITOR, false);
    if(!monitor) {
        LOG_WARN("Device monitor not available, skipping {} setting", firmwareLog_ ? "firmware log" : "heartbeat");
        return;
    }

    const bool enabled = jsonmodel::JsonTraits<bool>::from(v);
    if(firmwareLog_) {
        enabled ? monitor->enableFirmwareLog() : monitor->disableFirmwareLog();
        return;
    }
    // heartbeat
    enabled ? monitor->enableHeartbeat() : monitor->disableHeartbeat();
}

jsonmodel::ExportValue HeartbeatHandler::exportValue(const std::string &k) {
    utils::unusedVar(k);
    auto monitor = owner_->getComponentT<IDeviceMonitor>(OB_DEV_COMPONENT_DEVICE_MONITOR, false);
    if(!monitor) {
        return jsonmodel::ExportValue::nullValue();
    }

    if(firmwareLog_) {
        return jsonmodel::makeScalar(monitor->isFirmwareLogEnabled());
    }
    return jsonmodel::makeScalar(monitor->isHeartbeatEnabled());
}

}  // namespace libobsensor
