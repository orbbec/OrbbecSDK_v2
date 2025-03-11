#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))

#include "ObRTPNpCapReceiver.hpp"
#include "logger/Logger.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"
#include "utils/Utils.hpp"
#include "frame/FrameFactory.hpp"
#include "stream/StreamProfile.hpp"
#include "NpCapLoader.hpp"

#define OB_UDP_BUFFER_SIZE 26214400

namespace libobsensor {

struct ip_header {
    u_char         ip_hl : 4, ip_v : 4;
    u_char         ip_tos;
    u_short        ip_len;
    u_short        ip_id;
    u_short        ip_off;
    u_char         ip_ttl;
    u_char         ip_p;
    u_short        ip_sum;
    struct in_addr ip_src, ip_dst;
};

ObRTPNpCapReceiver::ObRTPNpCapReceiver(std::string localAddress, std::string address, uint16_t port)
    : localIp_(localAddress), serverIp_(address), serverPort_(port), startReceive_(false), alldevs_(nullptr), dev_(nullptr), handle_(nullptr) {
    findAlldevs();
}

ObRTPNpCapReceiver::~ObRTPNpCapReceiver() noexcept {
}

uint16_t ObRTPNpCapReceiver::getAvailableUdpPort() {
    uint16_t port = serverPort_;
    while(isPortInUse(port)) {
        port+=2;
    }
    LOG_DEBUG("Found available port: {}", port);
    return port;
}

bool ObRTPNpCapReceiver::isPortInUse(uint16_t port) {
    WSADATA wsaData;
    int     rst = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if(rst != 0) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Failed to load Winsock! err_code=" << GET_LAST_ERROR());
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock == INVALID_SOCKET) {
        close();
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Failed to create udpSocket! err_code=" << GET_LAST_ERROR());
    }

    int nRecvBuf = OB_UDP_BUFFER_SIZE;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char *)&nRecvBuf, sizeof(int));

    sockaddr_in serverAddr;
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_port        = htons(port);
    //serverAddr.sin_addr.s_addr = INADDR_ANY;
    //inet_pton(AF_INET, "192.168.2.31", &serverAddr.sin_addr);
    inet_pton(AF_INET, localIp_.c_str(), &serverAddr.sin_addr);
    int  bindResult            = bind(sock, (sockaddr *)&serverAddr, sizeof(serverAddr));
    bool inUse                 = (bindResult == SOCKET_ERROR && WSAGetLastError() == WSAEADDRINUSE);
    if(inUse) {
        closesocket(sock);
        WSACleanup();
    }
    else {
        recvSocket_ = sock;
    }
    return inUse;
}

uint16_t ObRTPNpCapReceiver::getPort() {
    return serverPort_;
}

