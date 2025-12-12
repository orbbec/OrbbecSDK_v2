#pragma once

#include "libobsensor/h/ObTypes.h"
#include "CommonFields.hpp"
#include "utils/Utils.hpp"
#include "xml/XmlReader.hpp"

#include <vector>
#include <map>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include <unordered_set>
#include <string>
#include <initializer_list>

namespace libobsensor {

class DeviceInfoConfig {
public:
    DeviceInfoConfig();
    virtual ~DeviceInfoConfig() noexcept = default;

    /**
     * @brief Load config from file
     *
     * @param[in] configPath config file path
     * @param[in] isExternalConfig true: external config file; false: built-in config file
     *
     * @return true/false; if false, call getLastError to get details errors
     */
    bool loadConfigFile(const std::string &configPath, bool isExternalConfig);

    /**
     * @brief Get vid list
     *
     * @return vid list
     */
    const std::unordered_set<uint16_t> &getVidList() const {
        return vidList_;
    }

    /**
     * @brief Read device list from node
     *
     * @param[in] categoryNode category config node name in root node
     * @param[out] vidpidList vidpid list of the devices in the node
     * @param[out] devInfoList device info list of the devices in the node
     *
     * @return true/false; if false, call getLastError to get details errors
     */
    bool readDeviceList(const std::string &categoryNode, std::vector<DeviceIdentifier> &vidpidList, std::vector<DeviceInfoEntry> &devInfoList);

    /**
     * @brief Read GVCP config
     *
     * @param[out] gvcpConfigList GVCP config list
     *
     * @return true/false; if false, call getLastError to get details errors
     */
    bool readGVCPConfig(std::unordered_map<std::string, uint16_t> &gvcpConfigList);

    /**
     * @brief Get last error message
     *
     * @return Last error message
     */
    const std::string &getLastError() const {
        return lastErrorMessage_;
    }

private:
    bool readDeviceInfoList(std::unordered_map<std::string, DeviceInfoEntry> &devInfoList);
    bool parseHexUint16(const std::string &str, uint16_t &outValue);
    bool isNodeContained(const std::string &nodePathName);
    bool getStringValue(const std::string &nodePathName, std::string &t);
    bool getChildNodeNames(const std::string &nodePathName, std::vector<std::string> &childNames);
    bool getChildNodeTextList(const std::string &nodePathName, std::vector<std::string> &texts);
    bool getAttributeValue(const std::string &nodePathName, const std::string &attrName, std::string &value);
    bool getChildNodeAttributeList(const std::string &parentPath, const std::string &childNodeName,
                                   std::vector<std::vector<std::pair<std::string, std::string>>> &outList);

private:
    std::shared_ptr<XmlReader>                       xmlReader_;
    std::string                                      lastErrorMessage_;
    std::unordered_set<uint16_t>                     vidList_;
    std::unordered_map<std::string, DeviceInfoEntry> deviceInfoList_;  // all device info list
};

}  // namespace libobsensor
