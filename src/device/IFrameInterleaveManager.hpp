#pragma once

#include "libobsensor/h/ObTypes.h"
#include "InternalTypes.hpp"
#include <string>
#include <vector>

namespace libobsensor {

constexpr const int32_t kDefaultFrameInterleaveCount = 2;

class IFrameInterleaveManager {
public:
    virtual ~IFrameInterleaveManager() = default;

    virtual void loadFrameInterleave(const std::string &frameInterleaveName) = 0;

    virtual const std::vector<std::string> &getAvailableFrameInterleaveList() const                                                               = 0;
    virtual const std::string              &getCurrentFrameInterleaveName() const                                                                 = 0;
    virtual int32_t                         getParamCount() const                                                                                 = 0;
    virtual const FrameInterleaveParam     &getParam(const std::string &frameInterleaveName, int32_t index) const                                 = 0;
    virtual void                            updateParam(const std::string &frameInterleaveName, const FrameInterleaveParam &param, int32_t index) = 0;
};
}  // namespace libobsensor

#ifdef __cplusplus
extern "C" {
#endif

struct ob_device_frame_interleave_list_t {
    std::vector<std::string> frameInterleaveList;
};

#ifdef __cplusplus
}
#endif