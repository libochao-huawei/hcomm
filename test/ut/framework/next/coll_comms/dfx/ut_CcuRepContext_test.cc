/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <memory>
#include <vector>

#include "ccu_rep_context_v1.h"
#include "ccu_rep_base_v1.h"
#include "ccu_rep_block_v1.h"
#include "ccu_rep_type_v1.h"
#include "ccu_datatype_v1.h"
#include "hcomm_c_adpt.h"
#include "ccu_urma_channel.h"

namespace hcomm {
namespace CcuRep {

class MockCcuRepBase : public CcuRepBase {
public:
    MockCcuRepBase(CcuRepType type = CcuRepType::BASE, uint16_t startId = 0, uint16_t count = 1)
        : type_(type), startId_(startId), count_(count) {}

    bool Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep) override {
        return true;
    }

    std::string Describe() override {
        return std::string("MockCcuRepBase");
    }

    uint16_t StartInstrId() const {
        return startId_;
    }

    uint16_t InstrCount() override {
        return count_;
    }

    CcuRepType Type() const {
        return type_;
    }

private:
    CcuRepType type_;
    uint16_t startId_;
    uint16_t count_;
};

}

class MockCcuUrmaChannel {
public:
    MockCcuUrmaChannel(uint16_t channelId = 101, uint32_t dieId = 0)
        : channelId_(channelId), dieId_(dieId) {}

    uint16_t GetChannelId() { return channelId_; }
    uint32_t GetDieId() { return dieId_; }
    uint32_t GetLocCkeByIndex(uint32_t signalIndex, uint32_t &ckeId) {
        ckeId = 1000 + signalIndex;
        return HCCL_SUCCESS;
    }

private:
    uint16_t channelId_;
    uint32_t dieId_;
};

static MockCcuUrmaChannel* g_mockChannel = nullptr;

HcclResult HcommChannelGet(ChannelHandle channel, void** channelPtr) {
    if (channelPtr == nullptr) return HCCL_E_PTR;
    *channelPtr = g_mockChannel;
    return HCCL_SUCCESS;
}

class CcuRepContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockChannel_ = new MockCcuUrmaChannel(101, 0);
        g_mockChannel = mockChannel_;
    }

    void TearDown() override {
        delete mockChannel_;
        g_mockChannel = nullptr;
        GlobalMockObject::verify();
    }

    MockCcuUrmaChannel* mockChannel_;
};

TEST_F(CcuRepContextTest, Constructor_Normal) {
    CcuRep::CcuRepContext context;
    EXPECT_NE(context.CurrentBlock(), nullptr);
}

TEST_F(CcuRepContextTest, CurrentBlock_Normal) {
    CcuRep::CcuRepContext context;
    auto block = context.CurrentBlock();
    EXPECT_NE(block, nullptr);
}

TEST_F(CcuRepContextTest, SetCurrentBlock_Normal) {
    CcuRep::CcuRepContext context;
    auto newBlock = std::make_shared<CcuRep::CcuRepBlock>("test_block");
    context.SetCurrentBlock(newBlock);
    EXPECT_EQ(context.CurrentBlock(), newBlock);
}

TEST_F(CcuRepContextTest, Append_Normal) {
    CcuRep::CcuRepContext context;
    auto rep = std::make_shared<CcuRep::MockCcuRepBase>(CcuRep::CcuRepType::BASE);
    context.Append(rep);
    auto& reps = context.GetRepSequence();
    EXPECT_EQ(reps.size(), 1);
    EXPECT_EQ(reps[0], rep);
}

TEST_F(CcuRepContextTest, GetRepSequence_Empty) {
    CcuRep::CcuRepContext context;
    auto& reps = context.GetRepSequence();
    EXPECT_EQ(reps.size(), 0);
}

