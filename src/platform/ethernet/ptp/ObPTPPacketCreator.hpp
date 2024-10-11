#pragma once
#include <string>

namespace libobsensor {

typedef enum {
    SYNC_CONTROL       = 0x00,
    DELAY_REQ_CONTROL  = 0x01,
    FOLLOW_UP_CONTROL  = 0x02,
    DELAY_RESP_CONTROL = 0x03,
    MANAGEMENT_CONTROL = 0x04,
    ALL_OTHERS_CONTROL = 0x05,
    PTP_ACK_CONTROL    = 0x06,
} PTPPacketControlType;

typedef enum {
    SYNC_MSSID            = 0x0,
    DELAY_REQ_MSSID       = 0x01,
    PATH_DELAY_REQ_MSSID  = 0x02,
    PATH_DELAY_RESP_MSSID = 0x03,

    FOLLOW_UP_MSSID            = 0x08,
    DELAY_RESP_MSSID           = 0x09,
    PATH_DELAY_FOLLOW_UP_MSSID = 0x0A,
    ANNOUNCE_MSSID             = 0x0B,
    SIGNALING_MSSID            = 0x0C,
    MANAGEMENT_MSSID           = 0x0D,
    RAPTORTEST_MSSID           = 0X0E,
    PTPACK_MSSID               = 0X0F,
} PTPMessageId;

#pragma pack(1)

typedef struct {
    unsigned char macdata[14];  // dst mac(6byte) ,src mac(6byte),type(2byte)

    unsigned char  transportSpecificAndMessageType;  // 00       1 (2 4-bit fields)
    unsigned char  reservedAndVersionPTP;            // 01       1 (2 4-bit fields)
    unsigned short messageLength;                    // 02       2
    unsigned char  domainNumber;                     // 04       1
    unsigned char  rxSeconds;                        // 05       1  // reserved - stores LSB of seconds receive timestamp inserted by DP83640
    unsigned char  flags[2];                         // 06       2
    long long      correctionField;                  // 08       8
    unsigned int   rxNanoSeconds;                    // 16       4
    unsigned char  sourcePortId[10];                 // 20      10
    unsigned short sequenceId;                       // 30       2
    unsigned char  control;                          // 32       1
    unsigned char  logMeanMessageInterval;           // 33       1
    unsigned short txEpoch;                          // 34       2  // used in Sync, Delay_Req, Delay_Resp
    unsigned int   txSeconds;                        // 36       4  // used in Sync, Delay_Req, Delay_Resp
    unsigned int   txNanoSeconds;                    // 40       4  // used in Sync, Delay_Req, Delay_Resp
    unsigned char  requestingPortId[10];             // 44       10 // used in Delay_Resp
} Frame1588;

#pragma pack()

class ObPTPPacketCreator {
public:
    ObPTPPacketCreator();
    ~ObPTPPacketCreator() noexcept;

public:
    void setMacAddress(unsigned char *destMac, unsigned char *srcMac);
    int  createPTPPacket(PTPPacketControlType type, Frame1588 *frame);
    void getCurrentTimeStamp(unsigned int &s_time, unsigned int &n_time);

private:
    unsigned char destMac_[6];  // Destination MAC address
    unsigned char srcMac_[6];   // Source MAC address
};

}  // namespace libobsensor
