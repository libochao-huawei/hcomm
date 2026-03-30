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
#include "ccuTaskException.h"

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

// +++++++++++++ CcuTaskException测试 +++++++++++++
class CcuTaskExceptionTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        // 可以在这里设置一些全局状态或环境变量
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
        // 清理全局状态或环境变量
    }


};

TEST_F(CcuTaskExceptionTest, ProccessCcuException_Normal) {
    // 构造异常信息和任务信息
    rtExceptionInfo_t exceptionInfo{};
    exceptionInfo.deviceid = 0;
    exceptionInfo.expandInfo.u.ccuInfo.ccuMissionNum = 1;
    exceptionInfo.expandInfo.u.ccuInfo.missionInfo[0].status = 0x01;
    exceptionInfo.expandInfo.u.ccuInfo.missionInfo[0].subStatus = 0x02;
    uint8_t panicLog[128] = {}; // 模拟panic日志内容
    std::memcpy(exceptionInfo.expandInfo.u.ccuInfo.missionInfo[0].panicLog,
        panicLog, sizeof(panicLog));

    hccl::TaskPara a;
    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo{1,2,3, taskParam, nullptr, true};
    MOCKER_CPP(&CcuComponent::CleanTaskKillState)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&CcuComponent::CleanDieCkes)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    // 调用处理函数
    CcuTaskException::ProcessCcuException(&exceptionInfo, taskInfo);

    // 验证输出
    EXPECT_TRUE(true);
}

TEST_F(CcuTaskExceptionTest, GetGroupRankInfo_Normal) {
    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1,2,3, taskParam, nullptr, false);
    taskInfo.dfxOpInfo_ = std::make_shared<Hccl::DfxOpInfo>();
    hccl::ManagerCallbacks callbacks;
    callbacks.getAicpuCommState = []() { return true; };
    callbacks.setAicpuCommState = [](bool) {};
    callbacks.kernelLaunchAicpuCommInit = []() { return HCCL_SUCCESS; };
    std::string commName = "TestComm";
    hccl::CollComm collComm(nullptr, 1, commName, callbacks);
    taskInfo.dfxOpInfo_->comm_ = &collComm;
    std::string testCommStr = "TestComm";
    MOCKER_CPP(&hccl::CollComm::GetCommId)
        .stubs()
        .will(returnValue(testCommStr));
    MOCKER_CPP(&hccl::CollComm::GetRankSize)
        .stubs()
        .will(returnValue(8u));
    MOCKER_CPP(&hccl::CollComm::GetMyRankId)
        .stubs()
        .will(returnValue(3u));
    std::string result = CcuTaskException::GetGroupRankInfo(taskInfo);
    EXPECT_NE(result, "");
    EXPECT_TRUE(result.find("group:[TestComm]") != std::string::npos);
    EXPECT_TRUE(result.find("rankSize[8]") != std::string::npos);
    EXPECT_TRUE(result.find("rankId[3]") != std::string::npos);

}

TEST_F(CcuTaskExceptionTest, GetGroupRankInfo_Nullptr) {
    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1,2,3, taskParam, nullptr, false);
    taskInfo.dfxOpInfo_ = nullptr;
    std::string result = CcuTaskException::GetGroupRankInfo(taskInfo);
    EXPECT_EQ(result, "");
}

struct CcumDfxInfoForTest {
    unsigned int queryResult; // 0:success, 1:fail
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

TEST_F(CcuTaskExceptionTest, PrintPanicLogInfo_Normal) {
    uint8_t panicLog[128] = {};
    struct CcumDfxInfoForTest *info = reinterpret_cast<struct CcumDfxInfoForTest *>(panicLog);
    info->queryResult = 0; // success
    info->ccumSqeRecvCnt = 100;
    info->ccumSqeSendCnt = 200;
    info->ccumMissionDfx = 300;
    info->ccumTifSqeCnt = 400;
    info->ccumTifCqeCnt = 500;
    info->ccumCifSqeCnt = 600;
    info->ccumCifCqeCnt = 700;
    info->ccumSqeDropCnt = 800;
    info->ccumSqeAddrLenErrDropCnt = 900;
    info->lqcCcuSecReg0 = 1; // enable

    // 调用函数
    EXPECT_NO_THROW(CcuTaskException::PrintPanicLogInfo(panicLog));
 	 
}
 	 
TEST_F(CcuTaskExceptionTest, PrintPaniclogInfo) {
    EXPECT_NO_THROW(CcuTaskException::PrintPanicLogInfo(nullptr));
}

TEST_F(CcuTaskExceptionTest, GetCcuCKEValue_Normal) {
    // 模拟hrtGetDevicePhyIdByIndex
    MOCKER(hrtGetDevicePhyIdByIndex)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_SUCCESS));

    // 模拟HccpRaCustomChannel
    MOCKER(HccpRaCustomChannel)
        .stubs()
        .with(any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));

    uint16_t result = CcuTaskException::GetCcuCKEValue(0, 0, 0);
    EXPECT_EQ(result, 0); // INVALID_U16
}

