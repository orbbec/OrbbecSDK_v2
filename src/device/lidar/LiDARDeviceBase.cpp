// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "LiDARDeviceBase.hpp"

namespace libobsensor {

LiDARDeviceBase::LiDARDeviceBase(const std::shared_ptr<const IDeviceEnumInfo> &info) : DeviceBase(info) {}

LiDARDeviceBase ::~LiDARDeviceBase() {}

int LiDARDeviceBase::getFirmwareVersionInt() {
    int    dotCount     = 0;
    char   buf[16]      = { 0 };
    size_t bufIndex     = 0;
    int    calFwVersion = 0;

    auto fwVersion = deviceInfo_->fwVersion_;
    for(size_t i = 0; i < fwVersion.size(); i++) {
        const char c = fwVersion[i];
        if(isdigit(c) && bufIndex < sizeof(buf)) {
            buf[bufIndex] = c;
            bufIndex++;
        }
        if('.' == c) {
            buf[sizeof(buf) - 1] = '\0';
            if(strlen(buf) > 0) {
                int value = atoi(buf);
                // The version number has only two digits
                if(value >= 100) {
                    LOG_ERROR("bad fwVersion: {}", deviceInfo_->fwVersion_);
                    return 0;
                }

                if(dotCount == 0) {  // Major version number
                    calFwVersion += 1000000 * value;
                }
                else if(dotCount == 1) {  // Minor version number
                    calFwVersion += 10000 * value;
                }
                else if(dotCount == 2) {  // Patch version number
                    calFwVersion += 100 * value;
                }
                else {
                    LOG_ERROR("bad fwVersion: {}", deviceInfo_->fwVersion_);
                    return 0;
                }

                dotCount++;
                bufIndex = 0;
                memset(buf, 0, sizeof(buf));
            }
        }
    }

    // Test version number
    buf[sizeof(buf) - 1] = '\0';
    if(strlen(buf) > 0 && strlen(buf) <= 2 && dotCount == 3) {
        int value = atoi(buf);
        calFwVersion += value;
    }

    // If the version number cannot be determined, then fix logic is given priority
    if(calFwVersion == 0 || dotCount < 3) {
        LOG_ERROR("bad fwVersion: {}, parse digital version failed", deviceInfo_->fwVersion_);
        return 0;
    }
    return calFwVersion;
}

std::string LiDARDeviceBase::Uint8toString(const std::vector<uint8_t> &data, const std::string &defValue) {
    if(data.empty()) {
        return defValue;
    }
    size_t len = strnlen(reinterpret_cast<const char *>(data.data()), data.size());
    return std::string(data.begin(), data.begin() + len);
}

}  // namespace libobsensor
