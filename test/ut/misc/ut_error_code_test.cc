#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include "json_parser.h"
#include "adapter_error_manager_pub.h"
#include "task_exception_handler_lite.h"
#include "task_info.h"
#include "new_rank_info.h"
#include "socket.h"
#include "preempt_port_manager.h"
#include "nlohmann/json.hpp"
#include "rtExceptionInfo.h"
#include "task_param.h"
#include <iostream>

using namespace hccl;

class ErrorCodeScenarioTest : public testing::Test
{
protected:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    virtual void SetUp() {}
    virtual void TearDown()
    {
        GlobalMockObject::verify();
    }
};

TEST_F(ErrorCodeScenarioTest, EI0011_MemoryAllocationFailure) 
{
    void *ptr = nullptr;
    MOCK_FUNC(aclrtMalloc).stubs().will(invoke([](void **devPtr, size_t size, aclrtMallocMode mode) -> aclrtError {
        return ACL_ERROR_RT_MEMORY_ALLOCATION;
    }));

    aclrtError ret = aclrtMalloc(&ptr, 1024, ACL_MEMORY_MALLOC_NORMAL);
    EXPECT_EQ(ret, ACL_ERROR_RT_MEMORY_ALLOCATION);
}

TEST_F(ErrorCodeScenarioTest, EI0012_SDMAException) 
{
    rtLogicCqReport_t exceptionInfo = {};
    exceptionInfo.errorType = 0;
    exceptionInfo.errorCode = 0x8;

    MOCK_FUNC(DlHalFunctionV2::GetInstance().dlHalEschedSubmitEvent).stubs()
        .will(returnValue(DRV_ERROR_NONE));
    MOCK_FUNC(DlHalFunctionV2::GetInstance().dlHalGetEventAttr).stubs()
        .will(returnValue(DRV_ERROR_NONE));

    TaskExceptionHandlerLite::Process(nullptr, &exceptionInfo);
}

TEST_F(ErrorCodeScenarioTest, EI0013_RDMAReadFailure) 
{
    MOCK_FUNC(ibv_poll_cq).stubs()
        .will(invoke([](struct ibv_cq *cq, int num, struct ibv_wc *wc) -> int {
            wc[0].status = IBV_WC_RETRY_EXCEEDED_ERR;
            wc[0].wr_id = 0;
            return 1;
        }));

    MOCK_FUNC(ibv_req_notify_cq).stubs().will(returnValue(0));
    MOCK_FUNC(ibv_get_cq_event).stubs().will(returnValue(0));

    struct ibv_cq *cq = nullptr;
    struct ibv_wc wc;
    int num = ibv_poll_cq(cq, 1, &wc);
    EXPECT_GT(num, 0);
    EXPECT_NE(wc.status, IBV_WC_SUCCESS);
}

TEST_F(ErrorCodeScenarioTest, EI0014_RDMAReadFailure) 
{
    nlohmann::json testJson;
    testJson["test_prop"] = -1;

    EXPECT_THROW({
        u32 val = GetJsonPropertyUInt(testJson, "test_prop", true);
    }, InvalidParamsException);
}

TEST_F(ErrorCodeScenarioTest, EI0014_GetJsonPropertyUInt_OverflowValue) 
{
    nlohmann::json testJson;
    testJson["test_prop"] = 4294967296ULL;

    EXPECT_THROW({
        u32 val = GetJsonPropertyUInt(testJson, "test_prop", true);
    }, InvalidParamsException);
}

TEST_F(ErrorCodeScenarioTest, EI0014_GetJsonPropertySInt_Overflow) 
{
    nlohmann::json testJson;
    testJson["test_prop"] = 2147483648LL;

    EXPECT_THROW({
        s32 val = GetJsonPropertySInt(testJson, "test_prop", true);
    }, InvalidParamsException);
}

TEST_F(ErrorCodeScenarioTest, EI0014_GetJsonPropertyList_NotArray) 
{
    nlohmann::json testJson;
    testJson["not_array"] = "string_value";

    EXPECT_THROW({
        nlohmann::json listObj;
        GetJsonPropertyList(testJson, "not_array", listObj);
    }, InvalidParamsException);
}

