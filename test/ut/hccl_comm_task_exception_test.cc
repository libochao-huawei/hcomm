#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "hccl/hccl.h"
#include "hccl_comm_task_exception.h"
#include "llt_next_orion_stub.h"

using namespace testing;

class HcclCommTaskExceptionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化测试环境
        hccl::HcclCommTaskException::GetInstance().Init();
    }

    void TearDown() override {
        // 清理测试环境
        hccl::HcclCommTaskException::GetInstance().Uninit();
    }
};

// 测试hcclCommTaskExceptionLite.cc中的Init函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionLiteInit) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    EXPECT_EQ(instance.Init(0), HCCL_SUCCESS);
}

// 测试hcclCommTaskExceptionLite.cc中的Call函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionLiteCall) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    instance.Call();
}

// 测试hcclCommTaskExceptionLite.cc中的HandleExceptionCqe函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionLiteHandleExceptionCqe) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    EXPECT_EQ(instance.HandleExceptionCqe(), HCCL_SUCCESS);
}

// 测试hcclCommTaskExceptionLite.cc中的GetThreadCqe函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionLiteGetThreadCqe) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    hccl::Thread *thread = new hccl::Thread();
    rtLogicCqReport_t cqeException;
    dfx::CqeStatus cqeStatus;
    
    EXPECT_EQ(instance.GetThreadCqe(thread, cqeException, cqeStatus), HCCL_SUCCESS);
    delete thread;
}

// 测试hcclCommTaskExceptionLite.cc中的ProcessCqe函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionLiteProcessCqe) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    hccl::CollCommAicpu *aicpuComm = new hccl::CollCommAicpu();
    rtLogicCqReport_t exceptionInfo;
    
    EXPECT_EQ(instance.ProcessCqe(aicpuComm, exceptionInfo), HCCL_SUCCESS);
    delete aicpuComm;
}

// 测试hcclCommTaskExceptionLite.cc中的GenerateErrorMessageReport函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionLiteGenerateErrorMessageReport) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    hccl::CollCommAicpu *aicpuComm = new hccl::CollCommAicpu();
    hccl::TaskInfo taskInfo;
    rtLogicCqReport_t exceptionInfo;
    hccl::HcclCommTaskExceptionLite::ErrorMessageReport errMsgInfo;
    
    EXPECT_EQ(instance.GenerateErrorMessageReport(aicpuComm, taskInfo, exceptionInfo, errMsgInfo), HCCL_SUCCESS);
    delete aicpuComm;
}

// 测试hcclCommTaskExceptionLite.cc中的GetErrMsgInfo函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionLiteGetErrMsgInfo) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    hccl::TaskInfo taskInfo;
    rtLogicCqReport_t exceptionInfo;
    hccl::HcclCommTaskExceptionLite::ErrorMessageReport errMsgInfo;
    
    instance.GetErrMsgInfo(taskInfo, errMsgInfo, exceptionInfo);
}

// 测试hcclCommTaskExceptionLite.cc中的SendTaskExceptionByMBox函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionLiteSendTaskExceptionByMBox) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    u32 notifyId = 123;
    u32 tsId = 1;
    rtLogicCqReport_t exceptionInfo;
    
    EXPECT_EQ(instance.SendTaskExceptionByMBox(notifyId, tsId, exceptionInfo), HCCL_SUCCESS);
}

// 测试hcclCommTaskExceptionLite.cc中的SwitchUBCqeErrCodeToTsErrCode函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionLiteSwitchUBCqeErrCodeToTsErrCode) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    
    uint16_t ubErrorCode = instance.SwitchUBCqeErrCodeToTsErrCode(RT_UB_LOCAL_OPERATIOINERR);
    EXPECT_EQ(ubErrorCode, TS_ERROR_LOCAL_MEM_ERROR);
    
    ubErrorCode = instance.SwitchUBCqeErrCodeToTsErrCode(RT_UB_REMOTE_OPERATIOINERR);
    EXPECT_EQ(ubErrorCode, TS_ERROR_REMOTE_MEM_ERROR);
}

