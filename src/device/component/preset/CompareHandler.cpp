// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "CompareHandler.hpp"
#include "exception/ObException.hpp"
#include "PresetDefinitions.hpp"

#include <cstdint>
#include <string>
#include <stdexcept>
#include <iosfwd>

namespace libobsensor {

const char *compareModeToString(CompareMode mode) {
    switch(mode) {
    case CompareMode::Equal:
        return "=";
    case CompareMode::LessEqual:
        return "<=";
    case CompareMode::GreaterEqual:
        return ">=";
    case CompareMode::Ignore:
        return "(ignored)";
    default:
        return "?";
    }
}

VersionHandler::VersionHandler(const std::string &expectedVersion, CompareMode mode) : expectedVersion_(expectedVersion), compareMode_(mode) {}

void VersionHandler::set(const std::string &k, const Json::Value &v) {
    auto value = jsonmodel::JsonTraits<std::string>::from(v);

    if(!match(value, expectedVersion_)) {
        std::ostringstream oss;
        oss << "Version mismatch for key '" << k << "': actual '" << value << "' does not satisfy " << compareModeToString(compareMode_) << " '"
            << expectedVersion_ << "'";
        THROW_INVALID_DATA_EXCEPTION(oss.str());
    }
}

bool VersionHandler::match(const std::string &current, const std::string &expected) {
    // match
    if(compareMode_ == CompareMode::Ignore) {
        return true;
    }
    auto currentInt  = toUintVersion(current);
    auto expectedInt = toUintVersion(expected);
    switch(compareMode_) {
    case CompareMode::Equal:
        return currentInt == expectedInt;
    case CompareMode::LessEqual:
        return currentInt <= expectedInt;
    case CompareMode::GreaterEqual:
        return currentInt >= expectedInt;
    default:
        return false;
    }
}

uint32_t VersionHandler::toUintVersion(const std::string &version) {
    uint32_t parts[4] = { 0, 0, 0, 0 };
    size_t   start    = 0;
    int      count    = 0;

    // version:
    // 1
    // 1.0
    // 1.0.0
    // 1.0.0.0
    do {
        size_t      end  = version.find('.', start);
        std::string part = (end == std::string::npos) ? version.substr(start) : version.substr(start, end - start);

        if(part.empty()) {
            throw std::invalid_argument("Empty version part");
        }

        uint32_t value = 0;
        for(char c: part) {
            if(c < '0' || c > '9') {
                throw std::invalid_argument("Non-numeric version part");
            }
            value = value * 10 + (c - '0');
            if(value > 255) {
                throw std::out_of_range("Version part out of range (0-255)");
            }
        }

        parts[count++] = value;
        if(end == std::string::npos) {
            break;
        }
        start = end + 1;
        if(count >= 4) {
            throw std::invalid_argument("Too many version parts");
        }
    } while(true);

    uint32_t result = 0;
    for(int i = 0; i < 4; ++i) {
        result |= (parts[i] << ((3 - i) * 8));
    }

    return result;
}

CompatibleDevicesHandler::CompatibleDevicesHandler(int vid, int pid, size_t hexWidth) : vid_(vid), pid_(pid), hexWidth_(hexWidth) {}

void CompatibleDevicesHandler::set(const std::string &k, const Json::Value &v) {
    if(!v.isArray()) {
        THROW_INVALID_PARAM_EXCEPTION("The value is not array for key '" + k + "'");
    }
    if(v.empty()) {
        THROW_INVALID_PARAM_EXCEPTION("The compatible devices list is empty for key '" + k + "'");
    }
    for(Json::ArrayIndex i = 0; i < v.size(); ++i) {
        const auto &item = v[i];
        if(!item.isObject() || !item.isMember(kVidKey) || !item.isMember(kPidKey)) {
            THROW_INVALID_PARAM_EXCEPTION(utils::string::to_string()
                                          << "Each compatible device must contain '" << kVidKey << "' and '" << kPidKey << "' for key '" + k + "'");
        }
        const int vid = preset_detail::parseHexValue<int>(item[kVidKey]);
        const int pid = preset_detail::parseHexValue<int>(item[kPidKey]);
        if(vid == vid_ && pid == pid_) {
            return;
        }
    }
    std::ostringstream oss;
    oss << "Current device (vid '" << preset_detail::formatHexValue(vid_, hexWidth_) << "', pid '" << preset_detail::formatHexValue(pid_, hexWidth_)
        << "') is not in the compatible devices list";
    THROW_INVALID_PARAM_EXCEPTION(oss.str());
}

jsonmodel::ExportValue CompatibleDevicesHandler::exportValue(const std::string &k) {
    utils::unusedVar(k);
    std::vector<jsonmodel::ExportField> fields;
    fields.emplace_back(jsonmodel::makeField(kVidKey, preset_detail::formatHexValue(vid_, hexWidth_)));
    fields.emplace_back(jsonmodel::makeField(kPidKey, preset_detail::formatHexValue(pid_, hexWidth_)));

    std::vector<jsonmodel::ExportValue> items;
    items.emplace_back(jsonmodel::ExportValue::object(std::move(fields)));
    return jsonmodel::ExportValue::array(std::move(items));
}

}  // namespace libobsensor
