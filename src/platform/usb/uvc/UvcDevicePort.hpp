// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "ISourcePort.hpp"
#include "UvcTypes.hpp"
#include "usb/enumerator/UsbTypes.hpp"
#include "libobsensor/h/Property.h"

namespace libobsensor {

#undef min
#undef max
struct UvcControlRange {
    UvcControlRange() {}

    UvcControlRange(int32_t in_min, int32_t in_max, int32_t in_step, int32_t in_def) {
        populate_raw_data(min, in_min);
        populate_raw_data(max, in_max);
        populate_raw_data(step, in_step);
        populate_raw_data(def, in_def);
    }
    UvcControlRange(std::vector<uint8_t> in_min, std::vector<uint8_t> in_max, std::vector<uint8_t> in_step, std::vector<uint8_t> in_def) {
        min  = in_min;
        max  = in_max;
        step = in_step;
        def  = in_def;
    }
    std::vector<uint8_t> min;
    std::vector<uint8_t> max;
    std::vector<uint8_t> step;
    std::vector<uint8_t> def;

private:
    static void populate_raw_data(std::vector<uint8_t> &vec, int32_t value) {
        vec.resize(sizeof(value));
        auto data = reinterpret_cast<const uint8_t *>(&value);
        std::copy(data, data + sizeof(value), vec.data());
    }
};

class UvcDevicePort : public IVideoStreamPort, public IVendorDataPort {
public:
    virtual bool         getPu(uint32_t propertyId, int32_t &value) = 0;
    virtual bool         setPu(uint32_t propertyId, int32_t value)  = 0;
    virtual UvcControlRange getPuRange(uint32_t propertyId)            = 0;

    ~UvcDevicePort() noexcept override = default;

    virtual void updateXuUnit(const ObExtensionUnit &xuUnit) {
        xuUnit_ = xuUnit;
    };

#ifdef __ANDROID__
    virtual std::string getUsbConnectType() = 0;
#endif

protected:
    ObExtensionUnit xuUnit_ = OB_COMMON_XU_UNIT;
};

}  // namespace libobsensor

