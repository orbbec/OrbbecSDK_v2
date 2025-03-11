#pragma once

#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))

#include "IStreamProfile.hpp"
#include "IFrame.hpp"
#include "ObRTPPacketProcessor.hpp"
#include "ObRTPPacketQueue.hpp"
#include "ethernet/socket/SocketTypes.hpp"
#include "pcap/pcap.h"

#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace libobsensor {

class ObRTPNpCapReceiver {
public:
    ObRTPNpCapReceiver(std::string localAddress, std::string address, uint16_t port);
    ~ObRTPNpCapReceiver() noexcept;

    void     start(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback);
    void     stop();
    void     close();
    uint16_t getPort();

private:
    uint16_t getAvailableUdpPort();
    bool     isPortInUse(uint16_t port);
    
    void     findAlldevs();
    void     parseIPAddress(const u_char *ip_header, std::string &src_ip, std::string &dst_ip);
    void     frameReceive();
    void     frameProcess();

    void frameReceive2(pcap_t *handle);
    
private:
    std::string       localIp_;
    std::string       serverIp_;
    uint16_t          serverPort_;
    std::atomic<bool> startReceive_;
    uint32_t          COMM_TIMEOUT_MS = 100;

    pcap_if_t *alldevs_;
    pcap_if_t *dev_;
    pcap_t *   handle_;

    std::shared_ptr<const StreamProfile> currentProfile_;
    MutableFrameCallback                 frameCallback_;

    std::thread receiverThread_;
    std::thread callbackThread_;

    std::vector<pcap_t *>    handles_;
    std::vector<std::thread> handleThreads_;
    bool                     foundPcapHandle_;

    ObRTPPacketQueue    rtpQueue_;
    ObRTPPacketProcessor rtpProcessor_;

    std::mutex rtpPacketMutex_;

    SOCKET   recvSocket_;
};

//==============================================================================================

#if 0

class ObRTPNpCapReceiver {
public:
    ObRTPNpCapReceiver(std::string localAddress, std::string address, uint16_t port);
    ~ObRTPNpCapReceiver() noexcept;

    void     start(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback);
    void     stop();
    void     close();
    uint16_t getPort();

private:
    uint16_t getAvailableUdpPort();
    bool     isPortInUse(uint16_t port);

    void findAlldevs();
    void parseIPAddress(const u_char *ip_header, std::string &src_ip, std::string &dst_ip);
    void frameReceive();
    void frameProcess();

    void frameReceive2(pcap_t *handle);

private:
    std::string       localIp_;
    std::string       serverIp_;
    uint16_t          serverPort_;
    std::atomic<bool> startReceive_;
    uint32_t          COMM_TIMEOUT_MS = 100;

    pcap_if_t *alldevs_;
    pcap_if_t *dev_;
    pcap_t *   handle_;

    std::shared_ptr<const StreamProfile> currentProfile_;
    MutableFrameCallback                 frameCallback_;

    std::thread receiverThread_;
    std::thread callbackThread_;

    std::vector<pcap_t *>    handles_;
    std::vector<std::thread> handleThreads_;
    bool                     foundPcapHandle_;

    ObRTPPacketQueue     rtpQueue_;
    ObRTPPacketProcessor rtpProcessor_;

    std::mutex rtpPacketMutex_;

    SOCKET recvSocket_;
};

#endif  // 0

}  // namespace libobsensor

#endif