// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <atomic>
#include <memory>
#include <vector>

namespace libobsensor {
class StreamProfile;

typedef std::vector<std::shared_ptr<const StreamProfile>> StreamProfileList;

class IStreamProfileFilter {
public:
    virtual ~IStreamProfileFilter() = default;

    virtual StreamProfileList filter(const StreamProfileList &profiles) const = 0;
};

}  // namespace libobsensor

#ifdef __cplusplus
extern "C" {
#endif
struct ob_stream_profile_t {
    std::shared_ptr<const libobsensor::StreamProfile> profile;
};

struct ob_stream_profile_list_t {
    std::vector<std::shared_ptr<const libobsensor::StreamProfile>> profileList;
};
#ifdef __cplusplus
}
#endif
