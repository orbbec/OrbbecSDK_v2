#pragma once
#include "DeviceBase.hpp"
#include "IDeviceManager.hpp"
#include "frameprocessor/FrameProcessor.hpp"

#include <map>
#include <memory>

namespace libobsensor {
class G2XLDeviceBase : public DeviceBase {
public:
    G2XLDeviceBase(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~G2XLDeviceBase() noexcept;

    void init() override;

    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type) override;

protected:
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);

protected:
    uint64_t deviceTimeFreq_ = 1000;
};

class G2XLUSBDevice : public G2XLDeviceBase {
public:
    G2XLUSBDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~G2XLUSBDevice() noexcept;

private:
    void init() override;
    void initSensorList();
    void initProperties();

private:
    const uint64_t frameTimeFreq_ = 1000000;
};

class G2XLNetDevice : public G2XLDeviceBase {
public:
    G2XLNetDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~G2XLNetDevice() noexcept;

private:
    void init() override;
    void initSensorList();
    void initProperties();
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);

    void fetchDeviceInfo() override;

private:
    const uint64_t frameTimeFreq_      = 1000;
    const uint64_t colorframeTimeFreq_ = 90000;
};

}  // namespace libobsensor