TEST_F(CcuRepContextTest, GetRepByInstrId_Found) {
    CcuRep::CcuRepContext context;
    auto rep1 = std::make_shared<CcuRep::MockCcuRepBase>(CcuRep::CcuRepType::BASE, 10, 5);
    auto rep2 = std::make_shared<CcuRep::MockCcuRepBase>(CcuRep::CcuRepType::BASE, 20, 3);
    context.Append(rep1);
    context.Append(rep2);

    auto foundRep = context.GetRepByInstrId(12);
    EXPECT_EQ(foundRep, rep1);

    foundRep = context.GetRepByInstrId(21);
    EXPECT_EQ(foundRep, rep2);
}

TEST_F(CcuRepContextTest, GetRepByInstrId_NotFound) {
    CcuRep::CcuRepContext context;
    auto rep = std::make_shared<CcuRep::MockCcuRepBase>(CcuRep::CcuRepType::BASE, 10, 5);
    context.Append(rep);

    auto foundRep = context.GetRepByInstrId(20);
    EXPECT_EQ(foundRep, nullptr);
}

TEST_F(CcuRepContextTest, SetDieId_GetDieId) {
    CcuRep::CcuRepContext context;
    uint32_t dieId = 5;
    context.SetDieId(dieId);
    EXPECT_EQ(context.GetDieId(), dieId);
}

TEST_F(CcuRepContextTest, SetMissionId_GetMissionId) {
    CcuRep::CcuRepContext context;
    uint32_t missionId = 100;
    context.SetMissionId(missionId);
    EXPECT_EQ(context.GetMissionId(), missionId);
}

TEST_F(CcuRepContextTest, SetMissionId_OnlyOnce) {
    CcuRep::CcuRepContext context;
    context.SetMissionId(100);
    context.SetMissionId(200);
    EXPECT_EQ(context.GetMissionId(), 100);
}

TEST_F(CcuRepContextTest, SetMissionKey_GetMissionKey) {
    CcuRep::CcuRepContext context;
    uint32_t missionKey = 300;
    context.SetMissionKey(missionKey);
    EXPECT_EQ(context.GetMissionKey(), missionKey);
}

TEST_F(CcuRepContextTest, GetProfilingInfo_Empty) {
    CcuRep::CcuRepContext context;
    auto& profilingInfo = context.GetProfilingInfo();
    EXPECT_EQ(profilingInfo.size(), 0);
}

TEST_F(CcuRepContextTest, GetLGProfilingInfo_Empty) {
    CcuRep::CcuRepContext context;
    auto& lgProfilingInfo = context.GetLGProfilingInfo();
    EXPECT_EQ(lgProfilingInfo.ccuProfilingInfos.size(), 0);
}

TEST_F(CcuRepContextTest, GetWaiteCkeProfilingReps_Empty) {
    CcuRep::CcuRepContext context;
    auto& waitReps = context.GetWaiteCkeProfilingReps();
    EXPECT_EQ(waitReps.size(), 0);
}

TEST_F(CcuRepContextTest, CollectProfilingReps_AssignVarToVar) {
    CcuRep::CcuRepContext context;
    auto rep = std::make_shared<CcuRep::MockCcuRepCcuRepBase>(CcuRep::CcuRepType::ASSIGN);
    context.CollectProfilingReps(rep);

    auto& profilingInfo = context.GetProfilingInfo();
    EXPECT_EQ(profilingInfo.size(), 0);
}

TEST_F(CcuRepContextTest, CollectProfilingReps_LocWaitEvent) {
    CcuRep::CcuRepContext context;
    auto rep = std::make_shared<CcuRep::MockCcuRepBase>(CcuRep::CcuRepType::LOC_WAIT_EVENT);
    context.CollectProfilingReps(rep);

    auto& waitReps = context.GetWaiteCkeProfilingReps();
    EXPECT_EQ(waitReps.size(), 1);
}

TEST_F(CcuRepContextTest, CollectProfilingReps_LoopGroup) {
    CcuRep::CcuRepContext context;
    auto rep = std::make_shared<CcuRep::MockCcuRepBase>(CcuRep::CcuRepType::LOOPGROUP);
    context.CollectProfilingReps(rep);

    EXPECT_EQ(context.allLgProfilingReps.size(), 1);
}

