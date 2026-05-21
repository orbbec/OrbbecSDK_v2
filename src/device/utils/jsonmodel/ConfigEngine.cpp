// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "Handler.hpp"
#include "ConfigEngine.hpp"
#include "logger/Logger.hpp"
#include <json/writer.h>
#include <algorithm>
#include <cstring>
#include <functional>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace libobsensor {
namespace jsonmodel {

namespace {

class ValueExporter {
public:
    static Json::Value toJson(const ExportValue &value) {
        switch(value.kind) {
        case ExportValue::Kind::Null:
            return Json::Value(Json::nullValue);
        case ExportValue::Kind::Scalar:
            return value.scalarValue;
        case ExportValue::Kind::Object: {
            Json::Value result(Json::objectValue);
            for(const auto &field: value.objectFields) {
                result[field.key] = field.value ? toJson(*field.value) : Json::Value(Json::nullValue);
            }
            return result;
        }
        case ExportValue::Kind::Array: {
            Json::Value result(Json::arrayValue);
            for(const auto &item: value.arrayItems) {
                result.append(item ? toJson(*item) : Json::Value(Json::nullValue));
            }
            return result;
        }
        default:
            return Json::Value(Json::nullValue);
        }
    }
};

std::string repeatIndent(const std::string &indentation, size_t depth);
std::string trimTrailingLineFeed(std::string text);
std::string serializeScalar(const Json::Value &value, const OrderedExportOptions &options);

class OrderedTextExporter {
public:
    OrderedTextExporter(std::ostream &os, const OrderedExportOptions &options)
        : os_(os), options_(options), colonSeparator_(options.enableYAMLCompatibility ? ": " : ":") {}

    void write(const ExportValue &value) {
        writeValue(value, 0);
    }

private:
    bool shouldWriteField(const ExportField &field) const {
        return !(options_.dropNullPlaceholders && field.value && field.value->isNull());
    }

    const ExportValue &resolveValue(const ExportValuePtr &value) const {
        static const ExportValue kNullExportValue = ExportValue::nullValue();
        return value ? *value : kNullExportValue;
    }

    bool shouldWriteArrayMultiline(const ExportValue &value) const {
        if(value.arrayItems.empty()) {
            return false;
        }

        if(value.arrayItems.size() > 4) {
            return true;
        }

        size_t singleLineLength = 2;  // []
        for(size_t index = 0; index < value.arrayItems.size(); ++index) {
            const auto &child = value.arrayItems[index];
            if(!child) {
                return true;
            }
            if(child->kind == ExportValue::Kind::Object || (child->kind == ExportValue::Kind::Array && !child->arrayItems.empty())) {
                return true;
            }
            if(index > 0) {
                singleLineLength += 2;
            }
            if(child->kind == ExportValue::Kind::Scalar && child->scalarValue.isString()) {
                const char *str = nullptr;
                const char *end = nullptr;
                if(child->scalarValue.getString(&str, &end) && end != nullptr && str != nullptr) {
                    singleLineLength += static_cast<size_t>(end - str) + 2;
                }
            }
            else {
                singleLineLength += 8;
            }
            if(singleLineLength > 60) {
                return true;
            }
        }

        return false;
    }

    void writeIndent(size_t depth) {
        os_ << repeatIndent(options_.indentation, depth);
    }

    void writeValue(const ExportValue &value, size_t depth) {
        switch(value.kind) {
        case ExportValue::Kind::Null:
            os_ << "null";
            break;
        case ExportValue::Kind::Scalar:
            os_ << serializeScalar(value.scalarValue, options_);
            break;
        case ExportValue::Kind::Object:
            writeObject(value, depth);
            break;
        case ExportValue::Kind::Array:
            writeArray(value, depth);
            break;
        default:
            break;
        }
    }

    void writeObject(const ExportValue &value, size_t depth) {
        size_t writableFieldCount = 0;
        for(const auto &field: value.objectFields) {
            if(shouldWriteField(field)) {
                ++writableFieldCount;
            }
        }

        if(writableFieldCount == 0) {
            os_ << "{}";
            return;
        }

        os_ << "{";
        size_t writtenCount = 0;
        for(const auto &field: value.objectFields) {
            if(!shouldWriteField(field)) {
                continue;
            }

            os_ << '\n';
            writeIndent(depth + 1);
            os_ << Json::valueToQuotedString(field.key.c_str()) << colonSeparator_;
            writeValue(resolveValue(field.value), depth + 1);

            if(++writtenCount < writableFieldCount) {
                os_ << ",";
            }
        }
        os_ << '\n';
        writeIndent(depth);
        os_ << "}";
    }

