// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "exception/ObException.hpp"
#include "PublicTypeHelper.hpp"
#include <vector>
#include <type_traits>

namespace libobsensor {
template <typename T> std::vector<T> parseBuffer(const uint8_t *data, const uint16_t dataSize) {
    if(dataSize <= 4) {
        throw invalid_value_exception(utils::string::to_string() << "Data size error, must large than 4! size=" << dataSize);
    }

    uint16_t itemSize = *(uint16_t *)(data);
    if(itemSize < sizeof(T)) {
        throw invalid_value_exception(utils::string::to_string() << "itemSize less than sizeof(T)! itemSize=" << itemSize);
    }

    uint16_t itemNum = *(uint16_t *)(data + 2);
    if(itemNum <= 0) {
        throw invalid_value_exception(utils::string::to_string() << "itemNum error! itemNum=" << itemNum);
    }

    std::vector<T> outputVec;
    outputVec.reserve(itemNum);
    for(uint16_t i = 0; i < itemNum; i++) {
        T item;
        memcpy(&item, data + 4 + i * itemSize, sizeof(T));
        outputVec.emplace_back(std::move(item));
    }
    return outputVec;
}
}  // namespace libobsensor
