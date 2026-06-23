// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "utils/jsonmodel/Handler.hpp"
#include "IDevice.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"
#include <atomic>
#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace libobsensor {

enum class CompareMode {
    Ignore,        // ignore
    Equal,         // =
    LessEqual,     // <=
    GreaterEqual,  // >=
};

const char *compareModeToString(CompareMode mode);

namespace preset_detail {

template <typename T> typename std::enable_if<std::is_signed<T>::value, T>::type parseInteger(const std::string &text, int base) {
    return static_cast<T>(std::stoll(text, nullptr, base));
}

template <typename T> typename std::enable_if<!std::is_signed<T>::value, T>::type parseInteger(const std::string &text, int base) {
    return static_cast<T>(std::stoull(text, nullptr, base));
}

// Parse an integer from a JSON value, accepting either a numeric value or a decimal/hex ("0x"-prefixed) string
template <typename T> T parseHexValue(const Json::Value &v) {
    if(!v.isString()) {
        return jsonmodel::JsonTraits<T>::from(v);
    }
    const auto text = v.asString();
    size_t     pos  = 0;
    if(!text.empty() && (text[0] == '+' || text[0] == '-')) {
        pos = 1;
    }
    const bool isHex = text.size() > pos + 1 && text[pos] == '0' && (text[pos + 1] == 'x' || text[pos + 1] == 'X');
    return parseInteger<T>(text, isHex ? 16 : 10);
}

template <typename T> typename std::enable_if<std::is_signed<T>::value>::type writeSignPrefix(std::ostringstream &oss, T &value) {
    if(value < 0) {
        oss << "-0x";
        value = static_cast<T>(-value);
    }
    else {
        oss << "0x";
    }
}

template <typename T> typename std::enable_if<!std::is_signed<T>::value>::type writeSignPrefix(std::ostringstream &oss, T &) {
    oss << "0x";
}

// Format an integer as an uppercase hex string with an optional zero-padded minimum width
template <typename T> std::string formatHexValue(T value, size_t minWidth = 0) {
    using UnsignedT = typename std::make_unsigned<T>::type;

    std::ostringstream oss;
    writeSignPrefix(oss, value);
    if(minWidth > 0) {
        oss << std::setw(static_cast<int>(minWidth)) << std::setfill('0');
    }
    oss << std::uppercase << std::hex << static_cast<UnsignedT>(value);
    return oss.str();
}

}  // namespace preset_detail

/**
 * @brief Version handler
 */
class VersionHandler : public jsonmodel::ILeafHandler {
public:
    VersionHandler(const std::string &expectedVersion, CompareMode mode = CompareMode::Ignore);
    virtual ~VersionHandler() override = default;

    /**
     * @brief Implementation of ILeafHandler::set
     */
    void set(const std::string &k, const Json::Value &v) override;

    /**
     * @brief Implementation of ILeafHandler::exportValue
     */
    jsonmodel::ExportValue exportValue(const std::string &k) override {
        utils::unusedVar(k);
        return jsonmodel::makeScalar(expectedVersion_);
    }

private:
    uint32_t toUintVersion(const std::string &version);
    bool     match(const std::string &current, const std::string &expected);

protected:
    std::string expectedVersion_;
    CompareMode compareMode_{ CompareMode::Ignore };
};

/**
 * @brief Value compare handler
 */
