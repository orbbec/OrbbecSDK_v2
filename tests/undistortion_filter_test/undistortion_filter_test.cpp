// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "UnDistortionImplGeneric.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <vector>

namespace {

constexpr int     kWidth  = 4;
constexpr int     kHeight = 2;
constexpr uint8_t kGuard  = 0xA5;

bool check(bool condition, const char *message) {
    if(!condition) {
        std::cerr << message << std::endl;
    }
    return condition;
}

std::vector<uint8_t> undistortWithHorizontalShift(float newCx) {
    const OBCameraIntrinsic sourceIntrinsic = { 1.0f, 1.0f, 0.0f, 0.0f, kWidth, kHeight };
    const OBCameraIntrinsic outputIntrinsic = { 1.0f, 1.0f, newCx, 0.0f, kWidth, kHeight };
    OBCameraDistortion      distortion{};
    distortion.model = OB_DISTORTION_NONE;

    // Two distinct rows of [Y0, U, Y1, V] macropixels.
    const std::vector<uint8_t> source = {
        10, 80, 20, 160, 30, 90, 40, 170, 50, 81, 60, 161, 70, 91, 80, 171,
    };
    std::vector<uint8_t> guardedOutput(source.size() + 2, kGuard);

    libobsensor::UnDistortionImplGeneric impl;
    impl.initialize(sourceIntrinsic, distortion, &outputIntrinsic, OB_FORMAT_YUYV, libobsensor::UNDIST_INTERP_BILINEAR);
    impl.undistort(source.data(), guardedOutput.data() + 1, kWidth, kHeight, OB_FORMAT_YUYV);
    return guardedOutput;
}

bool testFullyInvalidPairUsesBlackNeutralChroma() {
    const auto output = undistortWithHorizontalShift(2.0f);
    bool       ok     = true;
    ok &= check(output.front() == kGuard && output.back() == kGuard, "YUYV undistortion wrote outside the output buffer");
    ok &= check(output[1] == 0 && output[2] == 128 && output[3] == 0 && output[4] == 128, "fully invalid YUYV pair must be black with neutral chroma");
    return ok;
}

bool testSingleInvalidPixelPreservesPairedLuma() {
    const auto output = undistortWithHorizontalShift(1.0f);
    bool       ok     = true;
    ok &= check(output.front() == kGuard && output.back() == kGuard, "YUYV undistortion wrote outside the output buffer");
    ok &= check(output[1] == 0 && output[2] == 128 && output[3] == 10 && output[4] == 128,
                "single invalid YUYV pixel must preserve its valid neighbor luma and neutralize shared chroma");
    ok &= check(output[5] == 20 && output[7] == 30, "interior YUYV luma values changed unexpectedly");
    return ok;
}

bool testRightBorderStaysWithinEachRow() {
    const auto output = undistortWithHorizontalShift(-1.0f);
    bool       ok     = true;
    ok &= check(output[5] == 40 && output[6] == 128 && output[7] == 0 && output[8] == 128, "right-border fill changed the first row or its valid paired luma");
    ok &= check(output[13] == 80 && output[14] == 128 && output[15] == 0 && output[16] == 128, "right-border fill crossed a row boundary");
    return ok;
}

bool testOddWidthYuyvIsRejected() {
    constexpr int           kOddWidth = 5;
    const OBCameraIntrinsic intrinsic = { 1.0f, 1.0f, 0.0f, 0.0f, kOddWidth, 1 };
    OBCameraDistortion      distortion{};
    distortion.model = OB_DISTORTION_NONE;
    const std::vector<uint8_t> source(static_cast<size_t>(kOddWidth * 2), 128);
    std::vector<uint8_t>       output(source.size() + 8, kGuard);

    libobsensor::UnDistortionImplGeneric impl;
    impl.initialize(intrinsic, distortion, nullptr, OB_FORMAT_YUYV, libobsensor::UNDIST_INTERP_BILINEAR);
    try {
        impl.undistort(source.data(), output.data(), kOddWidth, 1, OB_FORMAT_YUYV);
    }
    catch(const std::exception &) {
        return true;
    }
    return check(false, "odd-width YUYV must be rejected before remapping");
}

}  // namespace

int main() {
    const bool ok = testFullyInvalidPairUsesBlackNeutralChroma() && testSingleInvalidPixelPreservesPairedLuma() && testRightBorderStaysWithinEachRow()
                    && testOddWidthYuyvIsRejected();
    return ok ? 0 : 1;
}
