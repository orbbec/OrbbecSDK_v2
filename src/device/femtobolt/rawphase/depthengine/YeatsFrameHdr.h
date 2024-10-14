// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

//
// YeatsFrameHdr.h
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
// Defines types for Yeats chip headers.
//

#pragma once
#ifndef _YEATSFRAMEHDR_H_
#define _YEATSFRAMEHDR_H_

#include <cstdint>
#include <cstddef>

const int YEATS_MIPI_HDR_SIZE_V1        = 128;
const int YEATS_MIPI_HDR_SIZE_V2        = 160;
const int YEATS_APP_FRAME_HDR_SIZE      = 512;
const int YEATS_APP_FRAME_HDR_SIZE_V2   = 512;
const int YEATS_MAX_NUM_CAPTURE_HEADERS = 16;

union CLKGEN_TYPE {
    uint16_t val;
    struct {
        uint8_t rise : 7;
        uint8_t resv0 : 1;
        uint8_t fall : 7;
        uint8_t resv1 : 1;
    } bits;
};

struct YEATS_MIPI_HDR {
    uint16_t chipId;
    uint16_t mipiHdrRev;

    union {
        uint32_t val;
        struct {
            uint16_t low;
            uint16_t high;
        } bits;

    } lx5FwRev;

    uint16_t frameNum;
    uint16_t roiNum;
    uint16_t frameWidth;   // One based value
    uint16_t frameHeight;  // One based value
    uint16_t startRow;     // Zero based value
    uint16_t startCol;     // Zero based value
    uint16_t dumpStyle;    // Not unpacking bit fields within this offset

    union {
        uint16_t val;
        struct {
            uint8_t outWidth : 5;
            uint8_t resv0 : 3;
            uint8_t raw : 2;
            uint8_t compressed : 2;
            uint8_t resv1 : 3;
            uint8_t linear : 1;
        } bits;

    } outFormat;

    uint16_t dynCal;
    uint16_t rowsPerPkt;

    union {
        uint32_t val;
        struct {
            uint16_t low;
            uint16_t high;
        } bits;

    } useqFWRev;

    uint16_t ts_cal_ver;
    uint16_t adc_cal_ver;
    uint16_t gain_cal_ver;

    uint16_t LX5_0;
    uint16_t LX5_1;
    uint16_t LX5_2;
    uint16_t LX5_3;

    uint16_t tie_pc_bypass;
    uint32_t capTimerStart;
    uint32_t capTimerEnd;

    union {
        uint16_t val;
        struct {
            uint16_t adcVal : 12;
            uint16_t adcOver : 1;
            uint16_t resv0 : 3;
        } bits;

    } tempSensorADC;

    CLKGEN_TYPE clkgen_CK1;
    CLKGEN_TYPE clkgen_CK2;
    CLKGEN_TYPE clkgen_CK1ref;
    CLKGEN_TYPE clkgen_CK2ref;
    CLKGEN_TYPE clkgen_L1;
    CLKGEN_TYPE clkgen_L2;

    union {
        uint16_t val;
        struct {
            uint8_t clkgen1_rise : 2;
            uint8_t clkgen1_fall : 2;
            uint8_t clkgen2_rise : 2;
            uint8_t clkgen2_fall : 2;
            uint8_t clkgen1Ref_rise : 2;
            uint8_t clkgen1Ref_fall : 2;
            uint8_t clkgen2Ref_rise : 2;
            uint8_t clkgen2Ref_fall : 2;
        } bits;

    } clkgen_CKX;

    uint16_t clkgen_Cycle;
    uint16_t clkgen_offset;
    uint16_t clkgen_ltoffset;
    uint16_t clkgen_burst_period;

    uint16_t reg_0;
    uint16_t reg_1;
    uint16_t reg_2;
    uint16_t reg_3;
    uint16_t reg_4;
    uint16_t reg_5;
    uint16_t reg_6;
    uint16_t reg_7;

