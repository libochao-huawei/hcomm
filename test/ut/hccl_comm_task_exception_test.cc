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

// 测试通信异常处理的初始化和反初始化
TEST_F(HcclCommTaskExceptionTest, InitAndUninit) {
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().Init(), HCCL_SUCCESS);
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().Uninit(), HCCL_SUCCESS);
}

// 测试注册和注销通信任务异常处理回调
TEST_F(HcclCommTaskExceptionTest, RegisterAndUnregisterCallback) {
    hccl::HcclCommTaskException::TaskExceptionCallback callback = [](hccl::TaskExceptionInfo& info) {
        return HCCL_SUCCESS;
    };
    
    // 注册回调
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().RegisterTaskExceptionCallback(callback), HCCL_SUCCESS);
    
    // 注销回调
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().UnregisterTaskExceptionCallback(), HCCL_SUCCESS);
}

// 测试CQE获取功能
TEST_F(HcclCommTaskExceptionTest, GetThreadCqe) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    hccl::Thread *thread = new hccl::Thread();
    rtLogicCqReport_t cqeException;
    dfx::CqeStatus cqeStatus;
    
    // 测试GetThreadCqe功能
    EXPECT_EQ(instance.GetThreadCqe(thread, cqeException, cqeStatus), HCCL_SUCCESS);
    delete thread;
}

// 测试错误码转换功能
TEST_F(HcclCommTaskExceptionTest, ErrorCodeConversion) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    
    // 测试UB错误码转换
    uint16_t ubErrorCode = instance.SwitchUBCqeErrCodeToTsErrCode(RT_UB_LOCAL_OPERATIOINERR);
    EXPECT_EQ(ubErrorCode, TS_ERROR_LOCAL_MEM_ERROR);
    
    ubErrorCode = instance.SwitchUBCqeErrCodeToTsErrCode(RT_UB_REMOTE_OPERATIOINERR);
    EXPECT_EQ(ubErrorCode, TS_ERROR_REMOTE_MEM_ERROR);
    
    // 测试SDMA错误码转换
    uint16_t sdmaErrorCode = instance.SwitchSdmaCqeErrCodeToTsErrCode(RT_SDMA_COMPERR);
    EXPECT_EQ(sdmaErrorCode, TS_ERROR_SDMA_LINK_ERROR);
    
    sdmaErrorCode = instance.SwitchSdmaCqeErrCodeToTsErrCode(RT_SDMA_COMPDATAERR);
    EXPECT_EQ(sdmaErrorCode, TS_ERROR_SDMA_POISON_ERROR);
    
    sdmaErrorCode = instance.SwitchSdmaCqeErrCodeToTsErrCode(RT_SDMA_DATAERR);
    EXPECT_EQ(sdmaErrorCode, TS_ERROR_SDMA_DDRC_ERROR);
}

// 测试任务上下文打印功能
TEST_F(HcclCommTaskExceptionTest, PrintTaskContextInfo) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    u32 sqId = 1;
    u32 taskId = 123;
    
    // 测试PrintTaskContextInfo功能
    EXPECT_EQ(instance.PrintTaskContextInfo(sqId, taskId), HCCL_SUCCESS);
}

// 测试信息获取功能
TEST_F(HcclCommTaskExceptionTest, GetInformation) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    hccl::TaskInfo taskInfo;
    taskInfo.streamId_ = 1;
    taskInfo.taskId_ = 123;
    
    // 测试GetBaseInfo
    std::string baseInfo = instance.GetBaseInfo(taskInfo);
    EXPECT_FALSE(baseInfo.empty());
    
    // 测试GetGroupInfo
    std::string groupInfo = instance.GetGroupInfo(taskInfo);
    EXPECT_FALSE(groupInfo.empty());
    
    // 测试GetOpDataInfo
    std::string opDataInfo = instance.GetOpDataInfo(taskInfo);
    EXPECT_FALSE(opDataInfo.empty());
}

// 测试错误信息生成功能
TEST_F(HcclCommTaskExceptionTest, GenerateErrorMessageReport) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    hccl::CollCommAicpu *aicpuComm = new hccl::CollCommAicpu();
    hccl::TaskInfo taskInfo;
    rtLogicCqReport_t exceptionInfo;
    
    hccl::HcclCommTaskExceptionLite::ErrorMessageReport errMsgInfo;
    
    // 测试GenerateErrorMessageReport
    EXPECT_EQ(instance.GenerateErrorMessageReport(aicpuComm, taskInfo, exceptionInfo, errMsgInfo), HCCL_SUCCESS);
    delete aicpuComm;
}

// 测试Mbox发送功能
TEST_F(HcclCommTaskExceptionTest, SendTaskExceptionByMBox) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    u32 notifyId = 123;
    u32 tsId = 1;
    rtLogicCqReport_t exceptionInfo;
    
    // 测试SendTaskExceptionByMBox
    EXPECT_EQ(instance.SendTaskExceptionByMBox(notifyId, tsId, exceptionInfo), HCCL_SUCCESS);
}

