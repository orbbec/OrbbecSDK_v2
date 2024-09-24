#include "PropertyServer.hpp"
#include "exception/ObException.hpp"
#include "logger/Logger.hpp"
#include "utils/Utils.hpp"
#include <memory>

namespace libobsensor {

PropertyServer::PropertyServer(IDevice *owner) : DeviceComponentBase(owner) {}

void PropertyServer::registerProperty(uint32_t propertyId, OBPermissionType userPerms, OBPermissionType intPerms, std::shared_ptr<IPropertyAccessor> accessor) {
    properties_[propertyId] = { propertyId, userPerms, intPerms, accessor };

    appendToPropertyMap(propertyId, userPerms, intPerms);
}

void PropertyServer::registerAccessCallback(uint32_t propertyId, PropertyAccessCallback callback) {
    auto it = properties_.find(propertyId);
    if(it == properties_.end()) {
        throw invalid_value_exception("Property not found to register callback, propertyId: " + std::to_string(propertyId));
    }
    it->second.accessCallbacks.push_back(callback);
}

void PropertyServer::registerAccessCallback(std::vector<uint32_t> propertyIds, PropertyAccessCallback callback) {
    for(auto propertyId: propertyIds) {
        registerAccessCallback(propertyId, callback);
    }
}

void PropertyServer::registerProperty(uint32_t propertyId, const std::string &userPermsStr, const std::string &intPermsStr,
                                      std::shared_ptr<IPropertyAccessor> accessor) {
    auto strToPermission = [](const std::string &str) {
        if(str == "r") {
            return OB_PERMISSION_READ;
        }
        else if(str == "w") {
            return OB_PERMISSION_WRITE;
        }
        else if(str == "rw") {
            return OB_PERMISSION_READ_WRITE;
        }
        else {
            return OB_PERMISSION_DENY;
        }
    };
    auto userPerms = strToPermission(userPermsStr);
    auto intPerms  = strToPermission(intPermsStr);
    registerProperty(propertyId, userPerms, intPerms, accessor);
}

void addProperty(std::vector<OBPropertyItem> &vec, int propertyId, const char *propName, OBPropertyType propType, OBPermissionType perms) {
    OBPropertyItem propertyItem;
    propertyItem.id         = static_cast<OBPropertyID>(propertyId);
    propertyItem.name       = propName;
    propertyItem.type       = propType;
    propertyItem.permission = perms;
    vec.push_back(propertyItem);
}

void PropertyServer::appendToPropertyMap(uint32_t propertyId, OBPermissionType userPerms, OBPermissionType intPerms) {
    for(auto &item: OBPropertyBaseInfoMap) {
        auto infoIter = OBPropertyBaseInfoMap.find(item.first);
        if(infoIter == OBPropertyBaseInfoMap.end()) {
            std::string msg = ", id=";
            msg += std::to_string(item.first);
            throw not_implemented_exception(msg);
        }
        if(propertyId == item.first) {
            if(userPerms & 0x3) {
                addProperty(userPropertiesVec_, propertyId, infoIter->second.name, infoIter->second.type, userPerms);
            }

            if(intPerms & 0x3) {
                addProperty(innerPropertiesVec_, propertyId, infoIter->second.name, infoIter->second.type, intPerms);
            }
            break;
        }
    }
}

void PropertyServer::aliasProperty(uint32_t aliasId, uint32_t propertyId) {
    auto it = properties_.find(propertyId);
    if(it == properties_.end()) {
        throw invalid_value_exception("Property not found for aliasing");
    }

    auto propertyItem = it->second;
    propertyItem.accessCallbacks.clear();
    properties_[aliasId] = propertyItem;

    auto infoIter = OBPropertyBaseInfoMap.find(aliasId);
    if(infoIter == OBPropertyBaseInfoMap.end()) {
        std::string msg = ", id=";
        msg += std::to_string(aliasId);
        throw not_implemented_exception(msg);
    }

    if(it->second.userPermission & 0x3) {
        addProperty(userPropertiesVec_, aliasId, infoIter->second.name, infoIter->second.type, it->second.userPermission);
    }
    if(it->second.InternalPermission & 0x3) {
        addProperty(innerPropertiesVec_, aliasId, infoIter->second.name, infoIter->second.type, it->second.InternalPermission);
    }
}

bool PropertyServer::isPropertySupported(uint32_t propertyId, PropertyOperationType operationType, PropertyAccessType accessType) const {
    auto it = properties_.find(propertyId);
    if(it == properties_.end()) {
        return false;
    }

    OBPermissionType permission = OB_PERMISSION_DENY;
    if(accessType == PROP_ACCESS_USER) {
        permission = it->second.userPermission;
    }
    else if(accessType == PROP_ACCESS_INTERNAL) {
        permission = it->second.InternalPermission;
    }

    if(operationType == PROP_OP_READ) {
        return permission & OB_PERMISSION_READ;
    }
    else if(operationType == PROP_OP_WRITE) {
        return permission & OB_PERMISSION_WRITE;
    }
    else if(operationType == PROP_OP_READ_WRITE) {
        return permission == OB_PERMISSION_READ_WRITE;
    }

    return false;
}

void PropertyServer::setPropertyValue(uint32_t propertyId, OBPropertyValue value, PropertyAccessType accessType) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if(!isPropertySupported(propertyId, PROP_OP_WRITE, accessType)) {
        throw invalid_value_exception("Property not writable");
    }