// 测试hcclCommTaskExceptionLite.cc中的SwitchSdmaCqeErrCodeToTsErrCode函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionLiteSwitchSdmaCqeErrCodeToTsErrCode) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    
    uint16_t sdmaErrorCode = instance.SwitchSdmaCqeErrCodeToTsErrCode(RT_SDMA_COMPERR);
    EXPECT_EQ(sdmaErrorCode, TS_ERROR_SDMA_LINK_ERROR);
    
    sdmaErrorCode = instance.SwitchSdmaCqeErrCodeToTsErrCode(RT_SDMA_COMPDATAERR);
    EXPECT_EQ(sdmaErrorCode, TS_ERROR_SDMA_POISON_ERROR);
    
    sdmaErrorCode = instance.SwitchSdmaCqeErrCodeToTsErrCode(RT_SDMA_DATAERR);
    EXPECT_EQ(sdmaErrorCode, TS_ERROR_SDMA_DDRC_ERROR);
}

// 测试hcclCommTaskExceptionLite.cc中的PrintTaskContextInfo函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionLitePrintTaskContextInfo) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    u32 sqId = 1;
    u32 taskId = 123;
    
    EXPECT_EQ(instance.PrintTaskContextInfo(sqId, taskId), HCCL_SUCCESS);
}

// 测试hcclCommTaskExceptionLite.cc中的GetBaseInfo函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionLiteGetBaseInfo) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    hccl::TaskInfo taskInfo;
    taskInfo.streamId_ = 1;
    taskInfo.taskId_ = 123;
    
    std::string baseInfo = instance.GetBaseInfo(taskInfo);
    EXPECT_FALSE(baseInfo.empty());
}

// 测试hcclCommTaskExceptionLite.cc中的GetGroupInfo函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionLiteGetGroupInfo) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    hccl::TaskInfo taskInfo;
    taskInfo.streamId_ = 1;
    taskInfo.taskId_ = 123;
    
    std::string groupInfo = instance.GetGroupInfo(taskInfo);
    EXPECT_FALSE(groupInfo.empty());
}

// 测试hcclCommTaskExceptionLite.cc中的GetOpDataInfo函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionLiteGetOpDataInfo) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    hccl::TaskInfo taskInfo;
    taskInfo.streamId_ = 1;
    taskInfo.taskId_ = 123;
    
    std::string opDataInfo = instance.GetOpDataInfo(taskInfo);
    EXPECT_FALSE(opDataInfo.empty());
}

// 测试hcclCommTaskExceptionLite.cc中的PrintEid函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionLitePrintEid) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    hccl::TaskInfo taskInfo;
    
    instance.PrintEid(taskInfo);
}

// 测试hcclCommTaskException.cc中的Register函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionRegister) {
    hccl::TaskExceptionHost hostHandler;
    EXPECT_EQ(hostHandler.Register(), HCCL_SUCCESS);
}

// 测试hcclCommTaskException.cc中的UnRegister函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionUnRegister) {
    hccl::TaskExceptionHost hostHandler;
    hostHandler.Register(); // 先注册
    EXPECT_EQ(hostHandler.UnRegister(), HCCL_SUCCESS);
}

// 测试hcclCommTaskException.cc中的Process函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionProcess) {
    hccl::TaskExceptionHost hostHandler;
    rtExceptionInfo_t exceptionInfo;
    hccl::TaskInfo taskInfo;
    
    hostHandler.Process(&exceptionInfo);
}

// 测试hcclCommTaskException.cc中的GetGroupRankInfo函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionGetGroupRankInfo) {
    hccl::TaskExceptionHost hostHandler;
    hccl::TaskInfo taskInfo;
    
    std::string groupRankInfo = hostHandler.GetGroupRankInfo(taskInfo);
    EXPECT_FALSE(groupRankInfo.empty());
}

// 测试hcclCommTaskException.cc中的ProcessException函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionProcessException) {
    hccl::TaskExceptionHost hostHandler;
    rtExceptionInfo_t exceptionInfo;
    hccl::TaskInfo taskInfo;
    
    hostHandler.ProcessException(&exceptionInfo, taskInfo);
}

// 测试hcclCommTaskException.cc中的PrintTaskContextInfo函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionPrintTaskContextInfo) {
    hccl::TaskExceptionHost hostHandler;
    u32 deviceId = 1;
    u32 streamId = 1;
    u32 taskId = 123;
    
    hostHandler.PrintTaskContextInfo(deviceId, streamId, taskId);
}

// 测试hcclCommTaskException.cc中的PrintAicpuErrorMessage函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionPrintAicpuErrorMessage) {
    hccl::TaskExceptionHost hostHandler;
    rtExceptionInfo_t exceptionInfo;
    hccl::ErrorMessageReport errorMessage;
    
    hostHandler.PrintAicpuErrorMessage(&exceptionInfo);
}

