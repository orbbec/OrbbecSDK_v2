#pragma once

#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))

#include "ObPTPPacketCreator.hpp"
#include "pcap/pcap.h"
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>

namespace libobsensor {

class ObPTPHost {
public:
    ObPTPHost(std::string localMac, std::string localIP, std::string address, uint16_t port, std::string mac);
    ~ObPTPHost() noexcept;

    void timeSync();
    void destroy();

private:
    void convertMacAddress();
    void findDevice();
    void receivePTPPacket(pcap_t *handle);
    void sendPTPPacket(pcap_t *handle, void *data, int len);
    
private:
    ObPTPPacketCreator ptpPacketCreator_;

    std::string localMac_;
    std::string serverIp_;
    uint16_t    serverPort_;
    std::string serverMac_;
    std::string localIp_;
    bool        startSync_;
    uint32_t    COMM_TIMEOUT_MS = 50;

    unsigned char destMac_[6]; // Destination MAC address
    unsigned char srcMac_[6];  // Source MAC address

    int t1[2];

    pcap_if_t *alldevs_;
    pcap_t *   handle_;
};

}  // namespace libobsensor

#endif