    void writeArray(const ExportValue &value, size_t depth) {
        if(value.arrayItems.empty()) {
            os_ << "[]";
            return;
        }

        if(!shouldWriteArrayMultiline(value)) {
            os_ << "[";
            for(size_t index = 0; index < value.arrayItems.size(); ++index) {
                if(index > 0) {
                    os_ << ", ";
                }
                writeValue(resolveValue(value.arrayItems[index]), depth);
            }
            os_ << "]";
            return;
        }

        os_ << "[";
        for(size_t index = 0; index < value.arrayItems.size(); ++index) {
            os_ << '\n';
            writeIndent(depth + 1);
            writeValue(resolveValue(value.arrayItems[index]), depth + 1);
            if(index + 1 < value.arrayItems.size()) {
                os_ << ",";
            }
        }
        os_ << '\n';
        writeIndent(depth);
        os_ << "]";
    }

private:
    std::ostream               &os_;
    const OrderedExportOptions &options_;
    const char                 *colonSeparator_;
};

std::string repeatIndent(const std::string &indentation, size_t depth) {
    std::string result;
    result.reserve(indentation.size() * depth);
    for(size_t i = 0; i < depth; ++i) {
        result += indentation;
    }
    return result;
}

std::string trimTrailingLineFeed(std::string text) {
    while(!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
        text.pop_back();
    }
    return text;
}

std::string serializeScalar(const Json::Value &value, const OrderedExportOptions &options) {
    switch(value.type()) {
    case Json::nullValue:
        return options.dropNullPlaceholders ? std::string() : "null";
    case Json::intValue:
        return Json::valueToString(value.asLargestInt());
    case Json::uintValue:
        return Json::valueToString(value.asLargestUInt());
    case Json::realValue:
        return Json::valueToString(value.asDouble(), options.realPrecision, options.realPrecisionType);
    case Json::booleanValue:
        return Json::valueToString(value.asBool());
    case Json::stringValue: {
        const char *str = nullptr;
        const char *end = nullptr;
        if(value.getString(&str, &end) && str != nullptr && end != nullptr && static_cast<size_t>(end - str) == std::strlen(str)) {
            return Json::valueToQuotedString(str);
        }

        Json::StreamWriterBuilder builder;
        builder.settings_["indentation"] = "";
        return trimTrailingLineFeed(Json::writeString(builder, value));
    }
    default:
        break;
    }

    return {};
}

}  // namespace

ConfigEngine::ConfigEngine() : rootNode_(std::make_shared<Node>("", NodeKind::Object)) {
    nodeStack_.push(rootNode_);
}

ConfigEngine::~ConfigEngine() = default;

void ConfigEngine::addObject(const std::string &key, ContinueFunc next, std::shared_ptr<IObjectHandler> handler, bool optional) {
    // 1. new node
    auto newNode = std::make_shared<Node>(key, std::move(handler), optional);

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
    auto newNode = std::make_shared<Node>(key, std::move(handler), optional);

    // 2. Attach to the current parent (Top of the nodeStack)
    nodeStack_.top()->children.emplace_back(newNode);
}

void ConfigEngine::importAll(const Json::Value &root) {
    std::vector<std::string> pathStack;
    importRecursive(rootNode_, root, pathStack);
}

Json::Value ConfigEngine::exportAll() {
    return ValueExporter::toJson(buildExportValue(rootNode_));
}

std::string ConfigEngine::exportAll(const OrderedExportOptions &options) {
    const auto         exportValue = buildExportValue(rootNode_);
    std::ostringstream oss;
    OrderedTextExporter(oss, options).write(exportValue);
    if(!options.indentation.empty()) {
        oss << '\n';
    }
    return oss.str();
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
    if(node->kind == NodeKind::Leaf) {
        if(value.isNull()) {
            LOG_WARN("Skipping setting because JSON value is null. Path: '{}'", currentPath);
            return;
        }
        if(value.isObject()) {
            throw std::runtime_error("Node type mismatch: expected leaf but object! Path: " + currentPath);
        }
        auto handler = node->leafHandler;
        if(handler) {
            handler->set(node->key, value);
        }
        return;
    }

    // 2. Object node
    if(!value.isObject()) {
        throw std::runtime_error("Node type mismatch: expected object but leaf! Path: " + currentPath);
    }
    auto objHandler = node->objectHandler;
    if(objHandler) {
        // Has Object handler
        if(objHandler->onPreChildrenSet(value)) {
            pathStack.push_back(node->key);
            for(auto &child: node->children) {
                if(value.isMember(child->key)) {
                    const bool hasHandler = (child->kind == NodeKind::Leaf && child->leafHandler) || (child->kind == NodeKind::Object && child->objectHandler);
                    if(hasHandler) {
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

ExportValue ConfigEngine::buildExportValue(const std::shared_ptr<Node> &node) {
    if(node->kind == NodeKind::Leaf) {
        if(node->leafHandler) {
            return node->leafHandler->exportValue(node->key);
        }
        return ExportValue::nullValue();
    }

    std::vector<ExportField> fields;
    auto                     objHandler = node->objectHandler;

    if(objHandler) {
        auto extraChildren = objHandler->onPreChildrenGet();
        fields.reserve(node->children.size() + extraChildren.size());

        for(auto &child: node->children) {
            const bool hasHandler = (child->kind == NodeKind::Leaf && child->leafHandler) || (child->kind == NodeKind::Object && child->objectHandler);
            if(hasHandler) {
                fields.emplace_back(makeField(child->key, buildExportValue(child)));
            }
            else {
                fields.emplace_back(makeField(child->key, objHandler->exportChildValue(child->key)));
            }
        }

        for(const auto &key: extraChildren) {
            if(std::none_of(node->children.begin(), node->children.end(), [&](const std::shared_ptr<Node> &child) { return child->key == key; })) {
                fields.emplace_back(makeField(key, objHandler->exportChildValue(key)));
            }
        }
    }
    else {
        fields.reserve(node->children.size());
        for(auto &child: node->children) {
            fields.emplace_back(makeField(child->key, buildExportValue(child)));
        }
    }

    return ExportValue::object(std::move(fields));
}

}  // namespace jsonmodel
}  // namespace libobsensor
