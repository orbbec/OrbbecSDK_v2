// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "PlaybackPropertyAccessor.hpp"
#include "frameprocessor/FrameProcessor.hpp"
#include "component/property/InternalProperty.hpp"
#include "PlaybackDevicePort.hpp"
#include "IDeviceComponent.hpp"
#include "logger/Logger.hpp"
#include "DevicePids.hpp"

#include "component/property/CommonPropertyAccessors.hpp"
#include "gemini2/G2PropertyAccessors.hpp"
#include "astra2/Astra2PropertyAccessors.hpp"
namespace libobsensor {

#pragma pack(1)
template <typename T> struct ImuSensorProfileList {
    uint32_t num;
    T        values[16];
};
using AccelSampleRateList     = ImuSensorProfileList<OBAccelSampleRate>;
using AccelFullScaleRangeList = ImuSensorProfileList<OBAccelFullScaleRange>;
using GyroSampleRateList      = ImuSensorProfileList<OBGyroSampleRate>;
using GyroFullScaleRangeList  = ImuSensorProfileList<OBGyroFullScaleRange>;
#pragma pack()

template <typename T> T *PlaybackVendorPropertyAccessor::allocateData(std::vector<uint8_t> &data) {
    data_.clear();
    data.resize(sizeof(T));
    return reinterpret_cast<T *>(data.data());
}

PlaybackVendorPropertyAccessor::PlaybackVendorPropertyAccessor(const std::shared_ptr<ISourcePort> &backend, IDevice *owner)
    : DeviceComponentBase(owner), port_(backend) {}

void PlaybackVendorPropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    // Playback device does not support write property
    (void)propertyId;
    (void)value;
    LOG_DEBUG("unsupported set property value for playback device, propertyId: {}", propertyId);
}

void PlaybackVendorPropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    auto playPort_ = std::dynamic_pointer_cast<PlaybackDevicePort>(port_);

    switch(propertyId) {
    case OB_PROP_ACCEL_ODR_INT:
    case OB_PROP_ACCEL_FULL_SCALE_INT: {
        const auto &profiles = playPort_->getStreamProfileList(OB_SENSOR_ACCEL);
        if(profiles.empty()) {
            LOG_WARN("No accelerometer profiles available");
            break;
        }
        auto accelStreamProfile = profiles.at(0)->as<AccelStreamProfile>();
        value->intValue         = (propertyId == OB_PROP_ACCEL_ODR_INT) ? static_cast<int>(accelStreamProfile->getSampleRate())
                                                                        : static_cast<int>(accelStreamProfile->getFullScaleRange());
    } break;
    case OB_PROP_GYRO_ODR_INT:
    case OB_PROP_GYRO_FULL_SCALE_INT: {
        const auto &profiles = playPort_->getStreamProfileList(OB_SENSOR_GYRO);
        if(profiles.empty()) {
            LOG_WARN("No gyroscope profiles available");
            break;
        }
        auto gyroStreamProfile = profiles.at(0)->as<GyroStreamProfile>();
        value->intValue        = (propertyId == OB_PROP_GYRO_ODR_INT) ? static_cast<int>(gyroStreamProfile->getSampleRate())
                                                                      : static_cast<int>(gyroStreamProfile->getFullScaleRange());
    } break;
    default:
        // v4l2 metadata property
        playPort_->getRecordedPropertyValue(propertyId, value);
        break;
    }
}

void PlaybackVendorPropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    auto playPort_ = std::dynamic_pointer_cast<PlaybackDevicePort>(port_);
    playPort_->getRecordedPropertyRange(propertyId, range);
}

void PlaybackVendorPropertyAccessor::setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) {
    (void)propertyId;
    (void)data;
    LOG_DEBUG("unsupported set structure data for playback device, propertyId: {}", propertyId);
}

const std::vector<uint8_t> &PlaybackVendorPropertyAccessor::getStructureData(uint32_t propertyId) {
    auto playPort_ = std::dynamic_pointer_cast<PlaybackDevicePort>(port_);

    switch(propertyId) {
    case OB_STRUCT_GET_ACCEL_PRESETS_ODR_LIST: {
        AccelSampleRateList *dataPtr = allocateData<AccelSampleRateList>(data_);

        // todo: check the vector is not empty
        dataPtr->values[0] = playPort_->getStreamProfileList(OB_SENSOR_ACCEL).at(0)->as<AccelStreamProfile>()->getSampleRate();
        dataPtr->num       = 1;
    } break;
    case OB_STRUCT_GET_ACCEL_PRESETS_FULL_SCALE_LIST: {
        AccelFullScaleRangeList *dataPtr = allocateData<AccelFullScaleRangeList>(data_);

        dataPtr->values[0] = playPort_->getStreamProfileList(OB_SENSOR_ACCEL).at(0)->as<AccelStreamProfile>()->getFullScaleRange();
        dataPtr->num       = 1;
    } break;
    case OB_STRUCT_GET_GYRO_PRESETS_ODR_LIST: {
        GyroSampleRateList *dataPtr = allocateData<GyroSampleRateList>(data_);

        dataPtr->values[0] = playPort_->getStreamProfileList(OB_SENSOR_GYRO).at(0)->as<GyroStreamProfile>()->getSampleRate();
        dataPtr->num       = 1;
    } break;
    case OB_STRUCT_GET_GYRO_PRESETS_FULL_SCALE_LIST: {
        GyroFullScaleRangeList *data = allocateData<GyroFullScaleRangeList>(data_);

        data->values[0] = playPort_->getStreamProfileList(OB_SENSOR_GYRO).at(0)->as<GyroStreamProfile>()->getFullScaleRange();
        data->num       = 1;
    } break;
    default: {
        data_ = playPort_->getRecordedStructData(propertyId);
    } break;
    }

    return data_;
}

