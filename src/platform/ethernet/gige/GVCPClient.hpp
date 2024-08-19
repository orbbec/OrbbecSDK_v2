#pragma once
#include "libobsensor/h/ObTypes.h"
#include "ethernet/socket/SocketTypes.hpp"

#include <vector>
#include <string>
#include <mutex>
#include <set>

namespace libobsensor {


#define GVCP_VERSION 0x42  // GVCP协议版本号
#define GVCP_DISCOVERY_FLAGS 0x11
#define GVCP_FORCEIP_FLAGS 0x01
#define GVCP_PORT 3956  // GVCP协议端口号
#define GVCP_REQUEST_ID 0x0001
#define GEV_STATUS_SUCCESS 0x0000

#define GVCP_DISCOVERY_CMD 0x0002
#define GVCP_DISCOVERY_ACK 0x0003
#define GVCP_FORCEIP_CMD 0x0004
#define GVCP_FORCEIP_ACK 0x0005

#pragma pack(push, 1)
struct gvcp_cmd_header {
    uint8_t  cMsgKeyCode;  // 0x42
    uint8_t  cFlag;        // 0x11 allow broadcast ack;ack required
    uint16_t wCmd;         // discovery_cmd=2;FORCEIP_CMD = 4;READREG_CMD=0x80
    uint16_t wLen;         // payload length
    uint16_t wReqID;       // request id = 1;READREG id=12345
};

struct gvcp_forceip_payload {
    uint8_t Mac[8];       // last 6 byte
    uint8_t CurIP[16];    // last 4 byte
    uint8_t SubMask[16];  // last 4 byte
    uint8_t Gateway[16];  // last 4 byte
};

struct gvcp_ack_header {
    uint16_t wStatus;  // success=0;
    uint16_t wAck;     // discover_ack=3;forceip_ack=5;READREG_ACK=0x81
    uint16_t wLen;
    uint16_t wReqID;
};

// 注意：大端模式
struct gvcp_ack_payload {
    uint32_t dwSpecVer;
    uint32_t dwDevMode;
    uint8_t  Mac[8];  // last 6 byte
    uint32_t dwSupIpSet;
    uint32_t dwCurIpSet;
    // uint8 unused1[12];
    uint8_t  CurIP[16];        // last 4 byte
    uint8_t  SubMask[16];      // last 4 byte
    uint8_t  Gateway[16];      // last 4 byte
    char     szFacName[32];    // first
    char     szModelName[28];  // first
    uint32_t dwPID;
    char     szDevVer[32];
    char     szFacInfo[48];
    char     szSerial[16];
    char     szUserName[16];
};

struct gvcp_discover_cmd {
    struct gvcp_cmd_header header;
};
struct gvcp_discover_ack {
    struct gvcp_ack_header  header;
    struct gvcp_ack_payload payload;
};
struct gvcp_forceip_cmd {
    struct gvcp_cmd_header      header;
    struct gvcp_forceip_payload payload;
};
struct gvcp_forceip_ack {
    struct gvcp_ack_header header;
};
#pragma pack(pop)

struct GVCPDeviceInfo {
    std::string mac     = "unknown";
    std::string ip      = "unknown";
    std::string mask    = "unknown";
    std::string gateway = "unknown";
    std::string sn      = "unknown";
    std::string name    = "unknown";
    uint32_t    pid     = 0;
    // std::string version      = "";
    // std::string manufacturer = "";

    virtual bool operator==(const GVCPDeviceInfo &other) const {
        return other.mac == mac && other.sn == sn && other.ip == ip;
    }
    virtual ~GVCPDeviceInfo() {}
};

#define MAX_SOCKETS 32

class GVCPClient {
public:
    ~GVCPClient();

    std::vector<GVCPDeviceInfo> queryNetDeviceList();
    bool                       changeNetDeviceIpConfig(std::string mac, const OBNetIpConfig &config);

    static GVCPClient &instance() {
        static GVCPClient instance;
        return instance;
    }

private:
    GVCPClient();
    int    openClientSockets();
    void   closeClientSockets();
    SOCKET openClientSocket(SOCKADDR_IN addr);
    void   sendGVCPDiscovery(SOCKET sock);
    void   sendGVCPForceIP(SOCKET sock, std::string mac, const OBNetIpConfig &config);

    //
    void checkAndUpdateSockets();

private:
    SOCKET                     socks_[MAX_SOCKETS];
    int                        sockCount_ = 0;
    std::vector<GVCPDeviceInfo> devInfoList_;
    std::mutex                 queryMtx_;
    std::mutex                 devInfoListMtx_;

#ifndef WIN32
    std::set<std::string> ipAddressStrSet_;
#endif
};


}  // namespace libobsensor