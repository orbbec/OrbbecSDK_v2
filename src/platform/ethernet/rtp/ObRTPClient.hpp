#pragma once

#include "IStreamProfile.hpp"
#include "IFrame.hpp"
#include "ObRTPUDPClient.hpp"
#include "ObRTPNpCapReceiver.hpp"

namespace libobsensor {

class ObRTPClient {
public:
    ObRTPClient();
    ~ObRTPClient() noexcept;

    void start(std::string address, uint16_t port, std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback);
    void close();

private:
    //std::shared_ptr<ObRTPUDPClient> udpClient_;
    std::shared_ptr<ObRTPNpCapReceiver> udpClient_;

};

}  // namespace libobsensor