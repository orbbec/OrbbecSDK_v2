// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "RecommendedPostFilterStrategy.hpp"

#include <memory>

namespace libobsensor {

// Creators for each device-family recommended-filter strategy. The concrete strategy classes
// stay file-local in their respective .cpp; only these factory functions are shared so the
// dispatch in RecommendedPostFilterStrategyFactory::create stays small.
std::shared_ptr<IRecommendedPostFilterStrategy> createGemini330PostFilterStrategy();
std::shared_ptr<IRecommendedPostFilterStrategy> createDabaiAPostFilterStrategy();
std::shared_ptr<IRecommendedPostFilterStrategy> createAstra2PostFilterStrategy();
std::shared_ptr<IRecommendedPostFilterStrategy> createGemini2PostFilterStrategy();
std::shared_ptr<IRecommendedPostFilterStrategy> createGemini2XLPostFilterStrategy();
std::shared_ptr<IRecommendedPostFilterStrategy> createG435LePostFilterStrategy();
std::shared_ptr<IRecommendedPostFilterStrategy> createFemtoMegaBoltPostFilterStrategy();
std::shared_ptr<IRecommendedPostFilterStrategy> createGemini305PostFilterStrategy();
std::shared_ptr<IRecommendedPostFilterStrategy> createOpenNIPostFilterStrategy();

}  // namespace libobsensor
