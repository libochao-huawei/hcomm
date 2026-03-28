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
#include <unordered_map>
#include "ccu_kernel_arg.h"
#include "ccu_kernel_signature.h"
#include "base.h"
#define private public
#include "ccu_task_param_v1.h"
#include "ccu_kernel.h"
#include "ccu_kernel_resource.h"
#include "ccu_rep_context_v1.h"
#include "ccu_task_arg_v1.h"
#include "string_util.h"
#include "task_param.h"
#include "channel_process.h" 
#include "hcomm_c_adpt.h"
#include "local_notify_impl.h"
#include "aicpu_launch_manager.h"
#include "llt_hccl_stub_rank_graph.h"
#include "hccl_api_base_test.h"
#include "hccl_ccu_res.h"
#include "ccu_kernel_mgr.h"
#include "hcomm_c_adpt.h"
#include "ccu_urma_channel.h"
#include "ccu_comp.h"

namespace hcomm {

// 模拟CcuUrmaChannel

class MockCcuUrmaChannel {
public:
    MockCcuUrmaChannel(uint16_t channelId = 101, uint32_t dieId = 0) 
        : channelId_(channelId), dieId_(dieId) {}
    uint16_t GetChannelId() { return channelId_; }
    uint32_t GetDieId() { return dieId_;}
private:
    uint16_t channelId_;
    uint32_t dieId_;
};

// 桩函数。 模拟HcommChannelGet
static MockCcuUrmaChannel* g_mockChannel = nullptr;

HcclResult HcommChannelGet(ChannelHandle channel, void** channelPtr) {
    if (channelPtr == nullptr) return HCCL_E_PTR;
    *channelPtr = g_mockChannel;
    return HCCL_SUCCESS;
}

// 桩函数，模拟SaveDfxTaskInfo
static HcclResult g_saveDfxRet = HCCL_SUCCESS;
HcclResult SaveDfxTaskInfo(HcclComm comm, const Hccl::TaskParam& taskParam, uint32_t remoteRankId, bool isMaster) {
    return g_saveDfxRet;
}

void SetMockCcuUrmaChannel(MockCcuUrmaChannel* channel) { g_mockChannel = channel; }
void SetSaveDfxTaskInfoRet(HcclResult ret) { g_saveDfxRet = ret; }
void ResetStubs() {
    g_mockChannel = nullptr;
    g_saveDfxRet = HCCL_SUCCESS;
}

// 测试子类
class TestCcuKernel : public CcuKernel {
public:
    using CcuKernel::CcuKernel;
    HcclResult Algorithm() override { return HCCL_SUCCESS; }
    std::vector<uint64_t> GeneArgs(const CcuTaskArg&) override { return {1,2,3,4};}
    // 暴露私有成员用于验证（利用-fno-access-control编译选项，无需修改源码）
    std::unordered_map<uint64_t, uint16_t>& GetChannelHandleToId() { return channelHandleToId_; }
    std::unordered_map<uint16_t, uint64_t>& GetChannelIdToHandle() { return channelIdToHandle_; }
    HcclResult GeneTaskParam(const CcuTaskArg &arg, std::vector<CcuTaskParam> &taskParams)
    {
    return HcclResult::HCCL_SUCCESS;
    }

    void SetLoadArgIndex(uint32_t idx) { loadArgIndex_ = idx; }
    void SetMissionId(uint32_t id) { missionId = id; }
    void SetDieId(uint32_t id) { dieId = id; }
    uint32_t GetDieId() const { return dieId; }
    void Work() {
        std::shared_ptr<CcuRep::CcuRepBase> rep;
        allLgProfilingReps.push_back(rep);
    }
};

// 测试数据
class MockCcuKernelArg : public hcomm::CcuKernelArg {
public:
    CcuKernelSignature GetKernelSignature() const override {
        return CcuKernelSignature{};
    }
};

class CcuKernelTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        // 1. 初始化桩函数
        hcomm::ResetStubs();  
        // 2. 创建桩函数通道
        mockChannel_ =  new MockCcuUrmaChannel(101, 0);
        hcomm::SetMockCcuUrmaChannel(mockChannel_);

        // 3. 构造CcuKernelArg
        kernelArg_.channels = {0x1001, 0x1002};


