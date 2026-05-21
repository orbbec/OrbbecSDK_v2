// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "utils/jsonmodel/Handler.hpp"
#include "IDevice.hpp"
#include "utils/Utils.hpp"
#include <atomic>
#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>

namespace libobsensor {

enum class CompareMode {
    Ignore,        // ignore
    Equal,         // =
    LessEqual,     // <=
    GreaterEqual,  // >=
};

const char *compareModeToString(CompareMode mode);

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
        const T value = parseValue(v);

        if(!match(value, expected_)) {
            std::ostringstream oss;
            oss << "Value mismatch for key '" << k << "': actual '" << formatHex(value) << "' does not satisfy " << compareModeToString(compareMode_) << " '"
                << formatHex(expected_) << "'";
            THROW_INVALID_PARAM_EXCEPTION(oss.str());
        }
    }

    /**
     * @brief Implementation of ILeafHandler::exportValue
     */
    jsonmodel::ExportValue exportValue(const std::string &k) override {
        utils::unusedVar(k);
        return jsonmodel::makeScalar(formatHex(expected_));
    }

private:
    T parseValue(const Json::Value &v) const {
        if(!v.isString()) {
            return jsonmodel::JsonTraits<T>::from(v);
        }

        const auto text = v.asString();
        size_t     pos  = 0;
        if(!text.empty() && (text[0] == '+' || text[0] == '-')) {
            pos = 1;
        }
        const bool isHex = text.size() > pos + 1 && text[pos] == '0' && (text[pos + 1] == 'x' || text[pos + 1] == 'X');

        return parseInteger(text, isHex ? 16 : 10);
    }

    std::string formatHex(T value) const {
        using UnsignedT = typename std::make_unsigned<T>::type;

        std::ostringstream oss;
        writeSignPrefix(oss, value);
        if(minWidth_ > 0) {
            oss << std::setw(static_cast<int>(minWidth_)) << std::setfill('0');
        }
        oss << std::uppercase << std::hex << static_cast<UnsignedT>(value);
        return oss.str();
    }

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

    template <typename U = T> typename std::enable_if<std::is_signed<U>::value, T>::type parseInteger(const std::string &text, int base) const {
        return static_cast<T>(std::stoll(text, nullptr, base));
    }

    template <typename U = T> typename std::enable_if<!std::is_signed<U>::value, T>::type parseInteger(const std::string &text, int base) const {
        return static_cast<T>(std::stoull(text, nullptr, base));
    }

    template <typename U = T> typename std::enable_if<std::is_signed<U>::value>::type writeSignPrefix(std::ostringstream &oss, T &value) const {
        if(value < 0) {
            oss << "-0x";
            value = static_cast<T>(-value);
        }
        else {
            oss << "0x";
        }
    }

    template <typename U = T> typename std::enable_if<!std::is_signed<U>::value>::type writeSignPrefix(std::ostringstream &oss, T &) const {
        oss << "0x";
    }

private:
    T           expected_;
    CompareMode compareMode_{ CompareMode::Ignore };
    size_t      minWidth_{ 0 };
};

}  // namespace libobsensor