    auto  it       = properties_.find(propertyId);
    auto &accessor = it->second.accessor;
    auto &propId   = it->second.propertyId;
    if(propId != propertyId) {
        LOG_DEBUG("Property {} alias to {}", propId, propertyId);
    }

    auto basicAccessor = std::dynamic_pointer_cast<IBasicPropertyAccessor>(accessor);
    basicAccessor->setPropertyValue(propId, value);

    for(auto &callback: it->second.accessCallbacks) {
        auto data = reinterpret_cast<uint8_t *>(&value);
        callback(propertyId, data, sizeof(OBPropertyValue), PROP_OP_WRITE);
    }

    LOG_DEBUG("Property {} set to {}|{}", propId, value.intValue, value.floatValue);
}

void PropertyServer::getPropertyValue(uint32_t propertyId, OBPropertyValue *value, PropertyAccessType accessType) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if(!isPropertySupported(propertyId, PROP_OP_READ, accessType)) {
        throw invalid_value_exception(utils::string::to_string() << "Property not readable: " << propertyId);
    }

    auto  it       = properties_.find(propertyId);
    auto &accessor = it->second.accessor;
    auto &propId   = it->second.propertyId;
    if(propId != propertyId) {
        LOG_DEBUG("Property {} alias to {}", propId, propertyId);
    }

    auto basicAccessor = std::dynamic_pointer_cast<IBasicPropertyAccessor>(accessor);
    basicAccessor->getPropertyValue(propId, value);

    for(auto &callback: it->second.accessCallbacks) {
        auto data = reinterpret_cast<uint8_t *>(value);
        callback(propertyId, data, sizeof(OBPropertyValue), PROP_OP_READ);
    }

    LOG_DEBUG("Property {} get as {}|{}", propId, value->intValue, value->floatValue);
}

// std::vector<OBPropertyItem> PropertyServer::getProperties(PropertyAccessType accessType) const{
//     std::lock_guard<std::recursive_mutex> lock(mutex_);
//     return properties_;
// }

void PropertyServer::getPropertyRange(uint32_t propertyId, OBPropertyRange *range, PropertyAccessType accessType) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if(!isPropertySupported(propertyId, PROP_OP_READ, accessType)) {
        throw invalid_value_exception(utils::string::to_string() << "Property not readable: " << propertyId);
    }

    auto  it       = properties_.find(propertyId);
    auto &accessor = it->second.accessor;
    auto &propId   = it->second.propertyId;
    if(propId != propertyId) {
        LOG_DEBUG("Property {} alias to {}", propId, propertyId);
    }

    auto basicAccessor = std::dynamic_pointer_cast<IBasicPropertyAccessor>(accessor);
    basicAccessor->getPropertyRange(propId, range);
    LOG_DEBUG("Property {} range as {}-{} step {} def {}|{}-{} step {} def {}", propId, range->min.intValue, range->max.intValue, range->step.intValue,
              range->def.intValue, range->min.floatValue, range->max.floatValue, range->step.floatValue, range->def.floatValue);
}

