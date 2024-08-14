#include "StringUtils.hpp"
#include "Utils.hpp"
#include <algorithm>
#include <locale>
#include <cstring>
#include <cstdlib>

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
    if("" == s)
        return res;
    char *strs = new char[s.length() + 1];
    strcpy(strs, s.c_str());

    char *d = new char[separator.length() + 1];
    strcpy(d, separator.c_str());

    char *p = strtok(strs, d);
    while(p) {
        std::string str = p;
        res.push_back(str);
        p = strtok(NULL, d);
    }
    delete[] strs;
    delete[] d;
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

bool cvt2Float(const std::string &string, float &dst) {
    if(!string.empty()) {
        char *nptr;
        dst              = std::strtof(string.c_str(), &nptr);
        std::string temp = clearHeadAndTailSpace(nptr);
        return temp.empty();
    }
    return false;
}

bool cvt2Double(const std::string &string, double &dst) {
    if(!string.empty()) {
        char *nptr;
        dst              = std::strtod(string.c_str(), &nptr);
        std::string temp = clearHeadAndTailSpace(nptr);
        return temp.empty();
    }
    return false;
}

}  // namespace string
}  // namespace utils
}  // namespace libobsensor
