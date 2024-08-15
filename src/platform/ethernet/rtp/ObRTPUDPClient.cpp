#include "ObRTPUDPClient.hpp"
#include "logger/Logger.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"

#define UDP_BUFFER_SIZE 4096

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
    recvSocket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(recvSocket_ == INVALID_SOCKET) {
        WSACleanup();
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
    setsockopt(recvSocket_, SOL_SOCKET, SO_RCVTIMEO, (char *)&commTimeout, sizeof(commTimeout));

    // blocking mode
    /*unsigned long mode = 0;  
    if(ioctlsocket(recvSocket_, FIONBIO, &mode) < 0) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "ObRTPUDPClient: ioctlsocket to blocking mode failed! addr=" << serverIp_
                                                                              << ", port=" << serverPort_ << ", err_code=" << GET_LAST_ERROR());
    }*/

    // 3.Set server address
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port   = htons(serverPort_);
    inet_pton(AF_INET, serverIp_.c_str(), &serverAddr.sin_addr);


    // 4.Bind the socket to a local address and port.
    if(bind(recvSocket_, (sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(recvSocket_);
        WSACleanup();
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Failed to bind server address! err_code=" << GET_LAST_ERROR());
    }
}

void ObRTPUDPClient::start(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    if(startReceive_) {
        LOG_WARN("The UDP data receive thread has been started!");
        return;
    }
    currentProfile_  = profile;
    currentCallback_ = callback;
    receiverThread_  = std::thread(&ObRTPUDPClient::startReceive, this);
    callbackThread_  = std::thread(&ObRTPUDPClient::frameProcess, this);
}

void ObRTPUDPClient::startReceive() {
    LOG_DEBUG("start udp data receive thread...");
    startReceive_ = true;
    sockaddr_in serverAddr;
    int         serverAddrSize = sizeof(serverAddr);
    uint8_t     recvBuffer[UDP_BUFFER_SIZE];
    int         recvBufferLen = UDP_BUFFER_SIZE;

    while(startReceive_) {
        int recvLen = recvfrom(recvSocket_, (char *)recvBuffer, recvBufferLen, 0, (sockaddr *)&serverAddr, &serverAddrSize);
        if(recvLen == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if(error == WSAETIMEDOUT) {
                LOG_ERROR("Receive timed out.");
                continue;  // 超时后继续等待
            }
            LOG_ERROR("Receive failed with error.");
            break;
        }
        LOG_INFO("Receive {} bytes of data", recvLen);

    }

    LOG_DEBUG("Exit udp data receive thread...");
}


void ObRTPUDPClient::frameProcess() {
    LOG_DEBUG("start frame process thread...");
    while(startReceive_) {
        
        //TODO:: callback frame
        //callbackThread_();
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
    closesocket(recvSocket_);

#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
    WSACleanup();
#endif
}

}  // namespace libobsensor