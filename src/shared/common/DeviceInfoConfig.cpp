// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DeviceInfoConfig.hpp"
#include "logger/Logger.hpp"

#include <iostream>
#include <iomanip>
#include <set>
#include <cmrc/cmrc.hpp>
CMRC_DECLARE(ob);

namespace libobsensor {

DeviceInfoConfig::DeviceInfoConfig() {}

bool DeviceInfoConfig::loadConfigFile(const std::string &configPath, bool isExternalConfig) {
    if(isExternalConfig) {
        std::string path;
        auto        currentDir = utils::getCurrentWorkDirectory();
        if(currentDir.empty()) {
            currentDir = "./";
        }
        path = utils::joinPaths(currentDir, configPath);
        try {
            if(utils::fileExists(path.c_str())) {
                xmlReader_ = std::make_shared<XmlReader>(path);
                LOG_DEBUG("External config file was found, path: {}", path.c_str());
            }
            else {
                lastErrorMessage_ = "External config file doesn't exist: " + path;
                return false;
            }
        }
        catch(const std::exception &e) {
            lastErrorMessage_ = "Failed to load external config (" + path + "). Error: {" + e.what() + "}. Ignore";
            return false;
        }
    }
    else {
        auto fs           = cmrc::ob::get_filesystem();
        auto defDeviceXml = fs.open(configPath);
        xmlReader_        = std::make_shared<XmlReader>(defDeviceXml.begin(), defDeviceXml.size());
    }

    return readDeviceInfoList(deviceInfoList_);
}

bool DeviceInfoConfig::readDeviceList(const std::string &categoryNode, std::vector<DeviceIdentifier> &vidpidList, std::vector<DeviceInfoEntry> &devInfoList) {
    // Retrieve device names in a given category.
    std::vector<std::string> deviceNodeNames;
    auto                     nodePath = categoryNode + "." + std::string(FIELD_DEVICE);
    if(!getChildNodeTextList(nodePath, deviceNodeNames)) {
        lastErrorMessage_ = "Failed to get list for category: " + categoryNode;
        return false;
    }

    std::unordered_set<std::string> deviceNodeSet;
    for(const auto &deviceNodeName: deviceNodeNames) {
        // Check if duplicate
        if(deviceNodeSet.count(deviceNodeName)) {
            LOG_WARN("Device node duplication detected - node name: {}", deviceNodeName);
            continue;
        }

        // Get detail info of the device
        auto it = deviceInfoList_.find(deviceNodeName);
        if(it == deviceInfoList_.end()) {
            lastErrorMessage_ = "Can't find details info for device name '" + deviceNodeName + "' listed in category " + categoryNode;
            return false;
        }
        // Note: the (VID, PID) pair in deviceInfoList_ is unique,
        // so no additional duplicate check is required.
        // Register device VID/PID in target category/series list
        auto &devInfo = it->second;
        vidpidList.emplace_back(DeviceIdentifier{ devInfo.vid_, devInfo.pid_ });
        DeviceInfoEntry entry{};
        entry.vid_ = devInfo.vid_;
        entry.pid_ = devInfo.pid_;
        std::snprintf(entry.deviceName_, sizeof(entry.deviceName_), "%s", devInfo.deviceName_);
        std::snprintf(entry.manufacturer_, sizeof(entry.manufacturer_), "%s", devInfo.manufacturer_);
        devInfoList.emplace_back(entry);
    }

    return true;
}

bool DeviceInfoConfig::readGVCPConfig(std::unordered_map<std::string, uint16_t> &gvcpConfigList) {
    std::vector<std::vector<std::pair<std::string, std::string>>> manufacturerAttrList;
    bool res = getChildNodeAttributeList(std::string(NODE_GVCP_CONFIG), std::string(FIELD_MANUFACTURER), manufacturerAttrList);
    if(!res) {
        lastErrorMessage_ = "GVCPConfigList not found!";
        return false;
    }

    for(const auto &attrs: manufacturerAttrList) {
        std::string manufacturer;
        std::string vidStr;
        for(const auto &kv: attrs) {
            const std::string &key   = kv.first;
            const std::string &value = kv.second;
            if(key == FIELD_MANUFACTURER_NAME) {
                manufacturer = value;
            }
            else if(key == FIELD_MANUFACTURER_VID) {
                vidStr = value;
            }
            // ignore invalid attr
        }
        if(manufacturer.empty() || vidStr.empty()) {
            lastErrorMessage_ = "GVCP Manufacturer entry: missing name or vid";
            return false;
        }

        uint16_t vid = 0;
        if(!parseHexUint16(vidStr, vid)) {
            lastErrorMessage_ = "GVCP Manufacturer entry: invalid vid: " + vidStr;
            return false;
        }

        // Check if duplicate
        // Each device name corresponds to exactly one VID,
        // but a single VID can correspond to multiple device names.
        auto iter = gvcpConfigList.find(manufacturer);
        if(iter != gvcpConfigList.end()) {
            LOG_WARN("GVCP node duplication detected - manufacture: {}, vid: {}", manufacturer, vidStr);
            continue;
        }
        gvcpConfigList.emplace(manufacturer, vid);
    }

    return true;
}

bool DeviceInfoConfig::readDeviceInfoList(std::unordered_map<std::string, DeviceInfoEntry> &devInfoList) {
    std::string              baseNode = NODE_DEVICE_CONFIG_BASE;  // Base node for devices
    std::vector<std::string> devNodes;
    if(!getChildNodeNames(baseNode + "." + std::string(NODE_DEVLIST), devNodes)) {
        lastErrorMessage_ = "DevList not found!";
        return false;
    }

    std::unordered_set<uint32_t> vidPidSet;
    // Load device info
    for(auto &devNode: devNodes) {
        DeviceInfoEntry entry{};

        // vid
        std::string vidStr;
        std::string node = std::string(NODE_DEVLIST) + "." + devNode;
        if(!getStringValue(node + "." + std::string(FIELD_VID), vidStr)) {
            lastErrorMessage_ = "Failed to retrieve VID for device node: " + node;
            return false;
        }
        if(!parseHexUint16(vidStr, entry.vid_)) {
            lastErrorMessage_ = "Invalid vid value: " + vidStr + " of node " + node;
            return false;
        }

        // pid
        std::string pidStr;
        if(!getStringValue(node + "." + std::string(FIELD_PID), pidStr)) {
            lastErrorMessage_ = "Failed to retrieve PID for device node: " + node;
            return false;
        }
        if(!parseHexUint16(pidStr, entry.pid_)) {
            lastErrorMessage_ = "Invalid pid value: " + pidStr + " of node " + node;
            return false;
        }

        // device name
        std::string deviceName;
        if(!getStringValue(node + "." + std::string(FIELD_NAME), deviceName)) {
            lastErrorMessage_ = "Failed to retrieve DeviceName for device node: " + node;
            return false;
        }
        if(deviceName.empty() || deviceName.size() >= sizeof(entry.deviceName_)) {
            lastErrorMessage_ = "Invalid device name value: " + deviceName + " of node " + node;
            return false;
        }
        std::snprintf(entry.deviceName_, sizeof(entry.deviceName_), "%s", deviceName.c_str());

        // manufacturer
        std::string manufacturer;
        if(!getStringValue(node + "." + std::string(FIELD_MANUFACTURER), manufacturer)) {
            lastErrorMessage_ = "Failed to retrieve manufacturer for device node: " + node;
            return false;
        }
        if(manufacturer.empty() || manufacturer.size() >= sizeof(entry.manufacturer_)) {
            lastErrorMessage_ = "Invalid manufacturer value: " + manufacturer + " of node " + node;
            return false;
        }
        std::snprintf(entry.manufacturer_, sizeof(entry.manufacturer_), "%s", manufacturer.c_str());

        // Check if duplicate
        uint32_t key = (static_cast<uint32_t>(entry.vid_) << 16) | static_cast<uint32_t>(entry.pid_);
        if(vidPidSet.count(key)) {
            LOG_WARN("Device node duplication detected - duplicate device vid: {}, pid: {}, name: {}, manufacture: {}", vidStr, pidStr, deviceName,
                     manufacturer);
            continue;
        }
        // save to list
        vidList_.insert(entry.vid_);
        devInfoList.emplace(devNode, entry);
        vidPidSet.insert(key);
    }

    return true;
}

bool DeviceInfoConfig::parseHexUint16(const std::string &str, uint16_t &outValue) {
    try {
        int value = std::stoi(str, nullptr, 16);
        if(value < 0 || value > 0xFFFF) {
            return false;
        }
        outValue = static_cast<uint16_t>(value);
        return true;
    }
    catch(...) {
        return false;
    }
}

bool DeviceInfoConfig::isNodeContained(const std::string &nodePathName) {
    return xmlReader_->isNodeContained(nodePathName);
}

bool DeviceInfoConfig::getStringValue(const std::string &nodePathName, std::string &t) {
    return xmlReader_->getStringValue(nodePathName, t);
}

bool DeviceInfoConfig::getChildNodeNames(const std::string &nodePathName, std::vector<std::string> &childNames) {
    if(nodePathName.empty()) {
        return false;
    }

    auto nodeList = utils::string::split(nodePathName, ".");
    if(nodeList.empty()) {
        return false;
    }

    libobsensor::XMLElement *element = xmlReader_->getRootElement();
    if(!element) {
        return false;
    }

    for(auto &n: nodeList) {
        if(element && element->Name() == n) {
            continue;
        }
        element = element->FirstChildElement(n.c_str());
        if(!element) {
            break;
        }
    }
    if(!element) {
        return false;
    }

    childNames.clear();
    auto *child = element->FirstChildElement();
    while(child) {
        childNames.push_back(child->Name());
        child = child->NextSiblingElement();
    }
    return true;
}

bool DeviceInfoConfig::getChildNodeTextList(const std::string &nodePathName, std::vector<std::string> &texts) {
    if(nodePathName.empty()) {
        return false;
    }

    auto nodeList = utils::string::split(nodePathName, ".");
    if(nodeList.empty()) {
        return false;
    }

    libobsensor::XMLElement *element = xmlReader_->getRootElement();
    if(!element) {
        return false;
    }

    auto size = nodeList.size();
    for(size_t i = 0; i < size; ++i) {
        if(i == size - 1) {
            for(auto *child = element->FirstChildElement(nodeList[i].c_str()); child; child = child->NextSiblingElement(nodeList[i].c_str())) {
                auto text = child->GetText();
                if(text && *text) {
                    texts.push_back(text);
                }
                else {
                    return false;
                }
            }
        }
        else {
            element = element->FirstChildElement(nodeList[i].c_str());
            if(!element) {
                return false;
            }
        }
    }

    return true;
}

bool DeviceInfoConfig::getAttributeValue(const std::string &nodePathName, const std::string &attrName, std::string &value) {
    return xmlReader_->getAttributeValue(nodePathName, attrName, value);
}

bool DeviceInfoConfig::getChildNodeAttributeList(const std::string &parentPath, const std::string &childNodeName,
                                                 std::vector<std::vector<std::pair<std::string, std::string>>> &outList) {
    if(parentPath.empty() || childNodeName.empty()) {
        return false;
    }

    const auto pathNodes = utils::string::split(parentPath, ".");
    if(pathNodes.empty()) {
        return false;
    }

    auto *element = xmlReader_->getRootElement();
    if(!element) {
        return false;
    }

    for(const auto &node: pathNodes) {
        element = element->FirstChildElement(node.c_str());
        if(!element) {
            break;
        }
    }
    if(!element) {
        return false;
    }

    for(auto *child = element->FirstChildElement(childNodeName.c_str()); child; child = child->NextSiblingElement(childNodeName.c_str())) {
        std::vector<std::pair<std::string, std::string>> attrs;
        attrs.reserve(4);
        for(const auto *attr = child->FirstAttribute(); attr; attr = attr->Next()) {
            auto attrName  = attr->Name();
            auto attrValue = attr->Value();
            if(attrName && attrValue) {
                attrs.emplace_back(attrName, attrValue);
            }
            else {
                return false;
            }
        }

        if(!attrs.empty()) {
            outList.emplace_back(std::move(attrs));
        }
        else {
            // Node exists but no attributes
            return false;
        }
    }

    return true;
}

}  // namespace libobsensor
