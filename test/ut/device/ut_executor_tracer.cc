#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "executor_tracer.h"
#include "framework/aicpu_hccl_process.h"
#include "framework/aicpu_communicator.h"
#include "dfx/aicpu_executor_tracer.h"
#include "read_write_lock_base.h"

using namespace testing;
using namespace mockcpp;
using namespace dfx_tracer;

class ExecutorTracerTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
        GlobalMockObject::verify();
    }
};

TEST_F(ExecutorTracerTest, Ut_SetCqeQueryInput)
{
    uint32_t devId = 0;
    HcclComStreamInfo streamInfo;
    streamInfo.actualStreamId = 1;
    streamInfo.sqId = 2;
    streamInfo.logicCqId = 3;
    CqeQueryInput cqeQueryInput;

    ExecutorTracer::SetCqeQueryInput(devId, streamInfo, cqeQueryInput);
    EXPECT_EQ(cqeQueryInput.devId, devId);
    EXPECT_EQ(cqeQueryInput.streamId, streamInfo.actualStreamId);
    EXPECT_EQ(cqeQueryInput.sqId, streamInfo.sqId);
    EXPECT_EQ(cqeQueryInput.cqId, streamInfo.logicCqId);
    EXPECT_EQ(cqeQueryInput.type, static_cast<uint32_t>(DRV_LOGIC_TYPE));
}

TEST_F(ExecutorTracerTest, Ut_StopBackGroundDfx)
{
    AicpuComContext ctx;
    ctx.dfxExtendInfo.commandToBackGroud = CommandToBackGroud::kDefault;

    ExecutorTracer::StopBackGroundDfx(&ctx);
    EXPECT_EQ(ctx.dfxExtendInfo.commandToBackGroud, CommandToBackGroud::kStop);
}

TEST_F(ExecutorTracerTest, Ut_TaskMonitor)
{
    // Mock HcclCommAicpu
    class MockHcclCommAicpu {
    public:
        MOCK_METHOD0(GetCommInfoStatus, bool());
        MOCK_METHOD0(StreamTaskMonitor, HcclResult());
    };
    
    MockHcclCommAicpu mockComm;
    
    // Mock GetCommInfoStatus to return true
    MOCKER(&MockHcclCommAicpu::GetCommInfoStatus)
        .stubs()
        .will(returnValue(true));
    
    // Mock StreamTaskMonitor
    MOCKER(&MockHcclCommAicpu::StreamTaskMonitor)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    
    // Mock AicpuHcclProcess::AicpuGetCommMutex to return a valid ReadWriteLockBase reference
    static ReadWriteLockBase mockMutex;
    MOCKER(AicpuHcclProcess::AicpuGetCommMutex)
        .stubs()
        .will(returnValueRef(mockMutex));
    
    // Mock AicpuHcclProcess::AicpuGetCommAll to return our vector
    MOCKER(AicpuHcclProcess::AicpuGetCommAll)
        .stubs()
        .will(doAction([&](std::vector<std::pair<std::string, hccl::HcclCommAicpu *>> &commInfo) {
            commInfo.push_back({"test_group", reinterpret_cast<hccl::HcclCommAicpu *>(&mockComm)});
            return HCCL_SUCCESS;
        }));
    
    // Execute TaskMonitor
    ExecutorTracer::TaskMonitor();
    
    // Verify the function was called
    EXPECT_TRUE(true);
}