    // Scratch Pad Regs
    union {
        uint16_t val;  // uSeq0
        struct {
            uint16_t mode : 4;
            uint16_t resv0 : 12;
        } bits;

    } mode_info;

    union {
        uint16_t val;
        struct {
            uint16_t id : 6;
            uint16_t nCaptures : 6;
            uint16_t nFreqs : 4;
        } bits;

    } capture_info;

    union {
        uint16_t val;  // uSeq2
        struct {
            uint8_t phaseStep : 4;
            uint8_t nPhaseSteps : 4;
            uint8_t freqId : 4;
            uint8_t rsvd0 : 4;
        } bits;
    } phase_info;

    uint16_t capture_clkgen;
    union {
        uint16_t val;
        struct {
            uint16_t ClkgenNumerator : 9;  // Bits 0-8
            uint16_t ClkgenDenom : 7;      // Bits 9-15
        } bits;
    } capture_phaseShift;

    uint16_t modFreqVal;  // units of 10KHz

    union {
        uint16_t val;  // uSeq6
        struct {
            uint16_t Dump_type : 4;  // Pixel Data Type : 0 - diff, 1 - PCM, 2 - CM, 3 - Single A, 4 - Single B, 5 - Other
            uint16_t resv0 : 12;
        } bits;
    } dump_info;

    uint16_t integrationTime;
    uint16_t intercaptureDelay;

    union {
        uint16_t val;
        struct {
            uint8_t LSDAC_Star;  // Starboard - Right
            uint8_t LSDAC_Port;  // Left
        } bits;
    } LSDAC;

    union {
        uint16_t val;  // uSeq10
        struct {
            uint16_t LaserEn : 3;  //[0:2] - Enable Laser, 0 - Starboard, 1 - Port, 2 - LT / ST
            uint16_t resv0 : 13;
        } bits;
    } LSFlags;

    uint16_t uSeq_11;
    uint16_t uSeq_12;
    uint16_t uSeq_13;
    uint16_t uSeq_14;
    uint16_t uSeq_15;

    // Bytes available in v2 of MIPI Header
    uint16_t resv[16];
};

// static_assert(sizeof(YEATS_MIPI_HDR) == YEATS_MIPI_HDR_SIZE_V2, "Check type definition of YEATS_MIPI_HDR");
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
struct YEATS_APP_FRAME_HDR {
    union {

        uint8_t rawBytes[YEATS_APP_FRAME_HDR_SIZE];

        struct {
            uint16_t chipId;
            uint16_t mipiHdrRev;
            uint32_t lx5FwRev;
            uint16_t frameNum;
            uint16_t roiNum;
            uint16_t frameWidth;   // One based value
            uint16_t frameHeight;  // One based value
            uint16_t startRow;     // Zero based value
            uint16_t startCol;     // Zero based value
            uint16_t dumpStyle;    // Not unpacking bit fields within this offset
            uint16_t outFormat;
            uint16_t dynCal;
            uint16_t rowsPerPkt;
            uint32_t useqFWRev;
            uint16_t ts_cal_ver;
            uint16_t adc_cal_ver;
            uint16_t gain_cal_ver;
            uint16_t LX5_0;
            uint16_t LX5_1;
            uint16_t LX5_2;
            uint16_t LX5_3;
            uint16_t tie_pc_bypass;
            uint32_t capTimerStart;
            uint32_t capTimerEnd;
            uint16_t tempSensorADC;
            uint16_t clkgen_CK1;
            uint16_t clkgen_CK2;
            uint16_t clkgen_CK1ref;
            uint16_t clkgen_CK2ref;
            uint16_t clkgen_L1;
            uint16_t clkgen_L2;
            uint16_t clkgen_CKX;
            uint16_t clkgen_Cycle;
            uint16_t clkgen_offset;
            uint16_t clkgen_ltoffset;
            uint16_t clkgen_burst_period;
            uint16_t reg_0;
            uint16_t reg_1;
            uint16_t reg_2;
            uint16_t reg_3;
            uint16_t reg_4;
            uint16_t reg_5;
            uint16_t reg_6;
            uint16_t reg_7;