// 测试PrintEid功能
TEST_F(HcclCommTaskExceptionTest, PrintEid) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    hccl::TaskInfo taskInfo;
    
    // 测试PrintEid
    instance.PrintEid(taskInfo);
}

// 测试GetErrMsgInfo功能
TEST_F(HcclCommTaskExceptionTest, GetErrMsgInfo) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    hccl::TaskInfo taskInfo;
    rtLogicCqReport_t exceptionInfo;
    hccl::HcclCommTaskExceptionLite::ErrorMessageReport errMsgInfo;
    
    // 测试GetErrMsgInfo
    instance.GetErrMsgInfo(taskInfo, errMsgInfo, exceptionInfo);
}

// 测试HandleExceptionCqe功能
TEST_F(HcclCommTaskExceptionTest, HandleExceptionCqe) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    
    // 测试HandleExceptionCqe
    EXPECT_EQ(instance.HandleExceptionCqe(), HCCL_SUCCESS);
}

// 测试ProcessCqe功能
TEST_F(HcclCommTaskExceptionTest, ProcessCqe) {
    hccl::HcclCommTaskExceptionLite &instance = hccl::HcclCommTaskExceptionLite::GetInstance();
    hccl::CollCommAicpu *aicpuComm = new hccl::CollCommAicpu();
    rtLogicCqReport_t exceptionInfo;
    
    // 测试ProcessCqe
    EXPECT_EQ(instance.ProcessCqe(aicpuComm, exceptionInfo), HCCL_SUCCESS);
    delete aicpuComm;
}

