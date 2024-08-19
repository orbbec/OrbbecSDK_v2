#include "IMUFrameReversion.hpp"
#include "frame/Frame.hpp"
#include "frame/FrameFactory.hpp"
#include "logger/LoggerInterval.hpp"
#include "exception/ObException.hpp"
#include "libobsensor/h/ObTypes.h"

namespace libobsensor {

IMUFrameReversion::IMUFrameReversion() {}

IMUFrameReversion::~IMUFrameReversion() noexcept {}

void IMUFrameReversion::updateConfig(std::vector<std::string> &params) {
    if(params.size() != 0) {
        throw unsupported_operation_exception("IMUFrameReversion update config error: unsupported operation.");
    }
}

const std::string &IMUFrameReversion::getConfigSchema() const {
    static const std::string schema = "";
    return schema;
}

std::shared_ptr<Frame> IMUFrameReversion::process(std::shared_ptr<const Frame> frame) {
    if(!frame) {
        return nullptr;
    }

    auto newFrame = FrameFactory::createFrameFromOtherFrame(frame, true);
    if(!frame->is<FrameSet>()) {
        return newFrame;
    }

    auto frameSet   = newFrame->as<FrameSet>();
    auto accelFrame = frameSet->getFrame(OB_FRAME_ACCEL);
    if(accelFrame) {
        AccelFrame::Data *frameData = (AccelFrame::Data *)accelFrame->getData();
        frameData->value.x *= -1;
        frameData->value.y *= -1;
        frameData->value.z *= -1;
    }

    auto gyroFrame = frameSet->getFrame(OB_FRAME_GYRO);
    if(gyroFrame) {
        GyroFrame::Data *gyroFrameData = (GyroFrame::Data *)gyroFrame->getData();
        gyroFrameData->value.x *= -1;
        gyroFrameData->value.y *= -1;
        gyroFrameData->value.z *= -1;
    }

    return newFrame;
}

}  // namespace libobsensor
