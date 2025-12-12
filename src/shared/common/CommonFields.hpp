#pragma once
#include <string>
#include <cstdint>

namespace libobsensor {
#pragma pack(push, 1)
struct DeviceInfoEntry {
    uint16_t vid_;
    uint16_t pid_;
    char     deviceName_[32];
    char     manufacturer_[32];
};
struct DeviceIdentifier {
    uint16_t vid_;
    uint16_t pid_;
};
#pragma pack(pop)

typedef enum {
    // Gemini 330 series (DeviceIdentifier)
    OB_DEVICE_GROUP_GEMINI330_PURE   = 0,
    OB_DEVICE_GROUP_GEMINI330L       = 1,
    OB_DEVICE_GROUP_GEMINI335LE      = 2,
    OB_DEVICE_GROUP_DABAIA           = 3,
    OB_DEVICE_GROUP_GEMINI330_DABAIA = 4,
    // Gemini 330 detailed info (DeviceInfoEntry)
    OB_DEVICE_GROUP_GEMINI330_INFO   = 5,
    // Gemini 435 series (DeviceIdentifier)
    OB_DEVICE_GROUP_GEMINI435LE      = 6,
} DeviceGroupType;

//  XML base nodes
constexpr const char *NODE_DEVICE_CONFIG_BASE = "DeviceConfig";
constexpr const char *NODE_DEVLIST            = "DevList";
constexpr const char *NODE_GVCP_CONFIG        = "GVCPConfigList";

// XML field names
constexpr const char *FIELD_VID          = "VID";
constexpr const char *FIELD_PID          = "PID";
constexpr const char *FIELD_NAME         = "Name";
constexpr const char *FIELD_MANUFACTURER = "Manufacturer";
constexpr const char *FIELD_DEVICE       = "Device";

//  XML category nodes
constexpr const char *CATEGORY_G330_DEVICE   = "G330DeviceList";
constexpr const char *CATEGORY_G330L_DEVICE  = "G330DeviceLongBaseList";
constexpr const char *CATEGORY_DABAIA_DEVICE = "DaBaiAList";
constexpr const char *CATEGORY_G335LE_DEVICE = "G335LeDeviceList";
constexpr const char *CATEGORY_G435LE_DEVICE = "G435LeDeviceList";

// GVCP config
constexpr const char *FIELD_MANUFACTURER_NAME = "name";
constexpr const char *FIELD_MANUFACTURER_VID  = "vid";

// Default VID
const uint16_t ORBBEC_DEVICE_VID = 0x2BC5;

}  // namespace libobsensor