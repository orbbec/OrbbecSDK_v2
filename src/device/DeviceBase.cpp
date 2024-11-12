// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DeviceBase.hpp"

#include "utils/Utils.hpp"
#include "context/Context.hpp"
#include "exception/ObException.hpp"
#include "property/InternalProperty.hpp"
#include "firmwareupdater/FirmwareUpdater.hpp"
#include "frameprocessor/FrameProcessor.hpp"
#include "InternalTypes.hpp"
#include "Platform.hpp"

#include <json/json.h>

namespace libobsensor {

const std::map<OBSensorType, DeviceComponentId> SensorTypeToComponentIdMap = {
    { OB_SENSOR_COLOR, OB_DEV_COMPONENT_COLOR_SENSOR },
    { OB_SENSOR_DEPTH, OB_DEV_COMPONENT_DEPTH_SENSOR },
    { OB_SENSOR_IR, OB_DEV_COMPONENT_IR_SENSOR },
    { OB_SENSOR_IR_LEFT, OB_DEV_COMPONENT_LEFT_IR_SENSOR },
    { OB_SENSOR_IR_RIGHT, OB_DEV_COMPONENT_RIGHT_IR_SENSOR },
    { OB_SENSOR_GYRO, OB_DEV_COMPONENT_GYRO_SENSOR },
    { OB_SENSOR_ACCEL, OB_DEV_COMPONENT_ACCEL_SENSOR },
};

DeviceBase::DeviceBase(const std::shared_ptr<const IDeviceEnumInfo> &info) : enumInfo_(info), ctx_(Context::getInstance()), isDeactivated_(false) {
    deviceInfo_                       = std::make_shared<DeviceInfo>();
    deviceInfo_->name_                = enumInfo_->getName();
    deviceInfo_->pid_                 = enumInfo_->getPid();
    deviceInfo_->vid_                 = enumInfo_->getVid();
    deviceInfo_->uid_                 = enumInfo_->getUid();
    deviceInfo_->connectionType_      = enumInfo_->getConnectionType();
}

void DeviceBase::fetchDeviceInfo() {
    auto propServer = getPropertyServer();

    auto version                      = propServer->getStructureDataT<OBVersionInfo>(OB_STRUCT_VERSION);
    deviceInfo_->name_                = version.deviceName;
    deviceInfo_->fwVersion_           = version.firmwareVersion;
    deviceInfo_->deviceSn_            = version.serialNumber;
    deviceInfo_->asicName_            = version.depthChip;
    deviceInfo_->hwVersion_           = version.hardwareVersion;
    deviceInfo_->type_                = static_cast<uint16_t>(version.deviceType);
    deviceInfo_->supportedSdkVersion_ = version.sdkVersion;

    // remove the prefix "Orbbec " from the device name if contained
    if(deviceInfo_->name_.find("Orbbec ") == 0) {
        deviceInfo_->name_ = deviceInfo_->name_.substr(7);
    }
    deviceInfo_->fullName_ = "Orbbec " + deviceInfo_->name_;

    // mark the device as a multi-sensor device with same clock at default
    extensionInfo_["AllSensorsUsingSameClock"] = "true";
}

std::shared_ptr<const DeviceInfo> DeviceBase::getInfo() const {
    return deviceInfo_;
}

void DeviceBase::fetchExtensionInfo() {
    auto propServer = getPropertyServer();
    if(propServer->isPropertySupported(OB_RAW_DATA_DEVICE_EXTENSION_INFORMATION, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
        uint8_t *data     = nullptr;
        uint32_t dataSize = 0;
        propServer->getRawData(
            OB_RAW_DATA_DEVICE_EXTENSION_INFORMATION,
            [&](OBDataTranState state, OBDataChunk *dataChunk) {
                if(state == DATA_TRAN_STAT_TRANSFERRING) {
                    if(data == nullptr) {
                        dataSize = dataChunk->fullDataSize;
                        data     = new uint8_t[dataSize];
                    }
                    memcpy(data + dataChunk->offset, dataChunk->data, dataChunk->size);
                }
            },
            PROP_ACCESS_INTERNAL);
        if(data) {
            std::string jsonStr((char *)data);
            extensionInfo_ = DeviceBase::parseExtensionInfo(jsonStr);
            delete[] data;
            data = nullptr;
        }
        else {
            LOG_ERROR("Get ExtensionInfo Data is Null!");
        }
    }

    extensionInfo_["AllSensorsUsingSameClock"] = "true";
}

bool DeviceBase::isExtensionInfoExists(const std::string &infoKey) const {
    return extensionInfo_.find(infoKey) != extensionInfo_.end();
}

const std::string &DeviceBase::getExtensionInfo(const std::string &infoKey) const {
    auto it = extensionInfo_.find(infoKey);
    if(it != extensionInfo_.end()) {
        return it->second;
    }
    throw invalid_value_exception(utils::string::to_string() << "Extension info " << infoKey << " not found!");
}

DeviceBase::~DeviceBase() noexcept {
    deactivate();  // deactivate() will clear all components
}

void DeviceBase::deactivate() {
    if(hasAnySensorStreamActivated()) {
        LOG_WARN("Device is deactivated or disconnected while there are still sensors streaming!");
    }

    isDeactivated_ = true;

    std::vector<ComponentItem> tempComponents;  // using temp to avoid deadlock when deactivating components
    {
        std::lock_guard<std::recursive_mutex> lock(componentsMutex_);
        tempComponents = components_;
        components_.clear();
    }
    while(!tempComponents.empty()) {
        // The clear order should be reversed as the order of the components are added.
        // Otherwise, the dependency between components may be broken and cause crash.
        tempComponents.erase(tempComponents.end() - 1);
    }
    sensorPortInfos_.clear();
}

void DeviceBase::reboot() {
    auto propServer = getPropertyServer();
    if(propServer->isPropertySupported(OB_PROP_DEVICE_RESET_BOOL, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        propServer->setPropertyValueT(OB_PROP_DEVICE_RESET_BOOL, true);
    }
    else if(propServer->isPropertySupported(OB_PROP_REBOOT_DEVICE_BOOL, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        propServer->setPropertyValueT(OB_PROP_REBOOT_DEVICE_BOOL, true);
    }
    else {
        throw invalid_value_exception("Device does not support reboot!");
    }
    deactivate();
}

void DeviceBase::reset() {
    deactivate();
    isDeactivated_ = false;
    init();
}

DeviceComponentLock DeviceBase::tryLockResource() {
    if(isDeactivated_) {
        throw libobsensor::wrong_api_call_sequence_exception("Device is deactivated/disconnected!");
    }
    DeviceComponentLock resLock(resourceMutex_, std::defer_lock);
    if(!resLock.try_lock_for(std::chrono::milliseconds(10000))) {
        throw libobsensor::wrong_api_call_sequence_exception("Resource busy! You can try again later!");
    }
    return resLock;
}

void DeviceBase::registerComponent(DeviceComponentId compId, std::function<std::shared_ptr<IDeviceComponent>()> creator, bool lockRequired) {
    std::lock_guard<std::recursive_mutex> lock(componentsMutex_);
    ComponentItem                         item;
    item.compId       = compId;
    item.component    = nullptr;
    item.lockRequired = lockRequired;
    item.initialized  = false;
    item.creator      = creator;
    components_.emplace_back(item);
}

void DeviceBase::registerComponent(DeviceComponentId compId, std::shared_ptr<IDeviceComponent> component, bool lockRequired) {
    std::lock_guard<std::recursive_mutex> lock(componentsMutex_);
    ComponentItem                         item;
    item.compId       = compId;
    item.component    = component;
    item.lockRequired = lockRequired;
    item.initialized  = true;
    item.creator      = nullptr;
    components_.emplace_back(item);
}

void DeviceBase::deregisterComponent(DeviceComponentId compId) {
    std::lock_guard<std::recursive_mutex> lock(componentsMutex_);
    auto it = std::find_if(components_.begin(), components_.end(), [compId](const ComponentItem &item) { return item.compId == compId; });
    if(it != components_.end()) {
        components_.erase(it);
    }
}

bool DeviceBase::isComponentExists(DeviceComponentId compId) const {
    std::lock_guard<std::recursive_mutex> lock(componentsMutex_);
    auto it = std::find_if(components_.begin(), components_.end(), [compId](const ComponentItem &item) { return item.compId == compId; });
    if(it != components_.end()) {
        return true;
    }
    return false;
}

bool DeviceBase::isComponentCreated(DeviceComponentId compId) const {
    std::lock_guard<std::recursive_mutex> lock(componentsMutex_);
    auto it = std::find_if(components_.begin(), components_.end(), [compId](const ComponentItem &item) { return item.compId == compId; });
    if(it != components_.end()) {
        return it->component != nullptr;
    }
    return false;
}

DeviceComponentPtr<IDeviceComponent> DeviceBase::getComponent(DeviceComponentId compId, bool throwExIfNotFound) {
    if(isDeactivated_) {
        throw libobsensor::wrong_api_call_sequence_exception("Device is deactivated/disconnected!");
    }

    ComponentItem item = { OB_DEV_COMPONENT_UNKNOWN };
    {
        std::lock_guard<std::recursive_mutex> lock(componentsMutex_);
        auto it = std::find_if(components_.begin(), components_.end(), [compId](const ComponentItem &item) { return item.compId == compId; });

        do {
            if(it == components_.end()) {
                break;
            }

            if(!it->component && (it->initialized || !it->creator)) {
                break;
            }

            if(!it->component) {
                it->initialized = true;
                BEGIN_TRY_EXECUTE({ it->component = it->creator(); })
                CATCH_EXCEPTION_AND_EXECUTE({
                    if(throwExIfNotFound) {
                        throw;
                    }
                    return DeviceComponentPtr<IDeviceComponent>();
                })
            }
            item = *it;

        } while(false);
    }

    if(item.compId != OB_DEV_COMPONENT_UNKNOWN) {
        if(item.lockRequired) {
            DeviceComponentLock resLock = tryLockResource();
            return DeviceComponentPtr<IDeviceComponent>(item.component, std::move(resLock));
        }
        return DeviceComponentPtr<IDeviceComponent>(item.component);
    }

    if(throwExIfNotFound) {
        throw invalid_value_exception(utils::string::to_string() << "Component " << compId << " not found!");
    }
    return DeviceComponentPtr<IDeviceComponent>();
}

DeviceComponentPtr<IPropertyServer> DeviceBase::getPropertyServer() {
    return getComponentT<IPropertyServer>(OB_DEV_COMPONENT_PROPERTY_SERVER, true);
}

void DeviceBase::registerSensorPortInfo(OBSensorType type, std::shared_ptr<const SourcePortInfo> sourcePortInfo) {
    sensorPortInfos_[type] = sourcePortInfo;
}

void DeviceBase::deregisterSensor(OBSensorType type) {
    sensorPortInfos_.erase(type);
    auto componentId = SensorTypeToComponentIdMap.at(type);
    deregisterComponent(componentId);
}

const std::shared_ptr<const SourcePortInfo> &DeviceBase::getSensorPortInfo(OBSensorType type) const {
    auto it = sensorPortInfos_.find(type);
    if(it != sensorPortInfos_.end()) {
        return it->second;
    }
    throw invalid_value_exception(utils::string::to_string() << "Sensor type " << type << " not found!");
}

bool DeviceBase::isSensorExists(OBSensorType type) const {
    auto it = sensorPortInfos_.find(type);
    if(it != sensorPortInfos_.end()) {
        return true;
    }
    return false;
}

bool DeviceBase::isSensorCreated(OBSensorType type) const {
    auto compId = SensorTypeToComponentIdMap.at(type);
    return isComponentCreated(compId);
}

DeviceComponentPtr<ISensor> DeviceBase::getSensor(OBSensorType type) {
    if(type == OB_SENSOR_IR && !isSensorExists(OB_SENSOR_IR) && isSensorExists(OB_SENSOR_IR_LEFT)) {
        type = OB_SENSOR_IR_LEFT;
        LOG_WARN("For stereo camera, dose not support IR sensor, use IR left instead here.");
    }
    auto compId = SensorTypeToComponentIdMap.at(type);
    return getComponentT<ISensor>(compId, true);
}

std::vector<OBSensorType> DeviceBase::getSensorTypeList() const {
    std::vector<OBSensorType> sensorTypeList;
    for(auto &item: sensorPortInfos_) {
        sensorTypeList.push_back(item.first);
    }
    return sensorTypeList;
}

bool DeviceBase::hasAnySensorStreamActivated() {
    for(auto &item: sensorPortInfos_) {
        if(isSensorCreated(item.first)) {
            auto sensor = getSensor(item.first);
            if(sensor && sensor->isStreamActivated()) {
                return true;
            }
        }
    }
    return false;
}

std::vector<std::shared_ptr<IFilter>> DeviceBase::createRecommendedPostProcessingFilters(OBSensorType type) {
    if(type == OB_SENSOR_DEPTH) {
        auto                                  filterFactory = FilterFactory::getInstance();
        std::vector<std::shared_ptr<IFilter>> depthFilterList;
        if(filterFactory->isFilterCreatorExists("ThresholdFilter")) {
            auto ThresholdFilter = filterFactory->createFilter("ThresholdFilter");
            depthFilterList.push_back(ThresholdFilter);
        }
        return depthFilterList;
    }
    return {};
}

std::shared_ptr<IFilter> DeviceBase::getSensorFrameFilter(const std::string &name, OBSensorType type, bool throwIfNotFound) {
    // Initialize frameprocess to ensure the private filter is activated before retrieval.
    DeviceComponentId frameProcessId = OB_DEV_COMPONENT_UNKNOWN;
    switch(type) {
    case OB_SENSOR_COLOR:
        frameProcessId = OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR;
        break;
    case OB_SENSOR_DEPTH:
        frameProcessId = OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR;
        break;
    case OB_SENSOR_IR:
        frameProcessId = OB_DEV_COMPONENT_IR_FRAME_PROCESSOR;
        break;
    case OB_SENSOR_IR_LEFT:
        frameProcessId = OB_DEV_COMPONENT_LEFT_IR_FRAME_PROCESSOR;
        break;
    case OB_SENSOR_IR_RIGHT:
        frameProcessId = OB_DEV_COMPONENT_RIGHT_IR_FRAME_PROCESSOR;
        break;
    default:
        LOG_WARN("Init frameprocessor failed, Unsupported sensor type: {}", type);
        break;
    }
    if (frameProcessId != OB_DEV_COMPONENT_UNKNOWN) {
        getComponentT<FrameProcessor>(frameProcessId, false);
    }
    
    auto filterIter =
        std::find_if(sensorFrameFilters_.begin(), sensorFrameFilters_.end(), [name, type](const std::pair<OBSensorType, std::shared_ptr<IFilter>> &pair) {
            if(type == OB_SENSOR_ACCEL || type == OB_SENSOR_GYRO) {
                return (pair.first == OB_SENSOR_ACCEL || pair.first == OB_SENSOR_GYRO) && (pair.second->getName() == name);
            }
            else {
                return (pair.first == type) && (pair.second->getName() == name);
            }
        });

    if(filterIter != sensorFrameFilters_.end()) {
        return filterIter->second;
    }

    auto filterFactory = FilterFactory::getInstance();
    TRY_EXECUTE({
        auto filter               = filterFactory->createFilter(name);
        sensorFrameFilters_[type] = filter;
        return filter;
    });

    if(throwIfNotFound) {
        throw invalid_value_exception(utils::string::to_string() << "Filter " << name << " not found!");
    }

    return nullptr;
}

int DeviceBase::getFirmwareVersionInt() {
    int    dotCount     = 0;
    char   buf[16]      = { 0 };
    size_t bufIndex     = 0;
    int    calFwVersion = 0;
    for(size_t i = 0; i < deviceInfo_->fwVersion_.size(); i++) {
        const char c = deviceInfo_->fwVersion_[i];
        if(isdigit(c) && bufIndex < sizeof(buf)) {
            buf[bufIndex] = c;
            bufIndex++;
        }
        if('.' == c) {
            buf[sizeof(buf) - 1] = '\0';
            if(strlen(buf) > 0) {
                int value = atoi(buf);
                // The version number has only two digits
                if(value >= 100) {
                    LOG_ERROR("bad fwVersion: {}", deviceInfo_->fwVersion_);
                    return false;
                }

                if(dotCount == 0) {  // Major version number
                    calFwVersion += 10000 * value;
                }
                else if(dotCount == 1) {  // minor version number
                    calFwVersion += 100 * value;
                }
                else {
                    LOG_ERROR("bad fwVersion: {}", deviceInfo_->fwVersion_);
                    return false;
                }

                dotCount++;
                bufIndex = 0;
                memset(buf, 0, sizeof(buf));
            }
        }
    }

    // Test version number
    buf[sizeof(buf) - 1] = '\0';
    if(strlen(buf) > 0 && strlen(buf) <= 2 && dotCount == 2) {
        int value = atoi(buf);
        calFwVersion += value;
    }

    // If the version number cannot be determined, then fix logic is given priority
    if(calFwVersion == 0 || dotCount < 2) {
        LOG_ERROR("bad fwVersion: {}, parse digital version failed", deviceInfo_->fwVersion_);
        return 0;
    }
    return calFwVersion;
}

std::shared_ptr<ISourcePort> DeviceBase::getSourcePort(std::shared_ptr<const SourcePortInfo> sourcePortInfo) const {
    auto platform = Platform::getInstance();
#ifdef __linux__
    if(sourcePortInfo->portType == SOURCE_PORT_USB_UVC) {
        auto        envConfig = EnvConfig::getInstance();
        std::string key       = std::string("Device.") + utils::string::removeSpace(deviceInfo_->name_) + std::string(".LinuxUVCDefaultBackend");
        auto        backend   = OB_UVC_BACKEND_TYPE_AUTO;
        std::string backendStr;
        if(envConfig->getStringValue(key, backendStr)) {
            if(backendStr == "Auto") {
                backend = OB_UVC_BACKEND_TYPE_AUTO;
            }
            else if(backendStr == "LibUVC") {
                backend = OB_UVC_BACKEND_TYPE_LIBUVC;
            }
            else if(backendStr == "V4L2") {
                backend = OB_UVC_BACKEND_TYPE_V4L2;
            }
        }
        return platform->getUvcSourcePort(sourcePortInfo, backend);
    }
#endif
    return platform->getSourcePort(sourcePortInfo);
}

void DeviceBase::updateFirmware(const std::vector<uint8_t> &firmware, DeviceFwUpdateCallback updateCallback, bool async) {
    if(hasAnySensorStreamActivated()) {
        throw libobsensor::wrong_api_call_sequence_exception("Device is streaming, please stop all sensors before updating firmware!");
    }

    auto updater = getComponentT<FirmwareUpdater>(OB_DEV_COMPONENT_FIRMWARE_UPDATER, true);
    updater->updateFirmwareFromRawDataExt(firmware.data(), static_cast<uint32_t>(firmware.size()), updateCallback, async);
}

std::map<std::string, std::string> DeviceBase::parseExtensionInfo(std::string extensionInfo) {
    Json::Value                        root;
    Json::Reader                       reader;
    std::map<std::string, std::string> dataMap;
    bool                               parseResult = reader.parse(extensionInfo, root);
    if(!parseResult) {
        LOG_ERROR("parse extensioninfo failed");
    }
    else {
        Json::Value extensionInfos = root["ExtensionInfo"];
        for(auto const &key: extensionInfos.getMemberNames()) {
            dataMap[key] = extensionInfos[key].asString();
            LOG_INFO("key: {},value:{}", key, extensionInfos[key].asString());
        }
    }
    return dataMap;
}

}  // namespace libobsensor
