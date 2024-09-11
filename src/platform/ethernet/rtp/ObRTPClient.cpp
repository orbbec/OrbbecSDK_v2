#include "ObRTPClient.hpp"

namespace libobsensor {

ObRTPClient::ObRTPClient() {}

ObRTPClient::~ObRTPClient() noexcept {}

void ObRTPClient::start(std::string localAddress, std::string address, uint16_t port, std::shared_ptr<const StreamProfile> profile,
                        MutableFrameCallback callback) {
    if(udpClient_) {
        udpClient_.reset();
    }
    //udpClient_ = std::make_shared<ObRTPUDPClient>(address, port);
    udpClient_ = std::make_shared<ObRTPNpCapReceiver>(localAddress, address, port);
    udpClient_->start(profile, callback);
}

void ObRTPClient::close() {
    if(udpClient_) {
        udpClient_->close();
        udpClient_.reset();
    }
}

}  // namespace libobsensor