        // 4. 创建测试实例
        kernel_ = std::make_unique<TestCcuKernel>(kernelArg_);
        kernel_ ->SetDieId(0);
        kernel_ ->SetMissionId(100);
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
         // 清理桩函数
        delete mockChannel_;
        hcomm::ResetStubs();
    };
        // 修改 CcuKernelTest 类成员变量
    MockCcuKernelArg kernelArg_;
    std::unique_ptr<TestCcuKernel> kernel_;
    MockCcuUrmaChannel* mockChannel_ = nullptr;
    
    //通用测试参数
    const hcomm::GroupInfo testGroupInfo{1, 2, 3};
    const std::vector<ChannelHandle> testChannelsVec{0x1001, 0x1002};
    const ChannelHandle testChannelsArr = 0x1001;
    const HcclDataType testDataType = HcclDataType::HCCL_DATA_TYPE_FP32;
    const HcclDataType testOutputDataType = HcclDataType::HCCL_DATA_TYPE_FP16;
    const HcclReduceOp testReduceOp = HcclReduceOp::HCCL_REDUCE_SUM;
};

class Ccukernel_ReportProfilingTest : public hcomm::CcuKernelTest {
protected:
    void SetUp() override {
        CcuKernelTest::SetUp();
        MOCKER(hrtGetDeviceType)
        .stubs()
        .with(outBound(DevType::DEV_TYPE_950))
        .will(returnValue(HCCL_SUCCESS));
        // 初始化测试数据
        testThreadHandle = 0x1000;
        testExecId = 0x2000;

        // 有效ProfilingInfo
        validProfInfo.dieId = 1;
        validProfInfo.missionId = 100;
        validProfInfo.instrId = 200;
        validProfInfo.channelId[0] = 101;
        validProfInfo.channelId[1] = 102;

        // 无效channelId的ProfilingInfo
        invalidChannelProfInfo.dieId = 2;
        invalidChannelProfInfo.missionId = 200;
        invalidChannelProfInfo.instrId = 300;
        invalidChannelProfInfo.channelId[0] = INVALID_VALUE_CHANNELID;

    }

    // 测试数据
    ThreadHandle testThreadHandle;
    uint64_t testExecId;
    CcuProfilingInfo validProfInfo;
    CcuProfilingInfo invalidChannelProfInfo;
};