template <typename T> class ValueCompareHandler : public jsonmodel::ILeafHandler {
    static_assert(std::is_integral<T>::value || std::is_same<T, bool>::value, "ValueCompareHandler requires an integral or bool type");

public:
    ValueCompareHandler(const T &expected, CompareMode mode = CompareMode::Ignore) : expected_(expected), compareMode_(mode) {}
    virtual ~ValueCompareHandler() override = default;

    /**
     * @brief Implementation of ILeafHandler::set
     */
    void set(const std::string &k, const Json::Value &v) override {
        T value = jsonmodel::JsonTraits<T>::from(v);

        if(!match(value, expected_)) {
            std::ostringstream oss;
            oss << "Value mismatch for key '" << k << "': actual '" << value << "' does not satisfy " << compareModeToString(compareMode_) << " '" << expected_
                << "'";
            THROW_INVALID_PARAM_EXCEPTION(oss.str());
        }
    }

    /**
     * @brief Implementation of ILeafHandler::exportValue
     */
    jsonmodel::ExportValue exportValue(const std::string &k) override {
        utils::unusedVar(k);
        return jsonmodel::makeScalar(expected_);
    }

private:
    bool match(const T &current, const T &expected) {
        if(compareMode_ == CompareMode::Ignore) {
            return true;
        }
        switch(compareMode_) {
        case CompareMode::Equal:
            return current == expected;
        case CompareMode::LessEqual:
            return current <= expected;
        case CompareMode::GreaterEqual:
            return current >= expected;
        default:
            return false;
        }
    }

protected:
    T           expected_;
    CompareMode compareMode_{ CompareMode::Ignore };
};

/**
 * @brief Value compare handler with hexadecimal string export.
 */
template <typename T> class HexValueCompareHandler : public jsonmodel::ILeafHandler {
    static_assert(std::is_integral<T>::value && !std::is_same<T, bool>::value, "HexValueCompareHandler requires an integral type");

public:
    HexValueCompareHandler(const T &expected, CompareMode mode = CompareMode::Ignore, size_t minWidth = 0)
        : expected_(expected), compareMode_(mode), minWidth_(minWidth) {}
    virtual ~HexValueCompareHandler() override = default;

    /**
     * @brief Implementation of ILeafHandler::set
     */
    void set(const std::string &k, const Json::Value &v) override {
        const T value = preset_detail::parseHexValue<T>(v);

        if(!match(value, expected_)) {
            std::ostringstream oss;
            oss << "Value mismatch for key '" << k << "': actual '" << preset_detail::formatHexValue(value, minWidth_) << "' does not satisfy "
                << compareModeToString(compareMode_) << " '" << preset_detail::formatHexValue(expected_, minWidth_) << "'";
            THROW_INVALID_PARAM_EXCEPTION(oss.str());
        }
    }

    /**
     * @brief Implementation of ILeafHandler::exportValue
     */
    jsonmodel::ExportValue exportValue(const std::string &k) override {
        utils::unusedVar(k);
        return jsonmodel::makeScalar(preset_detail::formatHexValue(expected_, minWidth_));
    }

private:
    bool match(const T &current, const T &expected) const {
        if(compareMode_ == CompareMode::Ignore) {
            return true;
        }
        switch(compareMode_) {
        case CompareMode::Equal:
            return current == expected;
        case CompareMode::LessEqual:
            return current <= expected;
        case CompareMode::GreaterEqual:
            return current >= expected;
        default:
            return false;
        }
    }

private:
    T           expected_;
    CompareMode compareMode_{ CompareMode::Ignore };
    size_t      minWidth_{ 0 };
};

/**
 * @brief Compatible devices handler
 *
 * Validates the connected device against a list of allowed {vid, pid} pairs during import.
 * The import succeeds only when the device's (vid, pid) matches one of the listed pairs.
 */
class CompatibleDevicesHandler : public jsonmodel::ILeafHandler {
public:
    CompatibleDevicesHandler(int vid, int pid, size_t hexWidth = 4);
    virtual ~CompatibleDevicesHandler() override = default;

    /**
     * @brief Implementation of ILeafHandler::set
     */
    void set(const std::string &k, const Json::Value &v) override;

    /**
     * @brief Implementation of ILeafHandler::exportValue
     */
    jsonmodel::ExportValue exportValue(const std::string &k) override;

private:
    int    vid_;
    int    pid_;
    size_t hexWidth_;
};

}  // namespace libobsensor
