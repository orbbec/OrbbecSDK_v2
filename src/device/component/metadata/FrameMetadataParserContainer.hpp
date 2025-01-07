// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IFrame.hpp"
#include "DeviceComponentBase.hpp"
#include "exception/ObException.hpp"
#include "utils/Utils.hpp"

#include <map>

namespace libobsensor {

class FrameMetadataParserContainer : public IFrameMetadataParserContainer, public DeviceComponentBase {
public:
    FrameMetadataParserContainer(IDevice *owner) : DeviceComponentBase(owner) {}
    virtual ~FrameMetadataParserContainer() = default;

    virtual void registerParser(OBFrameMetadataType type, std::shared_ptr<IFrameMetadataParser> phaser) {
        parsers[type] = phaser;
    }

    virtual bool isContained(OBFrameMetadataType type) {
        return parsers.find(type) != parsers.end();
    }

    virtual std::shared_ptr<IFrameMetadataParser> get(OBFrameMetadataType type) {
        if(!isContained(type)) {
            throw unsupported_operation_exception(utils::string::to_string() << "Not registered metadata parser for type: " << type);
        }
        return parsers[type];
    }

protected:
    std::map<OBFrameMetadataType, std::shared_ptr<IFrameMetadataParser>> parsers;

private:
    IDevice *owner_ = nullptr;
};

}  // namespace libobsensor