PlaybackFilterPropertyAccessor::PlaybackFilterPropertyAccessor(const std::shared_ptr<ISourcePort> &backend, IDevice *owner) : port_(backend), owner_(owner) {}

void PlaybackFilterPropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    switch(propertyId) {
    case OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL:
    case OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_BOOL:
    case OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_SPECKLE_SIZE_INT:
    case OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_DIFF_INT: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->setPropertyValue(propertyId, value);
        LOG_DEBUG("Set property value for playback device, propertyId: {}, value: {}");
    } break;
    default: {
        LOG_WARN("unsupported get property value for playback device, propertyId: {}", propertyId);
    } break;
    }
}

void PlaybackFilterPropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    switch(propertyId) {
    case OB_PROP_DISPARITY_TO_DEPTH_BOOL: {
        auto playPort = std::dynamic_pointer_cast<PlaybackDevicePort>(port_);
        playPort->getRecordedPropertyValue(OB_PROP_DISPARITY_TO_DEPTH_BOOL, value);
    } break;
    case OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL:
    case OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_BOOL:
    case OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_SPECKLE_SIZE_INT:
    case OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_DIFF_INT: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->getPropertyValue(propertyId, value);
    } break;
    default: {
        LOG_WARN("unsupported get property value for playback device, propertyId: {}", propertyId);
    } break;
    }
}

void PlaybackFilterPropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    switch(propertyId) {
    case OB_PROP_DISPARITY_TO_DEPTH_BOOL: {
        auto playPort = std::dynamic_pointer_cast<PlaybackDevicePort>(port_);
        playPort->getRecordedPropertyRange(OB_PROP_DISPARITY_TO_DEPTH_BOOL, range);
    } break;
    case OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL:
    case OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_BOOL:
    case OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_SPECKLE_SIZE_INT:
    case OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_DIFF_INT: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->getPropertyRange(propertyId, range);
    } break;
    default: {
        LOG_WARN("unsupported get property value for playback device, propertyId: {}", propertyId);
    } break;
    }
}

// PlaybackFrameTransformPropertyAccessor
PlaybackFrameTransformPropertyAccessor::PlaybackFrameTransformPropertyAccessor(const std::shared_ptr<ISourcePort> &backend, IDevice *owner)
    : port_(backend), owner_(owner) {
    auto pid = owner_->getInfo()->pid_;
    if(std::find(G330DevPids.begin(), G330DevPids.end(), pid) != G330DevPids.end()
       || std::find(DaBaiADevPids.begin(), DaBaiADevPids.end(), pid) != DaBaiADevPids.end() || pid == 0x0815) {
        accessor_ = std::make_shared<StereoFrameTransformPropertyAccessor>(owner_); // Gemini330, Gemini435Led, DaBaiA
    }
    else if(pid == 0x0808 || pid == 0x0809) {
        accessor_ = std::make_shared<G210FrameTransformPropertyAccessor>(owner_);  // Gemini210
    }
    else if(pid == 0x0670 || pid == 0x0673) {
        accessor_ = std::make_shared<G2FrameTransformPropertyAccessor>(owner_);  // Gemini2
    }
    else if(std::find(Astra2DevPids.begin(), Astra2DevPids.end(), pid) != Astra2DevPids.end()) {
        accessor_ = std::make_shared<Astra2FrameTransformPropertyAccessor>(owner_); // Astar2
    }
    else if(std::find(FemtoBoltDevPids.begin(), FemtoBoltDevPids.end(), pid) != FemtoBoltDevPids.end()
            || std::find(FemtoMegaDevPids.begin(), FemtoMegaDevPids.end(), pid) != FemtoMegaDevPids.end()) {
        accessor_ = std::make_shared<MonocularFrameTransformPropertyAccessor>(owner_); // Femto
    }
    else {
        LOG_ERROR("Unsupport PlaybackFrameTransformPropertyAccessor, pid: {}", pid);
    }
}

void PlaybackFrameTransformPropertyAccessor::initFrameTransformProperty() {
    if(!accessor_) {
        return;
    }

    auto                  playPort_ = std::dynamic_pointer_cast<PlaybackDevicePort>(port_);
    std::vector<uint32_t> list      = { OB_PROP_COLOR_MIRROR_BOOL, OB_PROP_COLOR_FLIP_BOOL,    OB_PROP_COLOR_ROTATE_INT,     OB_PROP_DEPTH_FLIP_BOOL,
                                        OB_PROP_DEPTH_MIRROR_BOOL, OB_PROP_DEPTH_ROTATE_INT,   OB_PROP_IR_FLIP_BOOL,         OB_PROP_IR_MIRROR_BOOL,
                                        OB_PROP_IR_ROTATE_INT,     OB_PROP_IR_RIGHT_FLIP_BOOL, OB_PROP_IR_RIGHT_MIRROR_BOOL, OB_PROP_IR_RIGHT_ROTATE_INT };

    for(auto propertyId: list) {
        OBPropertyValue value;
        if(playPort_->getRecordedPropertyValue(propertyId, &value)) {
            accessor_->setPropertyValue(propertyId, value);
        }
    }
}

void PlaybackFrameTransformPropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    if(accessor_) {
        accessor_->setPropertyValue(propertyId, value);
    }
}

void PlaybackFrameTransformPropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    if(accessor_) {
        accessor_->getPropertyValue(propertyId, value);
    }
}

void PlaybackFrameTransformPropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    if(accessor_) {
        accessor_->getPropertyRange(propertyId, range);
    }
}

}  // namespace libobsensor