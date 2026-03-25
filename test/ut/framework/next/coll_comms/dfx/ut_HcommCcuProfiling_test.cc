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
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include <memory>
#include <vector>
#include <unordered_map>
#include "ccu_kernel_arg.h"
#include "ccu_kernel_signature.h"
#include "base.h"
#include "ccu_task_param_v1.h"
#include "ccu_kernel.h"
#include "ccu_kernel_resource.h"
#include "ccu_rep_context_v1.h"
#include "ccu_task_arg_v1.h"
#include "string_util.h"
#include "task_param.h"
#include "hccl_types.h"
#include "channel_process.h" 


using namespace hcomm;

constexpr uint16_t INVALID_U16 = 65535;
// Mock CcuUrmaChannel，隔离通道依赖
class MockCcuUrmaChannel : public CcuUrmaChannel {
public:
    MockCcuUrmaChannel(uint16_t channelId = INVALID_U16, uint32_t dieId = 0) : channelId_(channelId), dieId_(dieId) {}
    uint16_t GetChannelId() { return channelId_; }
    uint32_t GetDieId() { return dieId_;}
private:
    uint16_t channelId_;
    uint32_t dieId_;
};

static MockCcuUrmaChannel* g_mockChannel = new MockCcuUrmaChannel(INVALID_U16, 0);

HcclResult HcommChannelGet(ChannelHandle channel, void** channelPtr) {
    if (channelPtr == nullptr) return HCCL_E_PTR;
    *channelPtr = g_mockChannel;
    return HCCL_SUCCESS;
}

HcclResult SaveDfxTaskInfo(HcclComm comm, const Hccl::TaskParam& taskParam, uint32_t remoteRankId, bool isMaster) {
    return HCCL_SUCCESS;
}

void SetMockCcuUrmaChannel(MockCcuUrmaChannel* channel) {}

// Mock Channel，用于HcommChannelGet的返回
class MockChannel : public Channel {
public:
    explicit MockChannel(CcuUrmaChannel* impl) : channelImpl_(impl) {}
    CcuUrmaChannel* GetImpl() { return channelImpl_; }
private:
    CcuUrmaChannel* channelImpl_;
};

// 全局Mock工具：重载HcommChannelGet，返回MockChannel


// 注入MockChannel的接口
class TestCcuKernel : public CcuKernel {
public:
    using CcuKernel::CcuKernel;
    HcclResult Algorithm() override { return HCCL_SUCCESS; }
    std::vector<uint64_t> GeneArgs(const CcuTaskArg&) override { return {1,2,3,4};}

    void SetLoadArgIndex(uint32_t idx)
}

class MockCcuKernelArg : public hcomm::CcuKernelArg {
public:
    CcuKernelSignature GetKernelSignature() const override {
        return CcuKernelSignature{};
    }
}

// 测试夹具：复用CcuKernel实例与Mock对象
class CcuKernelTest : public testing::Test {
protected:
    void SetUp() override {
        // 构造CcuKernelArg
        hcomm::ResetStubs();  
        // 创建Mock通道
        mockChannel_ =  new MockCcuUrmaChannel(INVALID_U16, 0);
        hcomm::SetMockCcuUrmaChannel(mockChannel_);

        kernelArg_.channels = {65535, 0x1002};


        // 初始化CcuKernel
        kernel_ = std::make_unique<TestCcuKernel>(ccuKernelArg_);
        kernel_ ->SetDieId(0);
        kernel_ ->SetMissionId(100);
    }

    void TearDown() override {
        delete mockCcuUrmaChannel_;
        hcomm::ResetStubs();
    }

    // 测试数据
    MockCcuKernelArg kernelArg_;
    std::unique_ptr<TestCcuKernel> kernel_;
    MockCcuUrmaChannel* mockCcuUrmaChannel_ = nullptr;
    
    //通用测试参数
    const hcomm::GroupInfo testGroupInfo{1, 2, 3};
    const std::vector<ChannelHandle> testChannelsVec{65535, 65535};
    const ChannelHandle testChannelsArr = 65535;
    const HcclDataType testDataType = HcclDataType::HCCL_DATA_TYPE_FP32;
    const HcclDataType testOutputDataType = HcclDataType::HCCL_DATA_TYPE_FP16;
    const HcclDataType testReduceOp = HcclReduceOp::HCCL_REDUCE_SUM;
};