void ObRTPNpCapReceiver::findAlldevs() {
    serverPort_ = getAvailableUdpPort();

    //IMU Port 20010
    if(serverPort_ == 20010) {
        COMM_TIMEOUT_MS = 50;
    }

    NpCapLoader &npcapLoder = NpCapLoader::getInstance();
    if(!npcapLoder.init()) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Failed to init NpCaploader!");
    }

    char    errbuf[PCAP_ERRBUF_SIZE];
    if(npcapLoder.pcap_findalldevs_(&alldevs_, errbuf) == -1) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Failed to finding net devices, err_msg=" << errbuf);
    }

    if(alldevs_ == nullptr) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "No net devices found!");
    }

    LOG_DEBUG("Available devices:");
    bool foundDevice = false;
    for(dev_ = alldevs_; dev_; dev_ = dev_->next) {
        if((dev_->flags & PCAP_IF_WIRELESS) || (dev_->flags & PCAP_IF_CONNECTION_STATUS_DISCONNECTED)) {
            continue;  // Skip wifi
        }

        LOG_DEBUG("Device Name: {}", (dev_->name ? dev_->name : "No name"));

        if(dev_->addresses) {
            // Check and log the device address
            if(dev_->addresses->addr) {
                char     ip[INET6_ADDRSTRLEN];
                uint16_t port = 0;

                sockaddr *sa = dev_->addresses->addr;
                if(sa->sa_family == AF_INET) {
                    sockaddr_in *sa_in = (sockaddr_in *)sa;
                    inet_ntop(AF_INET, &(sa_in->sin_addr), ip, sizeof(ip));
                    port = ntohs(sa_in->sin_port);
                    std::string sockIp(ip);
                    LOG_DEBUG("device Address: {}:{}", sockIp, port);
                    if(sockIp == localIp_) {
                        foundDevice = true;
                        LOG_DEBUG("Found device Address: {}:{}", sockIp, port);
                        break;
                    }
                }
                else if(sa->sa_family == AF_INET6) {
                    sockaddr_in6 *sa_in6 = (sockaddr_in6 *)sa;
                    inet_ntop(AF_INET6, &(sa_in6->sin6_addr), ip, sizeof(ip));
                    port = ntohs(sa_in6->sin6_port);
                    std::string sockIp(ip);
                    LOG_DEBUG("device Address: {}:{}", sockIp, port);
                    if(sockIp == localIp_) {
                        foundDevice = true;
                        LOG_DEBUG("Found device Address: {}:{}", sockIp, port);
                        break;
                    }
                }
                else {
                    LOG_DEBUG("Unknown address family.");
                }
            }
            else {
                LOG_DEBUG("No address available");
            }
        }
        else {
            LOG_DEBUG("No address information available.");
        }
    }

    if(foundDevice) {
        handle_ = npcapLoder.pcap_open_live_(dev_->name, OB_UDP_BUFFER_SIZE, 1, COMM_TIMEOUT_MS, errbuf);
        if(handle_ == nullptr) {
            npcapLoder.pcap_freealldevs_(alldevs_);
            throw libobsensor::invalid_value_exception(utils::string::to_string() << "Error opening net device:" << errbuf);
        }

        if(npcapLoder.pcap_datalink_(handle_) != DLT_EN10MB) {
            /* Free the device list */
            npcapLoder.pcap_close_(handle_);
            npcapLoder.pcap_freealldevs_(alldevs_);
            throw libobsensor::invalid_value_exception(utils::string::to_string() << "Error data link:" << npcapLoder.pcap_geterr_(handle_));
        }

        struct bpf_program fp;
        std::string        strPort = std::to_string(serverPort_);
        // std::string        stringFilter = "udp port " + strPort;
        // const char *       filter_exp   = "udp and src host 192.168.1.100 and src port 12345";
        std::string stringFilter = "udp and src host " + serverIp_ + " and src port " + strPort;
        //std::string stringFilter = "udp and (src host " + serverIp_ + " and src port " + strPort + " and dst host " + localIp_ + " and dst port " + strPort + ")";
        LOG_DEBUG("Set pcap filter: {}", stringFilter);
        if(npcapLoder.pcap_compile_(handle_, &fp, stringFilter.c_str(), 0, PCAP_NETMASK_UNKNOWN) == -1) {
            npcapLoder.pcap_close_(handle_);
            npcapLoder.pcap_freealldevs_(alldevs_);
            throw libobsensor::invalid_value_exception(utils::string::to_string() << "Error compiling filter:" << npcapLoder.pcap_geterr_(handle_));
        }

        if(npcapLoder.pcap_setfilter_(handle_, &fp) < 0) {
            /* Free the device list */
            npcapLoder.pcap_freecode_(&fp);
            npcapLoder.pcap_close_(handle_);
            npcapLoder.pcap_freealldevs_(alldevs_);
            throw libobsensor::invalid_value_exception(utils::string::to_string() << "Error setting the filter:" << npcapLoder.pcap_geterr_(handle_));
        }
        npcapLoder.pcap_freecode_(&fp);
    }
    else {
        LOG_DEBUG("Available devices:");
        for(dev_ = alldevs_; dev_; dev_ = dev_->next) {
            if((dev_->flags & PCAP_IF_WIRELESS) || (dev_->flags & PCAP_IF_CONNECTION_STATUS_DISCONNECTED)) {
                continue;  // Skip wifi
            }

            if(!dev_->addresses) {
                continue;
            }

            LOG_DEBUG("Device Name: {}", (dev_->name ? dev_->name : "No name"));

            pcap_t *handle = npcapLoder.pcap_open_live_(dev_->name, OB_UDP_BUFFER_SIZE, 1, COMM_TIMEOUT_MS, errbuf);
            if(handle == nullptr) {
                LOG_WARN("Error opening net device: {}", errbuf);
                continue;
            }

            if(npcapLoder.pcap_datalink_(handle) != DLT_EN10MB) {
                LOG_WARN("Error data link: {}", errbuf);
                npcapLoder.pcap_close_(handle);
                continue;
            }

            struct bpf_program fp;
            std::string        strPort      = std::to_string(serverPort_);
            std::string        stringFilter = "udp and src host " + serverIp_ + " and src port " + strPort;
            /*std::string        stringFilter =
                "udp and (src host " + serverIp_ + " and src port " + strPort + " and dst host " + localIp_ + " and dst port " + strPort + ")";*/
            LOG_DEBUG("Set pcap filter: {}", stringFilter);
            if(npcapLoder.pcap_compile_(handle, &fp, stringFilter.c_str(), 0, PCAP_NETMASK_UNKNOWN) == 0) {
                LOG_DEBUG("Found net device");
            }
            else {
                LOG_WARN("Error pcap compile!");
                npcapLoder.pcap_close_(handle);
                continue;
            }

            if(npcapLoder.pcap_setfilter_(handle, &fp) < 0) {
                LOG_WARN("Error setting the filter: {}", npcapLoder.pcap_geterr_(handle));
                npcapLoder.pcap_freecode_(&fp);
                npcapLoder.pcap_close_(handle);
                continue;
            }
            npcapLoder.pcap_freecode_(&fp);
            handles_.push_back(handle);
        }
    }

    if(!foundDevice && handles_.size() == 0) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Error opening net device, not found!");
    }
    LOG_DEBUG("Net device opened successfully");
}