TEST_F(CcuKernelTest, AddCcuProfilingInfo_Normal) {
    EndpointHandle locEndpointHandle;
    HcommChannelDesc channelDesc;
    Channel* channel = new (std::nothrow) CcuUrmaChannel(locEndpointHandle, channelDesc);
    void *channelHandle = static_cast<void*>(channel);
    void ** handle{nullptr};
    handle = &channelHandle;
    MOCKER_CPP(&ChannelProcess::ChannelGet)
        .stubs()
        .with(any(),outBoundP(handle, sizeof(handle)))
        .will(returnValue(HCCL_SUCCESS));  
    kernel_->Work();
    HcclResult ret = kernel_->AddCcuProfiling(testGroupInfo, testChannelsVec, testDataType, 
        testOutputDataType, testReduceOp, "GroupReduce");
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(CcuKernelTest, AddProfilingInfo_Normal) {
    EndpointHandle locEndpointHandle;
    HcommChannelDesc channelDesc;
    Channel* channel = new (std::nothrow) CcuUrmaChannel(locEndpointHandle, channelDesc);
    void *channelHandle = static_cast<void*>(channel);
    void ** handle{nullptr};
    handle = &channelHandle;
    MOCKER_CPP(&ChannelProcess::ChannelGet)
        .stubs()
        .with(any(),outBoundP(handle, sizeof(handle)))
        .will(returnValue(HCCL_SUCCESS));  
    kernel_->Work();
    HcclResult ret = kernel_->AddCcuProfiling(&testChannelsArr, 1, testDataType, 
        testOutputDataType, testReduceOp, "GroupReduce");
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(CcuKernelTest, UpdateChannelIdMap_Normal) {
    kernel_->channels_.push_back(0x1001);
    HcclResult ret = kernel_->UpdateChannelIdMap();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(CcuKernelTest, UpdateChannelIdMap_ChannelCastFail) {
    hcomm::SetMockCcuUrmaChannel(nullptr);

    kernel_->channels_.push_back(0x9999);
    HcclResult ret = kernel_->UpdateChannelIdMap();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(CcuKernelTest, GetChannelHandleById_InvalidId) {
    uint64_t channelHandle = 0;
    HcclResult ret = kernel_->GetChannelHandleById(999, channelHandle);
    
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
    EXPECT_EQ(channelHandle, 0);
}


TEST_F(Ccukernel_ReportProfilingTest, ReportCcuProfilingInfo_EmptyProfiling) {
    MOCKER_CPP(&CcuComponent::CheckDiesEnable)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    void* commV2 = (void*)0x2000;
    RankGraphStub rankGraphStub;
    std::shared_ptr<Hccl::RankGraph> rankGraphV2 = rankGraphStub.Create2PGraph();
    u32 rank = 1;
    HcclMem cclBuffer;
    cclBuffer.size = 1;
    cclBuffer.type = HcclMemType::HCCL_MEM_TYPE_HOST;
    cclBuffer.addr = (void*)0x1000;;
    char commName[128] = {};
    std::shared_ptr<hccl::hcclComm> hcclCommPtr = make_shared<hccl::hcclComm>(1, 1, commName);
    HcclCommConfig config;
    config.hcclOpExpansionMode = 6; 
    config.hcclRdmaServiceLevel = 0; 
    config.hcclRdmaTrafficClass = 0; 
    HcclResult ret = hcclCommPtr->InitCollComm(commV2, rankGraphV2.get(), rank, cclBuffer, commName, &config);
    EXPECT_EQ(ret, 0);
    ThreadHandle thread;
    HcclComm testComm = static_cast<HcclComm>(hcclCommPtr.get());
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
                .costumArgs    = {},
                .executeId     = 0
            }
        },
        .ccuDetailInfo  = nullptr
    };
    std::vector<CcuProfilingInfo> emptyProfiling;
    ret = HcclReportCcuProfilingInfo(
        testThreadHandle, testExecId, emptyProfiling.data(), emptyProfiling.size(), testComm, testTaskParam, true
    );
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(testTaskParam.taskPara.Ccu.dieId, 0);
}

TEST_F(Ccukernel_ReportProfilingTest, ReportCcuProfilingInfo_Normal_SaveSuccess) {
    MOCKER_CPP(&CcuComponent::CheckDiesEnable)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    void* commV2 = (void*)0x2000;
    RankGraphStub rankGraphStub;
    std::shared_ptr<Hccl::RankGraph> rankGraphV2 = rankGraphStub.Create2PGraph();
    u32 rank = 1;
    HcclMem cclBuffer;
    cclBuffer.size = 1;
    cclBuffer.type = HcclMemType::HCCL_MEM_TYPE_HOST;
    cclBuffer.addr = (void*)0x1000;;
    char commName[128] = {};
    std::shared_ptr<hccl::hcclComm> hcclCommPtr = make_shared<hccl::hcclComm>(1, 1, commName);
    HcclCommConfig config;
    config.hcclOpExpansionMode = 6; 
    config.hcclRdmaServiceLevel = 0; 
    config.hcclRdmaTrafficClass = 0; 
    HcclResult ret = hcclCommPtr->InitCollComm(commV2, rankGraphV2.get(), rank, cclBuffer, commName, &config);
    EXPECT_EQ(ret, 0);
    ThreadHandle thread;
    HcclComm testComm = static_cast<HcclComm>(hcclCommPtr.get());
    u32 rankId = 1;
    MOCKER_CPP(&HcclCommDfx::GetChannelRemoteRankId)
        .stubs()
        .with(any(),any(),outBound(rankId))
        .will(returnValue(HCCL_SUCCESS));  
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
                .costumArgs    = {},
                .executeId     = 0
            }
        },
        .ccuDetailInfo  = nullptr
    };
    std::vector<CcuProfilingInfo> profilingList = {validProfInfo};
    ret = HcclReportCcuProfilingInfo(
        testThreadHandle, testExecId, profilingList.data(), profilingList.size(), testComm, testTaskParam, true
    );
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 验证联合体taskPara.Ccu的字段（匹配真实定义）
    EXPECT_EQ(testTaskParam.taskPara.Ccu.dieId, 1);
    EXPECT_EQ(testTaskParam.taskPara.Ccu.missionId, 100);
    EXPECT_EQ(testTaskParam.taskPara.Ccu.execMissionId, 100);
    EXPECT_EQ(testTaskParam.taskPara.Ccu.instrId, 200);
    EXPECT_EQ(testTaskParam.taskPara.Ccu.executeId, 0x2000);
}

TEST_F(Ccukernel_ReportProfilingTest, ReportCcuProfilingInfo_Normal_SaveFailed) {
    MOCKER_CPP(&CcuComponent::CheckDiesEnable)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    void* commV2 = (void*)0x2000;
    RankGraphStub rankGraphStub;
    std::shared_ptr<Hccl::RankGraph> rankGraphV2 = rankGraphStub.Create2PGraph();
    u32 rank = 1;
    HcclMem cclBuffer;
    cclBuffer.size = 1;
    cclBuffer.type = HcclMemType::HCCL_MEM_TYPE_HOST;
    cclBuffer.addr = (void*)0x1000;;
    char commName[128] = {};
    std::shared_ptr<hccl::hcclComm> hcclCommPtr = make_shared<hccl::hcclComm>(1, 1, commName);
    HcclCommConfig config;
    config.hcclOpExpansionMode = 6; 
    config.hcclRdmaServiceLevel = 0; 
    config.hcclRdmaTrafficClass = 0; 
    HcclResult ret = hcclCommPtr->InitCollComm(commV2, rankGraphV2.get(), rank, cclBuffer, commName, &config);
    EXPECT_EQ(ret, 0);
    ThreadHandle thread;
    HcclComm testComm = static_cast<HcclComm>(hcclCommPtr.get());
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
                .costumArgs    = {},
                .executeId     = 0
            }
        },
        .ccuDetailInfo  = nullptr
    };
    std::vector<CcuProfilingInfo> profilingList = {validProfInfo};
    hcomm::SetSaveDfxTaskInfoRet(HCCL_E_PARA);
    ret = HcclReportCcuProfilingInfo(
        testThreadHandle, testExecId, profilingList.data(), profilingList.size(), testComm, testTaskParam, false
    );
    EXPECT_EQ(ret, HCCL_E_PARA);
}


