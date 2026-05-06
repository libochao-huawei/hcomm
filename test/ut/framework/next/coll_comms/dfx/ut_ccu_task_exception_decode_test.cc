/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <cstring>
#include <cstdint>
#include <sstream>
#include <string>
#define private public
#define protected public
#include "ccuTaskException.h"
#include "ccu_error_info_v1.h"
#undef protected
#undef private

namespace hcomm {

struct CcuMissionInfo;
struct CcuLoopInfo;
struct CcuVersionOps {
    const char *name;
    void (*printCcumDfxInfo)(const void *rawData, std::ostringstream &oss);
    HcclResult (*getMissionInfo)(const void *rawData, CcuMissionInfo *out);
    HcclResult (*getLoopInfo)(const void *rawData, CcuLoopInfo *out);
};

HcclResult GetCcuOps(const CcuVersionOps *&ops);

struct TestCcumDfxInfoV1 {
    unsigned int queryResult;
    unsigned int ccumSqeRecvCnt;
    unsigned int ccumSqeSendCnt;
    unsigned int ccumMissionDfx;
    unsigned int ccumSqeDropCnt;
    unsigned int ccumSqeAddrLenErrDropCnt;
    unsigned int lqcCcuSecReg0;
    unsigned int ccumTifSqeCnt;
    unsigned int ccumTifCqeCnt;
    unsigned int ccumCifSqeCnt;
    unsigned int ccumCifCqeCnt;
};

struct TestCcumDfxInfoV2 {
    union {
        struct {
            unsigned int queryResult : 1; // 0:success, 1:fail
            unsigned int sqeRecvCnt : 1;
            unsigned int sqeSendCnt : 1;
            unsigned int missionDfx : 1;
            unsigned int sqeDropCnt : 1;
            unsigned int sqeErrDropCnt : 1;
            unsigned int secReg0 : 1;
            unsigned int tifSqeCnt : 1;
            unsigned int tifCqeCnt : 1;
            unsigned int cifSqeCnt : 1;
            unsigned int cifCqeCnt : 1;
            unsigned int mcmDfx : 1;
            unsigned int resv : 20;
        } bs;
        unsigned int validBits : 32;
    };
    union {
        struct {
            unsigned int ccumSqeRecvCnt;
            unsigned int ccumSqeSendCnt;
            unsigned int ccumMissionDfx;
            unsigned int ccumSqeDropCnt;
            unsigned int ccumSqeAddrLenErrDropCnt;
            unsigned int lqcCcuSecReg0;
            unsigned int ccumTifSqeCnt;
            unsigned int ccumTifCqeCnt;
            unsigned int ccumCifSqeCnt;
            unsigned int ccumCifCqeCnt;
            unsigned int ccumMcmDfx;
            unsigned int resv[20];
        } dfxInfo;
        unsigned int regVal[31];
    };
};

static_assert(sizeof(TestCcumDfxInfoV1) <= 128U, "TestCcumDfxInfoV1 layout changed unexpectedly");
static_assert(sizeof(TestCcumDfxInfoV2) <= 128U, "TestCcumDfxInfoV2 must mirror 128-byte raw layout");

struct CcuMissionInfo {
    uint16_t currentIns;
    uint16_t endIns;
    uint16_t startIns;
};

struct CcuLoopInfo {
    uint16_t currentIns;
    uint16_t currentCnt;
    uint32_t addrStride;
};

static std::string FormatPanicLogWithOps(DevType deviceType, const uint8_t *panicLog)
{
    MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));

    const CcuVersionOps *ops = nullptr;
    const HcclResult ret = GetCcuOps(ops);
    if (ret != HCCL_SUCCESS || ops == nullptr || ops->printCcumDfxInfo == nullptr) {
        ADD_FAILURE() << "FormatPanicLogWithOps failed to resolve printer ops, ret=" << ret;
        return "";
    }

    std::ostringstream oss;
    ops->printCcumDfxInfo(panicLog, oss);
    return oss.str();
}

