#pragma once

#include "IStreamProfile.hpp"
#include "IFrame.hpp"
#include "ethernet/socket/SocketTypes.hpp"
#include "ObRTPPacketProcessor.hpp"
#include "ObRTPPacketQueue.hpp"

#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace libobsensor {

class ObRTPUDPClient {
public:
    ObRTPUDPClient(std::string localAddress, std::string address, uint16_t port);
    ~ObRTPUDPClient() noexcept;

    void     start(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback);
    void     stop();
    void     close();
    uint16_t getPort();

private:
    void socketConnect();
    void socketClose();
    void frameReceive();
    void frameProcess();
    
private:
    std::string       localIp_;
    std::string       serverIp_;
    uint16_t          serverPort_;
    std::atomic<bool> startReceive_;
    SOCKET            recvSocket_;
    uint32_t          COMM_TIMEOUT_MS = 100;

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
