// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IPal.hpp"
#include "IDeviceWatcher.hpp"
#include "ISourcePort.hpp"
#include "VendorNetDataPort.hpp"
#include "RTSPStreamPort.hpp"
#include "NetDataStreamPort.hpp"
#include "gige/GVCPClient.hpp"
#include "mDNS/MDNSDiscovery.hpp"

#include <vector>
#include <map>
#include <mutex>
#include <atomic>

namespace libobsensor {
class EthernetPal : virtual public IPal, virtual public IDeviceWatcher, public std::enable_shared_from_this<EthernetPal> {
public:
    EthernetPal();
    virtual ~EthernetPal() noexcept override;

    // IDeviceWatcher interface methods
    virtual void start(deviceChangedCallback callback) override;
    virtual void stop() override;

    // IPal interface methods
    std::shared_ptr<ISourcePort>    getSourcePort(std::shared_ptr<const SourcePortInfo>) override;
    SourcePortInfoList              querySourcePortInfos() override;
    std::shared_ptr<IDeviceWatcher> createDeviceWatcher() const override;

    static bool forceIpConfig(std::string macAddress, const OBNetIpConfig &config);

private:
    void updateSourcePortInfoList(const std::vector<GVCPDeviceInfo> &added, const std::vector<GVCPDeviceInfo> &removed);
    void updateMDNSDeviceSourceInfo(const std::vector<MDNSDeviceInfo> &added, const std::vector<MDNSDeviceInfo> &removed);

private:
    std::mutex         sourcePortInfoMetux_;
    SourcePortInfoList sourcePortInfoList_;

    std::mutex                                                                  sourcePortMapMutex_;
    std::map<std::shared_ptr<const SourcePortInfo>, std::weak_ptr<ISourcePort>> sourcePortMap_;

    // for device watcher
    deviceChangedCallback       callback_;
    std::thread                 deviceWatchThread_;
    std::atomic<bool>           stopWatch_{ false };
    std::vector<GVCPDeviceInfo> netDevInfoList_;
    std::condition_variable     condVar_;

    // mDNS
    std::vector<MDNSDeviceInfo>    mdnsDevInfoList_;
    std::shared_ptr<MDNSDiscovery> mdnsDiscovery_;
    std::thread                    mdnsWatchThread_;
    std::condition_variable        mdnsCondVar_;
};

}  // namespace libobsensor
