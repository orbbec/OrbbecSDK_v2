// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "CompareHandler.hpp"
#include "exception/ObException.hpp"
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

}  // namespace libobsensor
