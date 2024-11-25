// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IFilter.hpp"
#include "stream/StreamProfile.hpp"
#include "AlignImpl.hpp"

namespace libobsensor {

/**
 * @brief Aligh depth to color or vice verse
 */
class Align : public IFilterBase {
public:
    explicit Align();
    virtual ~Align() noexcept;

    OBStreamType getAlignToStreamType() {
        return alignToStreamType_;
    }

    void               updateConfig(std::vector<std::string> &params) override;
    const std::string &getConfigSchema() const override;

    void setAlignToStreamProfile(std::shared_ptr<const VideoStreamProfile> toProfile);

    void reset() override;

private:
    std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;

protected:
    OBFrameType getAlignFrameType();

    void alignFrames(const std::shared_ptr<const Frame> from, std::shared_ptr<Frame> align, const std::shared_ptr<const Frame> depth);

    std::shared_ptr<VideoStreamProfile> createAlignedProfile(std::shared_ptr<const VideoStreamProfile> fromProfile,
                                                             std::shared_ptr<const VideoStreamProfile> toProfile);

    virtual void resetCache() {};

private:
    std::recursive_mutex alignMutex_;

    AlignImpl                 *impl_;
    OBStreamType               alignToStreamType_;
    bool                       addTargetDistortion_;
    bool                       gapFillCopy_;
    bool                       matchTargetRes_;

    std::shared_ptr<const VideoStreamProfile> fromProfile_;
    std::shared_ptr<const VideoStreamProfile> toProfile_;
    std::shared_ptr<VideoStreamProfile>       alignProfile_;
};

}  // namespace libobsensor
