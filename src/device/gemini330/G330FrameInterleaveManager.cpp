#include "G330FrameInterleaveManager.hpp"
#include "property/InternalProperty.hpp"
#include "InternalTypes.hpp"
#include "exception/ObException.hpp"
#include "utils/Utils.hpp"

namespace libobsensor {
const std::string hdr_interleave   = "Depth from HDR";
const std::string laser_interleave = "Laser On-Off";
G330FrameInterleaveManager::G330FrameInterleaveManager(IDevice *owner) : DeviceComponentBase(owner) {
    availableFrameInterleaves_.emplace_back(hdr_interleave);
    availableFrameInterleaves_.emplace_back(laser_interleave);

    currentIndex_ = -1;

    hdrDefault_[0].depthExposureTime = 7500;
    hdrDefault_[0].depthGain         = 16;
    hdrDefault_[0].depthBrightness   = 60;
    hdrDefault_[0].depthMaxExposure  = 10000;
    hdrDefault_[0].laserSwitch       = 1;

    hdrDefault_[1].depthExposureTime = 1;
    hdrDefault_[1].depthGain         = 16;
    hdrDefault_[1].depthBrightness   = 20;
    hdrDefault_[1].depthMaxExposure  = 2000;
    hdrDefault_[1].laserSwitch       = 1;

    memcpy(hdr_, hdrDefault_, sizeof(hdrDefault_));
    laserInterleaveDefault_[0].depthExposureTime = 3000;
    laserInterleaveDefault_[0].depthGain         = 16;
    laserInterleaveDefault_[0].depthBrightness   = 60;
    laserInterleaveDefault_[0].depthMaxExposure  = 30000;
    laserInterleaveDefault_[0].laserSwitch       = 1;

    laserInterleaveDefault_[1].depthExposureTime = 3000;
    laserInterleaveDefault_[1].depthGain         = 16;
    laserInterleaveDefault_[1].depthBrightness   = 60;
    laserInterleaveDefault_[1].depthMaxExposure  = 30000;
    laserInterleaveDefault_[1].laserSwitch       = 0;

    memcpy(laserInterleave_, laserInterleaveDefault_, sizeof(hdrDefault_));

    auto propServer = owner->getPropertyServer();

    propServer->registerAccessCallback(
        {
            OB_PROP_DEPTH_EXPOSURE_INT,
            OB_PROP_DEPTH_GAIN_INT,
            OB_PROP_IR_BRIGHTNESS_INT,
            OB_PROP_IR_AE_MAX_EXPOSURE_INT,
            OB_PROP_FRAME_INTERLEAVE_CONFIG_INDEX_INT,
            OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL,
            OB_PROP_LASER_CONTROL_INT,
        },
        [&](uint32_t propertyId, const uint8_t *, size_t, PropertyOperationType operationType) {
            if(operationType == PROP_OP_WRITE) {
                updateFrameInterleaveParam(propertyId);
            }
        });
}

template <typename T> void setPropertyValue(IDevice *dev, uint32_t propertyId, T value) {
    // get and release property server on this scope to avoid handle device resource lock for an extended duration
    auto propServer = dev->getPropertyServer();
    return propServer->setPropertyValueT<T>(propertyId, value);
}

template <typename T> T getPropertyValue(IDevice *dev, uint32_t propertyId) {
    // get and release property server on this scope to avoid handle device resource lock for an extended duration
    auto propServer = dev->getPropertyServer();
    return propServer->getPropertyValueT<T>(propertyId);
}

void G330FrameInterleaveManager::loadFrameInterleave(const std::string &frameInterleaveName) {
    if(std::find(availableFrameInterleaves_.begin(), availableFrameInterleaves_.end(), frameInterleaveName) == availableFrameInterleaves_.end()) {
        throw std::invalid_argument("Invalid frame interleave name: " + frameInterleaveName);
    }
    auto owner = getOwner();
    for(int i = 1; i >= 0; i--) {
        setPropertyValue(owner, OB_PROP_FRAME_INTERLEAVE_CONFIG_INDEX_INT, i);

        auto setProperties = [&](const FrameInterleaveParam *interleave, int sequenceId) {
            setPropertyValue(owner, OB_PROP_DEPTH_EXPOSURE_INT, interleave[sequenceId].depthExposureTime);
            setPropertyValue(owner, OB_PROP_DEPTH_GAIN_INT, interleave[sequenceId].depthGain);

            setPropertyValue(owner, OB_PROP_IR_BRIGHTNESS_INT, interleave[sequenceId].depthBrightness);
            setPropertyValue(owner, OB_PROP_IR_AE_MAX_EXPOSURE_INT, interleave[sequenceId].depthMaxExposure);
            setPropertyValue(owner, OB_PROP_LASER_CONTROL_INT, interleave[sequenceId].laserSwitch);
        };

        if(frameInterleaveName == hdr_interleave) {
            setProperties(hdr_, i);
        }
        else if(frameInterleaveName == laser_interleave) {
            setProperties(laserInterleave_, i);
        }
    }

    currentFrameInterleave_ = frameInterleaveName;
}

const std::vector<std::string> &G330FrameInterleaveManager::getAvailableFrameInterleaveList() const {
    return availableFrameInterleaves_;
}

void G330FrameInterleaveManager::updateFrameInterleaveParam(uint32_t propertyId) {
    auto owner = getOwner();

    if(propertyId == OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL) {
        auto enable = getPropertyValue<bool>(owner, OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL);
        if(!enable) {
            currentIndex_ = -1;
        }
    }

    if(propertyId == OB_PROP_FRAME_INTERLEAVE_CONFIG_INDEX_INT) {
        currentIndex_ = getPropertyValue<int>(owner, OB_PROP_FRAME_INTERLEAVE_CONFIG_INDEX_INT);
        // std::cout << "current index:" << currentIndex_ << std::endl;
    }

    if(currentIndex_ < 0 || currentIndex_ > 1) {
        return;
    }

    auto updateProperty = [&](FrameInterleaveParam *interleave) {
        switch(propertyId) {
        case OB_PROP_DEPTH_EXPOSURE_INT:
            interleave[currentIndex_].depthExposureTime = getPropertyValue<int>(owner, OB_PROP_DEPTH_EXPOSURE_INT);
            break;
        case OB_PROP_DEPTH_GAIN_INT:
            interleave[currentIndex_].depthGain = getPropertyValue<int>(owner, OB_PROP_DEPTH_GAIN_INT);
            break;
        case OB_PROP_IR_BRIGHTNESS_INT:
            interleave[currentIndex_].depthBrightness = getPropertyValue<int>(owner, OB_PROP_IR_BRIGHTNESS_INT);
            break;
        case OB_PROP_IR_AE_MAX_EXPOSURE_INT:
            interleave[currentIndex_].depthMaxExposure = getPropertyValue<int>(owner, OB_PROP_IR_AE_MAX_EXPOSURE_INT);
            break;
        case OB_PROP_LASER_CONTROL_INT:
            interleave[currentIndex_].laserSwitch = getPropertyValue<int>(owner, OB_PROP_LASER_CONTROL_INT);
            break;
        default:
            break;
        }
    };

    if(currentFrameInterleave_ == hdr_interleave) {
        updateProperty(hdr_);
    }
    else if(currentFrameInterleave_ == laser_interleave) {
        updateProperty(laserInterleave_);
    }
}

}  // namespace libobsensor