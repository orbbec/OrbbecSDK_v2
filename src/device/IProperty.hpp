#pragma once
#include "libobsensor/h/ObTypes.h"
#include "libobsensor/h/Property.h"
#include "exception/ObException.hpp"

#include <vector>
#include <memory>

namespace libobsensor {
typedef union {
    int32_t intValue;
    float   floatValue;
} OBPropertyValue;

typedef struct {
    OBPropertyValue cur;
    OBPropertyValue max;
    OBPropertyValue min;
    OBPropertyValue step;
    OBPropertyValue def;
} OBPropertyRange;

template <typename T> struct OBPropertyRangeT {
    T cur;
    T max;
    T min;
    T step;
    T def;
};

using GetDataCallback = std::function<void(OBDataTranState state, OBDataChunk *dataChunk)>;

class IPropertyAccessor {
public:
    virtual ~IPropertyAccessor() noexcept = default;
};
class IBasicPropertyAccessor : virtual public IPropertyAccessor {
public:
    virtual ~IBasicPropertyAccessor() noexcept                                       = default;
    virtual void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) = 0;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value)       = 0;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range)       = 0;
};

class IStructureDataAccessor : virtual public IPropertyAccessor {
public:
    virtual ~IStructureDataAccessor() noexcept                                                                  = default;
    virtual void                        setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) = 0;
    virtual const std::vector<uint8_t> &getStructureData(uint32_t propertyId)                                   = 0;
};

class IStructureDataAccessorV1_1 : virtual public IPropertyAccessor {
public:
    virtual ~IStructureDataAccessorV1_1() noexcept = default;

    virtual uint16_t                    getCmdVersionProtoV1_1(uint32_t propertyId)                                                           = 0;
    virtual const std::vector<uint8_t> &getStructureDataProtoV1_1(uint32_t propertyId, uint16_t cmdVersion)                                   = 0;
    virtual void                        setStructureDataProtoV1_1(uint32_t propertyId, const std::vector<uint8_t> &data, uint16_t cmdVersion) = 0;
    virtual const std::vector<uint8_t> &getStructureDataListProtoV1_1(uint32_t propertyId, uint16_t cmdVersion)                               = 0;
};

class IRawDataAccessor : virtual public IPropertyAccessor {
public:
    virtual ~IRawDataAccessor() noexcept                                   = default;
    virtual void getRawData(uint32_t propertyId, GetDataCallback callback) = 0;
};

enum PropertyAccessType {
    PROP_ACCESS_USER     = 1,  // User access(by sdk user api)
    PROP_ACCESS_INTERNAL = 2,  // Internal access (by sdk or other internal modules)
    // PROP_ACCESS_ANY      = 3,  // Any access (user or internal)
};

enum PropertyOperationType {
    PROP_OP_READ       = 1,  // Read operation
    PROP_OP_WRITE      = 2,  // Write operation
    PROP_OP_READ_WRITE = 3,  // Read/Write operation
};

typedef std::function<void(uint32_t propertyId, const uint8_t *data, size_t dataSize, PropertyOperationType operationType)> PropertyAccessCallback;
class IPropertyServer {
public:
    virtual ~IPropertyServer() noexcept = default;

    virtual void registerAccessCallback(uint32_t propertyId, PropertyAccessCallback callback)               = 0;
    virtual void registerAccessCallback(std::vector<uint32_t> propertyIds, PropertyAccessCallback callback) = 0;

    virtual void registerProperty(uint32_t propertyId, OBPermissionType userPerms, OBPermissionType intPerms, std::shared_ptr<IPropertyAccessor> accessor) = 0;
    virtual void registerProperty(uint32_t propertyId, const std::string &userPerms, const std::string &intPerms,
                                  std::shared_ptr<IPropertyAccessor> accessor)                                                                             = 0;
    virtual void aliasProperty(uint32_t aliasId, uint32_t propertyId)                                                                                      = 0;

    virtual bool isPropertySupported(uint32_t propertyId, PropertyOperationType operationType, PropertyAccessType accessType) const = 0;
    virtual const std::vector<OBPropertyItem> &getAvailableProperties(PropertyAccessType accessType)                                = 0;
    virtual OBPropertyItem                     getPropertyItem(uint32_t propertyId, PropertyAccessType accessType)                  = 0;

    virtual void setPropertyValue(uint32_t propertyId, OBPropertyValue value, PropertyAccessType accessType)  = 0;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value, PropertyAccessType accessType) = 0;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range, PropertyAccessType accessType) = 0;

    virtual void                        setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data, PropertyAccessType accessType) = 0;
    virtual const std::vector<uint8_t> &getStructureData(uint32_t propertyId, PropertyAccessType accessType)                                   = 0;

    virtual void getRawData(uint32_t propertyId, GetDataCallback callback, PropertyAccessType accessType) = 0;

    virtual uint16_t getCmdVersionProtoV1_1(uint32_t propertyId, PropertyAccessType accessType) = 0;

    virtual const std::vector<uint8_t> &getStructureDataProtoV1_1(uint32_t propertyId, uint16_t cmdVersion, PropertyAccessType accessType)            = 0;
    virtual void setStructureDataProtoV1_1(uint32_t propertyId, const std::vector<uint8_t> &data, uint16_t cmdVersion, PropertyAccessType accessType) = 0;

    virtual const std::vector<uint8_t> &getStructureDataListProtoV1_1(uint32_t propertyId, uint16_t cmdVersion, PropertyAccessType accessType) = 0;