void ObRTPNpCapReceiver::start(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    if(startReceive_.load()) {
        LOG_WARN("The UDP data receive thread has been started!");
        return;
    }

    currentProfile_  = profile;
    frameCallback_   = callback;
    startReceive_.store(true);
    rtpQueue_.reset();
    if(handle_) {
        receiverThread_ = std::thread(&ObRTPNpCapReceiver::frameReceive, this);
        callbackThread_ = std::thread(&ObRTPNpCapReceiver::frameProcess, this);
    }
    else {
        foundPcapHandle_ = false;
        for(int i = 0; i < (int)handles_.size(); ++i) {
            handleThreads_.emplace_back(&ObRTPNpCapReceiver::frameReceive2, this, handles_[i]);
        }
        callbackThread_ = std::thread(&ObRTPNpCapReceiver::frameProcess, this);
    }
}

void ObRTPNpCapReceiver::frameReceive() {
    LOG_DEBUG("start udp data receive thread...");
    NpCapLoader &       npcapLoder = NpCapLoader::getInstance();
    struct pcap_pkthdr *header;
    const u_char *      packet;
    while(startReceive_.load()) {
        int res = npcapLoder.pcap_next_ex_(handle_, &header, &packet);
        if(res > 0) {
            // Ethernet + IP header lengths
            const u_char *payload = packet + 42;
            // Adjust based on header lengths
            int recvLen = header->len - 42;
            if(recvLen > 0) {
                std::vector<uint8_t> data(payload, payload + recvLen);
                rtpQueue_.push(data);
            }
        }
        else {
            if(res == 0) {
                LOG_ERROR_INTVL("Receive rtp packet timeout!");
            }
            else {
                LOG_ERROR_INTVL("Receive rtp packet error: {}", res);
            }
            
        }
    }

    LOG_DEBUG("Exit udp data receive thread...");
}