void PropertyServer::setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data, PropertyAccessType accessType) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if(!isPropertySupported(propertyId, PROP_OP_WRITE, accessType)) {
        throw invalid_value_exception("Property not writable");
    }

    auto  it       = properties_.find(propertyId);
    auto &accessor = it->second.accessor;
    auto &propId   = it->second.propertyId;
    if(propId != propertyId) {
        LOG_DEBUG("Property {} alias to {}", propId, propertyId);
    }
    auto structAccessor = std::dynamic_pointer_cast<IStructureDataAccessor>(accessor);
    if(structAccessor == nullptr) {
        throw invalid_value_exception(utils::string::to_string() << "Property" << propId << " does not support structure data setting");
    }
    structAccessor->setStructureData(propId, data);

    for(auto &callback: it->second.accessCallbacks) {
        callback(propertyId, data.data(), data.size(), PROP_OP_WRITE);
    }
    LOG_DEBUG("Property {} set structure data successfully", propId);
}

const std::vector<uint8_t> &PropertyServer::getStructureData(uint32_t propertyId, PropertyAccessType accessType) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if(!isPropertySupported(propertyId, PROP_OP_READ, accessType)) {
        throw invalid_value_exception(utils::string::to_string() << "Property not readable: " << propertyId);
    }

    auto  it       = properties_.find(propertyId);
    auto &accessor = it->second.accessor;
    auto &propId   = it->second.propertyId;
    if(propId != propertyId) {
        LOG_DEBUG("Property {} alias to {}", propId, propertyId);
    }

    auto structAccessor = std::dynamic_pointer_cast<IStructureDataAccessor>(accessor);
    if(structAccessor == nullptr) {
        throw invalid_value_exception(utils::string::to_string() << "Property " << propId << " does not support structure data getting");
    }
    const auto &data = structAccessor->getStructureData(propId);
    for(auto &callback: it->second.accessCallbacks) {
        callback(propertyId, data.data(), data.size(), PROP_OP_READ);
    }
    LOG_DEBUG("Property {} get structure data successfully, size {}", propId, data.size());
    return data;
}

void PropertyServer::getRawData(uint32_t propertyId, GetDataCallback callback, PropertyAccessType accessType) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if(!isPropertySupported(propertyId, PROP_OP_READ, accessType)) {
        throw invalid_value_exception(utils::string::to_string() << "Property not readable: " << propertyId);
    }

    auto  it       = properties_.find(propertyId);
    auto &accessor = it->second.accessor;
    auto &propId   = it->second.propertyId;
    if(propId != propertyId) {
        LOG_DEBUG("Property {} alias to {}", propId, propertyId);
    }
    auto rawDataAccessor = std::dynamic_pointer_cast<IRawDataAccessor>(accessor);
    if(rawDataAccessor == nullptr) {
        throw invalid_value_exception(utils::string::to_string() << "Property" << propId << " does not support raw data getting");
    }

    rawDataAccessor->getRawData(propId, callback);  // todo: add async support
    for(auto &accessCallback: it->second.accessCallbacks) {
        accessCallback(propertyId, nullptr, 0, PROP_OP_READ);
    }
    LOG_DEBUG("Property {} get raw data successfully", propId);
}

uint16_t PropertyServer::getCmdVersionProtoV1_1(uint32_t propertyId, PropertyAccessType accessType) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if(!isPropertySupported(propertyId, PROP_OP_READ, accessType)) {
        throw invalid_value_exception(utils::string::to_string() << "Property not readable: " << propertyId);
    }

    auto  it       = properties_.find(propertyId);
    auto &accessor = it->second.accessor;
    auto &propId   = it->second.propertyId;
    if(propId != propertyId) {
        LOG_DEBUG("Property {} alias to {}", propId, propertyId);
    }
    auto structAccessor = std::dynamic_pointer_cast<IStructureDataAccessorV1_1>(accessor);
    if(structAccessor == nullptr) {
        throw invalid_value_exception(utils::string::to_string() << "Property" << propId << " does not support cmd version getting");
    }

    auto ver = structAccessor->getCmdVersionProtoV1_1(propId);
    LOG_DEBUG("Property {} get cmd version successfully, version {}", propId, ver);
    return ver;
}

