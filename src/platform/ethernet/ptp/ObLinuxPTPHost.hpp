#pragma once

#if(!defined(WIN32))

#include "ObPTPHost.hpp"
#include "ethernet/socket/SocketTypes.hpp"
#include <string>
#include <linux/if_packet.h>

namespace libobsensor {

struct ob_sock_fprog {	/* Required for SO_ATTACH_FILTER. */
	unsigned short		len;	/* Number of filter blocks */
	struct sock_filter *filter;
};

class ObLinuxPTPHost : public ObPTPHost {
public:
    ObLinuxPTPHost(std::string localMac, std::string localIP, std::string address, uint16_t port, std::string mac);
    virtual ~ObLinuxPTPHost() noexcept;

    void timeSync() override;
    void destroy() override;

    void setPTPTimeSyncCallback(PTPTimeSyncCallback callback) override;

private:
    void convertMacAddress();
    void createSokcet();
    void receivePTPPacket();
    void sendPTPPacket(void *data, int len);
    
private:
    int ptpSocket_;

    bool     startSync_;
    uint32_t COMM_TIMEOUT_MS = 50;

    int		ifIndex_;

    unsigned char destMac_[6]; // Destination MAC address
    unsigned char srcMac_[6];  // Source MAC address

    int t1[2];

    struct sockaddr_ll socketAddress_;

    std::thread receiverThread_;

};

}  // namespace libobsensor

#endif