void ObRTPNpCapReceiver::frameReceive2(pcap_t *handle) {
    LOG_DEBUG("start udp data receive thread...");
    NpCapLoader &       npcapLoder = NpCapLoader::getInstance();
    struct pcap_pkthdr *header;
    const u_char *      packet;
    bool receiveData = false;
    while(startReceive_.load()) {
        int res = npcapLoder.pcap_next_ex_(handle, &header, &packet);
        if(res > 0) {
            foundPcapHandle_ = true;
            if(foundPcapHandle_) {
                handle_ = handle;
            }
            receiveData = true;

            //const u_char *ip_header = packet + 14;                  // 跳过Ethernet头部
            //struct ip_header *ip        = (struct ip_header *)(ip_header);  // IP头部开始处
            //// 获取源IP地址
            //struct in_addr src_ip;
            //src_ip.s_addr = ip->ip_src.s_addr;
            //char src_ip_str[INET_ADDRSTRLEN];
            //inet_ntop(AF_INET, &src_ip, src_ip_str, sizeof(src_ip_str));

            //// 打印源IP地址
            //if(n == 1000) {
            //    LOG_DEBUG("Received UDP packet from IP: {}", src_ip_str);
            //    n = 0;
            //}
            //n++;
            
            // Ethernet + IP header lengths
            const u_char *payload = packet + 42;

            // Adjust based on header lengths
            int recvLen = header->len - 42;
            if(recvLen > 0) {
                std::vector<uint8_t> data(payload, payload + recvLen);
                rtpQueue_.push(data);
            }
        }
        else {
            if(!receiveData && foundPcapHandle_) {
                break;
            }
            LOG_ERROR_INTVL("Receive rtp packet error: {}!", res);
        }
    }
    LOG_DEBUG("Exit udp data receive thread...");
}

void ObRTPNpCapReceiver::parseIPAddress(const u_char *ip_header, std::string &src_ip, std::string &dst_ip) {
    auto ip_src = reinterpret_cast<const uint8_t *>(ip_header + 12);  // 源 IP 地址
    auto ip_dst = reinterpret_cast<const uint8_t *>(ip_header + 16);  // 目的 IP 地址

    src_ip = std::to_string(ip_src[0]) + "." + std::to_string(ip_src[1]) + "." + std::to_string(ip_src[2]) + "." + std::to_string(ip_src[3]);

    dst_ip = std::to_string(ip_dst[0]) + "." + std::to_string(ip_dst[1]) + "." + std::to_string(ip_dst[2]) + "." + std::to_string(ip_dst[3]);
}