class CcuTaskExceptionDecodeTest : public testing::Test {
protected:
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(CcuTaskExceptionDecodeTest, Ut_PrintPanicLogInfo_WhenNullPtr_ExpectNoCrash)
{
    EXPECT_NO_FATAL_FAILURE(CcuTaskException::PrintPanicLogInfo(nullptr));
}

TEST_F(CcuTaskExceptionDecodeTest, Ut_GetCcuOps_WhenDeviceTypeIs950_ExpectV1Ops)
{
    DevType deviceType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));

    const CcuVersionOps *ops = nullptr;
    EXPECT_EQ(GetCcuOps(ops), HCCL_SUCCESS);
    ASSERT_NE(ops, nullptr);
    EXPECT_STREQ(ops->name, "CCU_V1");
}

TEST_F(CcuTaskExceptionDecodeTest, Ut_GetCcuOps_WhenDeviceTypeIs910B_ExpectV2Ops)
{
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));

    const CcuVersionOps *ops = nullptr;
    EXPECT_EQ(GetCcuOps(ops), HCCL_SUCCESS);
    ASSERT_NE(ops, nullptr);
    EXPECT_STREQ(ops->name, "CCU_V2");
}

TEST_F(CcuTaskExceptionDecodeTest, Ut_GetCcuOps_WhenDeviceTypeQueryFails_ExpectErrorReturned)
{
    MOCKER(hrtGetDeviceType).stubs().with(any()).will(returnValue(HCCL_E_PARA));

    const CcuVersionOps *ops = nullptr;
    EXPECT_EQ(GetCcuOps(ops), HCCL_E_PARA);
    EXPECT_EQ(ops, nullptr);
}

TEST_F(CcuTaskExceptionDecodeTest, Ut_GetCcuOps_WhenSwitchDeviceTypeInProcess_ExpectNoCrossCasePollution)
{
    DevType deviceTypeV1 = DevType::DEV_TYPE_950;
    DevType deviceTypeV2 = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType).expects(once()).with(outBound(deviceTypeV1)).will(returnValue(HCCL_SUCCESS));
    const CcuVersionOps *opsV1 = nullptr;
    const CcuVersionOps *opsV2 = nullptr;
    EXPECT_EQ(GetCcuOps(opsV1), HCCL_SUCCESS);
    GlobalMockObject::verify();
    MOCKER(hrtGetDeviceType).expects(once()).with(outBound(deviceTypeV2)).will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(GetCcuOps(opsV2), HCCL_SUCCESS);
    ASSERT_NE(opsV1, nullptr);
    ASSERT_NE(opsV2, nullptr);
    EXPECT_STREQ(opsV1->name, "CCU_V1");
    EXPECT_STREQ(opsV2->name, "CCU_V2");
    EXPECT_NE(opsV1, opsV2);
}

TEST_F(CcuTaskExceptionDecodeTest, Ut_PrintCcumDfxInfoV1_WhenDeviceTypeIs950_ExpectReadableString)
{
    uint8_t panicLog[sizeof(TestCcumDfxInfoV1)];
    std::memset(panicLog, 0, sizeof(panicLog));
    TestCcumDfxInfoV1 *info = reinterpret_cast<TestCcumDfxInfoV1 *>(panicLog);
    info->ccumSqeRecvCnt = 11;
    info->ccumSqeSendCnt = 12;
    info->ccumMissionDfx = 13;
    info->ccumTifSqeCnt = 14;
    info->ccumTifCqeCnt = 15;
    info->ccumCifSqeCnt = 16;
    info->ccumCifCqeCnt = 17;
    info->ccumSqeDropCnt = 18;
    info->ccumSqeAddrLenErrDropCnt = 19;
    info->lqcCcuSecReg0 = 1;

    const std::string output = FormatPanicLogWithOps(DevType::DEV_TYPE_950, panicLog);
    const std::string expected = " SQE_RECV_CNT[11] SQE_SEND_CNT[12] MISSION_DFX[13] "
                                 "TIF_SQE_CNT[14] TIF_CQE_CNT[15] CIF_SQE_CNT[16] CIF_CQE_CNT[17] SQE_DROP_CNT[18] "
                                 "SQE_ADDR_LEN_ERR_DROP_CNT[19] ccumIsEnable[1]";
    EXPECT_EQ(output, expected);
    EXPECT_EQ(output.find("INVALID(hw)"), std::string::npos);
}

