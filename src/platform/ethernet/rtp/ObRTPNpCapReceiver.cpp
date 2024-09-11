#include "ObRTPNpCapReceiver.hpp"
#include "logger/Logger.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"
#include "utils/Utils.hpp"
#include "frame/FrameFactory.hpp"
#include "stream/StreamProfile.hpp"

#define OB_UDP_BUFFER_SIZE 1500

namespace libobsensor {

ObRTPNpCapReceiver::ObRTPNpCapReceiver(std::string localAddress, std::string address, uint16_t port)
    : localIp_(localAddress), serverIp_(address), serverPort_(port), startReceive_(false), alldevs_(nullptr), dev_(nullptr), handle_(nullptr) {
    findAlldevs();
}

ObRTPNpCapReceiver::~ObRTPNpCapReceiver() noexcept {
}

void ObRTPNpCapReceiver::findAlldevs() {
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

    if(!foundDevice) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Error opening net device, not found!");
    }

    handle_ = pcap_open_live(dev_->name, 65536, 1, 100, errbuf);
    if(handle_ == nullptr) {
        pcap_freealldevs(alldevs_);
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Error opening net device:" << errbuf);
    }

    if(pcap_datalink(handle_) != DLT_EN10MB) {
        /* Free the device list */
        pcap_freealldevs(alldevs_);
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Error data link:" << pcap_geterr(handle_));
    }


    struct bpf_program fp;
    std::string        strPort      = std::to_string(serverPort_);
    //std::string        stringFilter = "udp port " + strPort;
    //const char *       filter_exp   = "udp and src host 192.168.1.100 and src port 12345";
    std::string stringFilter = "udp and src host " + serverIp_ + " and src port " + strPort;
    if(pcap_compile(handle_, &fp, stringFilter.c_str(), 0, PCAP_NETMASK_UNKNOWN) == -1) {
        pcap_freealldevs(alldevs_);
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Error compiling filter:" << pcap_geterr(handle_));
    }

    if(pcap_setfilter(handle_, &fp) < 0) {
        /* Free the device list */
        pcap_freealldevs(alldevs_);
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Error setting the filter:" << pcap_geterr(handle_));
    }
    pcap_freecode(&fp);

    LOG_DEBUG("Net device opened successfully");
}

void ObRTPNpCapReceiver::start(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    if(startReceive_) {
        LOG_WARN("The UDP data receive thread has been started!");
        return;
    }

    currentProfile_  = profile;
    frameCallback_   = callback;
    startReceive_    = true;
    receiverThread_  = std::thread(&ObRTPNpCapReceiver::startReceive, this);
    callbackThread_  = std::thread(&ObRTPNpCapReceiver::frameProcess, this);
}

void ObRTPNpCapReceiver::startReceive() {
    LOG_DEBUG("start udp data receive thread...");
    struct pcap_pkthdr *header;
    const u_char *      packet;
    //int                 packetCount = 0;
    while(startReceive_) {
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
            LOG_ERROR_INTVL("Receive rtp packet error {}", pcap_geterr(handle_));
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
    while(startReceive_) {
        if(rtpQueue_.pop(data)) {
            RTPHeader *header = (RTPHeader *)data.data();
            if(serverPort_ != 20010) {
                rtpProcessor_.process(header, data.data(), (uint32_t)data.size(), currentProfile_->getType());
                if(rtpProcessor_.processComplete()) {
                    uint32_t dataSize = rtpProcessor_.getDataSize();
                    //LOG_DEBUG("Callback new frame dataSize: {}, number: {}", dataSize, rtpProcessor_.getNumber());

                    auto frame = FrameFactory::createFrameFromStreamProfile(currentProfile_);
                    frame->setSystemTimeStampUsec(utils::getNowTimesUs());
                    frame->setTimeStampUsec(rtpProcessor_.getTimestamp());
                    frame->setNumber(rtpProcessor_.getNumber());
                    frame->updateData(rtpProcessor_.getData(), dataSize);

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

void ObRTPNpCapReceiver::close() {
    startReceive_ = false;
    if(receiverThread_.joinable()) {
        receiverThread_.join();
    }
    rtpQueue_.destroy();
    if(callbackThread_.joinable()) {
        callbackThread_.join();
    }

    if(handle_ != nullptr) {
        pcap_close(handle_);
    }
    
    if(alldevs_ != nullptr) {
        pcap_freealldevs(alldevs_);
    }
}

}  // namespace libobsensor