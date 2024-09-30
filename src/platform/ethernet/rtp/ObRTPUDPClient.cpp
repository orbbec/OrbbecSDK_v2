#include "ObRTPUDPClient.hpp"
#include "logger/Logger.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/Utils.hpp"
#include "frame/FrameFactory.hpp"
#include "stream/StreamProfile.hpp"

#define OB_UDP_BUFFER_SIZE 1500

namespace libobsensor {

ObRTPUDPClient::ObRTPUDPClient(std::string address, uint16_t port) : serverIp_(address), serverPort_(port), startReceive_(false), recvSocket_(INVALID_SOCKET) {

#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
    WSADATA wsaData;
    int     rst = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if(rst != 0) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Failed to load Winsock! err_code=" << GET_LAST_ERROR());
    }
#endif

#if(defined(OS_IOS) || defined(OS_MACOS))
    signal(SIGPIPE, SIG_IGN);
#endif

    socketConnect();
}

ObRTPUDPClient::~ObRTPUDPClient() noexcept {
    close();
}

void ObRTPUDPClient::socketConnect() {
    // 1.Create udpsocket
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
    recvSocket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#else
    recvSocket_ = socket(AF_INET, SOCK_DGRAM, 0);
#endif

    if(recvSocket_ < 0) {
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
        WSACleanup();
#endif
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Failed to create udpSocket! err_code=" << GET_LAST_ERROR());
    }

    // 2.set receive timeout
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
    uint32_t commTimeout = COMM_TIMEOUT_MS;
#else
    TIMEVAL commTimeout;
    commTimeout.tv_sec  = COMM_TIMEOUT_MS / 1000;
    commTimeout.tv_usec = COMM_TIMEOUT_MS % 1000 * 1000;
#endif
    int nRecvBuf = 512 * 1024 * 1024;
    setsockopt(recvSocket_, SOL_SOCKET, SO_RCVBUF, (const char *)&nRecvBuf, sizeof(int));
    setsockopt(recvSocket_, SOL_SOCKET, SO_RCVTIMEO, (char *)&commTimeout, sizeof(commTimeout));

    // blocking mode
    /*unsigned long mode = 0;  
    if(ioctlsocket(recvSocket_, FIONBIO, &mode) < 0) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "ObRTPUDPClient: ioctlsocket to blocking mode failed! addr=" << serverIp_
                                                                              << ", port=" << serverPort_ << ", err_code=" << GET_LAST_ERROR());
    }*/

    // 3.Set server address
    sockaddr_in serverAddr{};
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port        = htons(serverPort_);

    // 4.Bind the socket to a local address and port.
    if(bind(recvSocket_, (sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        socketClose();
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
        WSACleanup();
#endif
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Failed to bind server address! err_code=" << GET_LAST_ERROR());
    }
}

void ObRTPUDPClient::start(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    if(startReceive_) {
        LOG_WARN("The UDP data receive thread has been started!");
        return;
    }

    currentProfile_  = profile;
    frameCallback_   = callback;
    startReceive_    = true;
    receiverThread_  = std::thread(&ObRTPUDPClient::frameReceive, this);
    callbackThread_  = std::thread(&ObRTPUDPClient::frameProcess, this);
}

void ObRTPUDPClient::socketClose() {
    if(recvSocket_ > 0) {
        auto rst = ::closesocket(recvSocket_);
        if(rst < 0) {
            LOG_ERROR("close udp socket failed! socket={0}, err_code={1}", recvSocket_, GET_LAST_ERROR());
        }
    }
    LOG_DEBUG("udp socket closed! socket={}", recvSocket_);
    recvSocket_ = INVALID_SOCKET;
}

void ObRTPUDPClient::frameReceive() {
    LOG_DEBUG("start udp data receive thread...");
    sockaddr_in          serverAddr;
    socklen_t            serverAddrSize = sizeof(serverAddr);
    std::vector<uint8_t> buffer(OB_UDP_BUFFER_SIZE);
    while(startReceive_) {
        int recvLen = recvfrom(recvSocket_, (char *)buffer.data(), (int)buffer.size(), 0, (sockaddr *)&serverAddr, &serverAddrSize);
        if(recvLen < 0) {
            int error = GET_LAST_ERROR();
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
            if(error == WSAETIMEDOUT) {
                LOG_ERROR_INTVL("Receive rtp packet timed out!");
            }
            else {
                LOG_ERROR_INTVL("Receive rtp packet error!");
            }
#else
            if(error == EAGAIN || error == EWOULDBLOCK) {
                LOG_ERROR_INTVL("Receive rtp packet timed out!");
            }
            else {
                LOG_ERROR_INTVL("Receive rtp packet error!");
            }
#endif
            continue;
        }

        if(recvLen > 0) {
            char server_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &serverAddr.sin_addr, server_ip, sizeof(server_ip));
            std::string serverIpString(server_ip);
            if(serverIpString == serverIp_) {
                std::vector<uint8_t> data(buffer.begin(), buffer.begin() + recvLen);
                rtpQueue_.push(data);
            }
        }
    }

    LOG_DEBUG("Exit udp data receive thread...");
}

void ObRTPUDPClient::frameProcess() {
    LOG_DEBUG("start frame process thread...");
    std::vector<uint8_t> data;
    while(startReceive_) {
        if(rtpQueue_.pop(data)) {
            RTPHeader *header = (RTPHeader *)data.data();
            if(serverPort_ != 20010) {
                rtpProcessor_.process(header, data.data(), (uint32_t)data.size(), currentProfile_->getType());
                if(rtpProcessor_.processComplete()) {
                    uint32_t frameDataSize = rtpProcessor_.getFrameDataSize();
                    uint32_t metaDataSize = rtpProcessor_.getMetaDataSize();
                    // LOG_DEBUG("Callback new frame dataSize: {}, number: {}", dataSize, rtpProcessor_.getNumber());

                    auto frame = FrameFactory::createFrameFromStreamProfile(currentProfile_);
                    frame->setSystemTimeStampUsec(utils::getNowTimesUs());
                    frame->setTimeStampUsec(rtpProcessor_.getTimestamp());
                    frame->setNumber(rtpProcessor_.getNumber());
                    frame->updateMetadata(rtpProcessor_.getMetaData(), metaDataSize);
                    frame->updateData(rtpProcessor_.getFrameData(), frameDataSize);

                    frameCallback_(frame);
                    rtpProcessor_.reset();
                }
                else {
                    if(rtpProcessor_.processError()) {
                        LOG_DEBUG("{} rtp frame process error...", currentProfile_->getType());
                        rtpProcessor_.reset();
                    }
                }
            } else {
                //imu
                auto frame = FrameFactory::createFrame(OB_FRAME_UNKNOWN, OB_FORMAT_UNKNOWN, OB_UDP_BUFFER_SIZE);
                frame->updateData(data.data() + 12, data.size()-12);
                frame->setTimeStampUsec(header->timestamp);
                frame->setSystemTimeStampUsec(utils::getNowTimesUs());
                frameCallback_(frame);
            }
        }
    }

    LOG_DEBUG("Exit frame process thread...");
}

void ObRTPUDPClient::close() {
    LOG_DEBUG("close start...");
    startReceive_ = false;
    if(receiverThread_.joinable()) {
        receiverThread_.join();
    }
    rtpQueue_.destroy();
    if(callbackThread_.joinable()) {
        callbackThread_.join();
    }
    if(recvSocket_ > 0) {
        socketClose();
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
        WSACleanup();
#endif
    }
    
    LOG_DEBUG("close end...");
}

}  // namespace libobsensor