#if(!defined(WIN32))

#include "ObLinuxPTPHost.hpp"
#include "logger/Logger.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/Utils.hpp"
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <net/if.h>
#include <sys/time.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <cstring>

namespace libobsensor {

#define PTP_ETHERTYPE 0x88F7
#define OB_PTP_BACK_SIZE 60
#define OB_PTP_BUFFER_SIZE 1600

ObLinuxPTPHost::ObLinuxPTPHost(std::string localMac, std::string localIP, std::string address, uint16_t port, std::string mac)
    : ObPTPHost(localMac, localIP, address, port, mac), ptpSocket_(INVALID_SOCKET), startSync_(false), ifIndex_(0) {
    convertMacAddress();
    createSokcet();
}

ObLinuxPTPHost::~ObLinuxPTPHost() noexcept {
    destroy();
}

void ObLinuxPTPHost::convertMacAddress() {
    std::istringstream dstMac(serverMac_);
    std::string        item;
    int                i = 0;
    while(std::getline(dstMac, item, ':') && i < 6) {
        destMac_[i] = static_cast<unsigned char>(std::stoi(item, nullptr, 16));
        i++;
    }

    std::istringstream srcMac(localMac_);
    std::string        item2;
    int                j = 0;
    while(std::getline(srcMac, item2, ':') && j < 6) {
        srcMac_[j] = static_cast<unsigned char>(std::stoi(item2, nullptr, 16));
        j++;
    }

    ptpPacketCreator_.setMacAddress(destMac_, srcMac_);
}

void ObLinuxPTPHost::createSokcet() {
    ptpSocket_ = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if(ptpSocket_ < 0) {
        LOG_ERROR("PTP socket create failed,error: {}", strerror(errno));
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "PTP socket create failed!");
    }

    struct ifaddrs *ifaddr, *ifa;
    int             family;
    if(getifaddrs(&ifaddr) == -1) {
        LOG_ERROR("PTP socket getifaddrs failed,error: {}", strerror(errno));
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "PTP getifaddrs failed!");
    }

    for(ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if(ifa->ifa_addr == nullptr)
            continue;

        family = ifa->ifa_addr->sa_family;
        if(family == AF_INET) {
            LOG_DEBUG("getnameinfo-name: {}", ifa->ifa_name);
            struct ifreq ifr;
            std::memset(&ifr, 0, sizeof(ifr));
            std::strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ - 1);

            if(ioctl(ptpSocket_, SIOCGIFHWADDR, &ifr) >= 0) {
                unsigned char     *mac = reinterpret_cast<unsigned char *>(ifr.ifr_hwaddr.sa_data);
                std::ostringstream macAddressStream;
                for(int i = 0; i < 6; ++i) {
                    macAddressStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mac[i]);
                    if(i < 5) {
                        macAddressStream << ":";
                    }
                }

                std::string macAddress = macAddressStream.str();
                if(macAddress == localMac_) {
                    ifIndex_ = if_nametoindex(ifa->ifa_name);
                    LOG_DEBUG("getnameinfo-ifIndex: {}", ifIndex_);
                    break;
                }
            }
        }
    }

    freeifaddrs(ifaddr);

    if(ifIndex_ == -1) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Error opening ptp net device, not found!");
    }

    LOG_DEBUG("PTP net socket create successfully.");
}

void ObLinuxPTPHost::timeSync() {
    if(startSync_) {
        LOG_WARN("The PTP data receive has been started!");
        return;
    }

    if(ptpSocket_ < 0) {
        LOG_ERROR("PTP time synchronization failure, socket craete failed!");
        return;
    }

    startSync_ = true;

    // SYNC_CONTROL
    Frame1588 syncControlFrame;
    int       len = ptpPacketCreator_.createPTPPacket(SYNC_CONTROL, &syncControlFrame);
    t1[0]         = syncControlFrame.txSeconds;
    t1[1]         = syncControlFrame.txNanoSeconds;
    sendPTPPacket(&syncControlFrame, len);

    // FOLLOW_UP_CONTROL
    Frame1588 followUpControlControlFrame;
    len                                       = ptpPacketCreator_.createPTPPacket(FOLLOW_UP_CONTROL, &followUpControlControlFrame);
    followUpControlControlFrame.txSeconds     = t1[0];
    followUpControlControlFrame.txNanoSeconds = t1[1];
    sendPTPPacket(&followUpControlControlFrame, len);

    // Receive delay request
    receivePTPPacket();

    startSync_ = false;
}

