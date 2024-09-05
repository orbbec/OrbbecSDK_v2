#pragma once

#include "IPal.hpp"
#include "IDeviceWatcher.hpp"
#include "ISourcePort.hpp"
#include "VendorNetDataPort.hpp"
#include "RTSPStreamPort.hpp"
#include "NetDataStreamPort.hpp"
#include "gige/GVCPClient.hpp"

#include <vector>
#include <map>

namespace libobsensor {

typedef std::vector<std::shared_ptr<NetSourcePortInfo>> NetSourcePortList;

class NetDeviceWatcher : public IDeviceWatcher {
public:
    virtual ~NetDeviceWatcher() noexcept;
    virtual void start(deviceChangedCallback callback) override;
    virtual void stop() override;

private:
    deviceChangedCallback       callback_;
    std::thread                 deviceWatchThread_;
    bool                        stopWatch_ = false;
    std::vector<GVCPDeviceInfo> netDevInfoList_;
    std::condition_variable     condVar_;
};

class EthernetPal : public IPal {
public:
    EthernetPal() {}
    ~EthernetPal() = default;

    SourcePortInfoList queryAssociatedNetSourcePort(const std::shared_ptr<NetSourcePortInfo> info);

    bool changeNetDeviceIpConfig(std::string ip, const OBNetIpConfig &config);

    std::shared_ptr<ISourcePort>    getSourcePort(std::shared_ptr<const SourcePortInfo>) override;
    SourcePortInfoList              querySourcePortInfos() override;
    std::shared_ptr<IDeviceWatcher> createDeviceWatcher() const override {
        return std::make_shared<NetDeviceWatcher>();
    }

private:
    std::vector<GVCPDeviceInfo> netDeviceInfoList_;
    SourcePortInfoList          sourcePortInfoList_;

    std::mutex                                                                  sourcePortMapMutex_;
    std::map<std::shared_ptr<const SourcePortInfo>, std::weak_ptr<ISourcePort>> sourcePortMap_;
};

}  // namespace libobsensor