void ObRTPNpCapReceiver::frameProcess() {
    LOG_DEBUG("start frame process thread...");
    std::vector<uint8_t> data;
    while(startReceive_.load()) {
        if(rtpQueue_.pop(data)) {
            RTPHeader *header = (RTPHeader *)data.data();
            if(currentProfile_ != nullptr) {
                rtpProcessor_.process(header, data.data(), (uint32_t)data.size(), currentProfile_->getType());
                if(rtpProcessor_.processComplete()) {
                    uint32_t frameDataSize = rtpProcessor_.getFrameDataSize();
                    uint32_t metaDataSize = rtpProcessor_.getMetaDataSize();
                    //LOG_DEBUG("Callback new frame dataSize: {}, number: {}", dataSize, rtpProcessor_.getNumber());

                    auto frame = FrameFactory::createFrameFromStreamProfile(currentProfile_);
                    uint32_t expectedSize = static_cast<uint32_t>(frame->getDataSize());
                    if(frameDataSize > expectedSize) {
                        LOG_WARN_INTVL("{} Receive data size({}) >  expected data size! ({})", currentProfile_->getType(), frameDataSize, expectedSize);
                        rtpProcessor_.reset();
                        continue;
                    }

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
                        LOG_DEBUG("rtp frame {} process error...", currentProfile_->getType());
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

void ObRTPNpCapReceiver::stop() {
    LOG_DEBUG("stop stream start...");
    startReceive_.store(false);
    if(receiverThread_.joinable()) {
        receiverThread_.join();
    }
    for(auto &thread: handleThreads_) {
        if(thread.joinable()) {
            thread.join();
        }
    }
    rtpQueue_.destroy();
    if(callbackThread_.joinable()) {
        callbackThread_.join();
    }
    LOG_DEBUG("stop stream end...");
}

void ObRTPNpCapReceiver::close() {
    NpCapLoader &npcapLoder = NpCapLoader::getInstance();
    if(handles_.size() > 0) {
        for(auto &handle: handles_) {
            if(handle != nullptr) {
                if(npcapLoder.pcap_close_) {
                    npcapLoder.pcap_close_(handle);
                }
            }
        }
    }
    else {
        if(handle_ != nullptr) {
            if(npcapLoder.pcap_close_) {
                npcapLoder.pcap_close_(handle_);
            }
        }
    }
    
    
    if(alldevs_ != nullptr) {
        if(npcapLoder.pcap_freealldevs_) {
            npcapLoder.pcap_freealldevs_(alldevs_);
        }
    }

    if(recvSocket_ > 0) {
        auto rst = ::closesocket(recvSocket_);
        if(rst < 0) {
            LOG_ERROR("close udp socket failed! socket={0}, err_code={1}", recvSocket_, GET_LAST_ERROR());
        }
    }
    LOG_DEBUG("udp socket closed! socket={}", recvSocket_);
    WSACleanup();
    recvSocket_ = INVALID_SOCKET;
}

//===============================================================================================================================================

#if 0

struct ip_header {
    u_char         ip_hl : 4, ip_v : 4;
    u_char         ip_tos;
    u_short        ip_len;
    u_short        ip_id;
    u_short        ip_off;
    u_char         ip_ttl;
    u_char         ip_p;
    u_short        ip_sum;
    struct in_addr ip_src, ip_dst;
};

ObRTPNpCapReceiver::ObRTPNpCapReceiver(std::string localAddress, std::string address, uint16_t port)
    : localIp_(localAddress), serverIp_(address), serverPort_(port), startReceive_(false), alldevs_(nullptr), dev_(nullptr), handle_(nullptr) {
    findAlldevs();
}

ObRTPNpCapReceiver::~ObRTPNpCapReceiver() noexcept {
}

uint16_t ObRTPNpCapReceiver::getAvailableUdpPort() {
    uint16_t port = serverPort_;
    while(isPortInUse(port)) {
        port+=2;
    }
    LOG_DEBUG("Found available port: {}", port);
    return port;
}

bool ObRTPNpCapReceiver::isPortInUse(uint16_t port) {
    WSADATA wsaData;
    int     rst = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if(rst != 0) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Failed to load Winsock! err_code=" << GET_LAST_ERROR());
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock == INVALID_SOCKET) {
        close();
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Failed to create udpSocket! err_code=" << GET_LAST_ERROR());
    }

    int nRecvBuf = OB_UDP_BUFFER_SIZE;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char *)&nRecvBuf, sizeof(int));

    sockaddr_in serverAddr;
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_port        = htons(port);
    //serverAddr.sin_addr.s_addr = INADDR_ANY;
    //inet_pton(AF_INET, "192.168.2.31", &serverAddr.sin_addr);
    inet_pton(AF_INET, localIp_.c_str(), &serverAddr.sin_addr);
    int  bindResult            = bind(sock, (sockaddr *)&serverAddr, sizeof(serverAddr));
    bool inUse                 = (bindResult == SOCKET_ERROR && WSAGetLastError() == WSAEADDRINUSE);
    if(inUse) {
        closesocket(sock);
        WSACleanup();
    }
    else {
        recvSocket_ = sock;
    }
    return inUse;
}

uint16_t ObRTPNpCapReceiver::getPort() {
    return serverPort_;
}

void ObRTPNpCapReceiver::findAlldevs() {
    serverPort_ = getAvailableUdpPort();

    //IMU Port 20010
    if(serverPort_ == 20010) {
        COMM_TIMEOUT_MS = 50;
    }

    char    errbuf[PCAP_ERRBUF_SIZE];
    if(pcap_findalldevs(&alldevs_, errbuf) == -1) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Failed to finding net devices, err_msg=" << errbuf);
    }

    if(alldevs_ == nullptr) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "No net devices found!");
    }

    LOG_DEBUG("Available devices:");
    bool foundDevice = false;
    for(dev_ = alldevs_; dev_; dev_ = dev_->next) {
        if((dev_->flags & PCAP_IF_WIRELESS) || (dev_->flags & PCAP_IF_CONNECTION_STATUS_DISCONNECTED)) {
            continue;  // Skip wifi
        }

        LOG_DEBUG("Device Name: {}", (dev_->name ? dev_->name : "No name"));

        if(dev_->addresses) {
            // Check and log the device address
            if(dev_->addresses->addr) {
                char     ip[INET6_ADDRSTRLEN];
                uint16_t port = 0;

                sockaddr *sa = dev_->addresses->addr;
                if(sa->sa_family == AF_INET) {
                    sockaddr_in *sa_in = (sockaddr_in *)sa;
                    inet_ntop(AF_INET, &(sa_in->sin_addr), ip, sizeof(ip));
                    port = ntohs(sa_in->sin_port);
                    std::string sockIp(ip);
                    LOG_DEBUG("device Address: {}:{}", sockIp, port);
                    if(sockIp == localIp_) {
                        foundDevice = true;
                        LOG_DEBUG("Found device Address: {}:{}", sockIp, port);
                        break;
                    }
                }
                else if(sa->sa_family == AF_INET6) {
                    sockaddr_in6 *sa_in6 = (sockaddr_in6 *)sa;
                    inet_ntop(AF_INET6, &(sa_in6->sin6_addr), ip, sizeof(ip));
                    port = ntohs(sa_in6->sin6_port);
                    std::string sockIp(ip);
                    LOG_DEBUG("device Address: {}:{}", sockIp, port);
                    if(sockIp == localIp_) {
                        foundDevice = true;
                        LOG_DEBUG("Found device Address: {}:{}", sockIp, port);
                        break;
                    }
                }
                else {
                    LOG_DEBUG("Unknown address family.");
                }
            }
            else {
                LOG_DEBUG("No address available");
            }
        }
        else {
            LOG_DEBUG("No address information available.");
        }
    }

    if(foundDevice) {
        handle_ = pcap_open_live(dev_->name, OB_UDP_BUFFER_SIZE, 1, COMM_TIMEOUT_MS, errbuf);
        if(handle_ == nullptr) {
            pcap_freealldevs(alldevs_);
            throw libobsensor::invalid_value_exception(utils::string::to_string() << "Error opening net device:" << errbuf);
        }

        if(pcap_datalink(handle_) != DLT_EN10MB) {
            /* Free the device list */
            pcap_close(handle_);
            pcap_freealldevs(alldevs_);
            throw libobsensor::invalid_value_exception(utils::string::to_string() << "Error data link:" << pcap_geterr(handle_));
        }

        struct bpf_program fp;
        std::string        strPort = std::to_string(serverPort_);
        // std::string        stringFilter = "udp port " + strPort;
        // const char *       filter_exp   = "udp and src host 192.168.1.100 and src port 12345";
        std::string stringFilter = "udp and src host " + serverIp_ + " and src port " + strPort;
        //std::string stringFilter = "udp and (src host " + serverIp_ + " and src port " + strPort + " and dst host " + localIp_ + " and dst port " + strPort + ")";
        LOG_DEBUG("Set pcap filter: {}", stringFilter);
        if(pcap_compile(handle_, &fp, stringFilter.c_str(), 0, PCAP_NETMASK_UNKNOWN) == -1) {
            pcap_close(handle_);
            pcap_freealldevs(alldevs_);
            throw libobsensor::invalid_value_exception(utils::string::to_string() << "Error compiling filter:" << pcap_geterr(handle_));
        }

        if(pcap_setfilter(handle_, &fp) < 0) {
            /* Free the device list */
            pcap_freecode(&fp);
            pcap_close(handle_);
            pcap_freealldevs(alldevs_);
            throw libobsensor::invalid_value_exception(utils::string::to_string() << "Error setting the filter:" << pcap_geterr(handle_));
        }
        pcap_freecode(&fp);
    }
    else {
        LOG_DEBUG("Available devices:");
        for(dev_ = alldevs_; dev_; dev_ = dev_->next) {
            if((dev_->flags & PCAP_IF_WIRELESS) || (dev_->flags & PCAP_IF_CONNECTION_STATUS_DISCONNECTED)) {
                continue;  // Skip wifi
            }

            if(!dev_->addresses) {
                continue;
            }

            LOG_DEBUG("Device Name: {}", (dev_->name ? dev_->name : "No name"));

            pcap_t *handle = pcap_open_live(dev_->name, OB_UDP_BUFFER_SIZE, 1, COMM_TIMEOUT_MS, errbuf);
            if(handle == nullptr) {
                LOG_WARN("Error opening net device: {}", errbuf);
                continue;
            }

            if(pcap_datalink(handle) != DLT_EN10MB) {
                LOG_WARN("Error data link: {}", errbuf);
                pcap_close(handle);
                continue;
            }

            struct bpf_program fp;
            std::string        strPort      = std::to_string(serverPort_);
            std::string        stringFilter = "udp and src host " + serverIp_ + " and src port " + strPort;
            /*std::string        stringFilter =
                "udp and (src host " + serverIp_ + " and src port " + strPort + " and dst host " + localIp_ + " and dst port " + strPort + ")";*/
            LOG_DEBUG("Set pcap filter: {}", stringFilter);
            if(pcap_compile(handle, &fp, stringFilter.c_str(), 0, PCAP_NETMASK_UNKNOWN) == 0) {
                LOG_DEBUG("Found net device");
            }
            else {
                LOG_WARN("Error pcap compile!");
                pcap_close(handle);
                continue;
            }

            if(pcap_setfilter(handle, &fp) < 0) {
                LOG_WARN("Error setting the filter: {}", pcap_geterr(handle));
                pcap_freecode(&fp);
                pcap_close(handle);
                continue;
            }
            pcap_freecode(&fp);
            handles_.push_back(handle);
        }
    }

    if(!foundDevice && handles_.size() == 0) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Error opening net device, not found!");
    }
    LOG_DEBUG("Net device opened successfully");
}

