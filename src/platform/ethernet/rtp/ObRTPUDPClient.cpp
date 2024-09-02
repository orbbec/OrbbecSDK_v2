#include "ObRTPUDPClient.hpp"
#include "logger/Logger.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"
#include "utils/Utils.hpp"
#include "frame/FrameFactory.hpp"
#include "stream/StreamProfile.hpp"

#define OB_UDP_BUFFER_SIZE 1500

namespace libobsensor {

ObRTPUDPClient::ObRTPUDPClient(std::string address, uint16_t port) : serverIp_(address), serverPort_(port), startReceive_(false) {

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

ObRTPUDPClient::~ObRTPUDPClient() noexcept {}

void ObRTPUDPClient::socketConnect() {
    // 1.Create udpsocket
    recvSocket_ = INVALID_SOCKET;

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
    int nRecvBuf = 512 * 1024;
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
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
        closesocket(recvSocket_);
        WSACleanup();
#else
        close(recvSocket_);
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
    receiverThread_  = std::thread(&ObRTPUDPClient::startReceive, this);
    callbackThread_  = std::thread(&ObRTPUDPClient::frameProcess, this);
}

void ObRTPUDPClient::startReceive() {
    LOG_DEBUG("start udp data receive thread...");
    sockaddr_in          serverAddr;
    int                  serverAddrSize = sizeof(serverAddr);
    std::vector<uint8_t> buffer(OB_UDP_BUFFER_SIZE);
    int                  tryRecCount = 3;
    while(startReceive_) {
        int recvLen = recvfrom(recvSocket_, (char *)buffer.data(), (int)buffer.size(), 0, (sockaddr *)&serverAddr, &serverAddrSize);
        if(recvLen < 0) {
            int error = GET_LAST_ERROR();
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
            if(error == WSAETIMEDOUT) {
                LOG_ERROR("Receive timed out.");
            };
#endif
            if(tryRecCount == 0) {
                LOG_ERROR("Receive failed with error.");
                break;
            }

            tryRecCount--;
            continue;
        }
        tryRecCount = 3;

        if(recvLen > 0) {
            buffer.resize(recvLen);
            rtpQueue_.push(buffer);
            buffer.resize(OB_UDP_BUFFER_SIZE);
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
            rtpProcessor_.process(header, data.data(), (uint32_t)data.size(), currentProfile_->getType());
            if(rtpProcessor_.processComplete()) {
                uint32_t dataSize = rtpProcessor_.getDataSize();
                LOG_DEBUG("Callback new frame dataSize: {}, number: {}", dataSize, rtpProcessor_.getNumber());

                auto frame = FrameFactory::createFrameFromStreamProfile(currentProfile_);
                frame->setSystemTimeStampUsec(utils::getNowTimesUs());
                // frame->setTimeStampUsec(header->timestamp);
                frame->setNumber(rtpProcessor_.getNumber());
                frame->updateData(rtpProcessor_.getData(), dataSize);

                frameCallback_(frame);
                rtpProcessor_.reset();
            }
            else {
                if(rtpProcessor_.processTimeOut()) {
                    LOG_DEBUG("Callback new frame process timeout...");
                    rtpProcessor_.reset();
                }
            }
        }
    }

    LOG_DEBUG("Exit frame process thread...");
}

void ObRTPUDPClient::close() {
    startReceive_ = false;
    if(receiverThread_.joinable()) {
        receiverThread_.join();
    }
    if(callbackThread_.joinable()) {
        callbackThread_.join();
    }

    if(recvSocket_ > 0) {
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
        closesocket(recvSocket_);
        WSACleanup();
#else
        close(recvSocket_);
#endif
    }
}

}  // namespace libobsensor