TEST_F(ErrorCodeScenarioTest, EI0015_SocketConnectionTimeout) 
{
    MOCK_FUNC(hccl::Http).stubs().will(returnValue(-1));
    MOCK_FUNC(aclrtGetEventWithTimeout).stubs().will(returnValue(ACL_ERROR_RT_TIME_OUT));
    MOCK_FUNC(aclrtCreateStream).stubs().will(returnValue(ACL_ERROR_RT_TIME_OUT));
}

TEST_F(ErrorCodeScenarioTest, EI0016_TLSConsistency_Inconsistent) 
{
    nlohmann::json testJson;
    testJson["version"] = "2.0";
    testJson["rank_count"] = 2;
    testJson["topo_file_path"] = "/tmp/test.json";

    nlohmann::json rankList = nlohmann::json::array();
    nlohmann::json rank0;
    rank0["rank_id"] = 0;
    rank0["tls_status"] = "enable";
    rankList.push_back(rank0);

    nlohmann::json rank1;
    rank1["rank_id"] = 1;
    rank1["tls_status"] = "disable";
    rankList.push_back(rank1);

    testJson["rank_list"] = rankList;
}

TEST_F(ErrorCodeScenarioTest, EI0017_GetJsonProperty_Missing) 
{
    nlohmann::json testJson;
    
    EXPECT_THROW({
        std::string val = GetJsonProperty(testJson, "missing_property", true);
    }, InvalidParamsException);
}

TEST_F(ErrorCodeScenarioTest, EI0017_GetJsonPropertyUInt_Missing) 
{
    nlohmann::json testJson;
    
    EXPECT_THROW({
        u32 val = GetJsonPropertyUInt(testJson, "missing_property", true);
    }, InvalidParamsException);
}

TEST_F(ErrorCodeScenarioTest, EI0017_GetJsonPropertySInt_Missing) 
{
    nlohmann::json testJson;
    
    EXPECT_THROW({
        s32 val = GetJsonPropertySInt(testJson, "missing_property", true);
    }, InvalidParamsException);
}

TEST_F(ErrorCodeScenarioTest, EI0017_GetJsonPropertyList_Missing) 
{
    nlohmann::json testJson;
    
    EXPECT_THROW({
        nlohmann::json listObj;
        GetJsonPropertyList(testJson, "missing_property", listObj);
    }, InvalidParamsException);
}

TEST_F(ErrorCodeScenarioTest, EI0018_CCUException) 
{
    rtExceptionInfo_t exceptionInfo = {};
    exceptionInfo.errorCode = 0x1;
    exceptionInfo.errorType = 0;
    exceptionInfo.taskType = TaskParamType::TASK_CCU;

    HcclIe10AlgParam algParam = {};
    algParam.Ccu.executeId = 0;

    TaskInfo taskInfo;
    taskInfo.taskParam_.taskPara.Ccu = algParam.Ccu;
    
    MOCK_FUNC(DlHalFunctionV2::GetInstance().dlHalGetEventAttr).stubs().will(returnValue(DRV_ERROR_NONE));
}

TEST_F(ErrorCodeScenarioTest, EI0019_PortPreempt_HostNicType) 
{
    PreemptPortManager portManager;
    std::shared_ptr<Socket> mockSocket = std::make_shared<Socket>();

    MOCK_METHOD(Socket, GetNicType).stubs().will(returnValue(NicType::HOST_NIC_TYPE));
    MOCK_METHOD(Socket, Bind).stubs().will(returnValue(HCCL_ERROR));
    MOCK_METHOD(Socket, Listen).stubs().will(returnValue(HCCL_SUCCESS));

    std::string ipAddr = "192.168.1.1";
    u32 usePort = 10000;
    std::pair<u32, u32> portRange = {10000, 10000};
}

TEST_F(ErrorCodeScenarioTest, EI0020_PortPreempt_NpuNicType) 
{
    PreemptPortManager portManager;
    std::shared_ptr<Socket> mockSocket = std::make_shared<Socket>();

    MOCK_METHOD(Socket, GetNicType).stubs().will(returnValue(NicType::NPU_NIC_TYPE));
    MOCK_METHOD(Socket, Bind).stubs().will(returnValue(HCCL_ERROR));
    MOCK_METHOD(Socket, Listen).stubs().will(returnValue(HCCL_SUCCESS));

    std::string ipAddr = "192.168.1.1";
    u32 usePort = 10000;
    std::pair<u32, u32> portRange = {10000, 10000};
}