#include "ObPTPPacketCreator.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "logger/LoggerInterval.hpp"
#include "ethernet/socket/SocketTypes.hpp"

namespace libobsensor {

ObPTPPacketCreator::ObPTPPacketCreator(){
}

ObPTPPacketCreator::~ObPTPPacketCreator() noexcept {
}


void ObPTPPacketCreator::setMacAddress(unsigned char *destMac, unsigned char *srcMac) {
    memcpy(destMac_, destMac, 6);
    memcpy(srcMac_, srcMac, 6);
}

int ObPTPPacketCreator::createPTPPacket(PTPPacketControlType type, Frame1588 *frame1588) {
    LOG_DEBUG("Create PTP Packet type: {}", (int)type);

    memset(frame1588, 0, sizeof(Frame1588));

    unsigned short TmpType = htons(0x88F7);
    memcpy((void *)frame1588->macdata, (void *)destMac_, 6);       // dst mac
    memcpy((void *)(frame1588->macdata + 6), (void *)srcMac_, 6);  // src mac
    memcpy((void *)(frame1588->macdata + 12), (void *)&TmpType, 2);

    frame1588->reservedAndVersionPTP = 0;

    int len = 0;
    if(type == PTP_ACK_CONTROL) {
        frame1588->messageLength                   = htons(0x2c);
        frame1588->transportSpecificAndMessageType = PTPACK_MSSID;
        len                                        = 14 + 44;
    } else {
        if(type == SYNC_CONTROL) {
            frame1588->messageLength                   = htons(0x2c);
            frame1588->transportSpecificAndMessageType = SYNC_MSSID;
            frame1588->control                         = 0;
        }
        else if(type == FOLLOW_UP_CONTROL) {
            frame1588->messageLength                   = htons(0x2c);
            frame1588->transportSpecificAndMessageType = FOLLOW_UP_MSSID;
            frame1588->control                         = 2;
        }
        else if(type == DELAY_RESP_CONTROL) {
            frame1588->messageLength                   = htons(0x36);
            frame1588->transportSpecificAndMessageType = DELAY_RESP_MSSID;
            frame1588->control                         = 3;
        }

        frame1588->domainNumber = 0;
        frame1588->rxSeconds    = 0;
        memset((void *)frame1588->flags, 0, 2);
        frame1588->correctionField = 0;
        frame1588->rxNanoSeconds   = 0;
        memset((void *)frame1588->sourcePortId, 0, 10);

        frame1588->sequenceId = 0;

        frame1588->logMeanMessageInterval = 0;

        uint32_t s_time;
        uint32_t n_time;
        getCurrentTimeStamp(s_time, n_time);

        frame1588->txEpoch       = 0;
        frame1588->txSeconds     = s_time;
        frame1588->txNanoSeconds = n_time;
        if(type == SYNC_CONTROL || type == FOLLOW_UP_CONTROL) {
            len = 14 + 44;
        }
        else if(type == DELAY_RESP_CONTROL) {
            len = 14 + 54;
        }
    }
    
    return len;
}

void ObPTPPacketCreator::getCurrentTimeStamp(unsigned int &s_time, unsigned int &n_time) {
    /*std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm *   now_tm     = std::localtime(&now_time_t);

    char buffer[128];
    strftime(buffer, sizeof(buffer), "%F %T", now_tm);

    std::ostringstream ss;
    ss.fill('0');

    std::chrono::milliseconds ms;
    std::chrono::microseconds cs;
    std::chrono::nanoseconds  ns;

    std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    ms                     = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    cs                     = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()) % 1000000;
    ns                     = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()) % 1000000000;

    ss << buffer << ":" << ms.count() << ":" << cs.count() % 1000 << ":" << ns.count() % 1000;
    std::string s_print = ss.str();
    s_time              = static_cast<int>(s.count());
    n_time              = static_cast<int>(ns.count());*/

    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

    std::chrono::seconds seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    s_time                       = static_cast<unsigned int>(seconds.count());

    std::chrono::nanoseconds ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()) % 1000000000;
    n_time                      = static_cast<unsigned int>(ns.count());
}

}  // namespace libobsensor