TEST_F(CcuTaskExceptionTest, GetCcuCKEValue_GetDevicePhyIdFail) {
    MOCKER(hrtGetDevicePhyIdByIndex)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_E_PARA));

    uint16_t result = CcuTaskException::GetCcuCKEValue(0, 0, 0);
    EXPECT_EQ(result, 65535);
}

TEST_F(CcuTaskExceptionTest, GetCcuCKEValue_CustomChannelFail) {
    MOCKER(hrtGetDevicePhyIdByIndex)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(HccpRaCustomChannel)
        .stubs()
        .with(any(), any(), any(), any())
        .will(returnValue(HCCL_E_PARA));

    uint16_t result = CcuTaskException::GetCcuCKEValue(0, 0, 0);
    EXPECT_EQ(result, 65535);
}

TEST_F(CcuTaskExceptionTest, GetMissContectF) {
    MOCKER(hrtGetDevicePhyIdByIndex)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(HccpRaCustomChannel)
        .stubs()
        .with(any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));

    CcuMissionContext result = CcuTaskException::GetCcuMissionContext(0, 0, 0);
    EXPECT_EQ(result.part0.value, 0u);
}

TEST_F(CcuTaskExceptionTest, GetDevidFail) {
    MOCKER(hrtGetDevicePhyIdByIndex)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_E_PARA));

    CcuMissionContext result = CcuTaskException::GetCcuMissionContext(0, 0, 0);
    EXPECT_EQ(result.part0.value, 0u);
}

TEST_F(CcuTaskExceptionTest, GetMissContectFail) {
    MOCKER(hrtGetDevicePhyIdByIndex)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(HccpRaCustomChannel)
        .stubs()
        .with(any(), any(), any(), any())
        .will(returnValue(HCCL_E_PARA));

    CcuMissionContext result = CcuTaskException::GetCcuMissionContext(0, 0, 0);
    EXPECT_EQ(result.part0.value, 0u);
}

constexpr uint64_t INVALID_U64_VAL = 18446744073709551615ULL;
TEST_F(CcuTaskExceptionTest, GetCcuGSAValue) {
    MOCKER(hrtGetDevicePhyIdByIndex)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(HccpRaCustomChannel)
        .stubs()
        .with(any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));

    uint64_t result = CcuTaskException::GetCcuGSAValue(0, 0, 0);
    EXPECT_EQ(result, 0u);
}

TEST_F(CcuTaskExceptionTest, GetCcuGSAValuegetdevidfailed) {
    MOCKER(hrtGetDevicePhyIdByIndex)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_E_PARA));

    uint64_t result = CcuTaskException::GetCcuGSAValue(0, 0, 0);
    EXPECT_EQ(result, INVALID_U64_VAL);
}

TEST_F(CcuTaskExceptionTest, GetCcuGSAValuechannelfail) {
    MOCKER(hrtGetDevicePhyIdByIndex)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(HccpRaCustomChannel)
        .stubs()
        .with(any(), any(), any(), any())
        .will(returnValue(HCCL_E_PARA));

    uint64_t result = CcuTaskException::GetCcuGSAValue(0, 0, 0);
    EXPECT_EQ(result, INVALID_U64_VAL);
}

TEST_F(CcuTaskExceptionTest, GetCcuXnValue) {
    MOCKER(hrtGetDevicePhyIdByIndex)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(HccpRaCustomChannel)
        .stubs()
        .with(any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));

    uint64_t result = CcuTaskException::GetCcuXnValue(0, 0, 0);
    EXPECT_EQ(result, 0u);
}

TEST_F(CcuTaskExceptionTest, GetCcuXnValuedevidfailed) {
    MOCKER(hrtGetDevicePhyIdByIndex)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_E_PARA));

    uint64_t result = CcuTaskException::GetCcuXnValue(0, 0, 0);
    EXPECT_EQ(result, INVALID_U64_VAL);
}

TEST_F(CcuTaskExceptionTest, GetCcuXnValuechannelfail) {
    MOCKER(hrtGetDevicePhyIdByIndex)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(HccpRaCustomChannel)
        .stubs()
        .with(any(), any(), any(), any())
        .will(returnValue(HCCL_E_PARA));

    uint64_t result = CcuTaskException::GetCcuXnValue(0, 0, 0);
    EXPECT_EQ(result, INVALID_U64_VAL);
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsg) {
    string result = CcuTaskException::GetCcuLenErrorMsg(1024);
    EXPECT_EQ(result, "");
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgzero) {
    string result = CcuTaskException::GetCcuLenErrorMsg(0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgDefault) {
    CcuErrorInfo ccuErrorInfo = {};
    string  result = CcuTaskException::GetCcuErrorMsgDefault(ccuErrorInfo);
    EXPECT_FALSE(result.empty());
}
 	 
TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgMission) {
    CcuErrorInfo ccuErrorInfo = {};
    ccuErrorInfo.type = CcuErrorType::MISSION;
    string result = CcuTaskException::GetCcuErrorMsgMission(ccuErrorInfo);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuLoopContext_Normal) {
    MOCKER(hrtGetDevicePhyIdByIndex)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(HccpRaCustomChannel)
        .stubs()
        .with(any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));

    CcuLoopContext result = CcuTaskException::GetCcuLoopContext(0, 0, 0);
    EXPECT_EQ(result.part0.value, 0u);
}

