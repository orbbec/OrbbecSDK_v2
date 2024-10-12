#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))

#include "ObPTPHost.hpp"
#include "logger/Logger.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/Utils.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <cstring>

namespace libobsensor {

#define OB_UDP_BUFFER_SIZE 1600

ObPTPHost::ObPTPHost(std::string localMac, std::string localIP, std::string address, uint16_t port, std::string mac)
    : localMac_(localMac), localIp_(localIP), serverIp_(address), serverPort_(port), serverMac_(mac), startSync_(false), alldevs_(nullptr), handle_(nullptr) {
    convertMacAddress();
    findDevice();
}

ObPTPHost::~ObPTPHost() noexcept {
    destroy();
}

void ObPTPHost::convertMacAddress() {
    std::istringstream dstMac(serverMac_);
    std::string        item;
    int                i = 0;
    while(std::getline(dstMac, item, ':') && i < 6) {
        destMac_[i] = static_cast<unsigned char>(std::stoi(item, nullptr, 16));
        i++;
    }

    std::istringstream srcMac(localMac_);
    std::string        item2;
    int                j = 0;
    while(std::getline(srcMac, item2, ':') && j < 6) {
        srcMac_[j] = static_cast<unsigned char>(std::stoi(item2, nullptr, 16));
        j++;
    }

    ptpPacketCreator_.setMacAddress(destMac_, srcMac_);
}

void ObPTPHost::findDevice() {
    handle_  = nullptr;
    alldevs_ = nullptr;
    char errbuf[PCAP_ERRBUF_SIZE];
    if(pcap_findalldevs(&alldevs_, errbuf) == -1) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Failed to finding net devices, err_msg=" << errbuf);
    }

    if(alldevs_ == nullptr) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "No net devices found!");
    }

    LOG_DEBUG("Available devices:");
    pcap_if_t *dev_;
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
        std::string stringFilter = "ether src " + serverMac_;
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

            LOG_DEBUG("Device Name: {}", (dev_->name ? dev_->name : "No name"));

            pcap_t *handle = pcap_open_live(dev_->name, OB_UDP_BUFFER_SIZE, 1, COMM_TIMEOUT_MS, errbuf);
            if(handle == nullptr) {
                LOG_WARN("Error opening net device: {}", errbuf);
                continue;
            }

            //pcap_set_timeout(handle, 50);

            if(pcap_datalink(handle) != DLT_EN10MB) {
                LOG_WARN("Error data link: {}", errbuf);
                pcap_close(handle);
                continue;
            }

            struct bpf_program fp;
            std::string stringFilter = "ether src " + serverMac_;
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

            
            Frame1588 ackControlFrame;
            int       len = 0;
            len           = ptpPacketCreator_.createPTPPacket(PTP_ACK_CONTROL, &ackControlFrame);
            int ackCount  = 3;
            while(ackCount != 0) {
                ackCount--;
                sendPTPPacket(handle, &ackControlFrame, len);
                struct pcap_pkthdr *header;
                const u_char *      packet;
                int                 res = pcap_next_ex(handle, &header, &packet);
                if(res > 0) {
                    if(header->len == 60) {
                        bool equalMAC = true;
                        Frame1588 *ptpdata = (Frame1588 *)packet;
                        if(ptpdata->transportSpecificAndMessageType == PTPACK_MSSID) {
                            for(int i = 6; i < 12; i++) {
                                if(destMac_[i - 6] != ptpdata->macdata[i]) {
                                    equalMAC = false;
                                }
                            }
                        }
                        else {
                            equalMAC = false;
                        }

                        if(equalMAC) {
                            foundDevice = true;
                            handle_     = handle;
                            break;
                        }
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
            
            if(!foundDevice) {
                pcap_close(handle);
            }

        }
    }

    if(!foundDevice) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Error opening net device, not found!");
    }

    LOG_DEBUG("Net device opened successfully");
}

