// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "libobsensor/h/ObTypes.h"
#include "ethernet/socket/SocketTypes.hpp"

namespace libobsensor {

#define GVCP_KEY_CODE (0x42)  // GVCP Hard-coded key code value used for message validation
#define GVCP_DISCOVERY_FLAGS (0x11)
#define GVCP_FORCEIP_FLAGS (0x11)
#define GVCP_PORT (3956)  // GVCP protocol port number
#define GVCP_REQUEST_ID (0x0001)
#define GVCP_MAX_PACK_SIZE (548)  // Total 576 = 20(IP header without options) + 8(UDP header) + 8(GVCP header) + 540(Max. GVCP payload)

#pragma pack(push, 1)
// GVCP status codes (subset)
typedef enum : uint16_t {
    GEV_STATUS_SUCCESS           = 0x0000,
    GEV_STATUS_NOT_IMPLEMENTED   = 0x8001,
    GEV_STATUS_INVALID_PARAMETER = 0x8002,
    GEV_STATUS_INVALID_ADDRESS   = 0x8003,
    GEV_STATUS_WRITE_PROTECT     = 0x8004,
    GEV_STATUS_BAD_ALIGNMENT     = 0x8005,
    GEV_STATUS_ACCESS_DENIED     = 0x8006,
    GEV_STATUS_BUSY              = 0x8007,
    GEV_STATUS_ERROR             = 0x8FFF,
} GVCPStatusCode;

// GVCP commad and ack values (subset)
typedef enum : uint16_t {
    // Discovery Protocol Control
    GVCP_DISCOVERY_CMD = 0x0002,
    GVCP_DISCOVERY_ACK = 0x0003,
    GVCP_FORCEIP_CMD   = 0x0004,
    GVCP_FORCEIP_ACK   = 0x0005,
    // Device Memory Access
    GVCP_READREG_CMD  = 0x0080,
    GVCP_READREG_ACK  = 0x0081,
    GVCP_WRITEREG_CMD = 0x0082,
    GVCP_WRITEREG_ACK = 0x0083,
} GVCPCmdAndAck;

// GVCP register values (subset)
typedef enum : uint16_t {
    GVCP_CAPABILITY_REGISTER        = 0x0934,
    GVCP_HEARTBEAT_TIMEOUT_REGISTER = 0x0938,
    GVCP_CCP_REGISTER               = 0x0A00,
} GVCPRegister;

// CCP bit mask
// set bit 30 and 31 to 1 (bit0=MSB)
#define GVCP_CCP_EXCLUSIVE_ACCESS (0x01)
// set bit 30 to 1 ((bit0=MSB)
#define GVCP_CCP_CONTROL_ACCESS (0x02)
#define GVCP_CCP_OPEN_ACCESS (0x00)
// bit 29 30 and 31 ((bit0=MSB)
#define GVCP_CCP_MASK (0x07)

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

// Note: Big-endian mode
struct gvcp_discover_ack_payload {
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
    struct gvcp_ack_header           header;
    struct gvcp_discover_ack_payload payload;
};
struct gvcp_forceip_cmd {
    struct gvcp_cmd_header      header;
    struct gvcp_forceip_payload payload;
};
struct gvcp_forceip_ack {
    struct gvcp_ack_header header;
};
// read register cmd(single register)
struct gvcp_readreg_cmd {
    struct gvcp_cmd_header header;
    uint32_t               address;
};
// read register ack(single register)
struct gvcp_readreg_ack {
    struct gvcp_ack_header header;
    uint32_t               value;
};
// write register cmd(single register)
struct gvcp_writereg_cmd {
    struct gvcp_cmd_header header;
    uint32_t               address;
    uint32_t               value;
};
// write register ack(single register)
struct gvcp_writereg_ack {
    struct gvcp_ack_header header;
    uint16_t               reserved;
    uint16_t               index;
};
#pragma pack(pop)

}  // namespace libobsensor
