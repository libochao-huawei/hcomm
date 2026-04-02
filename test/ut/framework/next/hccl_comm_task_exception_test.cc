/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <cstring>

// Include header files for the functions we're testing
#include "hccl_api_data_aicpu_ts.h"
#include "aicpu_indop_process.h"
#include "hcclCommTaskExceptionLite.h"
#include "hcclCommTaskException.h"
#include "global_mirror_tasks.h"
#include "task_info.h"
#include "task_struct_v2.h"
#include "hccl_communicator.h"

using namespace testing;
using namespace hccl;
using namespace hcomm;

// Test fixture
class HcclCommTaskExceptionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize common test data
        devId_ = 0;
        streamId_ = 1;
        taskId_ = 100;
        commId_ = "test_comm_group";
    }

    void TearDown() override {
        // Cleanup
    }

    u32 devId_;
    u32 streamId_;
    u32 taskId_;
    std::string commId_;
};

// =============================================================================
// Test cases for HcommAcquireComm (hccl_api_data_aicpu_ts.cc)
// =============================================================================

TEST_F(HcclCommTaskExceptionTest, HcommAcquireComm_NullCommId) {
    int32_t ret = HcommAcquireComm(nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclCommTaskExceptionTest, HcommAcquireComm_ValidCommId) {
    // This test requires mocking of hrtGetDeviceType and AicpuHcclProcess/AicpuIndopProcess
    // For now, we'll just test that it doesn't crash with a valid string
    int32_t ret = HcommAcquireComm(commId_.c_str());
    // The actual return value depends on device type and comm manager state
    // We expect it to either succeed or fail gracefully
    EXPECT_TRUE(ret == HCCL_SUCCESS || ret == HCCL_E_PTR);
}

// =============================================================================
// Test cases for HcommReleaseComm (hccl_api_data_aicpu_ts.cc)
// =============================================================================

TEST_F(HcclCommTaskExceptionTest, HcommReleaseComm_NullCommId) {
    int32_t ret = HcommReleaseComm(nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclCommTaskExceptionTest, HcommReleaseComm_ValidCommId) {
    // Test releasing a valid comm ID
    int32_t ret = HcommReleaseComm(commId_.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// =============================================================================
// Test cases for AicpuIndOpThreadInit (aicpu_indop_process.cc)
// =============================================================================

TEST_F(HcclCommTaskExceptionTest, AicpuIndOpThreadInit_NullParam) {
    HcclResult ret = AicpuIndopProcess::AicpuIndOpThreadInit(nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclCommTaskExceptionTest, AicpuIndOpThreadInit_ValidParam) {
    ThreadMgrAicpuParam param;
    param.hcomId = commId_;
    // Set other required fields
    param.threadNum = 2;
    
    HcclResult ret = AicpuIndopProcess::AicpuIndOpThreadInit(&param);
    // The result depends on whether the comm manager exists
    // Either success or pointer error is acceptable
    EXPECT_TRUE(ret == HCCL_SUCCESS || ret == HCCL_E_PTR);
}

// =============================================================================
// Test cases for AicpuGetCommMgrbyGroup (aicpu_indop_process.cc)
// =============================================================================

TEST_F(HcclCommTaskExceptionTest, AicpuGetCommMgrbyGroup_NonExistentGroup) {
    CollCommAicpuMgr *mgr = AicpuIndopProcess::AicpuGetCommMgrbyGroup("non_existent_group");
    EXPECT_EQ(mgr, nullptr);
}

TEST_F(HcclCommTaskExceptionTest, AicpuGetCommMgrbyGroup_EmptyGroup) {
    CollCommAicpuMgr *mgr = AicpuIndopProcess::AicpuGetCommMgrbyGroup("");
    EXPECT_EQ(mgr, nullptr);
}

// =============================================================================
// Test cases for AicpuIndOpNotifyInit (aicpu_indop_process.cc)
// =============================================================================

TEST_F(HcclCommTaskExceptionTest, AicpuIndOpNotifyInit_NullParam) {
    HcclResult ret = AicpuIndopProcess::AicpuIndOpNotifyInit(nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclCommTaskExceptionTest, AicpuIndOpNotifyInit_AllocNotify) {
    NotifyMgrAicpuParam param;
    param.hcomId = commId_;
    param.freeFlag = false;
    param.notifyNum = 5;
    
    HcclResult ret = AicpuIndopProcess::AicpuIndOpNotifyInit(&param);
    // Result depends on comm manager existence
    EXPECT_TRUE(ret == HCCL_SUCCESS || ret == HCCL_E_PTR);
}

TEST_F(HcclCommTaskExceptionTest, AicpuIndOpNotifyInit_FreeNotify) {
    NotifyMgrAicpuParam param;
    param.hcomId = commId_;
    param.freeFlag = true;
    param.notifyNum = 5;
    
    HcclResult ret = AicpuIndopProcess::AicpuIndOpNotifyInit(&param);
    // Result depends on comm manager existence
    EXPECT_TRUE(ret == HCCL_SUCCESS || ret == HCCL_E_PTR);
}

// =============================================================================
// Test cases for AicpuDfxOpInfoInit (aicpu_indop_process.cc)
// =============================================================================

TEST_F(HcclCommTaskExceptionTest, AicpuDfxOpInfoInit_NullDfxInfo) {
    HcclResult ret = AicpuIndopProcess::AicpuDfxOpInfoInit(nullptr, commId_);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclCommTaskExceptionTest, AicpuDfxOpInfoInit_ValidDfxInfo) {
    HcclDfxOpInfo dfxInfo;
    // Initialize dfxInfo fields
    memset(&dfxInfo, 0, sizeof(dfxInfo));
    
    HcclResult ret = AicpuIndopProcess::AicpuDfxOpInfoInit(&dfxInfo, commId_);
    // Result depends on comm manager existence
    EXPECT_TRUE(ret == HCCL_SUCCESS || ret == HCCL_E_PTR);
}

// =============================================================================
// Test cases for PrintTaskContextInfo (hcclCommTaskExceptionLite.cc)
// =============================================================================

TEST_F(HcclCommTaskExceptionTest, PrintTaskContextInfo_InvalidStreamId) {
    HcclCommTaskExceptionLite& instance = HcclCommTaskExceptionLite::GetInstance();
    instance.Init(devId_);
    
    HcclResult ret = instance.PrintTaskContextInfo(999, taskId_);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcclCommTaskExceptionTest, PrintTaskContextInfo_InvalidTaskId) {
    HcclCommTaskExceptionLite& instance = HcclCommTaskExceptionLite::GetInstance();
    instance.Init(devId_);
    
    HcclResult ret = instance.PrintTaskContextInfo(streamId_, 999);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// =============================================================================
// Test cases for GetOpDataInfo (hcclCommTaskExceptionLite.cc)
// =============================================================================

TEST_F(HcclCommTaskExceptionTest, GetOpDataInfo_NullDfxOpInfo) {
    HcclCommTaskExceptionLite& instance = HcclCommTaskExceptionLite::GetInstance();
    
    Hccl::TaskInfo taskInfo(0, 0, 0, Hccl::TaskParam{}, nullptr);
    std::string result = instance.GetOpDataInfo(taskInfo);
    
    EXPECT_TRUE(result.empty());
}

TEST_F(HcclCommTaskExceptionTest, GetOpDataInfo_ValidDfxOpInfo) {
    HcclCommTaskExceptionLite& instance = HcclCommTaskExceptionLite::GetInstance();
    
    // Create a valid DfxOpInfo
    auto dfxOpInfo = std::shared_ptr<Hccl::DfxOpInfo>(new Hccl::DfxOpInfo());
    dfxOpInfo->opIndex_ = 5;
    dfxOpInfo->algTag_ = "test_alg";
    dfxOpInfo->op_.dataCount = 1024;
    
    Hccl::TaskInfo taskInfo(0, 0, 0, Hccl::TaskParam{}, dfxOpInfo);
    std::string result = instance.GetOpDataInfo(taskInfo);
    
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("opIndex"), std::string::npos);
    EXPECT_NE(result.find("algTag"), std::string::npos);
    EXPECT_NE(result.find("count"), std::string::npos);
}

// =============================================================================
// Test cases for Process (hcclCommTaskException.cc)
// =============================================================================

TEST_F(HcclCommTaskExceptionTest, Process_NullExceptionInfo) {
    TaskExceptionHost handler;
    
    // This should not crash with null pointer
    handler.Process(nullptr);
    // No assertion needed, just ensure no crash
}

TEST_F(HcclCommTaskExceptionTest, Process_ValidExceptionInfo) {
    TaskExceptionHost handler;
    rtExceptionInfo_t exceptionInfo;
    memset(&exceptionInfo, 0, sizeof(exceptionInfo));
    exceptionInfo.deviceid = devId_;
    exceptionInfo.streamid = streamId_;
    exceptionInfo.taskid = taskId_;
    
    // This should handle the exception gracefully
    handler.Process(&exceptionInfo);
    // No assertion needed, just ensure no crash
}

// =============================================================================
// Test cases for PrintTaskContextInfo (hcclCommTaskException.cc)
// =============================================================================

TEST_F(HcclCommTaskExceptionTest, HostPrintTaskContextInfo_InvalidDeviceId) {
    TaskExceptionHost handler;
    
    // This should handle invalid device ID gracefully
    handler.PrintTaskContextInfo(999, streamId_, taskId_);
    // No assertion needed, just ensure no crash
}

TEST_F(HcclCommTaskExceptionTest, HostPrintTaskContextInfo_InvalidStreamId) {
    TaskExceptionHost handler;
    
    // This should handle invalid stream ID gracefully
    handler.PrintTaskContextInfo(devId_, 999, taskId_);
    // No assertion needed, just ensure no crash
}

// =============================================================================
// Test cases for FindTaskInfo (global_mirror_tasks.cc)
// =============================================================================

TEST_F(HcclCommTaskExceptionTest, FindTaskInfo_InvalidDevId) {
    GlobalMirrorTasks& instance = GlobalMirrorTasks::Instance();
    std::shared_ptr<TaskInfo> curTask;
    
    HcclResult ret = instance.FindTaskInfo(999, streamId_, taskId_, curTask);
    EXPECT_EQ(ret, HCCL_E_PARA);
    EXPECT_EQ(curTask, nullptr);
}

TEST_F(HcclCommTaskExceptionTest, FindTaskInfo_NonExistentStream) {
    GlobalMirrorTasks& instance = GlobalMirrorTasks::Instance();
    std::shared_ptr<TaskInfo> curTask;
    
    HcclResult ret = instance.FindTaskInfo(devId_, 999, taskId_, curTask);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
    EXPECT_EQ(curTask, nullptr);
}

TEST_F(HcclCommTaskExceptionTest, FindTaskInfo_NonExistentTask) {
    GlobalMirrorTasks& instance = GlobalMirrorTasks::Instance();
    std::shared_ptr<TaskInfo> curTask;
    
    // Try to find a task that doesn't exist
    HcclResult ret = instance.FindTaskInfo(devId_, streamId_, 99999, curTask);
    // Either not found or parameter error is acceptable
    EXPECT_TRUE(ret == HCCL_E_NOT_FOUND || ret == HCCL_E_PARA);
}

// =============================================================================
// Additional edge case tests
// =============================================================================

TEST_F(HcclCommTaskExceptionTest, HcclCommTaskExceptionLite_GetInstance) {
    HcclCommTaskExceptionLite& instance = HcclCommTaskExceptionLite::GetInstance();
    // Should return the same instance
    HcclCommTaskExceptionLite& instance2 = HcclCommTaskExceptionLite::GetInstance();
    EXPECT_EQ(&instance, &instance2);
}

TEST_F(HcclCommTaskExceptionTest, HcclCommTaskExceptionLite_Init) {
    HcclCommTaskExceptionLite& instance = HcclCommTaskExceptionLite::GetInstance();
    instance.Init(devId_);
    
    // Calling Init twice should be safe
    instance.Init(devId_ + 1);
    // No assertion needed, just ensure no crash
}

TEST_F(HcclCommTaskExceptionTest, HcclCommTaskExceptionLite_Call) {
    HcclCommTaskExceptionLite& instance = HcclCommTaskExceptionLite::GetInstance();
    instance.Init(devId_);
    
    // Calling Call should not crash
    instance.Call();
    // No assertion needed, just ensure no crash
}

TEST_F(HcclCommTaskExceptionTest, GlobalMirrorTasks_Instance) {
    GlobalMirrorTasks& instance1 = GlobalMirrorTasks::Instance();
    GlobalMirrorTasks& instance2 = GlobalMirrorTasks::Instance();
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(HcclCommTaskExceptionTest, AicpuIndopProcess_GetCommAll) {
    std::vector<std::pair<std::string, CollCommAicpuMgr *>> aicpuCommInfo;
    HcclResult ret = AicpuIndopProcess::AicpuGetCommAll(aicpuCommInfo);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // The vector may be empty if no comms are initialized
}

TEST_F(HcclCommTaskExceptionTest, AicpuIndopProcess_DestroyCommbyGroup_NonExistent) {
    HcclResult ret = AicpuIndopProcess::AicpuDestroyCommbyGroup("non_existent_group");
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcclCommTaskExceptionTest, AicpuIndopProcess_ReleaseCommMgrbyGroup_NonExistent) {
    // This should not crash
    AicpuIndopProcess::AicpuReleaseCommMgrbyGroup("non_existent_group");
    // No assertion needed, just ensure no crash
}

// =============================================================================
// Test cases with various parameter combinations
// =============================================================================

TEST_F(HcclCommTaskExceptionTest, HcommAcquireComm_EmptyCommId) {
    int32_t ret = HcommAcquireComm("");
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclCommTaskExceptionTest, HcommReleaseComm_EmptyCommId) {
    int32_t ret = HcommReleaseComm("");
    EXPECT_EQ(ret, HCCL_SUCCESS); // Empty string is not null, so should succeed
}

TEST_F(HcclCommTaskExceptionTest, AicpuIndOpNotifyInit_ZeroNotifyNum) {
    NotifyMgrAicpuParam param;
    param.hcomId = commId_;
    param.freeFlag = false;
    param.notifyNum = 0;
    
    HcclResult ret = AicpuIndopProcess::AicpuIndOpNotifyInit(&param);
    // Result depends on comm manager existence
    EXPECT_TRUE(ret == HCCL_SUCCESS || ret == HCCL_E_PTR);
}

TEST_F(HcclCommTaskExceptionTest, TaskExceptionHost_GetBaseInfo) {
    Hccl::TaskInfo taskInfo(streamId_, taskId_, 0, Hccl::TaskParam{}, nullptr);
    std::string baseInfo = taskInfo.GetBaseInfo();
    
    // Should contain stream and task IDs
    EXPECT_FALSE(baseInfo.empty());
}

TEST_F(HcclCommTaskExceptionTest, TaskExceptionHost_GetParaInfo) {
    Hccl::TaskInfo taskInfo(streamId_, taskId_, 0, Hccl::TaskParam{}, nullptr);
    std::string paraInfo = taskInfo.GetParaInfo();
    
    // May be empty for null dfxOpInfo, but should not crash
    // No assertion needed
}

TEST_F(HcclCommTaskExceptionTest, TaskExceptionHost_GetOpInfo) {
    Hccl::TaskInfo taskInfo(streamId_, taskId_, 0, Hccl::TaskParam{}, nullptr);
    std::string opInfo = taskInfo.GetOpInfo();
    
    // May be empty for null dfxOpInfo, but should not crash
    // No assertion needed
}

TEST_F(HcclCommTaskExceptionTest, GetOpDataInfo_MultipleOpTypes) {
    HcclCommTaskExceptionLite& instance = HcclCommTaskExceptionLite::GetInstance();
    
    // Test with different data types
    std::vector<HcclDataType> dataTypes = {
        HcclDataType::HCCL_DATA_TYPE_FP32,
        HcclDataType::HCCL_DATA_TYPE_FP16,
        HcclDataType::HCCL_DATA_TYPE_INT32
    };
    
    for (auto dataType : dataTypes) {
        auto dfxOpInfo = std::shared_ptr<Hccl::DfxOpInfo>(new Hccl::DfxOpInfo());
        dfxOpInfo->opIndex_ = 1;
        dfxOpInfo->algTag_ = "test_alg";
        dfxOpInfo->op_.dataCount = 512;
        dfxOpInfo->op_.dataType = dataType;
        
        Hccl::TaskInfo taskInfo(0, 0, 0, Hccl::TaskParam{}, dfxOpInfo);
        std::string result = instance.GetOpDataInfo(taskInfo);
        
        EXPECT_FALSE(result.empty());
        EXPECT_NE(result.find("dataType"), std::string::npos);
    }
}

TEST_F(HcclCommTaskExceptionTest, GetOpDataInfo_DifferentReduceOps) {
    HcclCommTaskExceptionLite& instance = HcclCommTaskExceptionLite::GetInstance();
    
    // Test with different reduce operations
    std::vector<HcclReduceOp> reduceOps = {
        HcclReduceOp::HCCL_REDUCE_SUM,
        HcclReduceOp::HCCL_REDUCE_MAX,
        HcclReduceOp::HCCL_REDUCE_MIN
    };
    
    for (auto reduceOp : reduceOps) {
        auto dfxOpInfo = std::shared_ptr<Hccl::DfxOpInfo>(new Hccl::DfxOpInfo());
        dfxOpInfo->opIndex_ = 2;
        dfxOpInfo->algTag_ = "test_reduce";
        dfxOpInfo->op_.dataCount = 1024;
        dfxOpInfo->op_.reduceOp = reduceOp;
        
        Hccl::TaskInfo taskInfo(0, 0, 0, Hccl::TaskParam{}, dfxOpInfo);
        std::string result = instance.GetOpDataInfo(taskInfo);
        
        EXPECT_FALSE(result.empty());
        EXPECT_NE(result.find("reduceType"), std::string::npos);
    }
}
