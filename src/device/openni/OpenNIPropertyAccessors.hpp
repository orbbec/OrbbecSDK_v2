// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IProperty.hpp"
#include "IDevice.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>

namespace libobsensor {
class OpenNIDisp2DepthPropertyAccessor : public IBasicPropertyAccessor, public IStructureDataAccessor {
public:
    explicit OpenNIDisp2DepthPropertyAccessor(IDevice *owner);
    virtual ~OpenNIDisp2DepthPropertyAccessor() noexcept = default;

    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

    void                        setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) override;
    const std::vector<uint8_t> &getStructureData(uint32_t propertyId) override;

private:
    void markOutputDisparityFrame(bool enable);

private:
    IDevice *owner_; 

    bool swDisparityToDepthEnabled_;

    int32_t                     currentDepthUnitLevel_;
    const std::vector<uint16_t> swD2DSupportList_ = { OB_PRECISION_1MM, OB_PRECISION_0MM8, OB_PRECISION_0MM4, OB_PRECISION_0MM2, OB_PRECISION_0MM1 };
};



class OpenNIFrameTransformPropertyAccessor : public IBasicPropertyAccessor {
public:
    explicit OpenNIFrameTransformPropertyAccessor(IDevice *owner);
    virtual ~OpenNIFrameTransformPropertyAccessor() noexcept = default;

    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

private:
    IDevice *owner_;
};



class OpenNIHeartBeatPropertyAccessor : public IBasicPropertyAccessor {
public:
    explicit OpenNIHeartBeatPropertyAccessor(IDevice *owner);
    virtual ~OpenNIHeartBeatPropertyAccessor() noexcept;

    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

private:
    void startFeedingWatchDog();
    void stopFeedingWatchDog();

private:
    uint32_t OB_DEFAULT_HEARTBEAT_TIMEOUT = 3000;

    IDevice *owner_;

    bool heartBeatStatus_;

    std::mutex              heartBeatMutex_;
    bool                    heartBeatRunning_;
    std::thread             heartBeatThread_;
    std::condition_variable heartBeatCV_;
};



class OpenNITemperatureStructurePropertyAccessor : public IStructureDataAccessor {
public:
    explicit OpenNITemperatureStructurePropertyAccessor(IDevice *owner);
    virtual ~OpenNITemperatureStructurePropertyAccessor() noexcept = default;

    void                        setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) override;
    const std::vector<uint8_t> &getStructureData(uint32_t propertyId) override;

private:
    IDevice *owner_;
};

}  // namespace libobsensor
