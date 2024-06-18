#pragma once
#include "IDeviceEnumerator.hpp"
namespace libobsensor {
class NetDeviceEnumerator : public IDeviceEnumerator {
public:
    NetDeviceEnumerator(std::shared_ptr<ObPal> obPal, DeviceChangedCallback callback);
    virtual ~NetDeviceEnumerator() noexcept;
    virtual std::vector<std::shared_ptr<IDeviceEnumInfo>> getDeviceInfoList() override;
    virtual void                                          setDeviceChangedCallback(DeviceChangedCallback callback) override;

    static std::shared_ptr<IDevice> createDevice(std::shared_ptr<ObPal> obPal, std::string address, uint16_t port);

private:
    static std::vector<std::shared_ptr<IDeviceEnumInfo>> deviceInfoMatch(const SourcePortInfoList infoList);
    static std::shared_ptr<IDeviceEnumInfo>              associatedSourcePortCompletion(std::shared_ptr<ObPal> obPal, std::shared_ptr<IDeviceEnumInfo> info);

    void                                          onPalDeviceChanged(OBDeviceChangedType changeType, std::string devUid);
    std::vector<std::shared_ptr<IDeviceEnumInfo>> queryDeviceList();

private:
    std::mutex            deviceChangedCallbackMutex_;
    DeviceChangedCallback deviceChangedCallback_;
    // std::thread           devChangedCallbackThread_;

    std::recursive_mutex                          deviceInfoListMutex_;
    std::vector<std::shared_ptr<IDeviceEnumInfo>> deviceInfoList_;
    SourcePortInfoList                            sourcePortInfoList_;

    std::shared_ptr<DeviceWatcher> deviceWatcher_;
};
}  // namespace libobsensor