            uint16_t mode;  // Extracted field from REG_MODE_ID_CURR(uSeq0)
            uint16_t uSeq_0;

            uint16_t captureId;  // Extracted field from REG_CAPTURE_ID
            uint16_t nCaptures;  // Extracted field from REG_CAPTURE_ID
            uint16_t nFreqs;     // Extracted field from REG_CAPTURE_ID

            uint16_t phaseStep;    // Extracted field from REG_CAPTURE_PHASESHIFT_ID(uSeq2)
            uint16_t nPhaseSteps;  // Extracted field from REG_CAPTURE_PHASESHIFT_ID(uSeq2)
            uint16_t freqId;       // Extracted field from REG_CAPTURE_PHASESHIFT_ID(uSeq2)
            uint16_t uSeq_2;

            uint16_t capture_clkgen;
            uint16_t ClkgenNumerator;
            uint16_t ClkgenDenom;
            uint16_t modFreqVal;  // units of 10KHz

            uint16_t dumpType;  // Pixel Data Type : 0 - diff, 1 - PCM, 2 - CM, 3 - Single A, 4 - Single B, 5 - Other
            uint16_t uSeq_6;

            uint16_t integrationTime;
            uint16_t intercaptureDelay;

            // Expanded fields from LSDAC field of YEATS_MIPI_HDR
            uint8_t LSDAC_Star;
            uint8_t LSDAC_Port;

            uint16_t LSFlags;  //[0:2] - Enable Laser, 0 - Starboard, 1 - Port, 2 - LT/ST
            uint16_t uSeq_10;

            uint16_t uSeq_11;
            uint16_t uSeq_12;
            uint16_t uSeq_13;
            uint16_t uSeq_14;
            uint16_t uSeq_15;
            uint16_t rsvd;  // Complier alignment

            // App layer inserted fields
            float    Temp_sensor_DegC;
            float    Temp_sensor_slope;
            float    Temp_sensor_offset;
            float    Laser_Temp0;
            uint32_t AppTimeStamp;
            uint64_t FWTimeStamp;
            float    SoCTemp;
        } bits;
    };
};

// static_assert(sizeof(YEATS_APP_FRAME_HDR) == YEATS_APP_FRAME_HDR_SIZE, "Check type definition of YEATS_APP_FRAME_HDR");

// Appended at the end of YEATS_APP_FRAME_HDR_V2 nCaptureHeaders field.
struct YEATS_APP_CAPTURE_HEADER {
    uint8_t  appCaptureHdrVer;  // Version of this struct.
    uint8_t  captureId;         // Extracted field from REG_CAPTURE_ID
    uint8_t  phaseStep;         // Extracted field from REG_CAPTURE_PHASESHIFT_ID(uSeq2)
    uint8_t  freqId;            // Extracted field from REG_CAPTURE_PHASESHIFT_ID(uSeq2)
    uint32_t capTimerStart;
    uint32_t capTimerEnd;
    uint16_t ClkgenNumerator;
    uint16_t ClkgenDenom;
    uint16_t modFreqVal;  // units of 10KHz
    uint16_t integrationTime;
    uint16_t intercaptureDelay;
    uint16_t resv;
};

// static_assert(sizeof(YEATS_APP_CAPTURE_HEADER) == 24, "Check type definition of YEATS_APP_FRAME_HDR");

struct YEATS_APP_FRAME_HDR_V2 {
    union {
        uint8_t rawBytes[YEATS_APP_FRAME_HDR_SIZE_V2];

