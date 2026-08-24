// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/hpp/Utils.hpp>

#include <cmath>
#include <cstring>
#include <memory>
#include <string>

class TC_CPP_20_CoordTransform : public SDKTestBase {};

TEST_F(TC_CPP_20_CoordTransform, TC_CPP_20_01_3d_to_3d) {
    OBPoint3f   src      = { 100.0f, 200.0f, 300.0f };
    OBExtrinsic identity = {};
    identity.rot[0]      = 1.0f;
    identity.rot[4]      = 1.0f;
    identity.rot[8]      = 1.0f;
    identity.trans[0]    = 0.0f;
    identity.trans[1]    = 0.0f;
    identity.trans[2]    = 0.0f;

    OBPoint3f dst = {};
    bool      ok  = ob::CoordinateTransformHelper::transformation3dto3d(src, identity, &dst);
    ASSERT_TRUE(ok);
    EXPECT_NEAR(dst.x, src.x, 1e-3f);
    EXPECT_NEAR(dst.y, src.y, 1e-3f);
    EXPECT_NEAR(dst.z, src.z, 1e-3f);

    OBExtrinsic withTrans = identity;
    withTrans.trans[0]    = 10.0f;
    withTrans.trans[1]    = 20.0f;
    withTrans.trans[2]    = 30.0f;
    ok                    = ob::CoordinateTransformHelper::transformation3dto3d(src, withTrans, &dst);
    ASSERT_TRUE(ok);
    EXPECT_NEAR(dst.x, src.x + 10.0f, 1e-3f);
    EXPECT_NEAR(dst.y, src.y + 20.0f, 1e-3f);
    EXPECT_NEAR(dst.z, src.z + 30.0f, 1e-3f);
}

TEST_F(TC_CPP_20_CoordTransform, TC_CPP_20_02_2d_depth_to_3d) {
    OBCameraIntrinsic intrinsic = {};
    intrinsic.fx                = 500.0f;
    intrinsic.fy                = 500.0f;
    intrinsic.cx                = 320.0f;
    intrinsic.cy                = 240.0f;
    intrinsic.width             = 640;
    intrinsic.height            = 480;

    OBExtrinsic identity = {};
    identity.rot[0]      = 1.0f;
    identity.rot[4]      = 1.0f;
    identity.rot[8]      = 1.0f;

    OBPoint2f pixel   = { 320.0f, 240.0f };
    OBPoint3f point3d = {};
    bool      ok      = ob::CoordinateTransformHelper::transformation2dto3d(pixel, 1000.0f, intrinsic, identity, &point3d);
    ASSERT_TRUE(ok);
    EXPECT_NEAR(point3d.x, 0.0f, 0.5f);
    EXPECT_NEAR(point3d.y, 0.0f, 0.5f);
    EXPECT_NEAR(point3d.z, 1000.0f, 5.0f);
}

TEST_F(TC_CPP_20_CoordTransform, TC_CPP_20_03_3d_to_2d) {
    OBCameraIntrinsic intrinsic = {};
    intrinsic.fx                = 500.0f;
    intrinsic.fy                = 500.0f;
    intrinsic.cx                = 320.0f;
    intrinsic.cy                = 240.0f;
    intrinsic.width             = 640;
    intrinsic.height            = 480;

    OBCameraDistortion distortion = {};

    OBExtrinsic identity = {};
    identity.rot[0]      = 1.0f;
    identity.rot[4]      = 1.0f;
    identity.rot[8]      = 1.0f;

    OBPoint3f src   = { 0.0f, 0.0f, 1000.0f };
    OBPoint2f pixel = {};
    bool      ok    = ob::CoordinateTransformHelper::transformation3dto2d(src, intrinsic, distortion, identity, &pixel);
    ASSERT_TRUE(ok);
    EXPECT_NEAR(pixel.x, 320.0f, 1.0f);
    EXPECT_NEAR(pixel.y, 240.0f, 1.0f);
}

TEST_F(TC_CPP_20_CoordTransform, TC_CPP_20_04_2d_to_2d) {
    OBCameraIntrinsic srcIntrinsic = {};
    srcIntrinsic.fx                = 500.0f;
    srcIntrinsic.fy                = 500.0f;
    srcIntrinsic.cx                = 320.0f;
    srcIntrinsic.cy                = 240.0f;
    srcIntrinsic.width             = 640;
    srcIntrinsic.height            = 480;

    OBCameraDistortion srcDist      = {};
    OBCameraIntrinsic  tgtIntrinsic = srcIntrinsic;
    OBCameraDistortion tgtDist      = {};

    OBExtrinsic identity = {};
    identity.rot[0]      = 1.0f;
    identity.rot[4]      = 1.0f;
    identity.rot[8]      = 1.0f;

    OBPoint2f srcPx = { 400.0f, 300.0f };
    OBPoint2f dstPx = {};
    bool      ok    = ob::CoordinateTransformHelper::transformation2dto2d(srcPx, 1000.0f, srcIntrinsic, srcDist, tgtIntrinsic, tgtDist, identity, &dstPx);
    ASSERT_TRUE(ok);
    EXPECT_NEAR(dstPx.x, srcPx.x, 2.0f);
    EXPECT_NEAR(dstPx.y, srcPx.y, 2.0f);
}