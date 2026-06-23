// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <json/json.h>
#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include "JsonTraits.hpp"
#include "utils/Utils.hpp"

namespace libobsensor {
namespace jsonmodel {

class IHandler;
class ILeafHandler;
class IObjectHandler;
struct OrderedExportOptions;

enum class NodeKind {
    Leaf,
    Object,
};

struct ExportValue;
using ExportValuePtr = std::shared_ptr<ExportValue>;

struct ExportField {
    std::string    key;
    ExportValuePtr value;
};

struct ExportValue {
    enum class Kind {
        Null,
        Scalar,
        Object,
        Array,
    };

    Kind                        kind{ Kind::Null };
    Json::Value                 scalarValue{ Json::nullValue };
    std::vector<ExportField>    objectFields;
    std::vector<ExportValuePtr> arrayItems;

    static ExportValue nullValue() {
        return {};
    }

    static ExportValue scalar(Json::Value value) {
        ExportValue exportValue;
        if(value.isObject() || value.isArray()) {
            throw std::invalid_argument("ExportValue::scalar does not accept objects or arrays");
        }
        exportValue.kind        = value.isNull() ? Kind::Null : Kind::Scalar;
        exportValue.scalarValue = std::move(value);
        return exportValue;
    }

    static ExportValue object(std::vector<ExportField> fields) {
        ExportValue exportValue;
        exportValue.kind         = Kind::Object;
        exportValue.objectFields = std::move(fields);
        return exportValue;
    }

    static ExportValue array(std::vector<ExportValue> items) {
        ExportValue exportValue;
        exportValue.kind = Kind::Array;
        exportValue.arrayItems.reserve(items.size());
        for(auto &item: items) {
            exportValue.arrayItems.emplace_back(std::make_shared<ExportValue>(std::move(item)));
        }
        return exportValue;
    }

    bool isNull() const {
        return kind == Kind::Null;
    }
};

inline ExportValuePtr makeValuePtr(ExportValue value) {
    return std::make_shared<ExportValue>(std::move(value));
}

inline ExportField makeField(std::string key, ExportValue value) {
    return { std::move(key), makeValuePtr(std::move(value)) };
}

template <typename T> inline ExportValue makeScalar(const T &value) {
    return ExportValue::scalar(JsonTraits<T>::to(value));
}

inline ExportValue makeScalar(const char *value) {
    return makeScalar(std::string(value ? value : ""));
}

template <size_t N> inline ExportValue makeScalar(const char (&value)[N]) {
    return makeScalar(static_cast<const char *>(value));
}

template <typename T> inline ExportField makeField(std::string key, const T &value) {
    return makeField(std::move(key), makeScalar(value));
}

/**
 * @brief A recursive node in the configuration tree.
 */
struct Node {
    std::string                        key;  // Key name of the node
    NodeKind                           kind{ NodeKind::Leaf };
    std::shared_ptr<ILeafHandler>      leafHandler;
    std::shared_ptr<IObjectHandler>    objectHandler;
    bool                               required{ false };  // True if the node must be present in the input JSON
    std::vector<std::shared_ptr<Node>> children;           // children node for Object node

    Node(std::string key, NodeKind kind, bool required = false) : key(std::move(key)), kind(kind), required(required) {}

    Node(std::string key, std::shared_ptr<ILeafHandler> handler, bool required = false)
        : key(std::move(key)), kind(NodeKind::Leaf), leafHandler(std::move(handler)), required(required) {}

    Node(std::string key, std::shared_ptr<IObjectHandler> handler, bool required = false)
        : key(std::move(key)), kind(NodeKind::Object), objectHandler(std::move(handler)), required(required) {}
};

/**
 * @brief Abstract base interface for data handler
 */
class IHandler {
public:
    virtual ~IHandler();
};

/**
 * @brief Handler that manages data of a lead node
 */
class ILeafHandler : public IHandler {
public:
    virtual ~ILeafHandler() override;

    /**
     * @brief Sets data from a JSON value into the handler's logic
     * @param k Key of the JSON value
     * @param v The input JSON value to be processed
     *
     * @note
     *   - Throws an exception if an error occurs
     *   - Ignores unsupported config
     */
    virtual void set(const std::string &k, const Json::Value &v) = 0;

    /**
     * @brief Retrieves the current structured export value for this leaf node
     * @param k Key of the current JSON node
     * @return The structured export value for the current state
     */
    virtual ExportValue exportValue(const std::string &k) = 0;

protected:
    enum class CaseMode { Keep, Upper, Lower };
    std::string toString(const Json::Value &v, CaseMode mode = CaseMode::Keep) {
        auto str = jsonmodel::JsonTraits<std::string>::from(v);
        switch(mode) {
        case CaseMode::Upper:
            std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) -> char { return static_cast<char>(std::toupper(c)); });
            break;
        case CaseMode::Lower:
            std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });
            break;
        default:
            break;
        }
        return str;
    }
};

/**
 * @brief Handler that manages an explicit map of child leaf data
 */
class IObjectHandler : public IHandler {
public:
    virtual ~IObjectHandler() override;

    /**
     * @brief Called once before processing all child nodes during set
     *
     * @param value The current object JSON value
     * @return Return false to skip processing child nodes
     */
    virtual bool onPreChildrenSet(const Json::Value &value) {
        utils::unusedVar(value);
        return true;
    }

    /**
     * @brief Called for each child node during set
     *
     * @param k The child node key
     * @param v The child node value
     * @param parent The parent JSON value
     */
    virtual void onSetChild(const std::string &k, const Json::Value &v, const Json::Value &parent) = 0;

    /**
     * @brief Called once after processing all child nodes during set
     */
    virtual void onPostChildrenSet() = 0;

    /**
     * @brief Called once before generating all child nodes during export
     *
     * @return A vector of additional child node keys to include
     */
    virtual std::vector<std::string> onPreChildrenGet() {
        return {};
    };

    /**
     * @brief Called for each child node during export to retrieve its value
     *
     * @param k The child node key
     *
     * @return The structured export value for the given key
     */
    virtual ExportValue exportChildValue(const std::string &k) = 0;
};

}  // namespace jsonmodel
}  // namespace libobsensor