        struct {
            uint16_t appFrameHdrVer;  // Start off with the version of this struct.
            uint16_t chipId;
            uint32_t lx5FwRev;
            uint32_t useqFWRev;
            uint32_t appFrameNum;  // This refers to the processed frame number.
            uint16_t rawFrameNum;  // This refers to the captured frame number.
            uint16_t roiNum;
            uint16_t appFrameWidth;   // One based value OUTPUT image width.
            uint16_t appFrameHeight;  // ONE based value OUTPUT image height.
            uint16_t rawFrameWidth;   // One based value INPUT image width.
            uint16_t rawFrameHeight;  // One based value INPUT image height.
            uint16_t rawStartRow;     // Zero based value of the raw image offset row relative to sensor array.
            uint16_t rawStartCol;     // Zero based value of the raw image offset column relative to sensor array.
            uint16_t dumpStyle;       // Not unpacking bit fields within this offset
            uint16_t outFormat;
            uint16_t dynCal;
            uint16_t rowsPerPkt;
            uint16_t ts_cal_ver;
            uint16_t adc_cal_ver;
            uint16_t gain_cal_ver;
            uint16_t LX5_0;
            uint16_t LX5_1;
            uint16_t LX5_2;
            uint16_t LX5_3;
            uint16_t tie_pc_bypass;
            uint16_t mode;         // Extracted field from REG_MODE_ID_CURR(uSeq0)
            uint16_t nCaptures;    // Extracted field from REG_CAPTURE_ID
            uint16_t nFreqs;       // Extracted field from REG_CAPTURE_ID
            uint16_t nPhaseSteps;  // Extracted field from REG_CAPTURE_PHASESHIFT_ID(uSeq2)
            uint16_t capture_clkgen;
            uint16_t dumpType;  // Pixel Data Type : 0 - diff, 1 - PCM, 2 - CM, 3 - Single A, 4 - Single B, 5 - Other

            float ModuleTemp;
            float SoCTemp;
            float sensorTemp;  // The sensor temperature that is used by depth engine for temperature correction.

            uint64_t mipiRxNanoseconds;        // The time stamp when the last MIPI capture was received, in nanoseconds.
            uint64_t appStartNanoseconds;      // The time stamp when the app (eR2D) starts, in nanoseconds.
            uint64_t appEndNanoseconds;        // The time stamp when the app (eR2D) ends, in nanoseconds.
            uint64_t usbTxRequestNanoseconds;  // The time stamp when a USB transfer request is made, in nanoseconds.

            // Expanded fields from LSDAC field of YEATS_MIPI_HDR
            uint16_t LSDAC_Star;
            uint16_t LSDAC_Port;
            uint16_t LSFlags;  //[0:2] - Enable Laser, 0 - Starboard, 1 - Port, 2 - LT/ST

            // Depth engine app inserted fields
            uint8_t SensorId;
            uint8_t CaptureMode;  // 0 - Streaming, 1 - Offline/Playback
            uint8_t ProcMode;     // 0 - Raw Mode, 1 - Processed Z/AB
            uint8_t depthEngineType;

            uint16_t resv;  // Reserved for alignment.

            // ---------------------------------------------------------------
            // The following is metadata that varies per capture.
            YEATS_APP_CAPTURE_HEADER captureHeaders[YEATS_MAX_NUM_CAPTURE_HEADERS];
        } bits;
    };
};

// static_assert(offsetof(YEATS_APP_FRAME_HDR_V2, mipiRxNanoseconds) == 20 * 4, "Check type definition of YEATS_APP_FRAME_HDR_V2");
// static_assert(offsetof(YEATS_APP_FRAME_HDR_V2, captureHeaders) == 31 * 4, "Check type definition of YEATS_APP_FRAME_HDR_V2");
// static_assert(sizeof(YEATS_APP_FRAME_HDR_V2) == YEATS_APP_FRAME_HDR_SIZE_V2, "Check type definition of YEATS_APP_FRAME_HDR_V2");

struct FRAME_FOOTER {
    uint32_t Signature;
    uint16_t BlockSize;
    uint16_t BlockVer;
    uint64_t TimeStamp;  // The time stamp when the frame arrives over MIPI, in nanoseconds.
    float    SoCTemp;    // Same as SoCTemp in YEATS_APP_FRAME_HDR.
    float    ModuleTemp;
};
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#endif  // end of #ifndef _YEATSFRAMEHDR_H_

