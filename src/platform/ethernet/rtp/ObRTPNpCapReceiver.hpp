#pragma once

#include "IStreamProfile.hpp"
#include "IFrame.hpp"
#include "ObRTPPacketProcessor.hpp"
#include "ObRTPPacketQueue.hpp"
#include "pcap/pcap.h"

#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace libobsensor {

class ObRTPNpCapReceiver {
public:
    ObRTPNpCapReceiver(std::string address, uint16_t port);
    ~ObRTPNpCapReceiver() noexcept;

    void start(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback);
    void close();

private:
    void findAlldevs();
    void startReceive();
    void frameProcess();
    
private:
    std::string serverIp_;
    uint16_t    serverPort_;
    bool        startReceive_;
    uint32_t    COMM_TIMEOUT_MS = 5000;

    pcap_if_t *alldevs_;
    pcap_if_t *dev_;
    pcap_t *   handle_;

    std::shared_ptr<const StreamProfile> currentProfile_;
    MutableFrameCallback                 frameCallback_;

    std::thread receiverThread_;
    std::thread callbackThread_;

    ObRTPPacketQueue    rtpQueue_;
    ObRTPPacketProcessor rtpProcessor_;

    std::mutex              rtpPacketMutex_;
    std::condition_variable packetAvailableCv_;
};

}  // namespace libobsensor