TEST_F(CcuTaskExceptionTest, GetCcuLoopContext_GetDevicePhyIdFail) {
    MOCKER(hrtGetDevicePhyIdByIndex)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_E_PARA));
    CcuLoopContext result = CcuTaskException::GetCcuLoopContext(0, 0, 0);
    EXPECT_EQ(result.part0.value, 0u);
}

TEST_F(CcuTaskExceptionTest, GenStatusInfo_Normal) {
    ErrorInfoBase baseInfo = {};
    baseInfo.dieId = 0;
    baseInfo.missionId = 1;
    baseInfo.currentInsId = 0;
    baseInfo.status = 0x0102;

    vector<CcuErrorInfo> errorInfo;
    CcuTaskException::GenStatusInfo(baseInfo, errorInfo);

    EXPECT_EQ(errorInfo.size(), 1u);
    EXPECT_EQ(errorInfo[0].type, CcuErrorType::MISSION);
}

TEST_F(CcuTaskExceptionTest, GetCcuChannelHandleById_Normal) {
    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1,2,3, taskParam, nullptr, false);

    u64 channelHandle = 0;
    HcclResult ret = CcuTaskException::GetCcuChannelHandleById(0, 101, taskInfo, channelHandle);

    // 验证输出
    EXPECT_TRUE(true);
}

TEST_F(CcuTaskExceptionTest, GetRankIdByChannelId_Normal) {
    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1,2,3, taskParam, nullptr, false);

    RankId rankId = CcuTaskException::GetRankIdByChannelId(101,taskInfo, 0);

    // 验证输出
    EXPECT_TRUE(true);
}


TEST_F(CcuTaskExceptionTest, GetAddrPairByChannelId_Normal) {
    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1,2,3, taskParam, nullptr, false);

    auto addrPair = CcuTaskException::GetAddrPairByChannelId(101,taskInfo, 0);

    EXPECT_TRUE(true);
}

TEST_F(CcuTaskExceptionTest, GetMSIdPerDie_Normal) {
    uint16_t result1 = CcuTaskException::GetMSIdPerDie(0x8001);
    EXPECT_EQ(result1, 0x0001);

    uint16_t result2 = CcuTaskException::GetMSIdPerDie(0x7FFF);
    EXPECT_EQ(result2, 0x7FFF);

    uint16_t result3 = CcuTaskException::GetMSIdPerDie(0);
    EXPECT_EQ(result3, 0);
}

TEST_F(CcuTaskExceptionTest, GenStatusInfo_UnsupportedOpcode) {
    ErrorInfoBase baseInfo = {};
    baseInfo.dieId = 0;
    baseInfo.missionId = 1;
    baseInfo.currentInsId = 0;
    baseInfo.status = 0x0100;

    vector<CcuErrorInfo> errorInfo;
    CcuTaskException::GenStatusInfo(baseInfo, errorInfo);

    EXPECT_EQ(errorInfo.size(), 1u);
    EXPECT_EQ(errorInfo[0].type, CcuErrorType::MISSION);
    EXPECT_TRUE(string(errorInfo[0].msg.mission.missionError).find("Unsupported Opcode") != string::npos);
}

TEST_F(CcuTaskExceptionTest, GenStatusInfo_LocalOpError) {
    ErrorInfoBase baseInfo = {};
    baseInfo.dieId = 1;
    baseInfo.missionId = 2;
    baseInfo.currentInsId = 5;
    baseInfo.status = 0x0201;

    vector<CcuErrorInfo> errorInfo;
    CcuTaskException::GenStatusInfo(baseInfo, errorInfo);

    EXPECT_EQ(errorInfo.size(), 1u);
    EXPECT_EQ(errorInfo[0].type, CcuErrorType::MISSION);
    EXPECT_TRUE(string(errorInfo[0].msg.mission.missionError).find("Local Length Error") != string::npos);
}

TEST_F(CcuTaskExceptionTest, GenStatusInfo_RemoteOpError) {
    ErrorInfoBase baseInfo = {};
    baseInfo.dieId = 0;
    baseInfo.missionId = 3;
    baseInfo.currentInsId = 10;
    baseInfo.status = 0x0301;

    vector<CcuErrorInfo> errorInfo;
    CcuTaskException::GenStatusInfo(baseInfo, errorInfo);

    EXPECT_EQ(errorInfo.size(), 1u);
    EXPECT_EQ(errorInfo[0].type, CcuErrorType::MISSION);
    EXPECT_TRUE(string(errorInfo[0].msg.mission.missionError).find("Remote Unsupported Request") != string::npos);
}