TEST_F(CcuTaskExceptionDecodeTest, Ut_PrintCcumDfxInfoV2_WhenDeviceTypeIs910B_ExpectReadableString)
{
    uint8_t panicLog[sizeof(TestCcumDfxInfoV2)];
    std::memset(panicLog, 0, sizeof(panicLog));
    TestCcumDfxInfoV2 *info = reinterpret_cast<TestCcumDfxInfoV2 *>(panicLog);
    info->validBits = 0xFFEU;
    info->dfxInfo.ccumSqeRecvCnt = 21;
    info->dfxInfo.ccumSqeSendCnt = 22;
    info->dfxInfo.ccumMissionDfx = 23;
    info->dfxInfo.ccumTifSqeCnt = 24;
    info->dfxInfo.ccumTifCqeCnt = 25;
    info->dfxInfo.ccumCifSqeCnt = 26;
    info->dfxInfo.ccumCifCqeCnt = 27;
    info->dfxInfo.ccumSqeDropCnt = 28;
    info->dfxInfo.ccumSqeAddrLenErrDropCnt = 29;
    info->dfxInfo.lqcCcuSecReg0 = 1;
    info->dfxInfo.ccumMcmDfx = 30;

    const std::string output = FormatPanicLogWithOps(DevType::DEV_TYPE_910B, panicLog);
    const std::string expected = " SQE_RECV_CNT[21] SQE_SEND_CNT[22] MISSION_DFX[23] "
                                 "TIF_SQE_CNT[24] TIF_CQE_CNT[25] CIF_SQE_CNT[26] CIF_CQE_CNT[27] SQE_DROP_CNT[28] "
                                 "SQE_ADDR_LEN_ERR_DROP_CNT[29] ccumIsEnable[1] MCM_DFX[30]";
    EXPECT_EQ(output, expected);
    EXPECT_EQ(output.find("INVALID(hw)"), std::string::npos);
}

TEST_F(CcuTaskExceptionDecodeTest, Ut_PrintCcumDfxInfoV2_WhenFieldInvalid_ExpectInvalidMarkerWithoutRawValue)
{
    uint8_t panicLog[sizeof(TestCcumDfxInfoV2)];
    std::memset(panicLog, 0, sizeof(panicLog));
    TestCcumDfxInfoV2 *info = reinterpret_cast<TestCcumDfxInfoV2 *>(panicLog);
    info->bs.sqeRecvCnt = 1;
    info->bs.missionDfx = 1;
    info->bs.tifSqeCnt = 1;
    info->bs.tifCqeCnt = 1;
    info->bs.cifSqeCnt = 1;
    info->bs.cifCqeCnt = 1;
    info->bs.sqeDropCnt = 1;
    info->bs.sqeErrDropCnt = 1;
    info->bs.secReg0 = 1;
    info->bs.mcmDfx = 1;
    info->dfxInfo.ccumSqeRecvCnt = 31;
    info->dfxInfo.ccumSqeSendCnt = 222;
    info->dfxInfo.ccumMissionDfx = 33;
    info->dfxInfo.ccumTifSqeCnt = 34;
    info->dfxInfo.ccumTifCqeCnt = 35;
    info->dfxInfo.ccumCifSqeCnt = 36;
    info->dfxInfo.ccumCifCqeCnt = 37;
    info->dfxInfo.ccumSqeDropCnt = 38;
    info->dfxInfo.ccumSqeAddrLenErrDropCnt = 39;
    info->dfxInfo.lqcCcuSecReg0 = 1;
    info->dfxInfo.ccumMcmDfx = 40;

    const std::string output = FormatPanicLogWithOps(DevType::DEV_TYPE_910B, panicLog);
    EXPECT_NE(output.find("SQE_RECV_CNT[31]"), std::string::npos);
    EXPECT_NE(output.find("SQE_SEND_CNT[INVALID(hw)]"), std::string::npos);
    EXPECT_EQ(output.find("SQE_SEND_CNT[222]"), std::string::npos);
    EXPECT_NE(output.find("MCM_DFX[40]"), std::string::npos);
}

