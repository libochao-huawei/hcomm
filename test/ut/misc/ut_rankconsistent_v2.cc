/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <string>
#include <vector>
#include "llt_hccl_stub_sal_pub.h"
#include "hcomm_res_defs.h"
#define private public
#define protected public
#include "rank_consistency_checker_v2.h"
#undef protected
#undef private

using namespace hccl;

class RankConsistentV2Test : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "RankConsistentV2Test SetUp" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "RankConsistentV2Test TearDown" << std::endl;
    }
    virtual void SetUp()
    {
        std::cout << "A Test SetUp" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }

    RankConsistencyCheckerV2 &checker_ = RankConsistencyCheckerV2::GetInstance(0xFF);
};

TEST_F(RankConsistentV2Test, Ut_CompareCheckFrameV2_Engine_Expect_Success)
{
    checker_.RecordEngineV2(static_cast<s32>(COMM_ENGINE_AICPU));
    CheckFrameV2 localFrame;
    checker_.GenerateCheckFrameV2(localFrame);
    CheckFrameV2 remoteFrame = localFrame;

    HcclResult ret = checker_.CompareCheckFrameV2(localFrame, remoteFrame);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RankConsistentV2Test, Ut_CompareCheckFrameV2_Env_Expect_Success)
{
    u64 buff_size = 8;
    checker_.RecordEnvVarCrcV2(buff_size);
    CheckFrameV2 localFrame;
    checker_.GenerateCheckFrameV2(localFrame);
    CheckFrameV2 remoteFrame = localFrame;

    HcclResult ret = checker_.CompareCheckFrameV2(localFrame, remoteFrame);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RankConsistentV2Test, Ut_CompareCheckFrameV2_RankTable_Expect_Success)
{
    u32 rankTableCrc = 0x1234;
    checker_.RecordRankTableCrcV2(rankTableCrc);

    CheckFrameV2 localFrame;
    checker_.GenerateCheckFrameV2(localFrame);
    CheckFrameV2 remoteFrame = localFrame;

    HcclResult ret = checker_.CompareCheckFrameV2(localFrame, remoteFrame);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RankConsistentV2Test, Ut_CompareCheckFrameV2_SubComm_Expect_Success)
{
    std::string parentIdentifier = "test";
    uint32_t rankNum = 4;
    uint32_t rankIds[] = {0, 1, 2, 3};
    uint64_t subCommId = 100;
    checker_.RecordSubCommParaV2(parentIdentifier, rankNum, rankIds, subCommId);

    CheckFrameV2 localFrame;
    checker_.GenerateCheckFrameV2(localFrame);
    CheckFrameV2 remoteFrame = localFrame;

    HcclResult ret = checker_.CompareCheckFrameV2(localFrame, remoteFrame);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RankConsistentV2Test, Ut_CompareCheckFrameV2_Version_Expect_Success)
{
    strncpy_s(checker_.cannVersion_, CANN_VERSION_MAX_LEN + 1, "8.0.RC1", strlen("8.0.RC1"));

    CheckFrameV2 localFrame;
    checker_.GenerateCheckFrameV2(localFrame);
    CheckFrameV2 remoteFrame = localFrame;

    HcclResult ret = checker_.CompareCheckFrameV2(localFrame, remoteFrame);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(RankConsistentV2Test, Ut_CompareCheckFrameV2_Engine_Expect_INTERNAL)
{
    checker_.RecordEngineV2(static_cast<s32>(COMM_ENGINE_AICPU));
    CheckFrameV2 localFrame;
    checker_.GenerateCheckFrameV2(localFrame);
    CheckFrameV2 remoteFrame = localFrame;
    remoteFrame.engine = static_cast<s32>(COMM_ENGINE_CPU);

    HcclResult ret = checker_.CompareCheckFrameV2(localFrame, remoteFrame);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

// 异常：环境变量CRC不一致
TEST_F(RankConsistentV2Test, Ut_CompareCheckFrameV2_EnvVarMismatch_Expect_INTERNAL)
{
    u64 buff_size = 8;
    checker_.RecordEnvVarCrcV2(buff_size);
    CheckFrameV2 localFrame;
    checker_.GenerateCheckFrameV2(localFrame);
    CheckFrameV2 remoteFrame = localFrame;
    remoteFrame.crcArray[0] = 0xDEADBEEF;

    HcclResult ret = checker_.CompareCheckFrameV2(localFrame, remoteFrame);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

// 异常：ranktable CRC不一致
TEST_F(RankConsistentV2Test, Ut_CompareCheckFrameV2_RankTableMismatch_Expect_INTERNAL)
{
    u32 rankTableCrc = 0x1234;
    checker_.RecordRankTableCrcV2(rankTableCrc);

    CheckFrameV2 localFrame;
    checker_.GenerateCheckFrameV2(localFrame);
    CheckFrameV2 remoteFrame = localFrame;
    remoteFrame.rankTableCrcArray[0] = 0xDEADBEEF;

    HcclResult ret = checker_.CompareCheckFrameV2(localFrame, remoteFrame);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

// 异常：子通信域参数CRC不一致
TEST_F(RankConsistentV2Test, Ut_CompareCheckFrameV2_SubCommMismatch_Expect_INTERNAL)
{
    std::string parentIdentifier = "test";
    uint32_t rankNum = 4;
    uint32_t rankIds[] = {0, 1, 2, 3};
    uint64_t subCommId = 100;
    checker_.RecordSubCommParaV2(parentIdentifier, rankNum, rankIds, subCommId);

    CheckFrameV2 localFrame;
    checker_.GenerateCheckFrameV2(localFrame);
    CheckFrameV2 remoteFrame = localFrame;
    remoteFrame.subCommCrcArray[0] = 0xDEADBEEF;

    HcclResult ret = checker_.CompareCheckFrameV2(localFrame, remoteFrame);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

// 异常：CANN版本不一致
TEST_F(RankConsistentV2Test, Ut_CompareCheckFrameV2_VersionMismatch_Expect_INTERNAL)
{
    strncpy_s(checker_.cannVersion_, CANN_VERSION_MAX_LEN + 1, "8.0.RC1", strlen("8.0.RC1"));

    CheckFrameV2 localFrame;
    checker_.GenerateCheckFrameV2(localFrame);
    CheckFrameV2 remoteFrame = localFrame;
    strncpy_s(remoteFrame.version, CANN_VERSION_MAX_LEN + 1, "8.0.RC2", strlen("8.0.RC2"));

    HcclResult ret = checker_.CompareCheckFrameV2(localFrame, remoteFrame);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}