// 测试通信任务异常处理
TEST_F(HcclCommTaskExceptionTest, HandleTaskException) {
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

// 测试TaskExceptionHost的注册和反注册
TEST_F(HcclCommTaskExceptionTest, TaskExceptionHostRegisterUnregister) {
    hccl::TaskExceptionHost hostHandler;
    
    // 测试注册
    EXPECT_EQ(hostHandler.Register(), HCCL_SUCCESS);
    
    // 测试反注册
    EXPECT_EQ(hostHandler.UnRegister(), HCCL_SUCCESS);
}

// 测试ProcessException功能
TEST_F(HcclCommTaskExceptionTest, ProcessException) {
    hccl::TaskExceptionHost hostHandler;
    rtExceptionInfo_t exceptionInfo;
    hccl::TaskInfo taskInfo;
    
    // 测试ProcessException
    hostHandler.ProcessException(&exceptionInfo, taskInfo);
}

// 测试PrintTaskContextInfo功能
TEST_F(HcclCommTaskExceptionTest, PrintTaskContextInfoHost) {
    hccl::TaskExceptionHost hostHandler;
    u32 deviceId = 1;
    u32 streamId = 1;
    u32 taskId = 123;
    
    // 测试PrintTaskContextInfo
    hostHandler.PrintTaskContextInfo(deviceId, streamId, taskId);
}

// 测试PrintAicpuErrorMessage功能
TEST_F(HcclCommTaskExceptionTest, PrintAicpuErrorMessage) {
    hccl::TaskExceptionHost hostHandler;
    rtExceptionInfo_t exceptionInfo;
    hccl::ErrorMessageReport errorMessage;
    
    // 测试PrintAicpuErrorMessage
    hostHandler.PrintAicpuErrorMessage(&exceptionInfo);
}

// 测试GetGroupRankInfo功能
TEST_F(HcclCommTaskExceptionTest, GetGroupRankInfo) {
    hccl::TaskExceptionHost hostHandler;
    hccl::TaskInfo taskInfo;
    
    // 测试GetGroupRankInfo
    std::string groupRankInfo = hostHandler.GetGroupRankInfo(taskInfo);
    EXPECT_FALSE(groupRankInfo.empty());
}

// 测试PrintGroupErrorMessage功能
TEST_F(HcclCommTaskExceptionTest, PrintGroupErrorMessage) {
    hccl::TaskExceptionHost hostHandler;
    hccl::ErrorMessageReport errorMessage;
    hccl::TaskInfo exceptionTaskInfo;
    std::string groupRankContent;
    std::string stageErrInfo = "[TaskException][AICPU]";
    
    // 测试PrintGroupErrorMessage
    hostHandler.PrintGroupErrorMessage(errorMessage, exceptionTaskInfo, groupRankContent, stageErrInfo);
}

// 测试PrintOpDataErrorMessage功能
TEST_F(HcclCommTaskExceptionTest, PrintOpDataErrorMessage) {
    hccl::TaskExceptionHost hostHandler;
    u32 deviceId = 1;
    hccl::ErrorMessageReport errorMessage;
    std::string stageErrInfo = "[TaskException][AICPU]";
    
    // 测试PrintOpDataErrorMessage
    hostHandler.PrintOpDataErrorMessage(deviceId, errorMessage, stageErrInfo);
}

// 测试GetReduceOpEnumStr2功能
TEST_F(HcclCommTaskExceptionTest, GetReduceOpEnumStr2) {
    hccl::TaskExceptionHost hostHandler;
    
    // 测试GetReduceOpEnumStr2
    std::string reduceOpStr = hostHandler.GetReduceOpEnumStr2(hccl::HcclReduceOp::HCCL_REDUCE_SUM);
    EXPECT_EQ(reduceOpStr, "sum");
    
    reduceOpStr = hostHandler.GetReduceOpEnumStr2(hccl::HcclReduceOp::HCCL_REDUCE_PROD);
    EXPECT_EQ(reduceOpStr, "prod");
    
    reduceOpStr = hostHandler.GetReduceOpEnumStr2(hccl::HcclReduceOp::HCCL_REDUCE_MAX);
    EXPECT_EQ(reduceOpStr, "max");
    
    reduceOpStr = hostHandler.GetReduceOpEnumStr2(hccl::HcclReduceOp::HCCL_REDUCE_MIN);
    EXPECT_EQ(reduceOpStr, "min");
}

// 测试GetDataTypeEnumStr2功能
TEST_F(HcclCommTaskExceptionTest, GetDataTypeEnumStr2) {
    hccl::TaskExceptionHost hostHandler;
    
    // 测试GetDataTypeEnumStr2
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

// 测试GetOpTypeEnumStr功能
TEST_F(HcclCommTaskExceptionTest, GetOpTypeEnumStr) {
    hccl::TaskExceptionHost hostHandler;
    
    // 测试GetOpTypeEnumStr
    std::string opTypeStr = hostHandler.GetOpTypeEnumStr(static_cast<u32>(hccl::OpType::ALL_REDUCE));
    EXPECT_FALSE(opTypeStr.empty());
}

// 测试ReportErrorMsg功能
TEST_F(HcclCommTaskExceptionTest, ReportErrorMsg) {
    hccl::TaskExceptionHost hostHandler;
    hccl::TaskInfo exceptionTaskInfo;
    std::string groupRankContent;
    hccl::ErrorMessageReport errorMessage;
    rtExceptionInfo_t exceptionInfo;
    
    // 测试ReportErrorMsg
    hostHandler.ReportErrorMsg(exceptionTaskInfo, groupRankContent, errorMessage, &exceptionInfo);
}

// 测试GetTaskParam功能
TEST_F(HcclCommTaskExceptionTest, GetTaskParam) {
    hccl::TaskExceptionHost hostHandler;
    hccl::ErrorMessageReport errorMessage;
    hccl::TaskParam taskParam;
    
    // 测试GetTaskParam
    hostHandler.GetTaskParam(taskParam, errorMessage);
}

// 测试TaskExceptionHostManager的GetHandler功能
TEST_F(HcclCommTaskExceptionTest, TaskExceptionHostManagerGetHandler) {
    hccl::TaskExceptionHostManager manager;
    size_t devId = 0;
    
    // 测试GetHandler
    hccl::TaskExceptionHost *handler = manager.GetHandler(devId);
    EXPECT_NE(handler, nullptr);
}

// 测试RegisterGetAicpuTaskExceptionCallBack功能
TEST_F(HcclCommTaskExceptionTest, RegisterGetAicpuTaskExceptionCallBack) {
    hccl::TaskExceptionHostManager manager;
    s32 streamId = 1;
    u32 deviceLogicId = 0;
    hccl::GetAicpuTaskExceptionCallBackHcomm callback = [](hccl::Hccl::ErrorMessageReport& report) {
        return report;
    };
    
    // 测试RegisterGetAicpuTaskExceptionCallBack
    manager.RegisterGetAicpuTaskExceptionCallBack(streamId, deviceLogicId, callback);
}

// 测试PrintUbRegisters功能
TEST_F(HcclCommTaskExceptionTest, PrintUbRegisters) {
    hccl::TaskExceptionHost hostHandler;
    s32 devLogicId = 0;
    hccl::RdmaHandle rdmaHandle = (hccl::RdmaHandle)0x12345678;
    
    // 测试PrintUbRegisters
    EXPECT_EQ(hostHandler.PrintUbRegisters(devLogicId, rdmaHandle), HCCL_SUCCESS);
}

// 测试多线程环境下的异常处理
TEST_F(HcclCommTaskExceptionTest, MultiThreadExceptionHandling) {
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
TEST_F(HcclCommTaskExceptionTest, DifferentExceptionTypes) {
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
TEST_F(HcclCommTaskExceptionTest, CallbackWithError) {
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
TEST_F(HcclCommTaskExceptionTest, DuplicateRegisterCallback) {
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
TEST_F(HcclCommTaskExceptionTest, HandleExceptionWithoutCallback) {
    hccl::TaskExceptionInfo info;
    info.taskId = 888;
    info.exceptionType = hccl::TASK_EXCEPTION_TYPE_TIMEOUT;
    info.errorMessage = "No callback test";
    
    // 未注册回调时，HandleTaskException应返回成功
    EXPECT_EQ(hccl::HcclCommTaskException::GetInstance().HandleTaskException(info), HCCL_SUCCESS);
}

// 测试边界条件和无效输入
TEST_F(HcclCommTaskExceptionTest, BoundaryConditions) {
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
TEST_F(HcclCommTaskExceptionTest, PerformanceStressTest) {
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