void ObRTPNpCapReceiver::start(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    if(startReceive_.load()) {
        LOG_WARN("The UDP data receive thread has been started!");
        return;
    }

    currentProfile_  = profile;
    frameCallback_   = callback;
    startReceive_.store(true);
    rtpQueue_.reset();
    if(handle_) {
        receiverThread_ = std::thread(&ObRTPNpCapReceiver::frameReceive, this);
        callbackThread_ = std::thread(&ObRTPNpCapReceiver::frameProcess, this);
    }
    else {
        foundPcapHandle_ = false;
        for(int i = 0; i < (int)handles_.size(); ++i) {
            handleThreads_.emplace_back(&ObRTPNpCapReceiver::frameReceive2, this, handles_[i]);
        }
        callbackThread_ = std::thread(&ObRTPNpCapReceiver::frameProcess, this);
    }
}

void ObRTPNpCapReceiver::frameReceive() {
    LOG_DEBUG("start udp data receive thread...");
    struct pcap_pkthdr *header;
    const u_char *      packet;
    while(startReceive_.load()) {
        int res = pcap_next_ex(handle_, &header, &packet);
        if(res > 0) {
            // Ethernet + IP header lengths
            const u_char *payload = packet + 42;
            // Adjust based on header lengths
            int recvLen = header->len - 42;
            if(recvLen > 0) {
                std::vector<uint8_t> data(payload, payload + recvLen);
                rtpQueue_.push(data);
            }
        }
        else {
            if(res == 0) {
                LOG_ERROR_INTVL("Receive rtp packet timeout!");
            }
            else {
                LOG_ERROR_INTVL("Receive rtp packet error: {}", res);
            }
            
        }
    }

    LOG_DEBUG("Exit udp data receive thread...");
}