TEST_F(CcuTaskExceptionDecodeTest, Ut_PrintCcumDfxInfoV1_WhenRawDataNull_ExpectReadableNullMarker)
{
    const std::string output = FormatPanicLogWithOps(DevType::DEV_TYPE_950, nullptr);
    EXPECT_EQ(output, " [rawData is null]");
}

TEST_F(CcuTaskExceptionDecodeTest, Ut_PrintCcumDfxInfoV2_WhenRawDataNull_ExpectReadableNullMarker)
{
    const std::string output = FormatPanicLogWithOps(DevType::DEV_TYPE_910B, nullptr);
    EXPECT_EQ(output, " [rawData is null]");
}

TEST_F(CcuTaskExceptionDecodeTest, Ut_PrintPanicLogInfo_WhenDeviceTypeQueryFails_ExpectNoCrash)
{
    MOCKER(hrtGetDeviceType).stubs().with(any()).will(returnValue(HCCL_E_PARA));

    uint8_t panicLog[16] = {0};
    EXPECT_NO_FATAL_FAILURE(CcuTaskException::PrintPanicLogInfo(panicLog));
}

TEST_F(CcuTaskExceptionDecodeTest, Ut_GetCcuMissionContext_WhenGetDevicePhyIdFail_ExpectZeroInitializedContext)
{
    MOCKER(hrtGetDevicePhyIdByIndex).stubs().with(any(), any()).will(returnValue(HCCL_E_PARA));

    const CcuMissionContext ctx = CcuTaskException::GetCcuMissionContext(0, 0, 0);
    EXPECT_EQ(ctx.part0.value, 0U);
    EXPECT_EQ(ctx.part1.value, 0U);
    EXPECT_EQ(ctx.part2.value, 0U);
}

TEST_F(CcuTaskExceptionDecodeTest, Ut_GetCcuLoopContext_WhenGetDevicePhyIdFail_ExpectZeroInitializedContext)
{
    MOCKER(hrtGetDevicePhyIdByIndex).stubs().with(any(), any()).will(returnValue(HCCL_E_PARA));

    const CcuLoopContext ctx = CcuTaskException::GetCcuLoopContext(0, 0, 0);
    EXPECT_EQ(ctx.part0.value, 0U);
    EXPECT_EQ(ctx.part1.value, 0U);
    EXPECT_EQ(ctx.part2.value, 0U);
}

TEST(Ut_CcuMissionContextV1, WhenStatusPartsSet_ExpectStatusCombinedCorrectly)
{
    CcuMissionContext ctx{};
    memset(&ctx, 0, sizeof(ctx));
    ctx.part2.status = 0x1FFF;
    ctx.part3.status = 0x7;

    uint16_t expected = static_cast<uint16_t>((0x7U << 13) | 0x1FFFU);
    EXPECT_EQ(ctx.GetStatus(), expected);
}

TEST(Ut_CcuMissionContextV1, WhenCurrentInsPartsSet_ExpectCurrentInsCombinedCorrectly)
{
    CcuMissionContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.part4.currentIns = 0x7FF;
    ctx.part5.currentIns = 0x1F;

    uint16_t expected = static_cast<uint16_t>((0x1FU << 11) | 0x7FFU);
    EXPECT_EQ(ctx.GetCurrentIns(), expected);
}

