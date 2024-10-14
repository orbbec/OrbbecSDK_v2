// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FilterPropertyAccessors.hpp"
#include "utils/Utils.hpp"

namespace libobsensor {
FilterStatePropertyAccessor::FilterStatePropertyAccessor(std::shared_ptr<IFilter> filter) : filter_(filter) {}

void FilterStatePropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    utils::unusedVar(propertyId);
    bool enable = static_cast<bool>(value.intValue);
    filter_->enable(enable);
}

void FilterStatePropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    utils::unusedVar(propertyId);
    value->intValue = static_cast<int>(filter_->isEnabled());
}

void FilterStatePropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    utils::unusedVar(propertyId);
    range->cur.intValue  = static_cast<int>(filter_->isEnabled());
    range->min.intValue  = 0;
    range->max.intValue  = 1;
    range->step.intValue = 1;
    range->def.intValue  = 1;
}
}  // namespace libobsensor
