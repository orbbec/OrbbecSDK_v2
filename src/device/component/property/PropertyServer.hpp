// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IProperty.hpp"
#include "InternalProperty.hpp"
#include "libobsensor/h/Property.h"
#include "PropertyHelper.hpp"
#include "DeviceComponentBase.hpp"

namespace libobsensor {

class PropertyServer : public IPropertyServer, public DeviceComponentBase {

    struct PropertyItem {
        uint32_t                            propertyId;
        OBPermissionType                    userPermission;
        OBPermissionType                    InternalPermission;
        std::shared_ptr<IPropertyAccessor>  accessor;
        std::vector<PropertyAccessCallback> accessCallbacks;
    };

public:
    PropertyServer(IDevice *owner);
    ~PropertyServer() noexcept = default;

    virtual void registerAccessCallback(uint32_t propertyId, PropertyAccessCallback callback) override;
    virtual void registerAccessCallback(std::vector<uint32_t> propertyIds, PropertyAccessCallback callback) override;

    void registerProperty(uint32_t propertyId, OBPermissionType userPerms, OBPermissionType intPerms, std::shared_ptr<IPropertyAccessor> accessor) override;
    void registerProperty(uint32_t propertyId, const std::string &userPermsStr, const std::string &intPermsStr,
                          std::shared_ptr<IPropertyAccessor> accessor) override;
    void aliasProperty(uint32_t aliasId, uint32_t propertyId) override;

    bool isPropertySupported(uint32_t propertyId, PropertyOperationType operationType, PropertyAccessType accessType) const override;
    const std::vector<OBPropertyItem> &getAvailableProperties(PropertyAccessType accessType) override;
    virtual OBPropertyItem             getPropertyItem(uint32_t propertyId, PropertyAccessType accessType) override;

    void setPropertyValue(uint32_t propertyId, OBPropertyValue value, PropertyAccessType accessType) override;
    void getPropertyValue(uint32_t propertyId, OBPropertyValue *value, PropertyAccessType accessType) override;
    void getPropertyRange(uint32_t propertyId, OBPropertyRange *range, PropertyAccessType accessType) override;

    void                        setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data, PropertyAccessType accessType) override;
    const std::vector<uint8_t> &getStructureData(uint32_t propertyId, PropertyAccessType accessType) override;

    void getRawData(uint32_t propertyId, GetDataCallback callback, PropertyAccessType accessType) override;

    uint16_t                    getCmdVersionProtoV1_1(uint32_t propertyId, PropertyAccessType accessType) override;
    const std::vector<uint8_t> &getStructureDataProtoV1_1(uint32_t propertyId, uint16_t cmdVersion, PropertyAccessType accessType) override;
    void setStructureDataProtoV1_1(uint32_t propertyId, const std::vector<uint8_t> &data, uint16_t cmdVersion, PropertyAccessType accessType) override;
    const std::vector<uint8_t> &getStructureDataListProtoV1_1(uint32_t propertyId, uint16_t cmdVersion, PropertyAccessType accessType) override;

private:
    void appendToPropertyMap(uint32_t propertyId, OBPermissionType userPerms, OBPermissionType intPerms);

private:
    std::recursive_mutex             mutex_;
    std::map<uint32_t, PropertyItem> properties_;
    std::vector<OBPropertyItem>      userPropertiesVec_;
    std::vector<OBPropertyItem>      innerPropertiesVec_;
};

}  // namespace libobsensor
