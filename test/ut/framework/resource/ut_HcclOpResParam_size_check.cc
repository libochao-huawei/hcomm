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
#include "aicpu_operator_pub.h"

class HcclHcclOpResParamStructTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(HcclHcclOpResParamStructTest, TestHcclOpResParamSize)
{
    EXPECT_EQ(sizeof(HcclOpResParam), 2122720u);
}

TEST_F(HcclHcclOpResParamStructTest, TestHcclOpResParamFieldOffsets)
{
    EXPECT_EQ(offsetof(HcclOpResParam, mc2WorkSpace), 0u);
    EXPECT_EQ(offsetof(HcclOpResParam, localUsrRankId), 16u);
    EXPECT_EQ(offsetof(HcclOpResParam, rankSize), 20u);
    EXPECT_EQ(offsetof(HcclOpResParam, winSize), 24u);
    EXPECT_EQ(offsetof(HcclOpResParam, localWindowsIn), 32u);
    EXPECT_EQ(offsetof(HcclOpResParam, localWindowsOut), 40u);
    EXPECT_EQ(offsetof(HcclOpResParam, hcomId), 48u);
    EXPECT_EQ(offsetof(HcclOpResParam, winExpSize), 176u);
    EXPECT_EQ(offsetof(HcclOpResParam, localWindowsExp), 192u);
    EXPECT_EQ(offsetof(HcclOpResParam, rWinStart), 192u);
    EXPECT_EQ(offsetof(HcclOpResParam, rWinOffset), 196u);
    EXPECT_EQ(offsetof(HcclOpResParam, version), 200u);
    EXPECT_EQ(offsetof(HcclOpResParam, reservedStrcut), 208u);
    EXPECT_EQ(offsetof(HcclOpResParam, topoInfo), 2680u);
    EXPECT_EQ(offsetof(HcclOpResParam, config), 2840u);
    EXPECT_EQ(offsetof(HcclOpResParam, hostStateInfo), 2888u);
    EXPECT_EQ(offsetof(HcclOpResParam, aicpuStateInfo), 2896u);
    EXPECT_EQ(offsetof(HcclOpResParam, lockAddr), 2904u);
    EXPECT_EQ(offsetof(HcclOpResParam, rsv), 2912u);
    EXPECT_EQ(offsetof(HcclOpResParam, notifysize), 2976u);
    EXPECT_EQ(offsetof(HcclOpResParam, remoteResNum), 2980u);
    EXPECT_EQ(offsetof(HcclOpResParam, remoteRes), 2984u);
    EXPECT_EQ(offsetof(HcclOpResParam, kfcControlTransferH2DParams), 2100136u);
    EXPECT_EQ(offsetof(HcclOpResParam, kfcStatusTransferD2HParams), 2100176u);
    EXPECT_EQ(offsetof(HcclOpResParam, tinyMem), 2100216u);
    EXPECT_EQ(offsetof(HcclOpResParam, tinyMemSize), 2100224u);
    EXPECT_EQ(offsetof(HcclOpResParam, zeroCopyHeadPtr), 2100232u);
    EXPECT_EQ(offsetof(HcclOpResParam, zeroCopyTailPtr), 2100240u);
    EXPECT_EQ(offsetof(HcclOpResParam, zeroCopyRingBuffer), 2100248u);
    EXPECT_EQ(offsetof(HcclOpResParam, zeroCopyIpcPtrs), 2100256u);
    EXPECT_EQ(offsetof(HcclOpResParam, zeroCopyDevicePhyId), 2100512u);
    EXPECT_EQ(offsetof(HcclOpResParam, utraceStatusFlag), 2100640u);
    EXPECT_EQ(offsetof(HcclOpResParam, opCounterInfo), 2100648u);
    EXPECT_EQ(offsetof(HcclOpResParam, hierarchicalAlgInfo), 2100680u);
    EXPECT_EQ(offsetof(HcclOpResParam, localRes), 2100712u);
    EXPECT_EQ(offsetof(HcclOpResParam, debugConfig), 2104176u);
    EXPECT_EQ(offsetof(HcclOpResParam, aicpuCustomParamAddr), 2104184u);
    EXPECT_EQ(offsetof(HcclOpResParam, aicpuCustomParamSize), 2104192u);
    EXPECT_EQ(offsetof(HcclOpResParam, userMemRes), 2104200u);
    EXPECT_EQ(offsetof(HcclOpResParam, userMemType), 2122632u);
    EXPECT_EQ(offsetof(HcclOpResParam, aicpuOrderStreamParam), 2122640u);
    EXPECT_EQ(offsetof(HcclOpResParam, aicpuOrderNotifyAddr), 2122672u);
    EXPECT_EQ(offsetof(HcclOpResParam, aicpuOrderNotifySize), 2122680u);
    EXPECT_EQ(offsetof(HcclOpResParam, multiSuperPodDiffDeviceNumMode), 2122688u);
    EXPECT_EQ(offsetof(HcclOpResParam, isARSDoubleRing), 2122692u);
    EXPECT_EQ(offsetof(HcclOpResParam, opEntry), 2122693u);
    EXPECT_EQ(offsetof(HcclOpResParam, hcclSdmaQos), 2122696u);
    EXPECT_EQ(offsetof(HcclOpResParam, sizeOfAiRMAInfo), 2122704u);
    EXPECT_EQ(offsetof(HcclOpResParam, aiRMAInfo), 2122712u);
}

TEST_F(HcclHcclOpResParamStructTest, TestHcclOpResParamFieldSizes)
{
    EXPECT_EQ(sizeof(HcclMC2WorkSpace), 16u);
    EXPECT_EQ(sizeof(ReservedStruct), 2472u);
    EXPECT_EQ(sizeof(AlgoTopoInfo), 160u);
    EXPECT_EQ(sizeof(HcclOpConfig), 48u);
    EXPECT_EQ(sizeof(RemoteResPtr), 16u);
    EXPECT_EQ(sizeof(hccl::HDCommunicateParams), 40u);
    EXPECT_EQ(sizeof(OpCounterInfo), 32u);
    EXPECT_EQ(sizeof(HierarchicalAlgInfo), 32u);
    EXPECT_EQ(sizeof(LocalResInfoV2), 3464u);
    EXPECT_EQ(sizeof(MemDetails), 24u);
    EXPECT_EQ(sizeof(HcclStreamParam), 32u);
}
