#pragma once

#include <errno.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <unistd.h>

#include <iostream>
#include <thread>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>

#include <linux/uvcvideo.h>
#include <linux/videodev2.h>
#include <linux/usb/video.h>

#include "libobsensor/h/ObTypes.h"

#include <mutex>
#include <condition_variable>

namespace libobsensor {
//----------------------------------------------------------------------------
// GMSL MIPI & V4L2 CMD protocol code
#define DS5_DEPTH_STREAM_DT 0x4000
#define DS5_CAMERA_CID_BASE (V4L2_CTRL_CLASS_CAMERA | DS5_DEPTH_STREAM_DT)

#define V4L2_EXT_SENSOR_MODE_ID 0x009a4001
#define V4L2_EXT_TRIG_PIN_ID 0x009a4002
#define V4L2_EXT_TRIG_MODE_ID 0x009a4002

#define G2R_CAMERA_CID_HWMC_R (DS5_CAMERA_CID_BASE + 9)
#define G2R_CAMERA_CID_HWMC_W (DS5_CAMERA_CID_BASE + 32)
#define G2R_CAMERA_CID_SET_DATA (DS5_CAMERA_CID_BASE + 33)
#define G2R_CAMERA_CID_GET_VERSION_DATA (DS5_CAMERA_CID_BASE + 34)
#define G2R_CAMERA_CID_GET_IMU_STOCK (DS5_CAMERA_CID_BASE + 35)
#define G2R_CAMERA_CID_GET_IMU_DATA (DS5_CAMERA_CID_BASE + 36)
#define G2R_CAMERA_CID_SET_IMU (DS5_CAMERA_CID_BASE + 37)
#define ORBBEC_CAMERA_CID_GET_IMU_FPS (DS5_CAMERA_CID_BASE + 37)

#define G2R_CAMERA_CID_SET_DATA_LEN (DS5_CAMERA_CID_BASE + 38)
#define G2R_CAMERA_CID_GET_DATA (DS5_CAMERA_CID_BASE + 39)

#define G2R_CAMERA_CID_SET_GPIO (DS5_CAMERA_CID_BASE + 40)
#define G2R_CAMERA_CID_RESET_POWER (DS5_CAMERA_CID_BASE + 41)
#define G2R_CAMERA_CID_GET_PID_SN (DS5_CAMERA_CID_BASE + 42)

#define G2R_SET_IMU_LEN 2
#define G2R_GET_IMU_DATA_LEN 34
#define G2R_GET_IMU_STOCK_LEN 14

#define G2R_GET_VERSION_DATA_LEN 172
#define G2R_RW_DATA_LEN 256

#define G2R_GET_VERSION_CMD_CODE 3
#define G2R_GET_VERSION_CMD_LEN 8

//-------------------------------------------------------------------------------------------
// IMU
#define SENSOR_PROPERTY_GYRO_SWITCH_BOOL 2019
#define SENSOR_PROPERTY_ACCEL_SWITCH_BOOL 2020
#define SENSOR_PROPERTY_GYRO_ODR_INT 2021
#define SENSOR_PROPERTY_ACCEL_ODR_INT 2022
#define SENSOR_PROPERTY_GYRO_FULL_SCALE_INT 2023
#define SENSOR_PROPERTY_ACCEL_FULL_SCALE_INT 2024

typedef enum {
    PROTOCOL_IMU_ODR_1_5625_HZ = 1,
    PROTOCOL_IMU_ODR_3_125_HZ,
    PROTOCOL_IMU_ODR_6_25_HZ,
    PROTOCOL_IMU_ODR_12_5_HZ,
    PROTOCOL_IMU_ODR_25_HZ,
    PROTOCOL_IMU_ODR_50_HZ,
    PROTOCOL_IMU_ODR_100_HZ,
    PROTOCOL_IMU_ODR_200_HZ,
    PROTOCOL_IMU_ODR_500_HZ,
    PROTOCOL_IMU_ODR_1_KHZ,
    PROTOCOL_IMU_ODR_2_KHZ,
    PROTOCOL_IMU_ODR_4_KHZ,
    PROTOCOL_IMU_ODR_8_KHZ,
    PROTOCOL_IMU_ODR_16_KHZ,
    PROTOCOL_IMU_ODR_32_KHZ,
    PROTOCOL_IMU_ODR_MAX,
} PROTOCOL_IMU_ODR_EM;

typedef enum {
    PROTOCOL_GYRO_FS_16dps = 1,
    PROTOCOL_GYRO_FS_31dps,
    PROTOCOL_GYRO_FS_62dps,
    PROTOCOL_GYRO_FS_125dps,
    PROTOCOL_GYRO_FS_250dps,
    PROTOCOL_GYRO_FS_500dps,
    PROTOCOL_GYRO_FS_1000dps,
    PROTOCOL_GYRO_FS_2000dps,
    PROTOCOL_GYRO_FS_MAX,
} PROTOCOL_GYRO_FS_EM;

typedef enum {
    PROTOCOL_ACCEL_FS_2g = 1,
    PROTOCOL_ACCEL_FS_4g,
    PROTOCOL_ACCEL_FS_8g,
    PROTOCOL_ACCEL_FS_16g,
    PROTOCOL_ACCEL_FS_MAX,
} PROTOCOL_ACCEL_FS_EM;

typedef struct _imu_origin_data_t {
    int16_t  group_id;
    int16_t  accel_x;
    int16_t  accel_y;
    int16_t  accel_z;
    int16_t  gyro_x;
    int16_t  gyro_y;
    int16_t  gyro_z;
    int16_t  temp;
    uint32_t timestamp[2];
} imu_origin_data_t;
//-------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------
// USB pack protocol
#define MAX_PACKET_SIZE_GMSL 256 - 6

#pragma pack(push, 1)
typedef struct {
    uint16_t magic;
    uint16_t halfWordSize;  // 去除头部后剩余data的 bytesize/2 -> halfwordSize
    uint16_t opcode;
    uint16_t nId;
} ProtocolHeader;

typedef struct {
    uint16_t res;
    uint8_t  data[MAX_PACKET_SIZE_GMSL - 2];
} ProtocolResp;

typedef struct __attribute__((__packed__)) {
    union {
        uint8_t      _data[MAX_PACKET_SIZE_GMSL];
        ProtocolResp resp;
    } data;

    uint16_t len;
} ProtocolBuf;

typedef struct __attribute__((__packed__)) {
    ProtocolHeader header;
    ProtocolBuf    buf;
} ProtocolMsg;

#pragma pack(pop)

// static ProtocolMsg request, response;
//-------------------------------------------------------------------------------------------

struct device_info {
    uint16_t pid;
    char     sn[16];
    char     asic_sn[16];
};

//----------------------------------------------------
struct orbbec_device_info {
    uint16_t vid;
    uint16_t pid;
    uint8_t  sn[16];
    uint8_t  asic_sn[16];
    uint16_t video_type;
    uint16_t sub_num;
    uint32_t cam_num;
};
// video_type 的枚举
enum orb_mux_pad {
    ORB_MUX_PAD_EXTERNAL,  // external
    ORB_MUX_PAD_DEPTH,
    ORB_MUX_PAD_IR_L,
    ORB_MUX_PAD_IR_R,
    ORB_MUX_PAD_RGB,
    ORB_MUX_PAD_COUNT,
};
//----------------------------------------------------

//-------------------------------------------------------------------------------------------
/*
static imu_map_t imu_accel_odr_map[] = {
    {PROTOCOL_IMU_ODR_500_HZ, IMU_ACCEL_ODR_500HZ, 500},
    //{PROTOCOL_IMU_ODR_1_5625_HZ, IMU_ACCEL_ODR_1HZ5625},
    //{PROTOCOL_IMU_ODR_3_125_HZ, IMU_ACCEL_ODR_3HZ125},
    //{PROTOCOL_IMU_ODR_6_25_HZ, IMU_ACCEL_ODR_6HZ25},
    //{PROTOCOL_IMU_ODR_12_5_HZ, IMU_ACCEL_ODR_12HZ5},
    //{PROTOCOL_IMU_ODR_25_HZ, IMU_ACCEL_ODR_25HZ},
    {PROTOCOL_IMU_ODR_50_HZ, IMU_ACCEL_ODR_50HZ, 50},
    {PROTOCOL_IMU_ODR_100_HZ, IMU_ACCEL_ODR_100HZ, 100},
    {PROTOCOL_IMU_ODR_200_HZ, IMU_ACCEL_ODR_200HZ, 200},
    {PROTOCOL_IMU_ODR_1_KHZ, IMU_ACCEL_ODR_1KHZ, 1000},
    //{PROTOCOL_IMU_ODR_2_KHZ, IMU_ACCEL_ODR_2KHZ},
    //{PROTOCOL_IMU_ODR_4_KHZ, IMU_ACCEL_ODR_4KHZ},
    //{PROTOCOL_IMU_ODR_8_KHZ, IMU_ACCEL_ODR_8KHZ},
    //{PROTOCOL_IMU_ODR_16_KHZ, IMU_ACCEL_ODR_16KHZ},
    //{PROTOCOL_IMU_ODR_32_KHZ, IMU_ACCEL_ODR_32KHZ},
};

static imu_map_t imu_gyro_odr_map[] = {
    {PROTOCOL_IMU_ODR_500_HZ, IMU_GYRO_ODR_500HZ, 500},
    //{PROTOCOL_IMU_ODR_12_5_HZ, IMU_GYRO_ODR_12HZ5},
    //{PROTOCOL_IMU_ODR_25_HZ, IMU_GYRO_ODR_25HZ},
    {PROTOCOL_IMU_ODR_50_HZ, IMU_GYRO_ODR_50HZ, 50},
    {PROTOCOL_IMU_ODR_100_HZ, IMU_GYRO_ODR_100HZ, 100},
    {PROTOCOL_IMU_ODR_200_HZ, IMU_GYRO_ODR_200HZ, 200},
    {PROTOCOL_IMU_ODR_1_KHZ, IMU_GYRO_ODR_1KHZ, 1000},
    //{PROTOCOL_IMU_ODR_2_KHZ, IMU_GYRO_ODR_2KHZ},
    //{PROTOCOL_IMU_ODR_4_KHZ, IMU_GYRO_ODR_4KHZ},
    //{PROTOCOL_IMU_ODR_8_KHZ, IMU_GYRO_ODR_8KHZ},
    //{PROTOCOL_IMU_ODR_16_KHZ, IMU_GYRO_ODR_16KHZ},
    //{PROTOCOL_IMU_ODR_32_KHZ, IMU_GYRO_ODR_32KHZ},
};

static imu_map_t imu_accel_fs_map[] = {
    //{PROTOCOL_ACCEL_FS_2g, IMU_ACCEL_FS_SEL_2g},
    {PROTOCOL_ACCEL_FS_4g, IMU_ACCEL_FS_SEL_4g, 4},
    //{PROTOCOL_ACCEL_FS_8g, IMU_ACCEL_FS_SEL_8g},
    //{PROTOCOL_ACCEL_FS_16g, IMU_ACCEL_FS_SEL_16g},
};

static imu_map_t imu_gyro_fs_map[] = {
    //{PROTOCOL_GYRO_FS_16dps, IMU_GYRO_FS_SEL_15DPS625},
    //{PROTOCOL_GYRO_FS_31dps, IMU_GYRO_FS_SEL_31DPS25},
    //{PROTOCOL_GYRO_FS_62dps, IMU_GYRO_FS_SEL_62DPS5},
    //{PROTOCOL_GYRO_FS_125dps, IMU_GYRO_FS_SEL_125DPS},
    //{PROTOCOL_GYRO_FS_250dps, IMU_GYRO_FS_SEL_250DPS, 250},
    //{PROTOCOL_GYRO_FS_500dps, IMU_GYRO_FS_SEL_500DPS},
    {PROTOCOL_GYRO_FS_1000dps, IMU_GYRO_FS_SEL_1000DPS, 1000},
    //{PROTOCOL_GYRO_FS_2000dps, IMU_GYRO_FS_SEL_2000DPS},
};
*/
//-------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------
// GMSL MIPI & firmware I2C protocol
#define MAX_I2C_PACKET_SIZE 256
#define MAX_I2C_PACKET_HEADER 6
#define MAX_I2C_PACKET_SEND_DATA_MAX_SIZE (MAX_I2C_PACKET_SIZE - MAX_I2C_PACKET_HEADER)
#define MAX_I2C_PACKET_RECV_DATA_MAX_SIZE (MAX_I2C_PACKET_SIZE - MAX_I2C_PACKET_HEADER - 2)

#define CUR_I2C_PACKET_SIZE (256 - 16 - 2 - 2) - 4              // 232 //246 //128 //160 //172 //192 //208 //224 //240 //246 //252  //fw max_read_len is 252
#define OB_GMSL_FW_I2C_DATA_LEN_CUR_MAX (256 - 16 - 2 - 2) - 4  // 232 //246 //128 //160 //172 //192 //208 //224 //240 //246 //252  //fw max_read_len is 252

struct orbbec_header {
    uint16_t len;
    uint16_t code;
    uint16_t index;
};

struct orbbec_cmd {
    struct orbbec_header header;
    uint8_t              _data[MAX_I2C_PACKET_SIZE];
};

typedef struct __attribute__((__packed__)) {
    uint16_t len;
    uint16_t code;
    uint16_t index;
} i2c_msg_header_t;

typedef struct __attribute__((__packed__)) {
    uint16_t res;
    uint8_t  data[MAX_I2C_PACKET_RECV_DATA_MAX_SIZE];
} i2c_msg_body_t;

typedef struct __attribute__((__packed__)) {
    i2c_msg_header_t header;
    union {
        uint8_t        _data[MAX_I2C_PACKET_SEND_DATA_MAX_SIZE];
        i2c_msg_body_t body;
    };
} i2c_msg_t;
// i2c_msg_t req, resp;

typedef struct __attribute__((__packed__)) {
    char     firmware_version[16];
    char     system_version[16];
    char     sdk_version[16];
    char     depth_chip[16];
    char     system_chip[16];
    char     serial_number[16];
    uint32_t device_type;
    char     deviceName[16];
    char     reseverd[48];
} VersionInfoTypeDef;
//-------------------------------------------------------------------------------------------

extern std::mutex mMultiThreadI2CMutex;
extern int        xioctlGmsl(int fh, unsigned long request, void *arg);

}  // namespace libobsensor