// 测试hcclCommTaskException.cc中的GetReduceOpEnumStr2函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionGetReduceOpEnumStr2) {
    hccl::TaskExceptionHost hostHandler;
    
    std::string reduceOpStr = hostHandler.GetReduceOpEnumStr2(hccl::HcclReduceOp::HCCL_REDUCE_SUM);
    EXPECT_EQ(reduceOpStr, "sum");
    
    reduceOpStr = hostHandler.GetReduceOpEnumStr2(hccl::HcclReduceOp::HCCL_REDUCE_PROD);
    EXPECT_EQ(reduceOpStr, "prod");
    
    reduceOpStr = hostHandler.GetReduceOpEnumStr2(hccl::HcclReduceOp::HCCL_REDUCE_MAX);
    EXPECT_EQ(reduceOpStr, "max");
    
    reduceOpStr = hostHandler.GetReduceOpEnumStr2(hccl::HcclReduceOp::HCCL_REDUCE_MIN);
    EXPECT_EQ(reduceOpStr, "min");
}

// 测试hcclCommTaskException.cc中的GetDataTypeEnumStr2函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionGetDataTypeEnumStr2) {
    hccl::TaskExceptionHost hostHandler;
    
    std::string dataTypeStr = hostHandler.GetDataTypeEnumStr2(hccl::HcclDataType::HCCL_DATA_TYPE_INT8);
    EXPECT_EQ(dataTypeStr, "int8");
    
    dataTypeStr = hostHandler.GetDataTypeEnumStr2(hccl::HcclDataType::HCCL_DATA_TYPE_INT16);
    EXPECT_EQ(dataTypeStr, "int16");
    
    dataTypeStr = hostHandler.GetDataTypeEnumStr2(hccl::HcclDataType::HCCL_DATA_TYPE_INT32);
    EXPECT_EQ(dataTypeStr, "int32");
    
    dataTypeStr = hostHandler.GetDataTypeEnumStr2(hccl::HcclDataType::HCCL_DATA_TYPE_INT64);
    EXPECT_EQ(dataTypeStr, "int64");
    
    dataTypeStr = hostHandler.GetDataTypeEnumStr2(hccl::HcclDataType::HCCL_DATA_TYPE_FP16);
    EXPECT_EQ(dataTypeStr, "float16");
}

// 测试hcclCommTaskException.cc中的GetOpTypeEnumStr函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionGetOpTypeEnumStr) {
    hccl::TaskExceptionHost hostHandler;
    
    std::string opTypeStr = hostHandler.GetOpTypeEnumStr(static_cast<u32>(hccl::OpType::ALL_REDUCE));
    EXPECT_FALSE(opTypeStr.empty());
}

// 测试hcclCommTaskException.cc中的PrintGroupErrorMessage函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionPrintGroupErrorMessage) {
    hccl::TaskExceptionHost hostHandler;
    hccl::ErrorMessageReport errorMessage;
    hccl::TaskInfo exceptionTaskInfo;
    std::string groupRankContent;
    std::string stageErrInfo = "[TaskException][AICPU]";
    
    hostHandler.PrintGroupErrorMessage(errorMessage, exceptionTaskInfo, groupRankContent, stageErrInfo);
}

// 测试hcclCommTaskException.cc中的PrintOpDataErrorMessage函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionPrintOpDataErrorMessage) {
    hccl::TaskExceptionHost hostHandler;
    u32 deviceId = 1;
    hccl::ErrorMessageReport errorMessage;
    std::string stageErrInfo = "[TaskException][AICPU]";
    
    hostHandler.PrintOpDataErrorMessage(deviceId, errorMessage, stageErrInfo);
}

// 测试hcclCommTaskException.cc中的ReportErrorMsg函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionReportErrorMsg) {
    hccl::TaskExceptionHost hostHandler;
    hccl::TaskInfo exceptionTaskInfo;
    std::string groupRankContent;
    hccl::ErrorMessageReport errorMessage;
    rtExceptionInfo_t exceptionInfo;
    
    hostHandler.ReportErrorMsg(exceptionTaskInfo, groupRankContent, errorMessage, &exceptionInfo);
}

// 测试hcclCommTaskException.cc中的GetTaskParam函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionGetTaskParam) {
    hccl::TaskExceptionHost hostHandler;
    hccl::ErrorMessageReport errorMessage;
    hccl::TaskParam taskParam;
    
    hostHandler.GetTaskParam(taskParam, errorMessage);
}

