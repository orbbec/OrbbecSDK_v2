// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G330DeviceLicenseInfoManager.hpp"

#include "environment/EnvConfig.hpp"
#include "exception/ObException.hpp"
#include "logger/Logger.hpp"
#include "utils/Utils.hpp"
#include "context/DynamicLibraryManager.hpp"

#include "libobsensor/h/Device.h"
#include "libobsensor/h/Error.h"

#include <dylib.hpp>
#include <json/json.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace libobsensor {

namespace {

// keys for license info json
constexpr const char kFormatVersionKey[] = "formatVersion";
constexpr const char kDeviceInfoKey[]    = "devInfo";
constexpr const char kDeviceSnKey[]      = "deviceSn";
constexpr const char kDeviceVidKey[]     = "vid";
constexpr const char kDevicePidKey[]     = "pid";
constexpr const char kLicenseInfoKey[]   = "licenseInfo";
constexpr const char kSchemaVersionKey[] = "schemaVersion";
constexpr const char kKeyVersionKey[]    = "keyVersion";
constexpr const char kFeatureFlagsKey[]  = "featureFlags";
constexpr const char kExpireDateKey[]    = "expireDate";
constexpr const char kDeviceHashKey[]    = "deviceHash";
constexpr const char kSignatureKey[]     = "signature";
constexpr const char kVendorLicenseKey[] = "vendorLicense";

constexpr uint16_t kFormatVersionV1 = 1;

constexpr size_t   kLicenseStorageRegionSize    = 4 * 1024;
constexpr size_t   kLicenseStorageHeaderSize    = 32;
constexpr uint16_t kLicenseStorageMagic         = 0x4C44;
constexpr uint16_t kLicenseStorageHeaderVersion = 1;
constexpr uint16_t kLicenseSchemaVersionV1      = 1;
constexpr size_t   kLicenseV1FixedDataSize      = 96;
constexpr size_t   kFeatureFlagsSize            = 8;
constexpr size_t   kDeviceHashSize              = 16;
constexpr size_t   kSignatureSize               = 64;
constexpr size_t   kMaxVendorLicenseSize        = 0x200;  // sanity bound for the variable-length vendorLicense blob

using ReadLicenseStorageExtFunc  = void (*)(ob_device *device, void *data, uint32_t offset, uint32_t data_size, ob_error **error);
using WriteLicenseStorageExtFunc = void (*)(ob_device *device, const void *data, uint32_t data_size, ob_error **error);

struct LicenseInfoFields {
    uint16_t             formatVersion = 0;
    std::string          deviceSN;
    uint16_t             deviceVid     = 0;
    uint16_t             devicePid     = 0;
    uint16_t             schemaVersion = 0;
    uint16_t             keyVersion    = 0;
    uint64_t             featureFlags  = 0;
    uint32_t             expireDate    = 0;
    std::vector<uint8_t> deviceHash;
    std::vector<uint8_t> signature;
    std::vector<uint8_t> lbLicense;
};

uint16_t getU16Le(const uint8_t *data) {
    return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t getU32Le(const uint8_t *data) {
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) | (static_cast<uint32_t>(data[2]) << 16)
           | (static_cast<uint32_t>(data[3]) << 24);
}

uint64_t getU64Le(const uint8_t *data) {
    uint64_t value = 0;
    for(size_t index = 0; index < sizeof(uint64_t); ++index) {
        value |= static_cast<uint64_t>(data[index]) << (index * 8);
    }
    return value;
}

uint32_t putU16Le(std::vector<uint8_t> &data, size_t offset, uint16_t value) {
    if(offset > data.size() || sizeof(value) + offset > data.size()) {
        throw std::out_of_range("putU16Le out of range");
    }
    data[offset]     = static_cast<uint8_t>(value & 0xFF);
    data[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    return sizeof(value);
}

uint32_t putU32Le(std::vector<uint8_t> &data, size_t offset, uint32_t value) {
    if(offset > data.size() || sizeof(value) + offset > data.size()) {
        throw std::out_of_range("putU32Le out of range");
    }
    data[offset]     = static_cast<uint8_t>(value & 0xFF);
    data[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    data[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    data[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
    return sizeof(value);
}

uint32_t putU64Le(std::vector<uint8_t> &data, size_t offset, uint64_t value) {
    if(offset > data.size() || sizeof(value) + offset > data.size()) {
        throw std::out_of_range("putU64Le out of range");
    }
    for(size_t index = 0; index < sizeof(uint64_t); ++index) {
        data[offset + index] = static_cast<uint8_t>((value >> (index * 8)) & 0xFF);
    }
    return sizeof(value);
}

uint32_t putArray(std::vector<uint8_t> &data, size_t offset, const std::vector<uint8_t> &src) {
    if(offset > data.size() || src.size() > data.size() - offset) {
        throw std::out_of_range("putArray out of range");
    }

    std::copy(src.begin(), src.end(), data.begin() + offset);
    return static_cast<uint32_t>(src.size());
}

int hexValue(char ch) {
    if(ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if(ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if(ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

// Strip an optional "0x"/"0X" prefix. Only integer-typed hex fields (vid/pid/featureFlags)
// accept a prefix; array-typed hex fields (deviceHash/signature/vendorLicense) must not carry
// one and are parsed as-is (a leading 'x' is rejected as an invalid hex character).
std::string stripOptionalHexPrefix(const std::string &value) {
    if(value.size() >= 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
        return value.substr(2);
    }
    return value;
}

std::vector<uint8_t> parseHexBytes(const std::string &value, size_t byteSize, const char *fieldName) {
    if(value.size() != byteSize * 2) {
        THROW_INVALID_PARAM_EXCEPTION(std::string("Invalid license_info ") + fieldName + " length");
    }

    std::vector<uint8_t> bytes(byteSize, 0);
    for(size_t index = 0; index < byteSize; ++index) {
        const int high = hexValue(value[index * 2]);
        const int low  = hexValue(value[index * 2 + 1]);
        if(high < 0 || low < 0) {
            THROW_INVALID_PARAM_EXCEPTION(std::string("Invalid license_info ") + fieldName + " hex string");
        }
        bytes[index] = static_cast<uint8_t>((high << 4) | low);
    }
    return bytes;
}

std::vector<uint8_t> parseHexBytesVariable(const std::string &value, const char *fieldName) {
    if(value.empty() || (value.size() % 2) != 0) {
        THROW_INVALID_PARAM_EXCEPTION(std::string("Invalid license_info ") + fieldName + " length");
    }

    std::vector<uint8_t> bytes(value.size() / 2, 0);
    for(size_t index = 0; index < bytes.size(); ++index) {
        const int high = hexValue(value[index * 2]);
        const int low  = hexValue(value[index * 2 + 1]);
        if(high < 0 || low < 0) {
            THROW_INVALID_PARAM_EXCEPTION(std::string("Invalid license_info ") + fieldName + " hex string");
        }
        bytes[index] = static_cast<uint8_t>((high << 4) | low);
    }
    return bytes;
}

std::string hexEncode(const uint8_t *data, size_t size) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for(size_t index = 0; index < size; ++index) {
        stream << std::setw(2) << static_cast<unsigned int>(data[index]);
    }
    return stream.str();
}

std::string hexEncodeU64(uint64_t value) {
    std::ostringstream stream;
    stream << std::hex << std::nouppercase << std::setfill('0') << std::setw(static_cast<int>(kFeatureFlagsSize * 2)) << value;
    return stream.str();
}

uint64_t parseFeatureFlagsHex(const std::string &value) {
    const std::string hex = stripOptionalHexPrefix(value);
    if(hex.size() != kFeatureFlagsSize * 2) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid license_info featureFlags length");
    }

    uint64_t parsed = 0;
    for(char ch: hex) {
        const int nibble = hexValue(ch);
        if(nibble < 0) {
            THROW_INVALID_PARAM_EXCEPTION("Invalid license_info featureFlags hex string");
        }
        parsed = (parsed << 4) | static_cast<uint64_t>(nibble);
    }
    return parsed;
}

uint16_t parseHex16(const std::string &value, const char *fieldName) {
    const std::string hex = stripOptionalHexPrefix(value);
    if(hex.empty() || hex.size() > 4) {
        THROW_INVALID_PARAM_EXCEPTION(std::string("Invalid license_info ") + fieldName + " length");
    }
    uint32_t parsed = 0;
    for(char ch: hex) {
        const int nibble = hexValue(ch);
        if(nibble < 0) {
            THROW_INVALID_PARAM_EXCEPTION(std::string("Invalid license_info ") + fieldName + " hex string");
        }
        parsed = (parsed << 4) | static_cast<uint32_t>(nibble);
    }
    return static_cast<uint16_t>(parsed);
}

std::string hexEncodeU16Upper(uint16_t value) {
    std::ostringstream stream;
    stream << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << static_cast<unsigned int>(value);
    return stream.str();
}

uint16_t readJsonUInt16(const Json::Value &root, const char *fieldName) {
    const auto &value = root[fieldName];
    if(!value.isUInt()) {
        THROW_INVALID_PARAM_EXCEPTION(std::string("Invalid license_info field: ") + fieldName);
    }
    auto tempValue = value.asUInt();
    if(tempValue > 0xFFFF) {
        THROW_INVALID_PARAM_EXCEPTION(std::string("Value of license_info field: ") + fieldName + " is out of range");
    }
    return static_cast<uint16_t>(tempValue);
}

uint32_t readJsonUInt32(const Json::Value &root, const char *fieldName) {
    const auto &value = root[fieldName];
    if(!value.isUInt()) {
        THROW_INVALID_PARAM_EXCEPTION(std::string("Invalid license_info field: ") + fieldName);
    }
    return value.asUInt();
}

std::string readJsonString(const Json::Value &root, const char *fieldName) {
    const auto &value = root[fieldName];
    if(!value.isString()) {
        THROW_INVALID_PARAM_EXCEPTION(std::string("Invalid license_info field: ") + fieldName);
    }
    return value.asString();
}

uint16_t updateCrc16Arc(uint16_t crc, const uint8_t *data, size_t size) {
    for(size_t index = 0; index < size; ++index) {
        crc ^= data[index];
        for(int bit = 0; bit < 8; ++bit) {
            if((crc & 0x0001U) != 0U) {
                crc = static_cast<uint16_t>((crc >> 1) ^ 0xA001U);
            }
            else {
                crc = static_cast<uint16_t>(crc >> 1);
            }
        }
    }
    return crc;
}

uint16_t calcHeaderCrc16(const std::vector<uint8_t> &header) {
    uint16_t crc = 0x0000;
    crc          = updateCrc16Arc(crc, header.data(), 4);
    crc          = updateCrc16Arc(crc, header.data() + 6, header.size() - 6);
    return crc;
}

uint32_t calcCrc32(const uint8_t *data, size_t size) {
    uint32_t crc = 0xFFFFFFFFU;
    for(size_t index = 0; index < size; ++index) {
        crc ^= data[index];
        for(int bit = 0; bit < 8; ++bit) {
            const uint32_t mask = static_cast<uint32_t>(-(static_cast<int32_t>(crc & 1U)));
            crc                 = (crc >> 1) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

bool isErasedFlash(const std::vector<uint8_t> &data) {
    // Treat both an all-0xFF (NOR erased) and an all-0x00 region as blank/unwritten
    // license data, depending on the flash/erase backend behavior.
    const bool allFF = std::all_of(data.begin(), data.end(), [](uint8_t value) { return value == 0xFF; });
    const bool all00 = std::all_of(data.begin(), data.end(), [](uint8_t value) { return value == 0x00; });
    return allFF || all00;
}

LicenseInfoFields parseLicenseInfoJson(const char *licenseInfoJson, size_t licenseInfoJsonSize) {
    Json::Value  root;
    Json::Reader reader;
    if(!reader.parse(licenseInfoJson, licenseInfoJson + licenseInfoJsonSize, root, false) || !root.isObject()) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid license_info JSON");
    }
    LicenseInfoFields fields;

    fields.formatVersion = readJsonUInt16(root, kFormatVersionKey);
    if(fields.formatVersion != kFormatVersionV1) {
        THROW_INVALID_PARAM_EXCEPTION("Unsupported license_info formatVersion");
    }

    // device info node
    const auto &devInfoNode = root[kDeviceInfoKey];
    if(devInfoNode.isNull() || !devInfoNode.isObject()) {
        THROW_INVALID_PARAM_EXCEPTION(std::string("Invalid license_info field: ") + kDeviceInfoKey);
    }
    fields.deviceSN  = readJsonString(devInfoNode, kDeviceSnKey);
    fields.deviceVid = parseHex16(readJsonString(devInfoNode, kDeviceVidKey), kDeviceVidKey);
    fields.devicePid = parseHex16(readJsonString(devInfoNode, kDevicePidKey), kDevicePidKey);

    // license info node
    Json::Value licenseInfoNode = root[kLicenseInfoKey];
    if(licenseInfoNode.isNull() || !licenseInfoNode.isObject()) {
        THROW_INVALID_PARAM_EXCEPTION(std::string("Invalid license_info field: ") + kLicenseInfoKey);
    }

    fields.schemaVersion = readJsonUInt16(licenseInfoNode, kSchemaVersionKey);
    fields.keyVersion    = readJsonUInt16(licenseInfoNode, kKeyVersionKey);
    fields.featureFlags  = parseFeatureFlagsHex(readJsonString(licenseInfoNode, kFeatureFlagsKey));
    fields.expireDate    = readJsonUInt32(licenseInfoNode, kExpireDateKey);
    fields.deviceHash    = parseHexBytes(readJsonString(licenseInfoNode, kDeviceHashKey), kDeviceHashSize, kDeviceHashKey);
    fields.signature     = parseHexBytes(readJsonString(licenseInfoNode, kSignatureKey), kSignatureSize, kSignatureKey);
    fields.lbLicense     = parseHexBytesVariable(readJsonString(licenseInfoNode, kVendorLicenseKey), kVendorLicenseKey);
    return fields;
}

std::string buildLicenseInfoJson(const LicenseInfoFields &fields) {
    Json::Value root;
    root[kFormatVersionKey] = Json::UInt(fields.formatVersion);
    // device info node
    Json::Value devInfoNode;
    devInfoNode[kDeviceSnKey]  = fields.deviceSN;
    devInfoNode[kDeviceVidKey] = hexEncodeU16Upper(fields.deviceVid);
    devInfoNode[kDevicePidKey] = hexEncodeU16Upper(fields.devicePid);
    root[kDeviceInfoKey]       = devInfoNode;

    Json::Value licenseInfoNode;
    licenseInfoNode[kSchemaVersionKey] = Json::UInt(fields.schemaVersion);
    licenseInfoNode[kKeyVersionKey]    = Json::UInt(fields.keyVersion);
    licenseInfoNode[kFeatureFlagsKey]  = hexEncodeU64(fields.featureFlags);
    licenseInfoNode[kExpireDateKey]    = Json::UInt(fields.expireDate);
    licenseInfoNode[kDeviceHashKey]    = hexEncode(fields.deviceHash.data(), fields.deviceHash.size());
    licenseInfoNode[kSignatureKey]     = hexEncode(fields.signature.data(), fields.signature.size());
    licenseInfoNode[kVendorLicenseKey] = hexEncode(fields.lbLicense.data(), fields.lbLicense.size());
    root[kLicenseInfoKey]              = licenseInfoNode;

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

std::vector<uint8_t> licenseFieldsToStorage(const LicenseInfoFields &fields) {
    if(fields.schemaVersion != kLicenseSchemaVersionV1) {
        THROW_INVALID_PARAM_EXCEPTION("Unsupported license_info schemaVersion");
    }
    if(fields.lbLicense.size() > kMaxVendorLicenseSize) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid license_info vendorLicense length");
    }

    const size_t dataSize = kLicenseV1FixedDataSize + fields.lbLicense.size();
    if(kLicenseStorageHeaderSize + dataSize > kLicenseStorageRegionSize) {
        THROW_INVALID_PARAM_EXCEPTION("license_info is too large for device storage");
    }

    std::vector<uint8_t> data(dataSize, 0);
    uint32_t             offset = 0;
    offset += putU16Le(data, offset, fields.keyVersion);
    offset += putU64Le(data, offset, fields.featureFlags);
    offset += putU32Le(data, offset, fields.expireDate);
    offset += putArray(data, offset, fields.deviceHash);
    offset += putArray(data, offset, fields.signature);
    offset += putU16Le(data, offset, static_cast<uint16_t>(fields.lbLicense.size()));
    offset += putArray(data, offset, fields.lbLicense);

    // header is NOT a contiguous layout: headerCrc16 sits at offset 4-5 (between
    // headerVersion@2 and dataVersion@6), so it must be written by explicit offset
    // and computed last (calcHeaderCrc16 skips bytes 4-5).
    std::vector<uint8_t> header(kLicenseStorageHeaderSize, 0);
    putU16Le(header, 0, kLicenseStorageMagic);
    putU16Le(header, 2, kLicenseStorageHeaderVersion);
    putU16Le(header, 6, fields.schemaVersion);
    putU32Le(header, 8, static_cast<uint32_t>(dataSize));
    putU32Le(header, 12, calcCrc32(data.data(), data.size()));
    putU16Le(header, 4, calcHeaderCrc16(header));

    std::vector<uint8_t> storage(header.size() + data.size(), 0);
    offset = 0;
    offset += putArray(storage, offset, header);
    offset += putArray(storage, offset, data);
    return storage;
}

LicenseInfoFields licenseFieldsFromStorage(const std::vector<uint8_t> &header, const std::vector<uint8_t> &data, std::shared_ptr<const DeviceInfo> devInfo) {
    if(header.size() != kLicenseStorageHeaderSize) {
        THROW_INVALID_DATA_EXCEPTION("Device license header size is invalid");
    }

    if(getU16Le(header.data()) != kLicenseStorageMagic) {
        THROW_INVALID_DATA_EXCEPTION("Device license header magic is invalid");
    }
    if(getU16Le(header.data() + 2) != kLicenseStorageHeaderVersion) {
        THROW_INVALID_DATA_EXCEPTION("Device license header version is invalid");
    }
    if(getU16Le(header.data() + 4) != calcHeaderCrc16(header)) {
        THROW_INVALID_DATA_EXCEPTION("Device license header CRC16 is invalid");
    }
    if(!std::all_of(header.begin() + 16, header.end(), [](uint8_t value) { return value == 0; })) {
        THROW_INVALID_DATA_EXCEPTION("Device license header reserved bytes are invalid");
    }

    const uint16_t dataVersion  = getU16Le(header.data() + 6);
    const uint32_t declaredSize = getU32Le(header.data() + 8);
    const uint32_t declaredCrc  = getU32Le(header.data() + 12);

    if(declaredSize != data.size()) {
        THROW_INVALID_DATA_EXCEPTION("Device license data size is invalid");
    }
    if(declaredSize == 0 || declaredSize > kLicenseStorageRegionSize - kLicenseStorageHeaderSize) {
        THROW_INVALID_DATA_EXCEPTION("Device license data size is out of range");
    }
    if(calcCrc32(data.data(), data.size()) != declaredCrc) {
        THROW_INVALID_DATA_EXCEPTION("Device license data CRC32 is invalid");
    }
    if(dataVersion != kLicenseSchemaVersionV1) {
        THROW_INVALID_DATA_EXCEPTION("Unsupported device license data version");
    }
    if(data.size() < kLicenseV1FixedDataSize) {
        THROW_INVALID_DATA_EXCEPTION("Device license payload is too short");
    }

    const uint16_t lbLicenseSize = getU16Le(data.data() + 94);
    if(data.size() != kLicenseV1FixedDataSize + lbLicenseSize) {
        THROW_INVALID_DATA_EXCEPTION("Device license lbLicense size is invalid");
    }

    LicenseInfoFields fields;
    fields.schemaVersion = dataVersion;
    fields.keyVersion    = getU16Le(data.data());
    fields.featureFlags  = getU64Le(data.data() + 2);
    fields.expireDate    = getU32Le(data.data() + 10);
    fields.deviceHash.assign(data.begin() + 14, data.begin() + 14 + kDeviceHashSize);
    fields.signature.assign(data.begin() + 30, data.begin() + 30 + kSignatureSize);
    fields.lbLicense.assign(data.begin() + kLicenseV1FixedDataSize, data.end());

    // device info
    fields.formatVersion = kFormatVersionV1;
    fields.deviceSN      = devInfo->deviceSn_;
    fields.deviceVid     = static_cast<uint16_t>(devInfo->vid_);
    fields.devicePid     = static_cast<uint16_t>(devInfo->pid_);

    return fields;
}

}  // namespace

struct G330DeviceLicenseInfoManager::LicenseStorageContext {
    std::shared_ptr<dylib>     dylib_                  = nullptr;
    ReadLicenseStorageExtFunc  readLicenseStorageExt_  = nullptr;
    WriteLicenseStorageExtFunc writeLicenseStorageExt_ = nullptr;
};

G330DeviceLicenseInfoManager::G330DeviceLicenseInfoManager(IDevice *owner) : DeviceComponentBase(owner) {
    initializeLicenseStorageContext();

    std::lock_guard<std::mutex> lock(licenseInfoMutex_);
    try {
        refreshLicenseInfoCacheLocked();
    }
    catch(const std::exception &e) {
        licenseInfoCache_.clear();
        LOG_WARN("Failed to preload device license info: {}", e.what());
    }
}

G330DeviceLicenseInfoManager::~G330DeviceLicenseInfoManager() {}

void G330DeviceLicenseInfoManager::initializeLicenseStorageContext() {
    try {
        auto        ctx       = std::make_shared<LicenseStorageContext>();
        const char *dylibName = "firmwareupdater";
        auto        extDir    = utils::joinPaths(EnvConfig::getExtensionsDirectory(), dylibName);

        ctx->dylib_ = DynamicLibraryManager::getInstance()->getLibrary(dylibName);
        if(!ctx->dylib_) {
            ctx->dylib_ = DynamicLibraryManager::getInstance()->loadLibrary(extDir.c_str(), dylibName);
        }
        ctx->readLicenseStorageExt_ =
            ctx->dylib_->get_function<void(ob_device *, void *, uint32_t, uint32_t, ob_error **)>("ob_device_read_license_storage_ext");
        ctx->writeLicenseStorageExt_ = ctx->dylib_->get_function<void(ob_device *, const void *, uint32_t, ob_error **)>("ob_device_write_license_storage_ext");

        if(!ctx->readLicenseStorageExt_ || !ctx->writeLicenseStorageExt_) {
            THROW_STANDARD_EXCEPTION("Failed to load license storage extension entrypoints");
        }

        licenseStorageCtx_ = ctx;
    }
    catch(const std::exception &e) {
        licenseStorageCtx_.reset();
        LOG_WARN("Failed to load OrbbecSDKExt license storage entrypoints: {}", e.what());
    }
}

std::string G330DeviceLicenseInfoManager::getLicenseInfo() {
    std::lock_guard<std::mutex> lock(licenseInfoMutex_);
    if(licenseInfoCache_.empty()) {
        refreshLicenseInfoCacheLocked();
    }
    return licenseInfoCache_;
}

std::vector<uint8_t> G330DeviceLicenseInfoManager::readLicenseStorage(uint32_t offset, uint32_t size) const {
    if(!licenseStorageCtx_ || !licenseStorageCtx_->readLicenseStorageExt_) {
        THROW_NOT_IMPLEMENTED_EXCEPTION("Device license storage extension is unavailable");
    }

    auto      device = std::make_shared<ob_device>();
    ob_error *error  = nullptr;
    device->device   = getOwner()->shared_from_this();

    std::vector<uint8_t> storage(size, 0);
    licenseStorageCtx_->readLicenseStorageExt_(device.get(), storage.data(), offset, size, &error);
    if(error) {
        const std::string message = error->message;
        delete error;
        THROW_IO_EXCEPTION(message);
    }

    return storage;
}

void G330DeviceLicenseInfoManager::writeLicenseStorage(const uint8_t *data, uint32_t size) const {
    if(!licenseStorageCtx_ || !licenseStorageCtx_->writeLicenseStorageExt_) {
        THROW_NOT_IMPLEMENTED_EXCEPTION("Device license storage extension is unavailable");
    }

    auto      device = std::make_shared<ob_device>();
    ob_error *error  = nullptr;
    device->device   = getOwner()->shared_from_this();

    licenseStorageCtx_->writeLicenseStorageExt_(device.get(), data, size, &error);
    if(error) {
        const std::string message = error->message;
        delete error;
        THROW_IO_EXCEPTION(message);
    }
}

void G330DeviceLicenseInfoManager::refreshLicenseInfoCacheLocked() {
    licenseInfoCache_.clear();

    auto header = readLicenseStorage(0, static_cast<uint32_t>(kLicenseStorageHeaderSize));
    if(isErasedFlash(header)) {
        THROW_ITEM_NOT_FOUND_EXCEPTION("Device license info not found");
    }

    const uint32_t dataSize = getU32Le(header.data() + 8);
    if(dataSize == 0 || dataSize > kLicenseStorageRegionSize - kLicenseStorageHeaderSize) {
        THROW_INVALID_DATA_EXCEPTION("Device license data size is out of range");
    }

    auto data         = readLicenseStorage(static_cast<uint32_t>(kLicenseStorageHeaderSize), dataSize);
    auto devInfo      = getOwner()->getInfo();
    auto info         = licenseFieldsFromStorage(header, data, devInfo);
    licenseInfoCache_ = buildLicenseInfoJson(info);
}

void G330DeviceLicenseInfoManager::writeLicenseInfo(const std::string &jsonInfo) {
    std::lock_guard<std::mutex> lock(licenseInfoMutex_);
    const auto                  fields  = parseLicenseInfoJson(jsonInfo.c_str(), jsonInfo.size());
    auto                        devInfo = getOwner()->getInfo();
    if(devInfo->deviceSn_ != fields.deviceSN || devInfo->vid_ != fields.deviceVid || devInfo->pid_ != fields.devicePid) {
        THROW_INVALID_PARAM_EXCEPTION("Device info mismatch");
    }

    const auto storage     = licenseFieldsToStorage(fields);
    auto       licenseInfo = buildLicenseInfoJson(fields);
    writeLicenseStorage(storage.data(), static_cast<uint32_t>(storage.size()));
    licenseInfoCache_ = licenseInfo;
}

void G330DeviceLicenseInfoManager::clearLicenseInfo() {
    std::lock_guard<std::mutex> lock(licenseInfoMutex_);
    // Write an erased (all-0xFF) header so the next read detects "no license"
    // via isErasedFlash(); 0x00 would instead fail magic/CRC and look corrupted.
    std::vector<uint8_t> erased(kLicenseStorageHeaderSize, 0xFF);
    writeLicenseStorage(erased.data(), static_cast<uint32_t>(erased.size()));
    licenseInfoCache_.clear();
}

}  // namespace libobsensor
