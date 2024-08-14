#pragma once

#include "IStreamProfile.hpp"
#include "IFrame.hpp"
#include "ethernet/socket/SocketTypes.hpp"

#include <string>
#include <thread>

namespace libobsensor {

class ObRTPUDPClient {
public:
    ObRTPUDPClient(std::string address, uint16_t port);
    ~ObRTPUDPClient() noexcept;

    void start(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback);
    void close();

private:
    void socketConnect();
    void startReceive();
    void frameProcess();
    
private:
    std::string serverIp_;
    uint16_t    serverPort_;
    bool        startReceive_;
    SOCKET      recvSocket_;
    uint32_t    COMM_TIMEOUT_MS = 5000;

    std::shared_ptr<const StreamProfile> currentProfile_;
    MutableFrameCallback                 currentCallback_;

    std::thread receiverThread_;
    std::thread callbackThread_;
};

}  // namespace libobsensor