TEST_F(CcuTaskExceptionTest, GenStatusInfo_TransactionRetryExceeded) {
    ErrorInfoBase baseInfo = {};
    baseInfo.dieId = 0;
    baseInfo.missionId = 4;
    baseInfo.currentInsId = 15;
    baseInfo.status = 0x0400;

    vector<CcuErrorInfo> errorInfo;
    CcuTaskException::GenStatusInfo(baseInfo, errorInfo);

    EXPECT_EQ(errorInfo.size(), 1u);
    EXPECT_EQ(errorInfo[0].type, CcuErrorType::MISSION);
    EXPECT_TRUE(string(errorInfo[0].msg.mission.missionError).find("Transaction Retry Counter Exceeded") != string::npos);
}

TEST_F(CcuTaskExceptionTest, GenStatusInfo_TransactionAckTimeout) {
    ErrorInfoBase baseInfo = {};
    baseInfo.dieId = 0;
    baseInfo.missionId = 5;
    baseInfo.currentInsId = 20;
    baseInfo.status = 0x0500;

    vector<CcuErrorInfo> errorInfo;
    CcuTaskException::GenStatusInfo(baseInfo, errorInfo);

    EXPECT_EQ(errorInfo.size(), 1u);
    EXPECT_EQ(errorInfo[0].type, CcuErrorType::MISSION);
}

TEST_F(CcuTaskExceptionTest, GenStatusInfo_JettyWorkRequestFlushed) {
    ErrorInfoBase baseInfo = {};
    baseInfo.dieId = 0;
    baseInfo.missionId = 6;
    baseInfo.currentInsId = 25;
    baseInfo.status = 0x0600;

    vector<CcuErrorInfo> errorInfo;
    CcuTaskException::GenStatusInfo(baseInfo, errorInfo);

    EXPECT_EQ(errorInfo.size(), 1u);
    EXPECT_EQ(errorInfo[0].type, CcuErrorType::MISSION);
}

TEST_F(CcuTaskExceptionTest, GenStatusInfo_CCUAAlgTaskError) {
    ErrorInfoBase baseInfo = {};
    baseInfo.dieId = 0;
    baseInfo.missionId = 7;
    baseInfo.currentInsId = 30;
    baseInfo.status = 0x0700;

    vector<CcuErrorInfo> errorInfo;
    CcuTaskException::GenStatusInfo(baseInfo, errorInfo);

    EXPECT_EQ(errorInfo.size(), 1u);
    EXPECT_EQ(errorInfo[0].type, CcuErrorType::MISSION);
}

TEST_F(CcuTaskExceptionTest, GenStatusInfo_MemoryECCError) {
    ErrorInfoBase baseInfo = {};
    baseInfo.dieId = 0;
    baseInfo.missionId = 8;
    baseInfo.currentInsId = 35;
    baseInfo.status = 0x0800;

    vector<CcuErrorInfo> errorInfo;
    CcuTaskException::GenStatusInfo(baseInfo, errorInfo);

    EXPECT_EQ(errorInfo.size(), 1u);
    EXPECT_EQ(errorInfo[0].type, CcuErrorType::MISSION);
}

TEST_F(CcuTaskExceptionTest, GenStatusInfo_CCUMExecuteError) {
    ErrorInfoBase baseInfo = {};
    baseInfo.dieId = 0;
    baseInfo.missionId = 9;
    baseInfo.currentInsId = 40;
    baseInfo.status = 0x0901;

    vector<CcuErrorInfo> errorInfo;
    CcuTaskException::GenStatusInfo(baseInfo, errorInfo);

    EXPECT_EQ(errorInfo.size(), 1u);
    EXPECT_EQ(errorInfo[0].type, CcuErrorType::MISSION);
    EXPECT_TRUE(string(errorInfo[0].msg.mission.missionError).find("SQE instr and key not match") != string::npos);
}

TEST_F(CcuTaskExceptionTest, GenStatusInfo_CCUAExecuteError) {
    ErrorInfoBase baseInfo = {};
    baseInfo.dieId = 0;
    baseInfo.missionId = 10;
    baseInfo.currentInsId = 45;
    baseInfo.status = 0x0A02;

    vector<CcuErrorInfo> errorInfo;
    CcuTaskException::GenStatusInfo(baseInfo, errorInfo);

    EXPECT_EQ(errorInfo.size(), 1u);
    EXPECT_EQ(errorInfo[0].type, CcuErrorType::MISSION);
    EXPECT_TRUE(string(errorInfo[0].msg.mission.missionError).find("SLVERR") != string::npos);
}