void ObRTPNpCapReceiver::frameReceive2(pcap_t *handle) {
    LOG_DEBUG("start udp data receive thread...");
    struct pcap_pkthdr *header;
    const u_char *      packet;
    bool receiveData = false;
    while(startReceive_.load()) {
        int res = pcap_next_ex(handle, &header, &packet);
        if(res > 0) {
            foundPcapHandle_ = true;
            if(foundPcapHandle_) {
                handle_ = handle;
            }
            receiveData = true;

            //const u_char *ip_header = packet + 14;                  // 跳过Ethernet头部
            //struct ip_header *ip        = (struct ip_header *)(ip_header);  // IP头部开始处
            //// 获取源IP地址
            //struct in_addr src_ip;
            //src_ip.s_addr = ip->ip_src.s_addr;
            //char src_ip_str[INET_ADDRSTRLEN];
            //inet_ntop(AF_INET, &src_ip, src_ip_str, sizeof(src_ip_str));

            //// 打印源IP地址
            //if(n == 1000) {
            //    LOG_DEBUG("Received UDP packet from IP: {}", src_ip_str);
            //    n = 0;
            //}
            //n++;
            
            // Ethernet + IP header lengths
            const u_char *payload = packet + 42;

            // Adjust based on header lengths
            int recvLen = header->len - 42;
            if(recvLen > 0) {
                std::vector<uint8_t> data(payload, payload + recvLen);
                rtpQueue_.push(data);
            }
        }
        else {
            if(!receiveData && foundPcapHandle_) {
                break;
            }
            LOG_ERROR_INTVL("Receive rtp packet error: {}!", res);
        }
    }
    LOG_DEBUG("Exit udp data receive thread...");
}

