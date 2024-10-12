#pragma once

#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))

#include "ObPTPPacketCreator.hpp"
#include "pcap/pcap.h"
#include "ObPTPHost.hpp"
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>

namespace libobsensor {

class ObWinPTPHost : public ObPTPHost {
public:
    ObWinPTPHost(std::string localMac, std::string localIP, std::string address, uint16_t port, std::string mac);
    virtual ~ObWinPTPHost() noexcept;

    void timeSync() override;
    void destroy() override;

private:
    void convertMacAddress();
    void findDevice();
    void receivePTPPacket(pcap_t *handle);
    void sendPTPPacket(pcap_t *handle, void *data, int len);
    
private:
    bool     startSync_;
    uint32_t COMM_TIMEOUT_MS = 50;

    unsigned char destMac_[6]; // Destination MAC address
    unsigned char srcMac_[6];  // Source MAC address

    int t1[2];

    pcap_if_t *alldevs_;
    pcap_t *   handle_;
};

}  // namespace libobsensor

#endif