TEST_F(CcuTaskExceptionTest, GenStatusInfo_HighPartNotInMap) {
    ErrorInfoBase baseInfo = {};
    baseInfo.dieId = 0;
    baseInfo.missionId = 11;
    baseInfo.currentInsId = 50;
    baseInfo.status = 0x1100;

    vector<CcuErrorInfo> errorInfo;
    CcuTaskException::GenStatusInfo(baseInfo, errorInfo);

    EXPECT_EQ(errorInfo.size(), 1u);
    EXPECT_EQ(errorInfo[0].type, CcuErrorType::MISSION);
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgLoop_DefaultValues) {
    CcuErrorInfo ccuErrorInfo = {};
    ccuErrorInfo.type = CcuErrorType::LOOP;
    ccuErrorInfo.repType = CcuRep::CcuRepType::LOOP;
    ccuErrorInfo.dieId = 0;
    ccuErrorInfo.missionId = 1;
    ccuErrorInfo.instrId = 10;
    ccuErrorInfo.msg.loop.startInstrId = 0;
    ccuErrorInfo.msg.loop.endInstrId = 100;
    ccuErrorInfo.msg.loop.loopEngineId = 5;
    ccuErrorInfo.msg.loop.loopCnt = 10;
    ccuErrorInfo.msg.loop.loopCurrentCnt = 5;
    ccuErrorInfo.msg.loop.addrStride = 256;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, false);

    string result = CcuTaskException::GetCcuErrorMsgLoop(ccuErrorInfo, taskInfo, 0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgLoopGroup_DefaultValues) {
    CcuErrorInfo ccuErrorInfo = {};
    ccuErrorInfo.type = CcuErrorType::LOOP_GROUP;
    ccuErrorInfo.repType = CcuRep::CcuRepType::LOOPGROUP;
    ccuErrorInfo.dieId = 0;
    ccuErrorInfo.missionId = 1;
    ccuErrorInfo.instrId = 20;
    ccuErrorInfo.msg.loopGroup.startLoopInsId = 0;
    ccuErrorInfo.msg.loopGroup.loopInsCnt = 8;
    ccuErrorInfo.msg.loopGroup.expandOffset = 1;
    ccuErrorInfo.msg.loopGroup.expandCnt = 4;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, false);

    string result = CcuTaskException::GetCcuErrorMsgLoopGroup(ccuErrorInfo, taskInfo, 0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgLocPostSem_DefaultValues) {
    CcuErrorInfo ccuErrorInfo = {};
    ccuErrorInfo.type = CcuErrorType::WAIT_SIGNAL;
    ccuErrorInfo.repType = CcuRep::CcuRepType::LOC_RECORD_EVENT;
    ccuErrorInfo.dieId = 0;
    ccuErrorInfo.missionId = 1;
    ccuErrorInfo.instrId = 5;
    ccuErrorInfo.msg.waitSignal.signalId = 10;
    ccuErrorInfo.msg.waitSignal.signalMask = 0xFF;
    ccuErrorInfo.msg.waitSignal.signalValue = 0xF0;
    ccuErrorInfo.msg.waitSignal.paramId = 1;
    ccuErrorInfo.msg.waitSignal.paramValue = 12345;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, false);

    string result = CcuTaskException::GetCcuErrorMsgLocPostSem(ccuErrorInfo, taskInfo, 0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgLocWaitEvent_DefaultValues) {
    CcuErrorInfo ccuErrorInfo = {};
    ccuErrorInfo.type = CcuErrorType::WAIT_SIGNAL;
    ccuErrorInfo.repType = CcuRep::CcuRepType::LOC_WAIT_EVENT;
    ccuErrorInfo.dieId = 0;
    ccuErrorInfo.missionId = 1;
    ccuErrorInfo.instrId = 6;
    ccuErrorInfo.msg.waitSignal.signalId = 20;
    ccuErrorInfo.msg.waitSignal.signalMask = 0x0F;
    ccuErrorInfo.msg.waitSignal.signalValue = 0x05;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, false);

    string result = CcuTaskException::GetCcuErrorMsgLocWaitEvent(ccuErrorInfo, taskInfo, 0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgLocWaitNotify_DefaultValues) {
    CcuErrorInfo ccuErrorInfo = {};
    ccuErrorInfo.type = CcuErrorType::WAIT_SIGNAL;
    ccuErrorInfo.repType = CcuRep::CcuRepType::LOC_WAIT_NOTIFY;
    ccuErrorInfo.dieId = 0;
    ccuErrorInfo.missionId = 1;
    ccuErrorInfo.instrId = 7;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, false);

    string result = CcuTaskException::GetCcuErrorMsgLocWaitNotify(ccuErrorInfo, taskInfo, 0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgRemPostSem_DefaultValues) {
    CcuErrorInfo ccuErrorInfo = {};
    ccuErrorInfo.type = CcuErrorType::WAIT_SIGNAL;
    ccuErrorInfo.repType = CcuRep::CcuRepType::REM_POST_SEM;
    ccuErrorInfo.dieId = 0;
    ccuErrorInfo.missionId = 1;
    ccuErrorInfo.instrId = 8;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, false);

    string result = CcuTaskException::GetCcuErrorMsgRemPostSem(ccuErrorInfo, taskInfo, 0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgRemWaitSem_DefaultValues) {
    CcuErrorInfo ccuErrorInfo = {};
    ccuErrorInfo.type = CcuErrorType::WAIT_SIGNAL;
    ccuErrorInfo.repType = CcuRep::CcuRepType::REM_WAIT_SEM;
    ccuErrorInfo.dieId = 0;
    ccuErrorInfo.missionId = 1;
    ccuErrorInfo.instrId = 9;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, false);

    string result = CcuTaskException::GetCcuErrorMsgRemWaitSem(ccuErrorInfo, taskInfo, 0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgRemPostVar_DefaultValues) {
    CcuErrorInfo ccuErrorInfo = {};
    ccuErrorInfo.type = CcuErrorType::WAIT_SIGNAL;
    ccuErrorInfo.repType = CcuRep::CcuRepType::REM_POST_VAR;
    ccuErrorInfo.dieId = 0;
    ccuErrorInfo.missionId = 1;
    ccuErrorInfo.instrId = 11;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, false);

    string result = CcuTaskException::GetCcuErrorMsgRemPostVar(ccuErrorInfo, taskInfo, 0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgPostSharedSem_DefaultValues) {
    CcuErrorInfo ccuErrorInfo = {};
    ccuErrorInfo.type = CcuErrorType::WAIT_SIGNAL;
    ccuErrorInfo.repType = CcuRep::CcuRepType::RECORD_SHARED_NOTIFY;
    ccuErrorInfo.dieId = 0;
    ccuErrorInfo.missionId = 1;
    ccuErrorInfo.instrId = 12;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, false);

    string result = CcuTaskException::GetCcuErrorMsgPostSharedSem(ccuErrorInfo, taskInfo, 0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgRead_DefaultValues) {
    CcuErrorInfo ccuErrorInfo = {};
    ccuErrorInfo.type = CcuErrorType::TRANS_MEM;
    ccuErrorInfo.repType = CcuRep::CcuRepType::READ;
    ccuErrorInfo.dieId = 0;
    ccuErrorInfo.missionId = 1;
    ccuErrorInfo.instrId = 13;
    ccuErrorInfo.msg.transMem.locAddr = 0x1000;
    ccuErrorInfo.msg.transMem.locToken = 0x1;
    ccuErrorInfo.msg.transMem.rmtAddr = 0x2000;
    ccuErrorInfo.msg.transMem.rmtToken = 0x2;
    ccuErrorInfo.msg.transMem.len = 1024;
    ccuErrorInfo.msg.transMem.channelId = 101;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, false);

    string result = CcuTaskException::GetCcuErrorMsgRead(ccuErrorInfo, taskInfo, 0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgWrite_DefaultValues) {
    CcuErrorInfo ccuErrorInfo = {};
    ccuErrorInfo.type = CcuErrorType::TRANS_MEM;
    ccuErrorInfo.repType = CcuRep::CcuRepType::WRITE;
    ccuErrorInfo.dieId = 0;
    ccuErrorInfo.missionId = 1;
    ccuErrorInfo.instrId = 14;
    ccuErrorInfo.msg.transMem.locAddr = 0x1000;
    ccuErrorInfo.msg.transMem.locToken = 0x1;
    ccuErrorInfo.msg.transMem.rmtAddr = 0x2000;
    ccuErrorInfo.msg.transMem.rmtToken = 0x2;
    ccuErrorInfo.msg.transMem.len = 2048;
    ccuErrorInfo.msg.transMem.channelId = 102;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, false);

    string result = CcuTaskException::GetCcuErrorMsgWrite(ccuErrorInfo, taskInfo, 0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgLocalCpy_DefaultValues) {
    CcuErrorInfo ccuErrorInfo = {};
    ccuErrorInfo.type = CcuErrorType::TRANS_MEM;
    ccuErrorInfo.repType = CcuRep::CcuRepType::LOCAL_CPY;
    ccuErrorInfo.dieId = 0;
    ccuErrorInfo.missionId = 1;
    ccuErrorInfo.instrId = 15;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, false);

    string result = CcuTaskException::GetCcuErrorMsgLocalCpy(ccuErrorInfo, taskInfo, 0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgLocalReduce_DefaultValues) {
    CcuErrorInfo ccuErrorInfo = {};
    ccuErrorInfo.type = CcuErrorType::TRANS_MEM;
    ccuErrorInfo.repType = CcuRep::CcuRepType::LOCAL_REDUCE;
    ccuErrorInfo.dieId = 0;
    ccuErrorInfo.missionId = 1;
    ccuErrorInfo.instrId = 16;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, false);

    string result = CcuTaskException::GetCcuErrorMsgLocalReduce(ccuErrorInfo, taskInfo, 0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgBufRead_DefaultValues) {
    CcuErrorInfo ccuErrorInfo = {};
    ccuErrorInfo.type = CcuErrorType::BUF_TRANS_MEM;
    ccuErrorInfo.repType = CcuRep::CcuRepType::BUF_READ;
    ccuErrorInfo.dieId = 0;
    ccuErrorInfo.missionId = 1;
    ccuErrorInfo.instrId = 17;
    ccuErrorInfo.msg.bufTransMem.bufId = 5;
    ccuErrorInfo.msg.bufTransMem.addr = 0x3000;
    ccuErrorInfo.msg.bufTransMem.len = 512;
    ccuErrorInfo.msg.bufTransMem.channelId = 103;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, false);

    string result = CcuTaskException::GetCcuErrorMsgBufRead(ccuErrorInfo, taskInfo, 0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgBufWrite_DefaultValues) {
    CcuErrorInfo ccuErrorInfo = {};
    ccuErrorInfo.type = CcuErrorType::BUF_TRANS_MEM;
    ccuErrorInfo.repType = CcuRep::CcuRepType::BUF_WRITE;
    ccuErrorInfo.dieId = 0;
    ccuErrorInfo.missionId = 1;
    ccuErrorInfo.instrId = 18;
    ccuErrorInfo.msg.bufTransMem.bufId = 6;
    ccuErrorInfo.msg.bufTransMem.addr = 0x4000;
    ccuErrorInfo.msg.bufTransMem.len = 256;
    ccuErrorInfo.msg.bufTransMem.channelId = 104;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, false);

    string result = CcuTaskException::GetCcuErrorMsgBufWrite(ccuErrorInfo, taskInfo, 0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgBufLocRead_DefaultValues) {
    CcuErrorInfo ccuErrorInfo = {};
    ccuErrorInfo.type = CcuErrorType::BUF_TRANS_MEM;
    ccuErrorInfo.repType = CcuRep::CcuRepType::BUF_LOC_READ;
    ccuErrorInfo.dieId = 0;
    ccuErrorInfo.missionId = 1;
    ccuErrorInfo.instrId = 19;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, false);

    string result = CcuTaskException::GetCcuErrorMsgBufLocRead(ccuErrorInfo, taskInfo, 0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgBufLocWrite_DefaultValues) {
    CcuErrorInfo ccuErrorInfo = {};
    ccuErrorInfo.type = CcuErrorType::BUF_TRANS_MEM;
    ccuErrorInfo.repType = CcuRep::CcuRepType::BUF_LOC_WRITE;
    ccuErrorInfo.dieId = 0;
    ccuErrorInfo.missionId = 1;
    ccuErrorInfo.instrId = 21;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, false);

    string result = CcuTaskException::GetCcuErrorMsgBufLocWrite(ccuErrorInfo, taskInfo, 0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgBufReduce_DefaultValues) {
    CcuErrorInfo ccuErrorInfo = {};
    ccuErrorInfo.type = CcuErrorType::BUF_REDUCE;
    ccuErrorInfo.repType = CcuRep::CcuRepType::BUF_REDUCE;
    ccuErrorInfo.dieId = 0;
    ccuErrorInfo.missionId = 1;
    ccuErrorInfo.instrId = 22;
    ccuErrorInfo.msg.bufReduce.count = 4;
    ccuErrorInfo.msg.bufReduce.dataType = 1;
    ccuErrorInfo.msg.bufReduce.outputDataType = 0;
    ccuErrorInfo.msg.bufReduce.opType = 0;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, false);

    string result = CcuTaskException::GetCcuErrorMsgBufReduce(ccuErrorInfo, taskInfo, 0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgByType_MISSION) {
    CcuErrorInfo ccuErrorInfo = {};
    ccuErrorInfo.type = CcuErrorType::MISSION;
    ccuErrorInfo.repType = CcuRep::CcuRepType::BASE;
    ccuErrorInfo.dieId = 0;
    ccuErrorInfo.missionId = 1;
    ccuErrorInfo.instrId = 0;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, false);

    string result = CcuTaskException::GetCcuErrorMsgByType(ccuErrorInfo, taskInfo, 0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuErrorMsgByType_DefaultType) {
    CcuErrorInfo ccuErrorInfo = {};
    ccuErrorInfo.type = CcuErrorType::DEFAULT;
    ccuErrorInfo.repType = CcuRep::CcuRepType::BASE;
    ccuErrorInfo.dieId = 0;
    ccuErrorInfo.missionId = 1;
    ccuErrorInfo.instrId = 0;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, false);

    string result = CcuTaskException::GetCcuErrorMsgByType(ccuErrorInfo, taskInfo, 0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuLenErrorMsg_LargeValue) {
    constexpr uint64_t CCU_MSG_256MB_LEN = 256 * 1024 * 1024;
    string result = CcuTaskException::GetCcuLenErrorMsg(CCU_MSG_256MB_LEN);
    EXPECT_TRUE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuLenErrorMsg_NormalValue) {
    string result = CcuTaskException::GetCcuLenErrorMsg(1024 * 1024);
    EXPECT_TRUE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuLenErrorMsg_Exceed256MB) {
    constexpr uint64_t CCU_MSG_256MB_LEN = 256 * 1024 * 1024;
    uint64_t exceedLen = static_cast<uint64_t>(CCU_MSG_256MB_LEN) + 1;
    string result = CcuTaskException::GetCcuLenErrorMsg(exceedLen);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, GetCcuLenErrorMsg_Zero) {
    string result = CcuTaskException::GetCcuLenErrorMsg(0);
    EXPECT_FALSE(result.empty());
}