const std::vector<uint8_t> &PropertyServer::getStructureDataProtoV1_1(uint32_t propertyId, uint16_t cmdVersion, PropertyAccessType accessType) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if(!isPropertySupported(propertyId, PROP_OP_READ, accessType)) {
        throw invalid_value_exception(utils::string::to_string() << "Property not readable: " << propertyId);
    }

    auto  it       = properties_.find(propertyId);
    auto &accessor = it->second.accessor;
    auto &propId   = it->second.propertyId;
    if(propId != propertyId) {
        LOG_DEBUG("Property {} alias to {}", propId, propertyId);
    }
    auto structAccessor = std::dynamic_pointer_cast<IStructureDataAccessorV1_1>(accessor);
    if(structAccessor == nullptr) {
        throw invalid_value_exception(utils::string::to_string() << "Property" << propId << " does not support structure data getting over proto v1.1");
    }
    const auto &data = structAccessor->getStructureDataProtoV1_1(propId, cmdVersion);
    for(auto callback: it->second.accessCallbacks) {
        callback(propertyId, data.data(), data.size(), PROP_OP_READ);
    }
    LOG_DEBUG("Property {} get structure data successfully over proto v1.1, size {}", propId, data.size());
    return data;
}

void PropertyServer::setStructureDataProtoV1_1(uint32_t propertyId, const std::vector<uint8_t> &data, uint16_t cmdVersion, PropertyAccessType accessType) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if(!isPropertySupported(propertyId, PROP_OP_WRITE, accessType)) {
        throw invalid_value_exception("Property not writable");
    }

    auto  it       = properties_.find(propertyId);
    auto &accessor = it->second.accessor;
    auto &propId   = it->second.propertyId;
    if(propId != propertyId) {
        LOG_DEBUG("Property {} alias to {}", propId, propertyId);
    }
    auto structAccessor = std::dynamic_pointer_cast<IStructureDataAccessorV1_1>(accessor);
    if(structAccessor == nullptr) {
        throw invalid_value_exception(utils::string::to_string() << "Property" << propId << " does not support structure data setting over proto v1.1");
    }
    structAccessor->setStructureDataProtoV1_1(propId, data, cmdVersion);
    for(auto callback: it->second.accessCallbacks) {
        callback(propertyId, data.data(), data.size(), PROP_OP_WRITE);
    }
    LOG_DEBUG("Property {} set structure data successfully over proto v1.1", propId);
}

const std::vector<uint8_t> &PropertyServer::getStructureDataListProtoV1_1(uint32_t propertyId, uint16_t cmdVersion, PropertyAccessType accessType) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if(!isPropertySupported(propertyId, PROP_OP_READ, accessType)) {
        throw invalid_value_exception(utils::string::to_string() << "Property not readable: " << propertyId);
    }

    auto  it       = properties_.find(propertyId);
    auto &accessor = it->second.accessor;
    auto &propId   = it->second.propertyId;
    if(propId != propertyId) {
        LOG_DEBUG("Property {} alias to {}", propId, propertyId);
    }
    auto structAccessor = std::dynamic_pointer_cast<IStructureDataAccessorV1_1>(accessor);
    if(structAccessor == nullptr) {
        throw invalid_value_exception(utils::string::to_string() << "Property" << propId << " does not support structure data list getting over proto v1.1");
    }
    const auto &data = structAccessor->getStructureDataListProtoV1_1(propId, cmdVersion);
    for(auto callback: it->second.accessCallbacks) {
        callback(propertyId, data.data(), data.size(), PROP_OP_READ);
    }
    LOG_DEBUG("Property {} get structure data list successfully over proto v1.1, size {}", propId, data.size());
    return data;
}

const std::vector<OBPropertyItem> &PropertyServer::getAvailableProperties(PropertyAccessType accessType) {
    if(accessType == PROP_ACCESS_USER) {
        return userPropertiesVec_;
    }
    else if(accessType == PROP_ACCESS_INTERNAL) {
        return innerPropertiesVec_;
    }

    static const std::vector<OBPropertyItem> emptyVec;
    return emptyVec;
}

OBPropertyItem PropertyServer::getPropertyItem(uint32_t propertyId, PropertyAccessType accessType) {
    OBPropertyItem retItem       = {};
    auto           propertiesVec = getAvailableProperties(accessType);
    for(const auto &item: propertiesVec) {
        if(static_cast<uint32_t>(item.id) == propertyId) {
            retItem = item;
            break;
        }
    }
    return retItem;
}

}  // namespace libobsensor