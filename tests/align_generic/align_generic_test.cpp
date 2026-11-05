// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "AlignImplGeneric.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <limits>
#include <vector>

using libobsensor::AlignImplGeneric;

class GenericAlign : public ::testing::Test {
protected:
    OBCameraIntrinsic     intr{ 100, 100, 2, 2, 5, 5 };
    OBCameraDistortion    distortion{};
    OBExtrinsic           extrinsic{};
    AlignImplGeneric      align;
    std::vector<uint16_t> depth  = std::vector<uint16_t>(25, 1000);
    std::vector<uint16_t> output = std::vector<uint16_t>(25, 123);
    void                  SetUp() override {
        extrinsic.rot[0] = extrinsic.rot[4] = extrinsic.rot[8] = 1;
    }
    void init(bool copy = true, float unit = 1, OBFormat format = OB_FORMAT_Y16) {
        align.initialize(intr, distortion, intr, distortion, extrinsic, unit, true, copy, false, format, 65535);
    }
    int d2c() {
        return align.D2C(depth.data(), 5, 5, output.data(), 5, 5, nullptr, false);
    }
};

TEST_F(GenericAlign, IdentityIncludesImageEdges) {
    init();
    ASSERT_EQ(0, d2c());
    EXPECT_EQ(depth, output);
}

TEST_F(GenericAlign, FootprintClipsAtImageEdges) {
    init(false);
    ASSERT_EQ(0, d2c());
    EXPECT_EQ(depth, output);
}

TEST_F(GenericAlign, InvalidDepthAndOcclusion) {
    depth.assign(25, 0);
    depth[0] = 65535;
    depth[1] = 2000;
    depth[2] = 1000;
    init();
    ASSERT_EQ(0, d2c());
    EXPECT_EQ(0, output[0]);
    EXPECT_EQ(2000, output[1]);
    EXPECT_EQ(1000, output[2]);
    EXPECT_EQ(1000, output[7]);
    EXPECT_EQ(0, output[24]);
}

TEST_F(GenericAlign, TranslationUsesDepthUnits) {
    depth.assign(25, 0);
    depth[12]          = 1000;
    extrinsic.trans[0] = 20;  // 20 mm / (1000 * 2 mm) * fx = one pixel.
    extrinsic.trans[2] = 200;
    init(true, 2);
    EXPECT_FLOAT_EQ(2, align.getDepthUnit());
    ASSERT_EQ(0, d2c());
    EXPECT_EQ(1100, output[13]);
    EXPECT_EQ(0, output[12]);
}

TEST_F(GenericAlign, BehindCameraDoesNotWrite) {
    extrinsic.trans[2] = -2000;
    init();
    ASSERT_EQ(0, d2c());
    EXPECT_EQ(0, std::count_if(output.begin(), output.end(), [](uint16_t v) { return v != 0; }));
}

TEST_F(GenericAlign, OutsideImageDoesNotWriteInEitherFillMode) {
    extrinsic.trans[0] = -10000;
    for(bool copy: { false, true }) {
        init(copy);
        ASSERT_EQ(0, d2c());
        EXPECT_EQ(std::vector<uint16_t>(25, 0), output);
    }
}

TEST_F(GenericAlign, ColorMappingClearsInvalidPixels) {
    init();
    depth[12] = 0;
    for(auto format: { OB_FORMAT_Y8, OB_FORMAT_Y16, OB_FORMAT_RGB, OB_FORMAT_BGR, OB_FORMAT_RGBA, OB_FORMAT_BGRA }) {
        const size_t         bytes = format == OB_FORMAT_Y8 ? 1 : format == OB_FORMAT_Y16 ? 2 : (format == OB_FORMAT_RGB || format == OB_FORMAT_BGR) ? 3 : 4;
        std::vector<uint8_t> color(25 * bytes), mapped(25 * bytes, 255);
        for(size_t i = 0; i < color.size(); ++i) {
            color[i] = static_cast<uint8_t>(i + 1);
        }
        auto expected = color;
        std::fill(expected.begin() + 12 * bytes, expected.begin() + 13 * bytes, 0);
        ASSERT_EQ(0, align.C2D(depth.data(), 5, 5, color.data(), mapped.data(), 5, 5, format, false));
        EXPECT_EQ(expected, mapped);
    }
}

TEST_F(GenericAlign, MapOnlyAndInvalidSentinels) {
    init();
    depth[0] = 0;
    std::vector<int> map(50, 123);
    ASSERT_EQ(0, align.D2C(depth.data(), 5, 5, nullptr, 5, 5, map.data(), false));
    EXPECT_EQ(-1, map[0]);
    EXPECT_EQ(-1, map[1]);
    EXPECT_EQ(4, map[48]);
    EXPECT_EQ(4, map[49]);
}