TEST_F(CcuTaskExceptionTest, ProcessCcuException_MultipleMission) {
    rtExceptionInfo_t exceptionInfo{};
    exceptionInfo.deviceid = 0;
    exceptionInfo.expandInfo.u.ccuInfo.ccuMissionNum = 2;
    for (uint32_t i = 0; i < 2; ++i) {
        exceptionInfo.expandInfo.u.ccuInfo.missionInfo[i].status = 0x01;
        exceptionInfo.expandInfo.u.ccuInfo.missionInfo[i].subStatus = 0x02;
    }

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, true);
    MOCKER_CPP(&CcuComponent::CleanTaskKillState)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&CcuComponent::CleanDieCkes)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    CcuTaskException::ProcessCcuException(&exceptionInfo, taskInfo);
    EXPECT_TRUE(true);
}

TEST_F(CcuTaskExceptionTest, ProcessCcuException_CleanTaskKillStateFail) {
    rtExceptionInfo_t exceptionInfo{};
    exceptionInfo.deviceid = 0;
    exceptionInfo.expandInfo.u.ccuInfo.ccuMissionNum = 1;
    exceptionInfo.expandInfo.u.ccuInfo.missionInfo[0].status = 0x01;
    exceptionInfo.expandInfo.u.ccuInfo.missionInfo[0].subStatus = 0x00;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, true);
    MOCKER_CPP(&CcuComponent::CleanTaskKillState)
        .stubs()
        .will(returnValue(HCCL_E_PARA));
    MOCKER_CPP(&CcuComponent::CleanDieCkes)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    CcuTaskException::ProcessCcuException(&exceptionInfo, taskInfo);
    EXPECT_TRUE(true);
}