void ObPTPHost::timeSync() {
    if(startSync_) {
        LOG_WARN("The PTP data receive has been started!");
        return;
    }

    if(handle_) {
        startSync_ = true;
        
        // SYNC_CONTROL
        Frame1588 syncControlFrame;
        int       len = ptpPacketCreator_.createPTPPacket(SYNC_CONTROL, &syncControlFrame);
        t1[0]         = syncControlFrame.txSeconds;
        t1[1]         = syncControlFrame.txNanoSeconds;
        sendPTPPacket(handle_, &syncControlFrame, len);

        // FOLLOW_UP_CONTROL
        Frame1588 followUpControlControlFrame;
        len                                       = ptpPacketCreator_.createPTPPacket(FOLLOW_UP_CONTROL, &followUpControlControlFrame);
        followUpControlControlFrame.txSeconds     = t1[0];
        followUpControlControlFrame.txNanoSeconds = t1[1];
        sendPTPPacket(handle_, &followUpControlControlFrame, len);

        receivePTPPacket(handle_);
    }
    else {
        LOG_ERROR("PTP time synchronization failure, net handle is null!");
    }
}

void ObPTPHost::receivePTPPacket(pcap_t *handle) {
    LOG_DEBUG("start ptp data receive...");
    struct pcap_pkthdr *header;
    const u_char *      packet;
    int                 maxRevCount = 20;
    while(startSync_) {
        if(maxRevCount == 0) {
            break;
        }
        int res = pcap_next_ex(handle, &header, &packet);
        if(res > 0) {
            if(header->len == 60) {
                LOG_DEBUG("Receive PTP packet!");
                bool       equalMAC = true;
                Frame1588 *ptpdata  = (Frame1588 *)packet;
                if(ptpdata->transportSpecificAndMessageType == DELAY_REQ_MSSID) {
                    LOG_DEBUG("Receive DELAY_REQ_MSSID PTP packet!");
                    for(int i = 6; i < 12; i++) {
                        if(destMac_[i - 6] != ptpdata->macdata[i]) {
                            equalMAC = false;
                        }
                    }
                }
                else {
                    equalMAC = false;
                }

                if(equalMAC) {
                    LOG_DEBUG("Send PTP DELAY_RESP_CONTROL packet.");
                    Frame1588 delayRespControlFrame;
                    int       len = ptpPacketCreator_.createPTPPacket(DELAY_RESP_CONTROL, &delayRespControlFrame);
                    sendPTPPacket(handle, &delayRespControlFrame, len);
                    break;
                }
            }

            // struct tm  ltime;
            // char       timestr[16];
            // Frame1588 *ptpdata;
            // time_t local_tv_sec;

            /* convert the timestamp to readable format */
            // local_tv_sec = header->ts.tv_sec;
            // localtime_s(&ltime, &local_tv_sec);
            // strftime(timestr, sizeof timestr, "%H:%M:%S", &ltime);

            /* print timestamp and length of the packet */
            // printf("%s.%.6d len:%d ", timestr, header->ts.tv_usec, header->len);

            // ptpdata = (Frame1588 *)(buffer.data() + 44);  // length of ethernet header
        }
        else {
            if(res == 0) {
                LOG_ERROR_INTVL("Receive ptp packet timeout!");
            }
            else {
                LOG_ERROR_INTVL("Receive ptp packet error: {}", res);
            }
        }
        maxRevCount--;
    }

    LOG_DEBUG("Exit ptp data receive...");
}

void ObPTPHost::sendPTPPacket(pcap_t *handle, void *data, int len) {
    int res = pcap_sendpacket(handle, (u_char *)data, len);
    if(res < 0) {
        LOG_ERROR("Send ptp data packet failed!");
    }
}

void ObPTPHost::destroy() {
    LOG_DEBUG("close start...");
    startSync_ = false;
    if(handle_ != nullptr) {
        pcap_close(handle_);
        handle_ = nullptr;
    }

    if(alldevs_ != nullptr) {
        pcap_freealldevs(alldevs_);
        alldevs_ = nullptr;
    }
    LOG_DEBUG("close end...");
}

}  // namespace libobsensor

#endif