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

ObRTPUDPClient::ObRTPUDPClient(std::string localAddress, std::string address, uint16_t port)
    : localIp_(localAddress), serverIp_(address), serverPort_(port), startReceive_(false), recvSocket_(INVALID_SOCKET) {
    serverAddr_.sin_family = AF_INET;
    inet_pton(AF_INET, serverIp_.c_str(), &serverAddr_.sin_addr);
    socketConnect();
}

ObRTPUDPClient::~ObRTPUDPClient() noexcept {
    close();
}

void ObRTPUDPClient::setReceiveBuffer() {
    const int requestSizes[3] = { 512 * 1024 * 1024, 256 * 1024 * 1024, 128 * 1024 * 1024 };
    int       actualOpt       = 0;
    int       realBuf         = 0;
    int       okIndex         = -1;

    for(int i = 0; i < 3; i++) {
        setsockopt(recvSocket_, SOL_SOCKET, SO_RCVBUF, (const char *)&requestSizes[i], sizeof(int));
        socklen_t optLen = sizeof(actualOpt);
        if(getsockopt(recvSocket_, SOL_SOCKET, SO_RCVBUF, (char *)&actualOpt, &optLen) != 0) {
            continue;
        }
        realBuf = actualOpt;
#if defined(__linux__)
        realBuf /= 2;
#endif
        if(realBuf >= requestSizes[i]) {
            okIndex = i;
            break;
        }
    }

    if(okIndex >= 0) {
        LOG_INFO("SO_RCVBUF fallback: requested={} bytes, actual={} bytes", requestSizes[okIndex], realBuf);
    }
    else {
        LOG_WARN("SO_RCVBUF configuration failed. Current buffer size={} bytes, RTP streaming requires at least 134217728 bytes(128MB) receive buffer. Please "
                 "increase the system UDP receive buffer limit.",
                 realBuf);
    }
}