TEST_F(CcuTaskExceptionTest, ProcessCcuException_CleanDieCkesFail) {
    rtExceptionInfo_t exceptionInfo{};
    exceptionInfo.deviceid = 0;
    exceptionInfo.expandInfo.u.ccuInfo.ccuMissionNum = 1;
    exceptionInfo.expandInfo.u.ccuInfo.missionInfo[0].status = 0x01;
    exceptionInfo.expandInfo.u.ccuInfo.missionInfo[0].subStatus = 0x00;

    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, true);
    MOCKER_CPP(&CcuComponent::CleanTaskKillState)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&CcuComponent::CleanDieCkes)
        .stubs()
        .will(returnValue(HCCL_E_PARA));

    CcuTaskException::ProcessCcuException(&exceptionInfo, taskInfo);
    EXPECT_TRUE(true);
}

TEST_F(CcuTaskExceptionTest, GetGroupRankInfo_WithValidComm) {
    Hccl::ParaCcu paraCcu = {};
    Hccl::TaskParam taskParam = {
        .taskType = Hccl::TaskParamType::TASK_CCU,
        .beginTime = 0,
        .endTime = 0,
        .isMaster = false,
        .taskPara = {.Ccu = paraCcu},
        .ccuDetailInfo = nullptr
    };
    Hccl::TaskInfo taskInfo(1, 2, 3, taskParam, nullptr, false);
    taskInfo.dfxOpInfo_ = std::make_shared<Hccl::DfxOpInfo>();
    hccl::ManagerCallbacks callbacks;
    callbacks.getAicpuCommState = []() { return true; };
    callbacks.setAicpuCommState = [](bool) {};
    callbacks.kernelLaunchAicpuCommInit = []() { return HCCL_SUCCESS; };
    std::string commName = "TestCommGroup";
    hccl::CollComm collComm(nullptr, 1, commName, callbacks);
    taskInfo.dfxOpInfo_->comm_ = &collComm;
    MOCKER_CPP(&hccl::CollComm::GetCommId)
        .stubs()
        .will(returnValue(commName));
    MOCKER_CPP(&hccl::CollComm::GetRankSize)
        .stubs()
        .will(returnValue(16u));
    MOCKER_CPP(&hccl::CollComm::GetMyRankId)
        .stubs()
        .will(returnValue(7u));

    std::string result = CcuTaskException::GetGroupRankInfo(taskInfo);
    EXPECT_NE(result, "");
    EXPECT_TRUE(result.find("group:[TestCommGroup]") != std::string::npos);
    EXPECT_TRUE(result.find("rankSize[16]") != std::string::npos);
    EXPECT_TRUE(result.find("rankId[7]") != std::string::npos);
}

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
    auto rep = std::make_shared<CcuRep::CcuRepAssign>(CcuRep::Variable(&context), CcuRep::Variable(&context));
    rep->subType = CcuRep::AssignSubType::VAR_TO_VAR;
    context.CollectProfilingReps(rep);

    auto& profilingInfo = context.GetLGProfilingInfo();
    EXPECT_EQ(profilingInfo.assignProfilingReps.size(), 1);
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

} // namespace hcomm

#undef private