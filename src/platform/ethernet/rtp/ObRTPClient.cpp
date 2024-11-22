#include "ObRTPClient.hpp"
#include "logger/Logger.hpp"

namespace libobsensor {

ObRTPClient::ObRTPClient() {}

ObRTPClient::~ObRTPClient() noexcept {
    close();
}

void ObRTPClient::init(std::string localAddress, std::string address, uint16_t port) {
    if(udpClient_) {
        udpClient_.reset();
    }

#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
    udpClient_ = std::make_shared<ObRTPNpCapReceiver>(localAddress, address, port);
#else
    udpClient_ = std::make_shared<ObRTPUDPClient>(address, port);
#endif

}

void ObRTPClient::start(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    if(udpClient_) {
        udpClient_->start(profile, callback);
    }
}

uint16_t ObRTPClient::getPort() {
    if(udpClient_) {
        return udpClient_->getPort();
    }
    return 0;
}

void ObRTPClient::stop() {
    if(udpClient_) {
        udpClient_->stop();
    }
}

void ObRTPClient::close() {
    if(udpClient_) {
        udpClient_->close();
        udpClient_.reset();
    }
}

}  // namespace libobsensor
