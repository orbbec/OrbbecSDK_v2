// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "utils/jsonmodel/Handler.hpp"
#include "IDevice.hpp"
#include "utils/Utils.hpp"
#include <atomic>
#include <string>

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
     * @brief Implementation of ILeafHandler::get
     */
    Json::Value get(const std::string &k) override {
        utils::unusedVar(k);
        return jsonmodel::JsonTraits<std::string>::to(expectedVersion_);
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
     * @brief Implementation of ILeafHandler::get
     */
    Json::Value get(const std::string &k) override {
        utils::unusedVar(k);
        return jsonmodel::JsonTraits<T>::to(expected_);
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

}  // namespace libobsensor