// 测试hcclCommTaskException.cc中的PrintUbRegisters函数
TEST_F(HcclCommTaskExceptionTest, TestHcclCommTaskExceptionPrintUbRegisters) {
    hccl::TaskExceptionHost hostHandler;
    s32 devLogicId = 0;
    hccl::RdmaHandle rdmaHandle = (hccl::RdmaHandle)0x12345678;
    
    EXPECT_EQ(hostHandler.PrintUbRegisters(devLogicId, rdmaHandle), HCCL_SUCCESS);
}

// 测试TaskExceptionHostManager的GetHandler功能
TEST_F(HcclCommTaskExceptionTest, TestTaskExceptionHostManagerGetHandler) {
    hccl::TaskExceptionHostManager manager;
    size_t devId = 0;
    
    hccl::TaskExceptionHost *handler = manager.GetHandler(devId);
    EXPECT_NE(handler, nullptr);
}

// 测试RegisterGetAicpuTaskExceptionCallBack功能
TEST_F(HcclCommTaskExceptionTest, TestRegisterGetAicpuTaskExceptionCallBack) {
    hccl::TaskExceptionHostManager manager;
    s32 streamId = 1;
    u32 deviceLogicId = 0;
    hccl::GetAicpuTaskExceptionCallBackHcomm callback = [](hccl::Hccl::ErrorMessageReport& report) {
        return report;
    };
    
    manager.RegisterGetAicpuTaskExceptionCallBack(streamId, deviceLogicId, callback);
}

// 测试通信任务异常处理的基本功能
TEST_F(HcclCommTaskExceptionTest, TestHandleTaskException) {
    hccl::TaskExceptionInfo testInfo;
    testInfo.taskId = 123;
    testInfo.exceptionType = hccl::TASK_EXCEPTION_TYPE_TIMEOUT;
    testInfo.errorMessage = "Test exception";
    
    // 注册回调
    hccl::HcclCommTaskException::TaskExceptionCallback callback = [](hccl::TaskExceptionInfo& info) {
        EXPECT_EQ(info.taskId, 123);
        EXPECT_EQ(info.exceptionType, hccl::TASK_EXCEPTION_TYPE_TIMEOUT);
        EXPECT_EQ(info.errorMessage, "Test exception");
        return HCCL_SUCCESS;
    };
    
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().RegisterTaskExceptionCallback(callback), HCCL_SUCCESS);
    
    // 触发异常处理
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().HandleTaskException(testInfo), HCCL_SUCCESS);
    
    // 注销回调
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().UnregisterTaskExceptionCallback(), HCCL_SUCCESS);
}

// 测试多线程环境下的异常处理
TEST_F(HcclCommTaskExceptionTest, TestMultiThreadExceptionHandling) {
    const int threadCount = 5;
    std::vector<std::thread> threads;
    
    // 注册回调
    hccl::HcclCommTaskException::TaskExceptionCallback callback = [](hccl::TaskExceptionInfo& info) {
        return HCCL_SUCCESS;
    };
    
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().RegisterTaskExceptionCallback(callback), HCCL_SUCCESS);
    
    // 创建多个线程并发触发异常处理
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([i]() {
            hccl::TaskExceptionInfo info;
            info.taskId = i;
            info.exceptionType = hccl::TASK_EXCEPTION_TYPE_TIMEOUT;
            info.errorMessage = "Thread " + std::to_string(i) + " exception";
            
            hccl::HcclCommTaskException::GetInstance().HandleTaskException(info);
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 注销回调
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().UnregisterTaskExceptionCallback(), HCCL_SUCCESS);
}

// 测试不同类型的通信异常
TEST_F(HcclCommTaskExceptionTest, TestDifferentExceptionTypes) {
    hccl::HcclCommTaskException::TaskExceptionCallback callback = [](hccl::TaskExceptionInfo& info) {
        EXPECT_TRUE(info.exceptionType >= hccl::TASK_EXCEPTION_TYPE_TIMEOUT && 
                   info.exceptionType <= hccl::TASK_EXCEPTION_TYPE_MAX);
        return HCCL_SUCCESS;
    };
    
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().RegisterTaskExceptionCallback(callback), HCCL_SUCCESS);
    
    // 测试各种异常类型
    for (int type = hccl::TASK_EXCEPTION_TYPE_TIMEOUT; type < hccl::TASK_EXCEPTION_TYPE_MAX; ++type) {
        hccl::TaskExceptionInfo info;
        info.taskId = 100 + type;
        info.exceptionType = static_cast<hccl::TaskExceptionType>(type);
        info.errorMessage = "Exception type " + std::to_string(type);
        
        EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().HandleTaskException(info), HCCL_SUCCESS);
    }
    
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().UnregisterTaskExceptionCallback(), HCCL_SUCCESS);
}

