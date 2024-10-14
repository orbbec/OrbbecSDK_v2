// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

//
// Created by zyb on 2020/9/16.
//

#pragma once

#include <libobsensor/h/ObTypes.h>

#include <stdarg.h>
#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>
#include <map>

namespace libobsensor {
class XMLDocument;
class XMLElement;
class XMLAttribute;
class XMLComment;
class XMLText;
class XMLDeclaration;
class XMLUnknown;
class XMLPrinter;
class XmlReader final {

public:
    explicit XmlReader(const std::string& filePath);
    XmlReader(const char *buffer, size_t size);

    ~XmlReader() noexcept = default;

private:
    bool loadConfigFile(const std::string &filePath);
    bool loadConfigFromBuffer(const char *buffer, size_t size);

public:
    bool        isNodeContained(const std::string &nodePathName);
    bool        getIntValue(const std::string &nodePathName, int &t);
    bool        getBooleanValue(const std::string &nodePathName, bool &t);
    bool        getFloatValue(const std::string &nodePathName, float &t);
    bool        getDoubleValue(const std::string &nodePathName, double &t);
    bool        getStringValue(const std::string &nodePathName, std::string &t);
    XMLElement *getRootElement() const;

private:
    bool getTextOfLeafNode(const std::string &nodePathName, std::string &t);

private:
    std::shared_ptr<XMLDocument> doc_;

private:
    XMLElement *rootXMLElement_;
};
}  // namespace libobsensor