TEST(Ut_CcuMissionContextV1, WhenStartInsPartsSet_ExpectStartInsCombinedCorrectly)
{
    CcuMissionContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.part6.startIns = 0x7FF;
    ctx.part7.startIns = 0x1F;

    uint16_t expected = static_cast<uint16_t>((0x1FU << 11) | 0x7FFU);
    EXPECT_EQ(ctx.GetStartIns(), expected);
}

TEST(Ut_CcuMissionContextV1, WhenEndInsPartsSet_ExpectEndInsCombinedCorrectly)
{
    CcuMissionContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.part5.endIns = 0x7FF;
    ctx.part6.endIns = 0x1F;

    uint16_t expected = static_cast<uint16_t>((0x1FU << 11) | 0x7FFU);
    EXPECT_EQ(ctx.GetEndIns(), expected);
}

TEST(Ut_CcuMissionContextV2, WhenStatusPartsSet_ExpectStatusCombinedCorrectly)
{
    CcuMissionContextV2 ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.part2.status = 0x1FFF;
    ctx.part3.status = 0x7;

    uint16_t expected = static_cast<uint16_t>((0x7U << 13) | 0x1FFFU);
    EXPECT_EQ(ctx.GetStatus(), expected);
}

TEST(Ut_CcuMissionContextV2, WhenCurrentInsPartsSet_ExpectCurrentInsCombinedCorrectly)
{
    CcuMissionContextV2 ctx{};
    std::memset(&ctx, 0, sizeof(ctx));
    ctx.part4.currentIns = 0x1FF;
    ctx.part5.currentIns = 0x7F;

    uint16_t expected = static_cast<uint16_t>((0x7FU << 9) | 0x1FFU);
    EXPECT_EQ(ctx.GetCurrentIns(), expected);
}

TEST(Ut_CcuMissionContextV2, WhenStartInsPartsSet_ExpectStartInsCombinedCorrectly)
{
    CcuMissionContextV2 ctx{};
    std::memset(&ctx, 0, sizeof(ctx));
    ctx.part6.startIns = 0x1FF;
    ctx.part7.startIns = 0x7F;

    uint16_t expected = static_cast<uint16_t>((0x7FU << 9) | 0x1FFU);
    EXPECT_EQ(ctx.GetStartIns(), expected);
}

TEST(Ut_CcuMissionContextV2, WhenEndInsPartsSet_ExpectEndInsCombinedCorrectly)
{
    CcuMissionContextV2 ctx{};
    std::memset(&ctx, 0, sizeof(ctx));
    ctx.part5.endIns = 0x1FF;
    ctx.part6.endIns = 0x7F;

    uint16_t expected = static_cast<uint16_t>((0x7FU << 9) | 0x1FFU);
    EXPECT_EQ(ctx.GetEndIns(), expected);
}

TEST(Ut_CcuLoopContextV1, WhenCurrentInsPartsSet_ExpectCurrentInsCombinedCorrectly)
{
    CcuLoopContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.part9.currentIns = 0x3F;
    ctx.part10.currentIns = 0x3FF;

    uint16_t expected = static_cast<uint16_t>((0x3FFU << 6) | 0x3FU);
    EXPECT_EQ(ctx.GetCurrentIns(), expected);
}

TEST(Ut_CcuLoopContextV1, WhenCurrentCntPartsSet_ExpectCurrentCntCombinedCorrectly)
{
    CcuLoopContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.part13.currentCnt = 0xFFF;
    ctx.part14.currentCnt = 0x1;

    uint16_t expected = static_cast<uint16_t>((0x1U << 12) | 0xFFFU);
    EXPECT_EQ(ctx.GetCurrentCnt(), expected);
}

