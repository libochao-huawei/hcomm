/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ut_hcomm_base.h"
#include "hccl_api_data_aicpu_ts.h"
#include "aicpu_indop_process.h"
#include "hcclCommTaskExceptionLite.h"
#include "hcclCommTaskException.h"
#include "global_mirror_tasks.h"

class TestHcclCommTaskException : public TestHcommCAdptBase {
public:
    void SetUp() override {
        TestHcommCAdptBase::SetUp();
        instanceLite = &hcom::HcclCommTaskExceptionLite::GetInstance();
        instanceHost = hcomm::TaskExceptionHostManager::GetHandler(0);
        instanceLite->Init(0);
    }
    void TearDown() override {
        TestHcommCAdptBase::TearDown();
        GlobalMockObject::verify();
    }

protected:
    hcomm::HcclCommTaskExceptionLite* instanceLite;
    hcomm::TaskExceptionHost* instanceHost;
};

// =============================================================================
// Test cases for HcommAcquireComm (hccl_api_data_aicpu_ts.cc)
// =============================================================================

TEST_F(TestHcclCommTaskException, Ut_HcommAcquireComm_When_CommIdNullptr_Return_HCCL_E_PTR)
{
    int32_t ret = HcommAcquireComm(nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestHcclCommTaskException, Ut_HcommAcquireComm_When_ValidCommId_Return_SuccessOrPtr)
{
    const char* commId = "test_comm_group";
    int32_t ret = HcommAcquireComm(commId);
    EXPECT_TRUE(ret == HCCL_SUCCESS || ret == HCCL_E_PTR);
}

// =============================================================================
// Test cases for HcommReleaseComm (hccl_api_data_aicpu_ts.cc)
// =============================================================================

TEST_F(TestHcclCommTaskException, Ut_HcommReleaseComm_When_CommIdNullptr_Return_HCCL_E_PTR)
{
    int32_t ret = HcommReleaseComm(nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestHcclCommTaskException, Ut_HcommReleaseComm_When_ValidCommId_Return_HCCL_SUCCESS)
{
    const char* commId = "test_comm_group";
    int32_t ret = HcommReleaseComm(commId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// =============================================================================
// Test cases for AicpuIndOpThreadInit (aicpu_indop_process.cc)
// =============================================================================

TEST_F(TestHcclCommTaskException, Ut_AicpuIndOpThreadInit_When_ParamNullptr_Return_HCCL_E_PTR)
{
    HcclResult ret = AicpuIndopProcess::AicpuIndOpThreadInit(nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestHcclCommTaskException, Ut_AicpuIndOpThreadInit_When_ValidParam_Return_SuccessOrPtr)
{
    ThreadMgrAicpuParam param;
    strcpy_s(param.hcomId, sizeof(param.hcomId), "test_comm_group");
    param.threadNum = 2;
    
    HcclResult ret = AicpuIndopProcess::AicpuIndOpThreadInit(&param);
    EXPECT_TRUE(ret == HCCL_SUCCESS || ret == HCCL_E_PTR);
}

// =============================================================================
// Test cases for AicpuGetCommMgrbyGroup (aicpu_indop_process.cc)
// =============================================================================

TEST_F(TestHcclCommTaskException, Ut_AicpuGetCommMgrbyGroup_When_NonExistentGroup_Return_Nullptr)
{
    CollCommAicpuMgr* mgr = AicpuIndopProcess::AicpuGetCommMgrbyGroup("non_existent_group");
    EXPECT_EQ(mgr, nullptr);
}

TEST_F(TestHcclCommTaskException, Ut_AicpuGetCommMgrbyGroup_When_EmptyGroup_Return_Nullptr)
{
    CollCommAicpuMgr* mgr = AicpuIndopProcess::AicpuGetCommMgrbyGroup("");
    EXPECT_EQ(mgr, nullptr);
}

// =============================================================================
// Test cases for AicpuIndOpNotifyInit (aicpu_indop_process.cc)
// =============================================================================

TEST_F(TestHcclCommTaskException, Ut_AicpuIndOpNotifyInit_When_ParamNullptr_Return_HCCL_E_PTR)
{
    HcclResult ret = AicpuIndopProcess::AicpuIndOpNotifyInit(nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestHcclCommTaskException, Ut_AicpuIndOpNotifyInit_When_AllocNotify_Return_SuccessOrPtr)
{
    NotifyMgrAicpuParam param;
    strcpy_s(param.hcomId, sizeof(param.hcomId), "test_comm_group");
    param.freeFlag = false;
    param.notifyNum = 5;
    
    HcclResult ret = AicpuIndopProcess::AicpuIndOpNotifyInit(&param);
    EXPECT_TRUE(ret == HCCL_SUCCESS || ret == HCCL_E_PTR);
}

TEST_F(TestHcclCommTaskException, Ut_AicpuIndOpNotifyInit_When_FreeNotify_Return_SuccessOrPtr)
{
    NotifyMgrAicpuParam param;
    strcpy_s(param.hcomId, sizeof(param.hcomId), "test_comm_group");
    param.freeFlag = true;
    param.notifyNum = 5;
    
    HcclResult ret = AicpuIndopProcess::AicpuIndOpNotifyInit(&param);
    EXPECT_TRUE(ret == HCCL_SUCCESS || ret == HCCL_E_PTR);
}

// =============================================================================
// Test cases for AicpuDfxOpInfoInit (aicpu_indop_process.cc)
// =============================================================================

TEST_F(TestHcclCommTaskException, Ut_AicpuDfxOpInfoInit_When_DfxInfoNullptr_Return_HCCL_E_PTR)
{
    HcclResult ret = AicpuIndopProcess::AicpuDfxOpInfoInit(nullptr, "test_comm_group");
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestHcclCommTaskException, Ut_AicpuDfxOpInfoInit_When_ValidDfxInfo_Return_SuccessOrPtr)
{
    HcclDfxOpInfo dfxInfo;
    memset(&dfxInfo, 0, sizeof(dfxInfo));
    
    HcclResult ret = AicpuIndopProcess::AicpuDfxOpInfoInit(&dfxInfo, "test_comm_group");
    EXPECT_TRUE(ret == HCCL_SUCCESS || ret == HCCL_E_PTR);
}

// =============================================================================
// Test cases for PrintTaskContextInfo (hcclCommTaskExceptionLite.cc)
// =============================================================================

TEST_F(TestHcclCommTaskException, Ut_PrintTaskContextInfo_When_InvalidStreamId_Return_HCCL_E_PARA)
{
    HcclResult ret = instanceLite->PrintTaskContextInfo(999, 100);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(TestHcclCommTaskException, Ut_PrintTaskContextInfo_When_InvalidTaskId_Return_HCCL_E_PARA)
{
    HcclResult ret = instanceLite->PrintTaskContextInfo(1, 999);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// =============================================================================
// Test cases for GetOpDataInfo (hcclCommTaskExceptionLite.cc)
// =============================================================================

TEST_F(TestHcclCommTaskException, Ut_GetOpDataInfo_When_DfxOpInfoNullptr_Return_EmptyString)
{
    Hccl::TaskInfo taskInfo(0, 0, 0, Hccl::TaskParam{}, nullptr);
    std::string result = instanceLite->GetOpDataInfo(taskInfo);
    EXPECT_TRUE(result.empty());
}

TEST_F(TestHcclCommTaskException, Ut_GetOpDataInfo_When_ValidDfxOpInfo_Return_NonEmptyString)
{
    auto dfxOpInfo = std::shared_ptr<Hccl::DfxOpInfo>(new Hccl::DfxOpInfo());
    dfxOpInfo->opIndex_ = 5;
    dfxOpInfo->algTag_ = "test_alg";
    dfxOpInfo->op_.dataCount = 1024;
    
    Hccl::TaskInfo taskInfo(0, 0, 0, Hccl::TaskParam{}, dfxOpInfo);
    std::string result = instanceLite->GetOpDataInfo(taskInfo);
    
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("opIndex"), std::string::npos);
}

// =============================================================================
// Test cases for Process (hcclCommTaskException.cc)
// =============================================================================

TEST_F(TestHcclCommTaskException, Ut_Process_When_ExceptionInfoNullptr_Return_Void)
{
    hcomm::TaskExceptionHost::Process(nullptr);
    // Should not crash
}

TEST_F(TestHcclCommTaskException, Ut_Process_When_ValidExceptionInfo_Return_Void)
{
    rtExceptionInfo_t exceptionInfo;
    memset(&exceptionInfo, 0, sizeof(exceptionInfo));
    exceptionInfo.deviceid = 0;
    exceptionInfo.streamid = 1;
    exceptionInfo.taskid = 100;
    
    hcomm::TaskExceptionHost::Process(&exceptionInfo);
    // Should not crash
}

// =============================================================================
// Test cases for PrintTaskContextInfo (hcclCommTaskException.cc)
// =============================================================================

TEST_F(TestHcclCommTaskException, Ut_HostPrintTaskContextInfo_When_InvalidDeviceId_Return_Void)
{
    hcomm::TaskExceptionHost::PrintTaskContextInfo(999, 1, 100);
    // Should not crash
}

TEST_F(TestHcclCommTaskException, Ut_HostPrintTaskContextInfo_When_InvalidStreamId_Return_Void)
{
    hcomm::TaskExceptionHost::PrintTaskContextInfo(0, 999, 100);
    // Should not crash
}

// =============================================================================
// Test cases for FindTaskInfo (global_mirror_tasks.cc)
// =============================================================================

TEST_F(TestHcclCommTaskException, Ut_FindTaskInfo_When_InvalidDevId_Return_HCCL_E_PARA)
{
    Hccl::GlobalMirrorTasks& instance = Hccl::GlobalMirrorTasks::Instance();
    std::shared_ptr<Hccl::TaskInfo> curTask;
    
    HcclResult ret = instance.FindTaskInfo(999, 1, 100, curTask);
    EXPECT_EQ(ret, HCCL_E_PARA);
    EXPECT_EQ(curTask, nullptr);
}

TEST_F(TestHcclCommTaskException, Ut_FindTaskInfo_When_NonExistentStream_Return_HCCL_E_NOT_FOUND)
{
    Hccl::GlobalMirrorTasks& instance = Hccl::GlobalMirrorTasks::Instance();
    std::shared_ptr<Hccl::TaskInfo> curTask;
    
    HcclResult ret = instance.FindTaskInfo(0, 999, 100, curTask);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
    EXPECT_EQ(curTask, nullptr);
}

TEST_F(TestHcclCommTaskException, Ut_FindTaskInfo_When_NonExistentTask_Return_HCCL_E_NOT_FOUNDOrHCCL_E_PARA)
{
    Hccl::GlobalMirrorTasks& instance = Hccl::GlobalMirrorTasks::Instance();
    std::shared_ptr<Hccl::TaskInfo> curTask;
    
    HcclResult ret = instance.FindTaskInfo(0, 1, 99999, curTask);
    EXPECT_TRUE(ret == HCCL_E_NOT_FOUND || ret == HCCL_E_PARA);
}

// =============================================================================
// Additional edge case tests
// =============================================================================

TEST_F(TestHcclCommTaskException, Ut_HcclCommTaskExceptionLite_GetInstance_Return_SameInstance)
{
    hcomm::HcclCommTaskExceptionLite& instance1 = hcomm::HcclCommTaskExceptionLite::GetInstance();
    hcomm::HcclCommTaskExceptionLite& instance2 = hcomm::HcclCommTaskExceptionLite::GetInstance();
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(TestHcclCommTaskException, Ut_HcclCommTaskExceptionLite_Init_Return_Void)
{
    instanceLite->Init(0);
    instanceLite->Init(1);
    // Should not crash
}

TEST_F(TestHcclCommTaskException, Ut_HcclCommTaskExceptionLite_Call_Return_Void)
{
    instanceLite->Call();
    // Should not crash
}

TEST_F(TestHcclCommTaskException, Ut_GlobalMirrorTasks_Instance_Return_SameInstance)
{
    Hccl::GlobalMirrorTasks& instance1 = Hccl::GlobalMirrorTasks::Instance();
    Hccl::GlobalMirrorTasks& instance2 = Hccl::GlobalMirrorTasks::Instance();
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(TestHcclCommTaskException, Ut_TaskExceptionHostManager_GetHandler_Return_SameHandler)
{
    hcomm::TaskExceptionHost* handler1 = hcomm::TaskExceptionHostManager::GetHandler(0);
    hcomm::TaskExceptionHost* handler2 = hcomm::TaskExceptionHostManager::GetHandler(0);
    EXPECT_EQ(handler1, handler2);
}

TEST_F(TestHcclCommTaskException, Ut_AicpuIndopProcess_GetCommAll_Return_HCCL_SUCCESS)
{
    std::vector<std::pair<std::string, CollCommAicpuMgr*>> aicpuCommInfo;
    HcclResult ret = AicpuIndopProcess::AicpuGetCommAll(aicpuCommInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TestHcclCommTaskException, Ut_AicpuIndopProcess_DestroyCommbyGroup_When_NonExistent_Return_HCCL_E_PARA)
{
    HcclResult ret = AicpuIndopProcess::AicpuDestroyCommbyGroup("non_existent_group");
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(TestHcclCommTaskException, Ut_AicpuIndopProcess_ReleaseCommMgrbyGroup_When_NonExistent_Return_Void)
{
    AicpuIndopProcess::AicpuReleaseCommMgrbyGroup("non_existent_group");
    // Should not crash
}