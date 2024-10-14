// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "XmlReader.hpp"
#include "tinyxml2.hpp"

#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "utils/Utils.hpp"

namespace libobsensor {
XmlReader::XmlReader(const std::string& filePath) {
    doc_ = std::make_shared<XMLDocument>();
    if(filePath.empty()) {
        throw invalid_value_exception("XmlReader::XmlReader: filePath is empty!");
    }

    if(!utils::fileExists(filePath.c_str())) {
        throw invalid_value_exception("XmlReader::XmlReader: file not exist!");
    }

    auto rc = doc_->LoadFile(filePath.c_str());
    if(rc != 0) {
        throw invalid_value_exception(utils::string::to_string() << "XmlReader::XmlReader: load file failed!, rc=" << rc);
    }

    rootXMLElement_ = doc_->RootElement();
    if(!rootXMLElement_) {
        throw invalid_value_exception("XmlReader::XmlReader: root element is null!");
    }
}

XmlReader::XmlReader(const char *buffer, size_t size) {
    doc_ = std::make_shared<XMLDocument>();

    if(!buffer || size == 0) {
        throw invalid_value_exception("XmlReader::XmlReader: buffer is null or size is 0!");
    }

    auto rc = doc_->Parse(buffer, size);
    if(rc != 0) {
        throw invalid_value_exception(utils::string::to_string() << "XmlReader::XmlReader: parse buffer failed!, rc=" << rc);
    }

    rootXMLElement_ = doc_->RootElement();
    if(!rootXMLElement_) {
        throw invalid_value_exception("XmlReader::XmlReader: root element is null!");
    }
}

XMLElement *XmlReader::getRootElement() const {
    return rootXMLElement_;
}

bool XmlReader::getTextOfLeafNode(const std::string &nodePathName, std::string &text) {
    std::string errorMessage;
    if(nodePathName.empty()) {
        return false;
    }
    auto NodeList        = utils::string::split(nodePathName, ".");
    auto rootXMLElement  = rootXMLElement_;
    auto ChildrenElement = rootXMLElement_;
    for(size_t i = 0; i < NodeList.size(); ++i) {
        libobsensor::XMLElement *childrenElement = nullptr;
        childrenElement                          = rootXMLElement->FirstChildElement(NodeList[i].c_str());
        if(!childrenElement) {
            return false;
        }
        if(i < NodeList.size() - 1) {
            rootXMLElement = childrenElement;
        }
        if(i == NodeList.size() - 1) {
            ChildrenElement = childrenElement;
        }
    }

    if(ChildrenElement == rootXMLElement_) {
        return false;
    }

    if(!ChildrenElement->FirstChild() || !ChildrenElement->FirstChild()->ToText()) {
        return false;
    }
    text = ChildrenElement->GetText();
    return true;
}

bool XmlReader::isNodeContained(const std::string &nodePathName) {
    if (nodePathName.empty()) {
        return false;
    }
    auto nodeList = utils::string::split(nodePathName, ".");
    if(nodeList.empty()) {
        return false;
    }

    XMLElement *currentElement = rootXMLElement_;
    for (const auto &nodeName : nodeList) {
        currentElement = currentElement->FirstChildElement(nodeName.c_str());
        if (!currentElement) {
            return false;
        }
    }
    return true;
}

bool XmlReader::getBooleanValue(const std::string &nodePathName, bool &t) {
    std::string text;
    bool        status = getTextOfLeafNode(nodePathName, text);
    if(!status) {
        return false;
    }
    status = utils::string::cvt2Boolean(text, t);
    if(!status) {
        return false;
    }
    return true;
}

bool XmlReader::getIntValue(const std::string &nodePathName, int &t) {
    std::string text;
    bool        status = getTextOfLeafNode(nodePathName, text);
    if(!status) {
        return false;
    }
    status = utils::string::cvt2Int(text, t);
    if(!status) {
        return false;
    }
    return true;
}

bool XmlReader::getFloatValue(const std::string &nodePathName, float &t) {
    std::string text;
    bool        status = getTextOfLeafNode(nodePathName, text);
    if(!status) {
        return false;
    }
    status = utils::string::cvt2Float(text, t);
    if(!status) {
        return false;
    }
    return true;
}

bool XmlReader::getDoubleValue(const std::string &nodePathName, double &t) {
    std::string text;
    bool        status = getTextOfLeafNode(nodePathName, text);
    if(!status) {
        return false;
    }
    status = utils::string::cvt2Double(text, t);
    if(!status) {
        return false;
    }
    return true;
}

bool XmlReader::getStringValue(const std::string &nodePathName, std::string &t) {
    std::string text;
    bool        status = getTextOfLeafNode(nodePathName, text);
    if(!status) {
        return false;
    }
    text = utils::string::clearHeadAndTailSpace(text);
    if(text.empty()) {
        return false;
    }
    t = text;
    return true;
}

}  // namespace libobsensor