TEST(Ut_CcuLoopContextV1, WhenAddrStridePartsSet_ExpectAddrStrideCombinedCorrectly)
{
    CcuLoopContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.part10.addrStride = 0x3F;
    ctx.part11.addrStride = 0xFFFF;
    ctx.part12.addrStride = 0x3FF;

    uint32_t expected
        = (static_cast<uint32_t>(0x3FFU) << 22) | (static_cast<uint32_t>(0xFFFFU) << 6) | static_cast<uint32_t>(0x3FU);
    EXPECT_EQ(ctx.GetAddrStride(), expected);
}

TEST(Ut_CcuLoopContextV2, WhenCurrentInsPartsSet_ExpectCurrentInsCombinedCorrectly)
{
    CcuLoopContextV2 ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.part4.currentIns = 0x1FF;
    ctx.part5.currentIns = 0x7F;

    uint16_t expected = static_cast<uint16_t>((0x7FU << 9) | 0x1FFU);
    EXPECT_EQ(ctx.GetCurrentIns(), expected);
}

TEST(Ut_CcuLoopContextV2, WhenCurrentCntSet_ExpectCurrentCntReturnedDirectly)
{
    CcuLoopContextV2 ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.part10.currentCnt = 0x1FFF;

    EXPECT_EQ(ctx.GetCurrentCnt(), 0x1FFFU);
}

TEST(Ut_CcuLoopContextV2, WhenAddrStridePartsSet_ExpectAddrStrideCombinedCorrectly)
{
    CcuLoopContextV2 ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.part5.addrStride = 0x1FF;
    ctx.part6.addrStride = 0xFFFF;
    ctx.part7.addrStride = 0x7F;

    uint32_t expected
        = (static_cast<uint32_t>(0x7FU) << 25) | (static_cast<uint32_t>(0xFFFFU) << 9) | static_cast<uint32_t>(0x1FFU);
    EXPECT_EQ(ctx.GetAddrStride(), expected);
}

TEST_F(CcuTaskExceptionDecodeTest, Ut_GetCcuMissionInfoV1_WhenRawDataIsNull_ExpectErrorReturned)
{
    MOCKER(hrtGetDeviceType).stubs().with(outBound(DevType::DEV_TYPE_950)).will(returnValue(HCCL_SUCCESS));

    const CcuVersionOps *ops = nullptr;
    EXPECT_EQ(GetCcuOps(ops), HCCL_SUCCESS);
    ASSERT_NE(ops, nullptr);
    ASSERT_NE(ops->getMissionInfo, nullptr);

    CcuMissionInfo missionInfo{};
    EXPECT_NE(ops->getMissionInfo(nullptr, &missionInfo), HCCL_SUCCESS);
}

TEST_F(CcuTaskExceptionDecodeTest, Ut_GetCcuMissionInfoV1_WhenOutIsNull_ExpectErrorReturned)
{
    MOCKER(hrtGetDeviceType).stubs().with(outBound(DevType::DEV_TYPE_950)).will(returnValue(HCCL_SUCCESS));

    const CcuVersionOps *ops = nullptr;
    EXPECT_EQ(GetCcuOps(ops), HCCL_SUCCESS);
    ASSERT_NE(ops, nullptr);
    ASSERT_NE(ops->getMissionInfo, nullptr);

    CcuMissionContext ctx{};
    EXPECT_NE(ops->getMissionInfo(&ctx, nullptr), HCCL_SUCCESS);
}

TEST_F(CcuTaskExceptionDecodeTest, Ut_GetCcuLoopInfoV1_WhenRawDataIsNull_ExpectErrorReturned)
{
    MOCKER(hrtGetDeviceType).stubs().with(outBound(DevType::DEV_TYPE_950)).will(returnValue(HCCL_SUCCESS));

    const CcuVersionOps *ops = nullptr;
    EXPECT_EQ(GetCcuOps(ops), HCCL_SUCCESS);
    ASSERT_NE(ops, nullptr);
    ASSERT_NE(ops->getLoopInfo, nullptr);

    CcuLoopInfo loopInfo{};
    EXPECT_NE(ops->getLoopInfo(nullptr, &loopInfo), HCCL_SUCCESS);
}

