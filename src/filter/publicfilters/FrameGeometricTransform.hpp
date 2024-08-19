#pragma once
#include "IFilter.hpp"
#include "stream/StreamProfile.hpp"
#include <mutex>
#include <thread>
#include <atomic>

namespace libobsensor {

class FrameMirror : public IFilterBase {
public:
    FrameMirror();
    virtual ~FrameMirror() noexcept;

    void               updateConfig(std::vector<std::string> &params) override;
    const std::string &getConfigSchema() const override;
    void               reset() override;

private:
    std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;

    static OBCameraIntrinsic  mirrorOBCameraIntrinsic(const OBCameraIntrinsic &src);
    static OBCameraDistortion mirrorOBCameraDistortion(const OBCameraDistortion &src);

protected:
    std::shared_ptr<const StreamProfile> srcStreamProfile_;
    std::shared_ptr<VideoStreamProfile>  rstStreamProfile_;
};

class FrameFlip : public IFilterBase {
public:
    FrameFlip();
    virtual ~FrameFlip() noexcept;

    void               updateConfig(std::vector<std::string> &params) override;
    const std::string &getConfigSchema() const override;
    void               reset() override;

private:
    std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;

    static OBCameraIntrinsic  flipOBCameraIntrinsic(const OBCameraIntrinsic &src);
    static OBCameraDistortion flipOBCameraDistortion(const OBCameraDistortion &src);

protected:
    std::shared_ptr<const StreamProfile> srcStreamProfile_;
    std::shared_ptr<VideoStreamProfile>  rstStreamProfile_;
};

class FrameRotate : public IFilterBase {
public:
    FrameRotate();
    virtual ~FrameRotate() noexcept;

    void               updateConfig(std::vector<std::string> &params) override;
    const std::string &getConfigSchema() const override;
    void               reset() override;

private:
    std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;

    static OBCameraIntrinsic  rotateOBCameraIntrinsic(const OBCameraIntrinsic &src, uint32_t rotateDegree);
    static OBCameraDistortion rotateOBCameraDistortion(const OBCameraDistortion &src, uint32_t rotateDegree);
    static OBExtrinsic        rotateOBExtrinsic(uint32_t rotateDegree);

protected:
    std::mutex                           mtx_;
    uint32_t                             rotateDegree_ = 0;
    std::atomic<bool>                    rotateDegreeUpdated_;
    std::shared_ptr<const StreamProfile> srcStreamProfile_;
    std::shared_ptr<VideoStreamProfile>  rstStreamProfile_;
};
}  // namespace libobsensor
