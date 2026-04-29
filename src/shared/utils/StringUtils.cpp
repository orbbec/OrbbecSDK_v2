// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "StringUtils.hpp"
#include "Utils.hpp"
#include <algorithm>
#include <locale>
#include <cstring>
#include <cstdlib>
#include <clocale>
#if defined(__APPLE__) && defined(__MACH__)
#include <xlocale.h>
#endif

namespace libobsensor {
namespace utils {
namespace string {

std::vector<std::string> tokenize(const std::string &s, const char separator) {

    std::vector<std::string> ret;
    if(!s.empty()) {
        std::istringstream stream(s);
        std::string        str;
        while(getline(stream, str, separator)) {
            ret.push_back(str);
        }
    }
    return ret;
}

std::vector<std::string> split(const std::string &s, const std::string &separator) {
    std::vector<std::string> res;
    if(s.empty()) {
        return res;
    }

    if(separator.empty()) {
        res.push_back(s);
        return res;
    }

    if(separator.length() == 1) {
        res.reserve(std::count(s.begin(), s.end(), separator[0]) + 1);
    }

    std::string::size_type start = 0;
    std::string::size_type end   = s.find(separator);

    while(end != std::string::npos) {
        res.push_back(s.substr(start, end - start));
        start = end + separator.length();
        end   = s.find(separator, start);
    }
    res.push_back(s.substr(start));

    return res;
}

std::string remove(const std::string &s, const std::string &rm) {
    auto        strSplitVec = split(s, rm);
    std::string outStr      = "";
    for(auto &str: strSplitVec) {
        outStr += str;
    }
    return outStr;
}

std::string removeSpace(const std::string &s) {
    std::string outStr = s;
    outStr.erase(std::remove(outStr.begin(), outStr.end(), ' '), outStr.end());
    return outStr;
}

std::string replace(const std::string &s, const std::string &src, const std::string &rep) {
    std::string outStr = s;
    auto        place  = outStr.find(src);
    while(place < outStr.length()) {
        outStr = outStr.replace(place, rep.length(), rep);
        place  = outStr.find(src);
    }
    return outStr;
}

std::string replaceFirst(const std::string &s, const std::string &src, const std::string &rep) {
    std::string outStr = s;
    auto        place  = outStr.find(src);
    if(place < outStr.length()) {
        outStr = outStr.replace(place, src.length(), rep);
    }
    return outStr;
}

std::string toLower(const std::string &s) {
    std::string outStr = s;
    for(auto &c: outStr) {
        if(c >= 'A' && c <= 'Z') {
            c = c - 'A' + 'a';
        }
    }
    return outStr;
}

std::string toUpper(const std::string &s) {
    std::string outStr = s;
    for(auto &c: outStr) {
        if(c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        }
    }
    return outStr;
}

std::string clearHeadAndTailSpace(const std::string &str) {
    std::string temp = str;
    if(temp.empty()) {
        return temp;
    }
    temp.erase(0, temp.find_first_not_of(" "));
    temp.erase(temp.find_last_not_of(" ") + 1);
    return temp;
}

bool cvt2Boolean(const std::string &string, bool &dst) {
    if(!string.empty()) {
        std::string loweredCaseValue = toLower(string);
        bool        result           = false;
        if(!(std::istringstream(loweredCaseValue) >> std::boolalpha >> result)) {
            std::string tempStr = clearHeadAndTailSpace(string);
            if(tempStr.size() != 1) {
                return false;
            }
            else {
                if(*tempStr.c_str() >= '0' && *tempStr.c_str() <= '1') {
                    dst = static_cast<bool>(std::atoi(tempStr.c_str()));
                    return true;
                }
                else {
                    return false;
                }
            }
        }
        dst = result;
        return true;
    }
    return false;
}

bool cvt2Int(const std::string &string, int &dst) {
    std::string temp = clearHeadAndTailSpace(string);
    if(!temp.empty()) {
        if(temp.size() == 1) {
            if(*temp.c_str() >= '0' && *temp.c_str() <= '9') {
                dst = std::atoi(temp.c_str());
                return true;
            }
            else {
                return false;
            }
        }
        else {
            dst = std::atoi(temp.c_str());
            return std::to_string(dst).size() == temp.size();
        }
    }
    return false;
}

static double strtod_clocale(const char *str, char **endptr) {
    // Use a locale-independent approach to avoid issues when the global locale
    // has a different decimal separator (e.g. "," in some European locales).
#ifdef _WIN32
    static _locale_t cLocale = _create_locale(LC_NUMERIC, "C");
    return _strtod_l(str, endptr, cLocale);
#elif defined(__APPLE__) && defined(__MACH__)
    static locale_t cLocale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    return strtod_l(str, endptr, cLocale);
#elif defined(__GLIBC__) || defined(__linux__)
    static locale_t cLocale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    return strtod_l(str, endptr, cLocale);
#else
    char *prev = setlocale(LC_NUMERIC, nullptr);
    if(prev) prev = strdup(prev);
    setlocale(LC_NUMERIC, "C");
    double r = std::strtod(str, endptr);
    if(prev) {
        setlocale(LC_NUMERIC, prev);
        free(prev);
    }
    return r;
#endif
}

static float strtof_clocale(const char *str, char **endptr) {
    return static_cast<float>(strtod_clocale(str, endptr));
}

bool cvt2Float(const std::string &string, float &dst) {
    if(!string.empty()) {
        char *nptr;
        dst              = strtof_clocale(string.c_str(), &nptr);
        std::string temp = clearHeadAndTailSpace(nptr);
        return temp.empty();
    }
    return false;
}

bool cvt2Double(const std::string &string, double &dst) {
    if(!string.empty()) {
        char *nptr;
        dst              = strtod_clocale(string.c_str(), &nptr);
        std::string temp = clearHeadAndTailSpace(nptr);
        return temp.empty();
    }
    return false;
}

}  // namespace string
}  // namespace utils
}  // namespace libobsensor
