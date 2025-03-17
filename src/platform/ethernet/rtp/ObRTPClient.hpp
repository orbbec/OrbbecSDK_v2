#pragma once

#include "IStreamProfile.hpp"
#include "IFrame.hpp"
#include "ObRTPUDPClient.hpp"

namespace libobsensor {

class ObRTPClient {
public:
    ObRTPClient();
    ~ObRTPClient() noexcept;

    void     init(std::string localAddress, std::string address, uint16_t port);
    void     start(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback);
    uint16_t getPort();
    void     stop();
    void     close();

private:
    std::shared_ptr<ObRTPUDPClient> udpClient_;

};

}  // namespace libobsensor