TEST_F(CcuKernelTest, UpdateChannelIdMap_Normal) {
    // 模拟ChannelId返回值
    EXPECT_CALL(*mockCcuUrmaChannel_, GetChannelId()).WillOnce(testing::Return(100));
    
    // 向kernel添加channelHandle
    ccuKernel_->AddChannelHandle(0x12345678);
    
    HcclResult ret = ccuKernel_->UpdateChannelIdMap();
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 验证映射关系
    uint64_t channelHandle = 0;
    ret = ccuKernel_->GetChannelHandleById(100, channelHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(channelHandle, 0x12345678);
}

TEST_F(CcuKernelTest, UpdateChannelIdMap_ChannelCastFail) {
    // 模拟HcommChannelGet返回非CcuUrmaChannel类型
    MockChannel invalidChannel(nullptr); // channelImpl为null
    hcomm::SetMockChannel(&invalidChannel);
    
    ccuKernel_->AddChannelHandle(0x99999999);
    
    HcclResult ret = ccuKernel_->UpdateChannelIdMap();
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(CcuKernelTest, GetChannelHandleById_Normal) {
    // 先更新映射
    EXPECT_CALL(*mockCcuUrmaChannel_, GetChannelId()).WillOnce(testing::Return(200));
    ccuKernel_->AddChannelHandle(0x88888888);
    ccuKernel_->UpdateChannelIdMap();
    
    // 执行测试
    uint64_t channelHandle = 0;
    HcclResult ret = ccuKernel_->GetChannelHandleById(200, channelHandle);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(channelHandle, 0x88888888);
}

TEST_F(CcuKernelTest, GetChannelHandleById_InvalidId) {
    uint64_t channelHandle = 0;
    HcclResult ret = ccuKernel_->GetChannelHandleById(9999, channelHandle);
    
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
    EXPECT_EQ(channelHandle, 0);
}

// TEST_F(CcuKernelTest, GetCcuProfilingInfo_Normal) {
//     // 构造有效TaskArg
//     CcuTaskArg taskArg;
//     taskArg.args = {1, 2, 3};
//     ccuKernel_->SetMissionId(1001);
//     ccuKernel_->SetInstrId(2002);
    
//     // 模拟Profiling缓存
//     CcuProfilingInfo profInfo;
//     profInfo.type = static_cast<uint8_t>(CcuProfilinType::CCU_TASK_PROFILING);
//     profInfo.name = "test_prof";
//     ccuKernel_->AddProfilingInfo(profInfo);
    
//     std::vector<CcuProfilingInfo> allProfInfo;
//     HcclResult ret = ccuKernel_->GetCcuProfilingInfo(taskArg, allProfInfo);
    
//     EXPECT_EQ(ret, HCCL_SUCCESS);
//     EXPECT_FALSE(allProfInfo.empty());
//     EXPECT_EQ(allProfInfo[0].missionId, 1001);
//     EXPECT_EQ(allProfInfo[0].instrId, 2002);
// }

// TEST_F(CcuKernelTest, GetCcuProfilingInfo_InvalidVarId) {
//     CcuTaskArg taskArg;
//     taskArg.args = {}; // 空参数
    
//     // 模拟无效VarId的Profiling信息
//     CcuProfilingInfo profInfo;
//     profInfo.type = static_cast<uint8_t>(CcuProfilinType::CCU_LOOPGROUP_PROFILING);
//     ccuKernel_->AddLGProfilingInfo(profInfo);
    
//     std::vector<CcuProfilingInfo> allProfInfo;
//     HcclResult ret = ccuKernel_->GetCcuProfilingInfo(taskArg, allProfInfo);
    
//     EXPECT_EQ(ret, HCCL_E_INTERNAL); // VarId映射失败触发异常
// }

TEST_F(CcuKernelTest, AddCcuProfiling_Normal) {
    std::string groupOpSize = "GroupReduce";
    HcclResult ret = kernel_->AddCcuProfiling(&testGroupInfo, test, testDataType,
                            testOutputDataType, testReduceOp, groupOpSize);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(CcuKernelTest, AddCcuProfiling_Num_Normal) {
    std::string groupOpSize = "GroupReduce";
    HcclResult ret = kernel_->AddCcuProfiling(&testChannelsArr, 2, testDataType,
                            testOutputDataType, testReduceOp, groupOpSize);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(CcuKernelTest, ReportCcuProfilingInfo_Normal_SaveSuccess) {
    Hccl::TaskParam testTaskParam = {
        .taskType  = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime   = 0,
        .isMaster = false,
        .taskPara  = {
            .Ccu = {
                .dieId         = 0,
                .missionId     = 0,
                .execMissionId = 0,
                .instrId       = 0,
                .costumArgs    = {0},
                .executeId     = 0
            }
        },
        .ccuDetailInfo  = nullptr
    };
    std::vector<>
    HcclResult ret = ccuKernel_->ReportCcuProfilingInfo(execId, streamProfInfo, comm, taskParam, true);
    std::vector<CcuProfilingInfo> profilingList = {validProfInfo};
    HcclResult ret = kernel_->ReportCcuProfilingInfo(
        testThreadHandle, testExecId, profilingList, testComm, testTaskParam, true
    );
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(testTaskParam.taskPara.Ccu.dieId, 1);
    EXPECT_EQ(testTaskParam.taskPara.Ccu.missionId, 100);
    EXPECT_EQ(testTaskParam.taskPara.Ccu.execMissionId, 100);
    EXPECT_EQ(testTaskParam.taskPara.Ccu.instrId, 200);
    EXPECT_EQ(testTaskParam.taskPara.Ccu.executeId, 0x2000);
}

TEST_F(CcuKernelTest, ReportCcuProfilingInfo_Normal_SaveFailed) {
    Hccl::TaskParam testTaskParam = {
        .taskType  = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime   = 0,
        .isMaster = false,
        .taskPara  = {
            .Ccu = {
                .dieId         = 0,
                .missionId     = 0,
                .execMissionId = 0,
                .instrId       = 0,
                .costumArgs    = {0},
                .executeId     = 0
            }
        },
        .ccuDetailInfo  = nullptr
    };
    std::vector<CcuProfilingInfo> profilingList = {validProfInfo};
    hcomm::SetSaveDfxTaskInfoRet(HCCL_E_PARA);
    HcclResult ret = kernel_->ReportCcuProfilingInfo(
        testThreadHandle, testExecId, profilingList, testComm, testTaskParam, false
    );
    EXPECT_EQ(ret, HCCL_E_PARA);
}