// 测试异常回调返回错误码的处理
TEST_F(HcclCommTaskExceptionTest, TestCallbackWithError) {
    hccl::HcclCommTaskException::TaskExceptionCallback callback = [](hccl::TaskExceptionInfo& info) {
        return HCCL_ERROR;
    };
    
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().RegisterTaskExceptionCallback(callback), HCCL_SUCCESS);
    
    hccl::TaskExceptionInfo info;
    info.taskId = 999;
    info.exceptionType = hccl::TASK_EXCEPTION_TYPE_TIMEOUT;
    info.errorMessage = "Error callback test";
    
    // 验证即使回调返回错误，HandleTaskException仍应返回成功
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().HandleTaskException(info), HCCL_SUCCESS);
    
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().UnregisterTaskExceptionCallback(), HCCL_SUCCESS);
}

// 测试重复注册回调
TEST_F(HcclCommTaskExceptionTest, TestDuplicateRegisterCallback) {
    hccl::HcclCommTaskException::TaskExceptionCallback callback1 = [](hccl::TaskExceptionInfo& info) {
        return HCCL_SUCCESS;
    };
    
    hccl::HcclCommTaskException::TaskExceptionCallback callback2 = [](hccl::TaskExceptionInfo& info) {
        return HCCL_SUCCESS;
    };
    
    // 第一次注册成功
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().RegisterTaskExceptionCallback(callback1), HCCL_SUCCESS);
    
    // 第二次注册应失败（假设只允许一个回调）
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().RegisterTaskExceptionCallback(callback2), HCCL_ERROR);
    
    // 注销回调
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().UnregisterTaskExceptionCallback(), HCCL_SUCCESS);
}

// 测试未注册回调时的异常处理
TEST_F(HcclCommTaskExceptionTest, TestHandleExceptionWithoutCallback) {
    hccl::TaskExceptionInfo info;
    info.taskId = 888;
    info.exceptionType = hccl::TASK_EXCEPTION_TYPE_TIMEOUT;
    info.errorMessage = "No callback test";
    
    // 未注册回调时，HandleTaskException应返回成功
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().HandleTaskException(info), HCCL_SUCCESS);
}

// 测试边界条件和无效输入
TEST_F(HcclCommTaskExceptionTest, TestBoundaryConditions) {
    // 测试无效的任务ID
    hccl::TaskExceptionInfo invalidInfo;
    invalidInfo.taskId = -1;
    invalidInfo.exceptionType = hccl::TASK_EXCEPTION_TYPE_TIMEOUT;
    invalidInfo.errorMessage = "Invalid task ID";
    
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().HandleTaskException(invalidInfo), HCCL_SUCCESS);
    
    // 测试无效的异常类型
    hccl::TaskExceptionInfo invalidTypeInfo;
    invalidTypeInfo.taskId = 999;
    invalidTypeInfo.exceptionType = static_cast<hccl::TaskExceptionType>(-1);
    invalidTypeInfo.errorMessage = "Invalid exception type";
    
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().HandleTaskException(invalidTypeInfo), HCCL_SUCCESS);
}

// 测试性能和压力测试
TEST_F(HcclCommTaskExceptionTest, TestPerformanceStressTest) {
    const int iterations = 1000;
    hccl::HcclCommTaskException::TaskExceptionCallback callback = [](hccl::TaskExceptionInfo& info) {
        return HCCL_SUCCESS;
    };
    
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().RegisterTaskExceptionCallback(callback), HCCL_SUCCESS);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 高频触发异常处理
    for (int i = 0; i < iterations; ++i) {
        hccl::TaskExceptionInfo info;
        info.taskId = i;
        info.exceptionType = hccl::TASK_EXCEPTION_TYPE_TIMEOUT;
        info.errorMessage = "Stress test " + std::to_string(i);
        
        EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().HandleTaskException(info), HCCL_SUCCESS);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // 确保处理时间在合理范围内
    EXPECT_LT(duration.count(), 1000); // 1秒内处理1000次异常
    
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().UnregisterTaskExceptionCallback(), HCCL_SUCCESS);
}