TEST_F(GenericAlign, PackedDepthPreservesConfidence) {
    depth.assign(25, static_cast<uint16_t>((1000 << 4) | 7));
    depth[0] = 7;  // A zero depth with nonzero confidence is still invalid.
    init(true, 1, OB_FORMAT_Y12C4);
    ASSERT_EQ(0, d2c());
    EXPECT_EQ(0, output[0]);
    EXPECT_EQ((1000 << 4) | 7, output[12]);
}

TEST_F(GenericAlign, DistortionRoundTrip) {
    intr.fx = intr.fy = 4;
    for(auto model: { OB_DISTORTION_BROWN_CONRADY, OB_DISTORTION_BROWN_CONRADY_K6, OB_DISTORTION_KANNALA_BRANDT4 }) {
        distortion.model = model;
        distortion.k1    = 0.01f;
        distortion.k4    = 0.005f;
        distortion.p1    = 0.001f;
        init();
        ASSERT_EQ(0, d2c());
        EXPECT_EQ(depth, output);
    }
}

TEST_F(GenericAlign, TargetTangentialDistortionIsApplied) {
    depth.assign(25, 0);
    depth[12] = 1000;
    OBCameraDistortion target{};
    target.model        = OB_DISTORTION_BROWN_CONRADY_K6;
    target.p2           = 1;
    auto colorIntrinsic = intr;
    colorIntrinsic.fx   = 1;
    extrinsic.trans[0]  = 1000;
    align.initialize(intr, distortion, colorIntrinsic, target, extrinsic, 1, true, true, false, OB_FORMAT_Y16, 65535);
    ASSERT_EQ(0, d2c());
    // x/z=1, distorted x=1+3*p2=4, then u=6, outside the image.
    EXPECT_EQ(std::vector<uint16_t>(25, 0), output);
}

TEST_F(GenericAlign, ResetAndInvalidArguments) {
    EXPECT_EQ(-1, d2c());
    init();
    EXPECT_EQ(-1, align.D2C(nullptr, 5, 5, output.data(), 5, 5, nullptr, false));
    EXPECT_EQ(-1, align.D2C(depth.data(), 4, 5, output.data(), 5, 5, nullptr, false));
    EXPECT_EQ(-1, align.C2D(depth.data(), -1, 5, output.data(), output.data(), 5, 5, OB_FORMAT_RGB, false));
    EXPECT_EQ(-1, align.C2D(depth.data(), 5, 5, output.data(), output.data(), 5, 5, OB_FORMAT_MJPG, false));
    align.reset();
    EXPECT_EQ(-1, d2c());
    EXPECT_ANY_THROW(init(true, 0));
    EXPECT_ANY_THROW(init(true, std::numeric_limits<float>::quiet_NaN()));
}

TEST_F(GenericAlign, NonfiniteProjectionDoesNotWrite) {
    extrinsic.trans[0] = std::numeric_limits<float>::infinity();
    init();
    ASSERT_EQ(0, d2c());
    EXPECT_EQ(std::vector<uint16_t>(25, 0), output);
}

TEST_F(GenericAlign, DifferentResolutionsFillPixelFootprints) {
    OBCameraIntrinsic color{ 200, 200, 4, 4, 9, 9 };
    align.initialize(intr, distortion, color, distortion, extrinsic, 1, false, false, false, OB_FORMAT_Y16, 65535);
    std::vector<uint16_t> scaled(81, 0);
    ASSERT_EQ(0, align.D2C(depth.data(), 5, 5, scaled.data(), 9, 9, nullptr, false));
    EXPECT_EQ(std::vector<uint16_t>(81, 1000), scaled);
}

TEST_F(GenericAlign, NearestDepthWinsWhenProjectedPixelsOverlap) {
    depth.assign(25, 2000);
    depth[12]  = 1000;
    auto color = intr;
    color.fx = color.fy = 1;
    align.initialize(intr, distortion, color, distortion, extrinsic, 1, false, true, false, OB_FORMAT_Y16, 65535);
    ASSERT_EQ(0, d2c());
    EXPECT_EQ(1000, output[12]);
}

TEST_F(GenericAlign, PackedDepthTranslationKeepsConfidence) {
    depth.assign(25, static_cast<uint16_t>((1000 << 4) | 7));
    extrinsic.trans[2] = 100;
    init(true, 1, OB_FORMAT_Y12C4);
    ASSERT_EQ(0, d2c());
    EXPECT_EQ((1100 << 4) | 7, output[12]);
}