void ObRTPNpCapReceiver::parseIPAddress(const u_char *ip_header, std::string &src_ip, std::string &dst_ip) {
    auto ip_src = reinterpret_cast<const uint8_t *>(ip_header + 12);  // 源 IP 地址
    auto ip_dst = reinterpret_cast<const uint8_t *>(ip_header + 16);  // 目的 IP 地址

    src_ip = std::to_string(ip_src[0]) + "." + std::to_string(ip_src[1]) + "." + std::to_string(ip_src[2]) + "." + std::to_string(ip_src[3]);

    dst_ip = std::to_string(ip_dst[0]) + "." + std::to_string(ip_dst[1]) + "." + std::to_string(ip_dst[2]) + "." + std::to_string(ip_dst[3]);
}

void ObRTPNpCapReceiver::frameProcess() {
    LOG_DEBUG("start frame process thread...");
    std::vector<uint8_t> data;
    while(startReceive_.load()) {
        if(rtpQueue_.pop(data)) {
            RTPHeader *header = (RTPHeader *)data.data();
            if(currentProfile_ != nullptr) {
                rtpProcessor_.process(header, data.data(), (uint32_t)data.size(), currentProfile_->getType());
                if(rtpProcessor_.processComplete()) {
                    uint32_t frameDataSize = rtpProcessor_.getFrameDataSize();
                    uint32_t metaDataSize = rtpProcessor_.getMetaDataSize();
                    //LOG_DEBUG("Callback new frame dataSize: {}, number: {}", dataSize, rtpProcessor_.getNumber());

                    auto frame = FrameFactory::createFrameFromStreamProfile(currentProfile_);
                    uint32_t expectedSize = static_cast<uint32_t>(frame->getDataSize());
                    if(frameDataSize > expectedSize) {
                        LOG_WARN_INTVL("{} Receive data size({}) >  expected data size! ({})", currentProfile_->getType(), frameDataSize, expectedSize);
                        rtpProcessor_.reset();
                        continue;
                    }

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
                        LOG_DEBUG("rtp frame {} process error...", currentProfile_->getType());
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

void ObRTPNpCapReceiver::stop() {
    LOG_DEBUG("stop stream start...");
    startReceive_.store(false);
    if(receiverThread_.joinable()) {
        receiverThread_.join();
    }
    for(auto &thread: handleThreads_) {
        if(thread.joinable()) {
            thread.join();
        }
    }
    rtpQueue_.destroy();
    if(callbackThread_.joinable()) {
        callbackThread_.join();
    }
    LOG_DEBUG("stop stream end...");
}

void ObRTPNpCapReceiver::close() {
    if(handles_.size() > 0) {
        for(auto &handle: handles_) {
            if(handle != nullptr) {
                pcap_close(handle);
            }
        }
    }
    else {
        if(handle_ != nullptr) {
            pcap_close(handle_);
        }
    }
    
    
    if(alldevs_ != nullptr) {
        pcap_freealldevs(alldevs_);
    }

    if(recvSocket_ > 0) {
        auto rst = ::closesocket(recvSocket_);
        if(rst < 0) {
            LOG_ERROR("close udp socket failed! socket={0}, err_code={1}", recvSocket_, GET_LAST_ERROR());
        }
    }
    LOG_DEBUG("udp socket closed! socket={}", recvSocket_);
    WSACleanup();
    recvSocket_ = INVALID_SOCKET;
}

#endif  // 0


}  // namespace libobsensor

#endif