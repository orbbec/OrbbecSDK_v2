// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "Handler.hpp"

namespace libobsensor {
namespace jsonmodel {

IHandler::~IHandler()             = default;
ILeafHandler::~ILeafHandler()     = default;
IObjectHandler::~IObjectHandler() = default;

}  // namespace jsonmodel
}  // namespace libobsensor
