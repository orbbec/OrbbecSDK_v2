#pragma once

namespace libobsensor {
#include <string>
#include <vector>

class IFrameInterleaveManager {
public:
    virtual ~IFrameInterleaveManager() = default;

    virtual void               loadFrameInterleave(const std::string &frameInterleaveName) = 0;

    virtual const std::vector<std::string> &getAvailableFrameInterleaveList() const = 0;
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