TEST_F(Ccukernel_ReportProfilingTest, WhenReporccuprofiling_expect_HcclSucess) {
using namespace hccl;
using namespace CcuRep;
  MockCcuKernelArg agrs;
  CcuKernel * ccuKernel = new TestCcuKernel(agrs);
  std::vector<CcuTaskParam> taskParams;
  CcuTaskParam taskParam;
  taskParams.push_back(taskParam);

  MOCKER(hrtGetDeviceType)
        .stubs()
        .with(outBound(DevType::DEV_TYPE_950))
        .will(returnValue(HCCL_SUCCESS));
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));  
    MOCKER_CPP(&CcuKernelMgr::GetKernel)
        .stubs()
        .will(returnValue(ccuKernel));
    MOCKER_CPP(&CcuKernel::GeneTaskParam)
        .stubs()
        .with(any(),outBound(taskParams))
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&CcuComponent::CheckDiesEnable)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
        
    
    void* commV2 = (void*)0x2000;
    RankGraphStub rankGraphStub;
    std::shared_ptr<Hccl::RankGraph> rankGraphV2 = rankGraphStub.Create2PGraph();
    u32 rank = 1;
    HcclMem cclBuffer;
    cclBuffer.size = 1;
    cclBuffer.type = HcclMemType::HCCL_MEM_TYPE_HOST;
    cclBuffer.addr = (void*)0x1000;;
    char commName[128] = {};
    std::shared_ptr<hccl::hcclComm> hcclCommPtr = make_shared<hccl::hcclComm>(1, 1, commName);
    HcclCommConfig config;
    config.hcclOpExpansionMode = 6; 
    config.hcclRdmaServiceLevel = 0; 
    config.hcclRdmaTrafficClass = 0;
    HcclResult ret = hcclCommPtr->InitCollComm(commV2, rankGraphV2.get(), rank, cclBuffer, commName, &config);
    EXPECT_EQ(ret, 0);
    ThreadHandle thread;
    void* comm = static_cast<HcclComm>(hcclCommPtr.get());
    ret =  HcclThreadAcquire(comm, COMM_ENGINE_CCU, 1, 2, &thread);
    EXPECT_EQ(ret, 0);
    CcuKernelHandle kernelHandle = 0;
    void * taskArgs = (void*)0x12345678;
    // hcomm::CcuKernelMgr::GetInstance(HcclGetThreadDeviceId());
    ret =  HcclCcuKernelLaunch(comm, thread, kernelHandle, taskArgs);
    EXPECT_EQ(ret, 0);
}

TEST_F(CcuKernelTest, GetCcuProfilingInfo_EmptyCache) {
    // Ensure profiling cache is empty
    kernel_->GetProfilingInfo().clear();

    std::vector<CcuProfilingInfo> out;
    CcuTaskArg arg{}; // default task arg
    HcclResult ret = kernel_->GetCcuProfilingInfo(arg, out);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(out.empty());
}

TEST_F(CcuKernelTest, GetCcuProfilingInfo_TaskProfilingInfo) {
    // Prepare a task profiling entry in the kernel's profiling cache
    kernel_->GetProfilingInfo().clear();
    CcuProfilingInfo prof;
    prof.type = static_cast<uint8_t>(CcuProfilinType::CCU_TASK_PROFILING);
    prof.name = "UnitTestTaskProfiling";
    prof.dieId = 1;
    prof.missionId = 0; // will be overwritten by GetCcuProfilingInfo
    prof.instrId = 0;   // will be overwritten by GetCcuProfilingInfo

    kernel_->GetProfilingInfo().push_back(prof);

    // Set expected mission and instr ids
    kernel_->SetMissionId(1234);
    kernel_->SetInstrId(4321);

    std::vector<CcuProfilingInfo> out;
    CcuTaskArg arg{};
    HcclResult ret = kernel_->GetCcuProfilingInfo(arg, out);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].missionId, 210);
    EXPECT_EQ(out[0].instrId, 4321u);
    EXPECT_EQ(out[0].name, "UnitTestTaskProfiling");
    EXPECT_EQ(out[0].dieId, 1u);
}

}

#undef private