void ObRTPUDPClient::socketConnect() {
    // 1.Create udpsocket
#if (defined(WIN32) || defined(_WIN32) || defined(WINCE))
    recvSocket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#else
    recvSocket_ = socket(AF_INET, SOCK_DGRAM, 0);
#endif

    if(recvSocket_ == INVALID_SOCKET) {
        THROW_IO_EXCEPTION(utils::string::to_string() << "Failed to create udpSocket! err_code=" << GET_LAST_ERROR());
    }

    // 2.set receive buffer
    setReceiveBuffer();

    // 3.Set server address
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port   = htons(serverPort_);
    inet_pton(AF_INET, localIp_.c_str(), &serverAddr.sin_addr);

    // 4.Bind the socket to a local address and port.
    int bindResult = bind(recvSocket_, (sockaddr *)&serverAddr, sizeof(serverAddr));
    if(bindResult < 0) {
#if (defined(WIN32) || defined(_WIN32) || defined(WINCE))
        if(GET_LAST_ERROR() == WSAEADDRINUSE) {
#else
        if(GET_LAST_ERROR() == EADDRINUSE) {
#endif
            socketClose();
            serverPort_ += 2;
            socketConnect();
        }
        else {
            socketClose();
            THROW_INVALID_PARAM_EXCEPTION(utils::string::to_string() << "Failed to bind server address! err_code=" << GET_LAST_ERROR());
        }
    }
    LOG_DEBUG("Net device socket open successfully");
}

uint16_t ObRTPUDPClient::getPort() {
    return serverPort_;
}

void ObRTPUDPClient::start(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    if(recvSocket_ == INVALID_SOCKET) {
        LOG_WARN("The UDP socket is not initialized!");
        return;
    }

    if(startReceive_.load()) {
        LOG_WARN("The udp data receive thread has been started!");
        return;
    }

    startReceive_.store(true);
    // Clear any stale data from previous runs
    rtpQueue_.reset();
    rtpProcessor_.reset();
    rtpProcessor_.resetNumber();
    currentProfile_ = profile;
    frameCallback_  = callback;
    receiverThread_ = std::thread(&ObRTPUDPClient::frameReceive, this);
    callbackThread_ = std::thread(&ObRTPUDPClient::frameProcess, this);
}

void ObRTPUDPClient::socketClose() {
    if(recvSocket_ != INVALID_SOCKET) {
        auto rst = ::closesocket(recvSocket_);
        if(rst < 0) {
            LOG_WARN("close udp socket failed! socket={0}, err_code={1}", recvSocket_, GET_LAST_ERROR());
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
    while(startReceive_.load()) {
        // Wait with select() rather than arming recvfrom() with SO_RCVTIMEO. select()
        // only observes readiness, so its timeout leaves any queued datagram intact.
        struct timeval selectTimeout;
        selectTimeout.tv_sec  = COMM_TIMEOUT_MS / 1000;
        selectTimeout.tv_usec = COMM_TIMEOUT_MS % 1000 * 1000;
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(recvSocket_, &readfds);
        int ready = select(static_cast<int>(recvSocket_) + 1, &readfds, nullptr, nullptr, &selectTimeout);
        if(ready == 0) {
            LOG_WARN_INTVL("Receive rtp packet timed out!");
            continue;
        }
        if(ready < 0) {
            int error = GET_LAST_ERROR();
#if (defined(WIN32) || defined(_WIN32) || defined(WINCE))
            if(error == WSAEINTR) {
                continue;
            }
#else
            if(error == EINTR) {
                continue;
            }
#endif
            LOG_ERROR_INTVL("Receive rtp packet select error!");
            continue;
        }

        int recvLen = recvfrom(recvSocket_, (char *)buffer.data(), (int)buffer.size(), 0, (sockaddr *)&serverAddr, &serverAddrSize);
        if(recvLen < 0) {
            LOG_ERROR_INTVL("Receive rtp packet error!");
            continue;
        }

        if(recvLen > 0) {
            if(serverAddr.sin_addr.s_addr == serverAddr_.sin_addr.s_addr) {
                std::vector<uint8_t> data(buffer.begin(), buffer.begin() + recvLen);
                rtpQueue_.push(std::move(data));
            }
        }
    }

    LOG_DEBUG("Exit udp data receive thread...");
}

void ObRTPUDPClient::flush() {
    char         buf[OB_UDP_BUFFER_SIZE];
    utils::Timer timer;
    uint64_t     elapsed = 0;

    do {
        struct timeval timeout;
        timeout.tv_sec  = 0;
        timeout.tv_usec = 0;  // return immediately

        auto   sock = recvSocket_;
        auto   nfds = static_cast<int>(sock) + 1;
        fd_set readfs;

        FD_ZERO(&readfs);
        FD_SET(sock, &readfs);
        auto res = select(nfds, &readfs, 0, 0, &timeout);
        if(res <= 0) {
            // no any data
            break;
        }

        // read data and discard
        res = recvfrom(sock, buf, sizeof(buf), 0, NULL, NULL);
        if(res > 0) {
            LOG_DEBUG("Discarding {} bytes of leftover frame data. IP: {}", res, serverIp_);
        }
        elapsed = timer.touchMs(false);
        // Loop until timeout to avoid blocking indefinitely
    } while(elapsed < 3000);
}

void ObRTPUDPClient::frameProcess() {
    LOG_DEBUG("start frame process thread...");
    std::vector<uint8_t> data;
    while(startReceive_.load()) {
        if(rtpQueue_.pop(data)) {
            RTPHeader *header = (RTPHeader *)data.data();
            if(currentProfile_ != nullptr) {
                rtpProcessor_.process(header, data.data(), (uint32_t)data.size(), currentProfile_->getType(), currentProfile_->getFormat());
                if(rtpProcessor_.processComplete()) {
                    uint32_t frameDataSize = rtpProcessor_.getFrameDataSize();
                    uint32_t metaDataSize  = rtpProcessor_.getMetaDataSize();
                    // LOG_DEBUG("Callback new frame dataSize: {}, number: {}", dataSize, rtpProcessor_.getNumber());

                    auto     frame        = FrameFactory::createFrameFromStreamProfile(currentProfile_);
                    uint32_t expectedSize = static_cast<uint32_t>(frame->getDataSize());
                    if(frameDataSize > expectedSize) {
                        LOG_WARN_INTVL("{} Receive data size({}) >  expected data size! ({})", currentProfile_->getType(), frameDataSize, expectedSize);
                        rtpProcessor_.reset();
                        continue;
                    }

                    const auto timestamp = utils::getHostTimestampUs();
                    frame->setSystemTimeStampUsec(timestamp.systemTimeUs);
                    frame->setSteadyTimeStampUsec(timestamp.steadyTimeUs);
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
            }
            else {
                // imu
                auto frame = FrameFactory::createFrame(OB_FRAME_UNKNOWN, OB_FORMAT_UNKNOWN, OB_UDP_BUFFER_SIZE);
                frame->updateData(data.data() + 12, data.size() - 12);
                frame->setTimeStampUsec(header->timestamp);
                const auto timestamp = utils::getHostTimestampUs();
                frame->setSystemTimeStampUsec(timestamp.systemTimeUs);
                frame->setSteadyTimeStampUsec(timestamp.steadyTimeUs);
                frameCallback_(frame);
            }
        }
    }

    LOG_DEBUG("Exit frame process thread...");
}

void ObRTPUDPClient::stop() {
    LOG_DEBUG("stop stream start...");
    startReceive_.store(false);
    if(receiverThread_.joinable()) {
        receiverThread_.join();
        flush();
    }

    // Ensure any buffered RTP packets are discarded when stopping
    rtpQueue_.destroy();
    rtpProcessor_.reset();

    if(callbackThread_.joinable()) {
        callbackThread_.join();
    }
    LOG_DEBUG("stop stream end...");
}

void ObRTPUDPClient::close() {
    LOG_DEBUG("close start...");
    stop();
    if(recvSocket_ != INVALID_SOCKET) {
        socketClose();
    }
    LOG_DEBUG("close end...");
}

}  // namespace libobsensor