public:  // template functions to simplify the usage of IPropertyServer
    template <typename T>
    typename std::enable_if<!std::is_same<T, float>::value, void>::type setPropertyValueT(uint32_t propertyId, const T &value,
                                                                                          PropertyAccessType accessType = PROP_ACCESS_INTERNAL) {
        OBPropertyValue obValue;
        obValue.intValue = static_cast<int32_t>(value);
        setPropertyValue(propertyId, obValue, accessType);
    }

    template <typename T>
    typename std::enable_if<std::is_same<T, float>::value, void>::type setPropertyValueT(uint32_t propertyId, const T &value,
                                                                                         PropertyAccessType accessType = PROP_ACCESS_INTERNAL) {
        OBPropertyValue obValue;
        obValue.floatValue = static_cast<float>(value);
        setPropertyValue(propertyId, obValue, accessType);
    }

    template <typename T>
    typename std::enable_if<!std::is_same<T, float>::value, T>::type getPropertyValueT(uint32_t           propertyId,
                                                                                       PropertyAccessType accessType = PROP_ACCESS_INTERNAL) {
        OBPropertyValue obValue;
        getPropertyValue(propertyId, &obValue, accessType);
        return static_cast<T>(obValue.intValue);
    }

    template <typename T>
    typename std::enable_if<std::is_same<T, float>::value, T>::type getPropertyValueT(uint32_t           propertyId,
                                                                                      PropertyAccessType accessType = PROP_ACCESS_INTERNAL) {
        OBPropertyValue obValue;
        getPropertyValue(propertyId, &obValue, accessType);
        return static_cast<T>(obValue.floatValue);
    }

    template <typename T>
    typename std::enable_if<!std::is_same<T, float>::value, OBPropertyRangeT<T>>::type getPropertyRangeT(uint32_t           propertyId,
                                                                                                         PropertyAccessType accessType = PROP_ACCESS_INTERNAL) {
        OBPropertyRangeT<T> rangeT;
        OBPropertyRange     range;
        getPropertyRange(propertyId, (OBPropertyRange *)&range, accessType);

        rangeT.cur  = range.cur.intValue;
        rangeT.max  = range.max.intValue;
        rangeT.min  = range.min.intValue;
        rangeT.step = range.step.intValue;
        rangeT.def  = range.def.intValue;

        return rangeT;
    }

    template <typename T>
    typename std::enable_if<std::is_same<T, float>::value, OBPropertyRangeT<T>>::type getPropertyRangeT(uint32_t           propertyId,
                                                                                                        PropertyAccessType accessType = PROP_ACCESS_INTERNAL) {
        OBPropertyRangeT<T> rangeT;
        OBPropertyRange     range;
        getPropertyRange(propertyId, (OBPropertyRange *)&range, accessType);

        rangeT.cur  = range.cur.floatValue;
        rangeT.max  = range.max.floatValue;
        rangeT.min  = range.min.floatValue;
        rangeT.step = range.step.floatValue;
        rangeT.def  = range.def.floatValue;

        return rangeT;
    }

    template <typename T> void setStructureDataT(uint32_t propertyId, const T &data, PropertyAccessType accessType = PROP_ACCESS_INTERNAL) {
        std::vector<uint8_t> vec(sizeof(T));
        std::memcpy(vec.data(), &data, sizeof(T));
        setStructureData(propertyId, vec, accessType);
    }

    template <typename T> T getStructureDataT(uint32_t propertyId, PropertyAccessType accessType = PROP_ACCESS_INTERNAL) {
        std::vector<uint8_t> vec = getStructureData(propertyId, accessType);
        T                    data;
        if(vec.size() != sizeof(T) && vec.size() + 1 != sizeof(T) && vec.size() - 1 != sizeof(T)) {
            LOG_WARN("Firmware data size is not match with property type");
        }
        std::memcpy(&data, vec.data(), std::min(vec.size(), sizeof(T)));
        return data;
    }

    template <typename T, uint32_t CMD_VER> T getStructureDataProtoV1_1_T(uint32_t propertyId, PropertyAccessType accessType = PROP_ACCESS_INTERNAL) {
        std::vector<uint8_t> vec = getStructureDataProtoV1_1(propertyId, CMD_VER, accessType);
        T                    data;
        if(vec.size() != sizeof(T) && vec.size() + 1 != sizeof(T) && vec.size() - 1 != sizeof(T)) {
            LOG_WARN("Firmware data size is not match with property type");
        }
        std::memcpy(&data, vec.data(), std::min(vec.size(), sizeof(T)));
        return data;
    }

    template <typename T, uint32_t CMD_VER>
    void setStructureDataProtoV1_1_T(uint32_t propertyId, const T &structureData, PropertyAccessType accessType = PROP_ACCESS_INTERNAL) {
        std::vector<uint8_t> vec(sizeof(T));
        std::memcpy(vec.data(), &structureData, sizeof(T));
        setStructureDataProtoV1_1(propertyId, vec, CMD_VER, accessType);
    }

    template <typename T, uint32_t CMD_VER>
    std::vector<T> getStructureDataListProtoV1_1_T(uint32_t propertyId, PropertyAccessType accessType = PROP_ACCESS_INTERNAL) {
        auto data     = getStructureDataListProtoV1_1(propertyId, CMD_VER, accessType);
        auto itemSize = sizeof(T);
        auto rst      = std::vector<T>();
        for(size_t i = 0; i < data.size() && i + itemSize <= data.size(); i += itemSize) {
            T item;
            std::memcpy(&item, data.data() + i, itemSize);
            rst.emplace_back(item);
        }

        return rst;
    }
};

}  // namespace libobsensor