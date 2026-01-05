// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <json/json.h>
#include <vector>
#include <string>
#include <type_traits>
#include <algorithm>

namespace libobsensor {
namespace jsonmodel {

/**
 * @brief Entry for mapping an enum value to its string representation
 */
template <typename E> struct EnumEntry {
    E           value;
    const char *name;
};

/**
 * @brief Enum traits definition. Must be specialized for each enum type
 */
template <typename E> struct EnumTraits {
    static constexpr EnumEntry<E> entries[] = {};
};

// For Example
// 1. Define your C++ Enum
// enum class MyColor {
//     Red,
//     Green,
//     Blue
// };

// 2. Specialize EnumTraits for VideoFormat
// template <> struct EnumTraits<MyColor> {
//     static constexpr EnumEntry<MyColor> entries[] = { { MyColor::Red, "Red" }, { MyColor::Green, "Green" }, { MyColor::Blue, "Blue" } };
// };

/**
 * @brief Core traits for JSON <-> C++ conversion
 */
template <typename T, typename Enable = void> struct JsonTraits {
    static_assert(sizeof(T) == 0, "JsonTraits is not defined for this type");
};

/**
 * @brief Specialization for int
 */
template <> struct JsonTraits<int> {
    static int from(const Json::Value &v) {
        if(v.isString()) {
            return std::stoi(v.asString());
        }
        return v.asInt();
    }
    static Json::Value to(int v) {
        return Json::Value(v);
    }
};

/**
 * @brief Specialization for unsigned int
 */
template <> struct JsonTraits<unsigned int> {
    static int from(const Json::Value &v) {
        if(v.isString()) {
            return static_cast<unsigned int>(std::stoul(v.asString()));
        }
        return v.asUInt();
    }
    static Json::Value to(unsigned int v) {
        return Json::Value(v);
    }
};

/**
 * @brief Specialization for float
 */
template <> struct JsonTraits<float> {
    static float from(const Json::Value &v) {
        if(v.isString()) {
            return std::stof(v.asString());
        }
        return v.asFloat();
    }
    static Json::Value to(float v) {
        return Json::Value(v);
    }
};

/**
 * @brief Specialization for double
 */
template <> struct JsonTraits<double> {
    static double from(const Json::Value &v) {
        if(v.isString()) {
            return std::stod(v.asString());
        }
        return v.asDouble();
    }
    static Json::Value to(double v) {
        return Json::Value(v);
    }
};

/**
 * @brief Specialization for bool
 */
template <> struct JsonTraits<bool> {
    static bool from(const Json::Value &v) {
        if(v.isBool()) {
            return v.asBool();
        }
        if(v.isInt()) {
            return v.asInt() != 0;
        }
        std::string s = v.asString();
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if(s == "true" || s == "1" || s == "yes" || s == "on") {
            return true;
        }
        if(s == "false" || s == "0" || s == "no" || s == "off") {
            return false;
        }
        throw std::invalid_argument("invalid bool string: " + s);
    }
    static Json::Value to(bool v) {
        return Json::Value(v);
    }
};

/**
 * @brief Specialization for std::string
 */
template <> struct JsonTraits<std::string> {
    static std::string from(const Json::Value &v) {
        return v.asString();
    }
    static Json::Value to(const std::string &v) {
        return Json::Value(v);
    }
};

/**
 * @brief Specialization for Enums using SFINAE
 */
template <typename E> struct JsonTraits<E, typename std::enable_if<std::is_enum<E>::value>::type> {
    static E from(const Json::Value &v) {
        if(v.isInt()) {
            return static_cast<E>(v.asInt());
        }
        if(v.isString()) {
            const std::string s = v.asString();
            for(const auto &e: EnumTraits<E>::entries) {
                if(s == e.name) {
                    return e.value;
                }
            }
        }
        return static_cast<E>(0);
    }
    static Json::Value to(E v) {
        for(const auto &e: EnumTraits<E>::entries) {
            if(e.value == v) {
                return Json::Value(e.name);
            }
        }
        return Json::Value(static_cast<int>(v));
    }
};

}  // namespace jsonmodel
}  // namespace libobsensor