TEST_F(CcuTaskExceptionDecodeTest, Ut_GetCcuLoopInfoV1_WhenOutIsNull_ExpectErrorReturned)
{
    MOCKER(hrtGetDeviceType).stubs().with(outBound(DevType::DEV_TYPE_950)).will(returnValue(HCCL_SUCCESS));

    const CcuVersionOps *ops = nullptr;
    EXPECT_EQ(GetCcuOps(ops), HCCL_SUCCESS);
    ASSERT_NE(ops, nullptr);
    ASSERT_NE(ops->getLoopInfo, nullptr);

    CcuLoopContext ctx{};
    EXPECT_NE(ops->getLoopInfo(&ctx, nullptr), HCCL_SUCCESS);
}

TEST_F(CcuTaskExceptionDecodeTest, Ut_GetCcuMissionInfoV2_WhenRawDataIsNull_ExpectErrorReturned)
{
    MOCKER(hrtGetDeviceType).stubs().with(outBound(DevType::DEV_TYPE_910B)).will(returnValue(HCCL_SUCCESS));

    const CcuVersionOps *ops = nullptr;
    EXPECT_EQ(GetCcuOps(ops), HCCL_SUCCESS);
    ASSERT_NE(ops, nullptr);
    ASSERT_NE(ops->getMissionInfo, nullptr);

    CcuMissionInfo missionInfo{};
    EXPECT_NE(ops->getMissionInfo(nullptr, &missionInfo), HCCL_SUCCESS);
}

TEST_F(CcuTaskExceptionDecodeTest, Ut_GetCcuMissionInfoV2_WhenOutIsNull_ExpectErrorReturned)
{
    MOCKER(hrtGetDeviceType).stubs().with(outBound(DevType::DEV_TYPE_910B)).will(returnValue(HCCL_SUCCESS));

    const CcuVersionOps *ops = nullptr;
    EXPECT_EQ(GetCcuOps(ops), HCCL_SUCCESS);
    ASSERT_NE(ops, nullptr);
    ASSERT_NE(ops->getMissionInfo, nullptr);

    CcuMissionContextV2 ctx{};
    EXPECT_NE(ops->getMissionInfo(&ctx, nullptr), HCCL_SUCCESS);
}

TEST_F(CcuTaskExceptionDecodeTest, Ut_GetCcuLoopInfoV2_WhenRawDataIsNull_ExpectErrorReturned)
{
    MOCKER(hrtGetDeviceType).stubs().with(outBound(DevType::DEV_TYPE_910B)).will(returnValue(HCCL_SUCCESS));

    const CcuVersionOps *ops = nullptr;
    EXPECT_EQ(GetCcuOps(ops), HCCL_SUCCESS);
    ASSERT_NE(ops, nullptr);
    ASSERT_NE(ops->getLoopInfo, nullptr);

    CcuLoopInfo loopInfo{};
    EXPECT_NE(ops->getLoopInfo(nullptr, &loopInfo), HCCL_SUCCESS);
}

TEST_F(CcuTaskExceptionDecodeTest, Ut_GetCcuLoopInfoV2_WhenOutIsNull_ExpectErrorReturned)
{
    MOCKER(hrtGetDeviceType).stubs().with(outBound(DevType::DEV_TYPE_910B)).will(returnValue(HCCL_SUCCESS));

    const CcuVersionOps *ops = nullptr;
    EXPECT_EQ(GetCcuOps(ops), HCCL_SUCCESS);
    ASSERT_NE(ops, nullptr);
    ASSERT_NE(ops->getLoopInfo, nullptr);

    CcuLoopContextV2 ctx{};
    EXPECT_NE(ops->getLoopInfo(&ctx, nullptr), HCCL_SUCCESS);
}

} // namespace hcomm
