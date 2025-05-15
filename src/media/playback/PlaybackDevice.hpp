// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "DeviceBase.hpp"
#include "PlaybackDevicePort.hpp"
#include "PlaybackPropertyAccessor.hpp"
#include "component/property/PropertyServer.hpp"

#include <string>
#include <memory>

namespace libobsensor {

typedef std::function<bool()> ConditionCheckHandler;
class PlaybackDevice : public DeviceBase {
public:
    PlaybackDevice(const std::string &filePath);
    virtual ~PlaybackDevice() noexcept override;

    void pause();
    void resume();

    void     seek(const uint64_t timestamp);
    void     setPlaybackRate(const float rate);
    uint64_t getDuration() const;
    uint64_t getPosition() const;

    void             setPlaybackStatusCallback(const PlaybackStatusCallback callback);
    OBPlaybackStatus getCurrentPlaybackStatus() const;

    void fetchProperties();

public:
    virtual void              fetchDeviceInfo() override;
    virtual void              fetchExtensionInfo() override;
    std::vector<OBSensorType> getSensorTypeList() const override;

    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type) override;

private:
    void init() override;
    void initSensorList();
    void initProperties();

    bool isDeviceInSeries(const std::vector<uint16_t> &pids, const uint16_t &pid);

    void registerPropertyCondition(std::shared_ptr<PropertyServer> server, uint32_t propertyId, const std::string &userPermsStr, const std::string &intPermsStr,
                                   std::shared_ptr<IPropertyAccessor> accessor, ConditionCheckHandler condition = nullptr, bool skipSupportCheck = false);

private:
    const std::string                   filePath_;
    std::shared_ptr<PlaybackDevicePort> port_;
    std::vector<OBSensorType>           sensorTypeList_;

    std::shared_ptr<PlaybackFrameTransformPropertyAccessor> frameTransformAccessor_;

    const uint32_t G330DeviceTimeFreq_ = 1000;     // in ms
    const uint32_t G330FrameTimeFreq_  = 1000000;  // in us
};
}  // namespace libobsensor