void ObLinuxPTPHost::sendPTPPacket(void *data, int len) {
    struct sockaddr_ll socketAddress;
    memset(&socketAddress, 0, sizeof(socketAddress));
    socketAddress.sll_family   = AF_PACKET;
    socketAddress.sll_protocol = htons(PTP_ETHERTYPE);
    socketAddress.sll_ifindex  = ifIndex_;
    int status                 = sendto(ptpSocket_, data, len, 0, (sockaddr *)&socketAddress, sizeof(socketAddress));
    if(status < 0) {
        LOG_ERROR("PTP time synchronization send data failed,error:{}", strerror(errno));
    }
}

void ObLinuxPTPHost::receivePTPPacket() {
    LOG_DEBUG("start ptp data receive...");
    int                maxRevCount = 20;
    unsigned char      buffer[OB_PTP_BUFFER_SIZE];
    struct sockaddr_ll socketAddress;
    memset(&socketAddress, 0, sizeof(socketAddress));
    socketAddress.sll_family   = AF_PACKET;
    socketAddress.sll_protocol = htons(PTP_ETHERTYPE);
    socketAddress.sll_ifindex  = ifIndex_;
    socklen_t addrLen          = sizeof(socketAddress);

    while(startSync_) {
        if(maxRevCount == 0) {
            break;
        }

        int recvLen = recvfrom(ptpSocket_, buffer, sizeof(buffer), 0, (sockaddr *)&socketAddress, &addrLen);
        if(recvLen > 0) {
            if(recvLen == OB_PTP_BACK_SIZE) {
                LOG_DEBUG("Receive PTP packet!");
                bool       equalMAC = true;
                Frame1588 *ptpdata  = (Frame1588 *)buffer;
                if(ptpdata->transportSpecificAndMessageType == DELAY_REQ_MSSID) {
                    LOG_DEBUG("Receive ptp delay req message packet.");
                    for(int i = 6; i < 12; i++) {
                        if(destMac_[i - 6] != ptpdata->macdata[i]) {
                            equalMAC = false;
                        }
                    }
                }
                else {
                    equalMAC = false;
                }

                if(equalMAC) {
                    LOG_DEBUG("Send ptp delay resp control packet.");
                    Frame1588 delayRespControlFrame;
                    int       len = ptpPacketCreator_.createPTPPacket(DELAY_RESP_CONTROL, &delayRespControlFrame);
                    sendPTPPacket(&delayRespControlFrame, len);
                    LOG_DEBUG("Send ptp delay resp control finished.");
                    break;
                }
            }
        }
        else {
            if(recvLen == 0) {
                LOG_ERROR_INTVL("Receive ptp packet timeout!");
            }
            else {
                LOG_ERROR_INTVL("Receive ptp packet error: {}", recvLen);
            }
        }
        maxRevCount--;
    }

    LOG_DEBUG("Exit ptp data receive...");
}

void ObLinuxPTPHost::destroy() {
    LOG_DEBUG("close start...");
    startSync_ = false;
    if(ptpSocket_ > 0) {
        auto rst = ::closesocket(ptpSocket_);
        if(rst < 0) {
            LOG_ERROR("close udp socket failed! socket={0}, err_code={1}", ptpSocket_, GET_LAST_ERROR());
        }
    }
    LOG_DEBUG("ptp socket closed!");
    ptpSocket_ = INVALID_SOCKET;
    LOG_DEBUG("close end...");
}

}  // namespace libobsensor

#endif