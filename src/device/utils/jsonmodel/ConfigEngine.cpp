// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "Handler.hpp"
#include "ConfigEngine.hpp"
#include "logger/Logger.hpp"
#include <algorithm>
#include <stdexcept>
#include <iosfwd>

namespace libobsensor {
namespace jsonmodel {

ConfigEngine::ConfigEngine() : rootNode_(std::make_shared<Node>("", false)) {
    nodeStack_.push(rootNode_);
}

ConfigEngine::~ConfigEngine() = default;

void ConfigEngine::addObject(const std::string &key, ContinueFunc next, std::shared_ptr<IObjectHandler> handler, bool optional) {
    // 1. new node
    auto newNode = std::make_shared<Node>(key, false, handler, optional);

    // 2. Attach to the current parent (Top of the nodeStack)
    nodeStack_.top()->children.emplace_back(newNode);

    // 3. Establish context for the next depth level
    nodeStack_.push(newNode);

    // 4. Recursive definition call
    if(next) {
        next(*this);
    }

    // 5. Cleanup context after children are registered
    nodeStack_.pop();
}

void ConfigEngine::addLeaf(const std::string &key, std::shared_ptr<ILeafHandler> handler, bool optional) {
    // 1. new node
    auto newNode = std::make_shared<Node>(key, true, handler, optional);

    // 2. Attach to the current parent (Top of the nodeStack)
    nodeStack_.top()->children.emplace_back(newNode);
}

void ConfigEngine::importAll(const Json::Value &root) {
    std::vector<std::string> pathStack;
    importRecursive(rootNode_, root, pathStack);
}

Json::Value ConfigEngine::exportAll() {
    return exportRecursive(rootNode_);
}

void ConfigEngine::importRecursive(std::shared_ptr<Node> node, const Json::Value &value, std::vector<std::string> &pathStack) {
    auto fullPath = [](const std::vector<std::string> &pathStack, const std::string &name) {
        std::ostringstream oss;
        auto               size = pathStack.size();
        if(size > 0) {
            oss << pathStack[0];
            for(size_t i = 1; i < size; i++) {
                oss << "." << pathStack[i];
            }
            if(!name.empty()) {
                oss << "." << name;
            }
        }
        else {
            oss << name;
        }

        return oss.str();
    };

    auto currentPath = fullPath(pathStack, node->key);

    // 1. Leaf node: set and return
    if(node->leafNode) {
        if(value.isNull()) {
            LOG_WARN("Skipping setting because JSON value is null. Path: '{}'", currentPath);
            return;
        }
        if(value.isObject()) {
            throw std::runtime_error("Node type mismatch: expected leaf but object! Path: " + currentPath);
        }
        auto handler = std::dynamic_pointer_cast<ILeafHandler>(node->handler);
        if(handler) {
            handler->set(node->key, value);
        }
        return;
    }

    // 2. Object node
    if(!value.isObject()) {
        throw std::runtime_error("Node type mismatch: expected object but leaf! Path: " + currentPath);
    }
    auto objHandler = std::dynamic_pointer_cast<IObjectHandler>(node->handler);
    if(objHandler) {
        // Has Object handler
        if(objHandler->onPreChildrenSet(value)) {
            pathStack.push_back(node->key);
            for(auto &child: node->children) {
                if(value.isMember(child->key)) {
                    if(child->handler) {
                        // import child
                        importRecursive(child, value[child->key], pathStack);
                    }
                    else {
                        objHandler->onSetChild(child->key, value[child->key], value);
                    }
                }
                else if(!child->optional) {
                    currentPath = fullPath(pathStack, node->key);
                    throw std::runtime_error("Missing required field: " + currentPath);
                }
            }
            pathStack.pop_back();
        }
        objHandler->onPostChildrenSet();
    }
    else {
        // No Object handler
        pathStack.push_back(node->key);
        for(auto &child: node->children) {
            if(value.isMember(child->key)) {
                // import child
                importRecursive(child, value[child->key], pathStack);
            }
            else if(!child->optional) {
                currentPath = fullPath(pathStack, node->key);
                throw std::runtime_error("Missing required field: " + currentPath);
            }
        }
        pathStack.pop_back();
    }
}

Json::Value ConfigEngine::exportRecursive(std::shared_ptr<Node> node) {
    // 1. Leaf node: get and return
    if(node->leafNode) {
        auto handler = std::dynamic_pointer_cast<ILeafHandler>(node->handler);
        if(handler) {
            return handler->get(node->key);
        }
        return Json::Value(Json::nullValue);
    }

    // 2. Object node
    Json::Value result(Json::objectValue);
    auto        objHandler = std::dynamic_pointer_cast<IObjectHandler>(node->handler);

    if(objHandler) {
        // Has Object handler
        auto extraChildren = objHandler->onPreChildrenGet();
        for(auto &child: node->children) {
            if(child->handler) {
                result[child->key] = exportRecursive(child);
            }
            else {
                result[child->key] = objHandler->onGetChild(child->key);
            }
        }
        // extra dynamic children
        for(auto &key: extraChildren) {
            if(!result.isMember(key)) {
                result[key] = objHandler->onGetChild(key);
            }
        }
    }
    else {
        // No Object handler
        for(auto &child: node->children) {
            result[child->key] = exportRecursive(child);
        }
    }

    return result;
}

}  // namespace jsonmodel
}  // namespace libobsensor
