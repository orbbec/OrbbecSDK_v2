#include "publicFilterLoader.hpp"
#include "FramePixelValueProcess.hpp"
#include "HdrMergeProcess.hpp"
#include "SequenceIdProcess.hpp"
#include "DecimationProcess.hpp"
#include "IMUFrameReversion.hpp"
#include "FormatConverterProcess.hpp"
#include "FrameGeometricTransform.hpp"
#include "PointCloudProcess.hpp"
#include "IMUCorrector.hpp"
#include "Align.hpp"
#include "FilterDecorator.hpp"

namespace libobsensor {
publicFilterCreator::publicFilterCreator(std::function<std::shared_ptr<IFilter>()> creatorFunc) : creatorFunc_(creatorFunc) {}

std::shared_ptr<IFilter> publicFilterCreator::create() {
    return creatorFunc_();
}

namespace PublicFilterCreatorLoader {

#define ADD_FILTER_CREATOR(name)                                       \
    { #name, std::make_shared<publicFilterCreator>([]() {              \
          auto basefilter = std::make_shared<name>();                  \
          return std::make_shared<FilterDecorator>(#name, basefilter); \
      }) }

std::map<std::string, std::shared_ptr<IFilterCreator>> getCreators() {
    static std::map<std::string, std::shared_ptr<IFilterCreator>> filterCreators = {
        ADD_FILTER_CREATOR(PixelValueScaler),  ADD_FILTER_CREATOR(ThresholdFilter),
        ADD_FILTER_CREATOR(PixelValueOffset),  ADD_FILTER_CREATOR(HDRMerge),
        ADD_FILTER_CREATOR(SequenceIdFilter),  ADD_FILTER_CREATOR(DecimationFilter),
        ADD_FILTER_CREATOR(IMUFrameReversion), ADD_FILTER_CREATOR(FormatConverter),
        ADD_FILTER_CREATOR(FrameMirror),       ADD_FILTER_CREATOR(FrameFlip),
        ADD_FILTER_CREATOR(FrameRotate),       ADD_FILTER_CREATOR(PointCloudFilter),
        ADD_FILTER_CREATOR(IMUCorrector),      ADD_FILTER_CREATOR(Align),
    };

    return filterCreators;
}

}  // namespace PublicFilterCreatorLoader
}  // namespace libobsensor