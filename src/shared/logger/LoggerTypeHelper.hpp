#pragma once

#include <sstream>
#include <spdlog/fmt/bundled/format.h>
#include "utils/PublicTypeHelper.hpp"

#define OB_LOG_FORMATTER(type)                                                                                       \
    template <> struct fmt::formatter<type> {                                                                         \
        constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {                                    \
            return ctx.end();                                                                                         \
        }                                                                                                             \
        template <typename FormatContext> auto format(const type &input, FormatContext &ctx) -> decltype(ctx.out()) { \
            std::ostringstream oss;                                                                                   \
            oss << input;                                                                                             \
            return format_to(ctx.out(), "{}", oss.str());                                                             \
        }                                                                                                             \
    };

OB_LOG_FORMATTER(OBFormat)
OB_LOG_FORMATTER(OBFrameType)
OB_LOG_FORMATTER(OBStreamType)
OB_LOG_FORMATTER(OBSensorType)
OB_LOG_FORMATTER(OBIMUSampleRate)
OB_LOG_FORMATTER(OBGyroFullScaleRange)
OB_LOG_FORMATTER(OBAccelFullScaleRange)
OB_LOG_FORMATTER(OBCameraParam)
OB_LOG_FORMATTER(OBCalibrationParam)
OB_LOG_FORMATTER(OBPoint3f)
OB_LOG_FORMATTER(OBPoint2f)
OB_LOG_FORMATTER(OBExtrinsic)
OB_LOG_FORMATTER(OBCameraIntrinsic)
OB_LOG_FORMATTER(OBCameraDistortion)
OB_LOG_FORMATTER(OBFilterConfigValueType)
OB_LOG_FORMATTER(OBExceptionType)
