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

/**
 * @brief A recursive node in the configuration tree.
 */
struct Node {
    std::string                        key;                // Key name of the node
    bool                               leafNode{ true };   // True if the node is leaf node
    std::shared_ptr<IHandler>          handler;            // Logic handler(Leaf node: ILeafHandler; Object node: IObjectHandler) for the node
    bool                               optional{ false };  // True if the node is optional
    std::vector<std::shared_ptr<Node>> children;           // children node for Object node

    Node(std::string key, bool leafNode, std::shared_ptr<IHandler> handler = nullptr, bool optional = false)
        : key(key), leafNode(leafNode), handler(handler), optional(optional) {}
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
     * @brief Retrieves data from the handler's logic and converts it to JSON
     * @param k Key of the current JSON node
     * @return A Json::Value representing the current state
     *         Returns a JSON null value if the config is not supported
     */
    virtual Json::Value get(const std::string &k) = 0;

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
     * @brief Called once before generating all child nodes during get
     *
     * @return A vector of additional child node keys to include
     */
    virtual std::vector<std::string> onPreChildrenGet() {
        return {};
    };

    /**
     * @brief Called for each child node during get to retrieve its value
     *
     * @param k The child node key
     *
     * @return The JSON value for the given key
     */
    virtual Json::Value onGetChild(const std::string &k) = 0;
};

}  // namespace jsonmodel
}  // namespace libobsensor
