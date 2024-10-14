// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <map>
#include "IFilter.hpp"
#include <functional>

namespace libobsensor {

class publicFilterCreator : public IFilterCreator {
public:
    static std::map<std::string, std::shared_ptr<IFilterCreator>> getPublicFilterCreators();
public:
    publicFilterCreator(std::function<std::shared_ptr<IFilter>()> creatorFunc);

    virtual ~publicFilterCreator() = default;

    std::shared_ptr<IFilter> create() override;
private:
    std::function<std::shared_ptr<IFilter>()> creatorFunc_;
};

namespace PublicFilterCreatorLoader {
    std::map<std::string, std::shared_ptr<IFilterCreator>> getCreators();
}

}  // namespace libobsensor
