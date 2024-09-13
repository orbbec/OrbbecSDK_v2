#include "ObRTPClient.hpp"
#include "logger/Logger.hpp"

namespace libobsensor {

ObRTPClient::ObRTPClient() {}

ObRTPClient::~ObRTPClient() noexcept {}

void ObRTPClient::start(std::string localAddress, std::string address, uint16_t port, std::shared_ptr<const StreamProfile> profile,
                        MutableFrameCallback callback) {
    if(udpClient_) {
        udpClient_.reset();
    }

    if (!localAddress.empty()) {
        LOG_INFO("Local ip address: {}", localAddress);
    }
    
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
    udpClient_ = std::make_shared<ObRTPNpCapReceiver>(localAddress, address, port);
#else
    udpClient_ = std::make_shared<ObRTPUDPClient>(address, port);
#endif
    udpClient_->start(profile, callback);
}

void ObRTPClient::close() {
    if(udpClient_) {
        udpClient_->close();
        udpClient_.reset();
    }
}

}  // namespace libobsensor