TEST_F(CcuRepContextTest, AddSqeProfiling_Normal) {
    CcuRep::CcuRepContext context;
    context.SetDieId(1);
    context.AddSqeProfiling();

    auto& profilingInfo = context.GetProfilingInfo();
    EXPECT_EQ(profilingInfo.size(), 1);
    EXPECT_EQ(profilingInfo[0].type, (uint8_t)CcuProfilinType::CCU_TASK_PROFILING);
    EXPECT_EQ(profilingInfo[0].name, "CCU_KERNEL");
    EXPECT_EQ(profilingInfo[0].dieId, 1);
}

TEST_F(CcuRepContextTest, AddProfiling_WithNameAndMask) {
    CcuRep::CcuRepContext context;
    std::string name = "TestProfiling";
    uint32_t mask = 0xFF;
    HcclResult ret = context.AddProfiling(name, mask);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    auto& profilingInfo = context.GetProfilingInfo();
    EXPECT_EQ(profilingInfo.size(), 1);
    EXPECT_EQ(profilingInfo[0].name, name);
    EXPECT_EQ(profilingInfo[0].mask, mask);
}

TEST_F(CcuRepContextTest, AddProfiling_WithChannelAndSignalIndex) {
    CcuRep::CcuRepContext context;
    ChannelHandle channel = 0x1001;
    std::string name = "ChannelProfiling";
    uint32_t signalIndex = 1;
    uint32_t mask = 0xFF;

    HcclResult ret = context.AddProfiling(channel, name, signalIndex, mask);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    auto& profilingInfo = context.GetProfilingInfo();
    EXPECT_EQ(profilingInfo.size(), 1);
    EXPECT_EQ(profilingInfo[0].name, name);
    EXPECT_EQ(profilingInfo[0].ckeId, 1001);
}

TEST_F(CcuRepContextTest, AddProfiling_WithChannelsForGroupBroadcast) {
    CcuRep::CcuRepContext context;
    context.SetMissionId(100);
    std::vector<ChannelHandle> channels = {0x1001, 0x1002};

    HcclResult ret = context.AddProfiling(channels.data(), channels.size());

    EXPECT_EQ(ret, HCCL_SUCCESS);
    auto& lgProfilingInfo = context.GetLGProfilingInfo();
    EXPECT_EQ(lgProfilingInfo.ccuProfilingInfos.size(), 1);
    EXPECT_EQ(lgProfilingInfo.ccuProfilingInfos[0].name, "GroupBroadcast");
    EXPECT_EQ(lgProfilingInfo.ccuProfilingInfos[0].missionId, 100);
}

TEST_F(CcuRepContextTest, AddProfiling_WithChannelsForGroupReduce) {
    CcuRep::CcuRepContext context;
    context.SetMissionId(100);
    std::vector<ChannelHandle> channels = {0x1001, 0x1002};
    HcclDataType dataType = HcclDataType::HCCL_DATA_TYPE_FP32;
    HcclDataType outputDataType = HcclDataType::HCCL_DATA_TYPE_FP16;
    HcclReduceOp opType = HcclReduceOp::HCCL_REDUCE_SUM;

    HcclResult ret = context.AddProfiling(channels.data(), channels.size(), dataType, outputDataType, opType);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    auto& lgProfilingInfo = context.GetLGProfilingInfo();
    EXPECT_EQ(lgProfilingInfo.ccuProfilingInfos.size(), 1);
    EXPECT_EQ(lgProfilingInfo.ccuProfilingInfos[0].name, "GroupReduce");
    EXPECT_EQ(lgProfilingInfo.ccuProfilingInfos[0].reduceOpType, opType);
}

TEST_F(CcuRepContextTest, AddProfiling_WithChannelsNullPtr) {
    CcuRep::CcuRepContext context;
    HcclResult ret = context.AddProfiling(nullptr, 2);
    EXPECT_NE(ret, HCCL_SUCCESS);
}

} 
