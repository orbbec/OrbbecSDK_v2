// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "LiDARFormatConverter.hpp"
#include "exception/ObException.hpp"
#include "logger/LoggerInterval.hpp"
#include "frame/FrameFactory.hpp"
#include "stream/StreamProfile.hpp"
#include "InternalTypes.hpp"

static inline void convertToCartesianCoordinate(const OBLiDARSpherePoint *sphere, OBLiDARPoint *point) {

    float distance = sphere->distance;
    float theta    = sphere->theta;
    float phi      = sphere->phi;

    constexpr float MY_PI     = 3.14159265358979323846f;
    constexpr float DEG_2_RAD = MY_PI / 180.0f;

    theta *= DEG_2_RAD;  // degrees to rad
    phi *= DEG_2_RAD;    // degrees to rad

    point->x            = distance * cos(theta) * cos(phi);
    point->y            = distance * sin(theta) * cos(phi);
    point->z            = distance * sin(phi);
    point->reflectivity = sphere->reflectivity;
    point->tag          = sphere->tag;
}

namespace libobsensor {

LiDARFormatConverter::LiDARFormatConverter() {}

void LiDARFormatConverter::updateConfig(std::vector<std::string> &params) {
    if(params.size() != 0) {
        throw unsupported_operation_exception("IMUCorrector update config error: unsupported operation.");
    }
}

const std::string &LiDARFormatConverter::getConfigSchema() const {
    static const std::string schema = "";
    return schema;
}

std::shared_ptr<Frame> LiDARFormatConverter::process(std::shared_ptr<const Frame> frame) {
    if(!frame) {
        return nullptr;
    }

    if(frame->getFormat() != OB_FORMAT_LIDAR_POINT) {
        throw invalid_value_exception("Invalid target format, only support OB_FORMAT_LIDAR_POINT");
    }

    const auto &dataSize   = frame->getDataSize();
    auto        pointCount = dataSize / sizeof(OBLiDARSpherePoint);

    auto outFrame       = FrameFactory::createFrameFromOtherFrame(frame, false);
    auto spherePointPtr = reinterpret_cast<const OBLiDARSpherePoint *>(frame->getData());
    auto obPointPtr     = reinterpret_cast<OBLiDARPoint *>(outFrame->getDataMutable());

    for(uint32_t i = 0; i < pointCount; i++) {
        convertToCartesianCoordinate(spherePointPtr, obPointPtr);
        spherePointPtr++;
        obPointPtr++;
    }

    return outFrame;
}

}  // namespace libobsensor
