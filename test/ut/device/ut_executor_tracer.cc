#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "executor_tracer.h"
#include "framework/aicpu_hccl_process.h"
#include "framework/aicpu_communicator.h"
#include "dfx/aicpu_executor_tracer.h"

using namespace testing;
using namespace mockcpp;
using namespace dfx_tracer;

// Mock HcclCommAicpu class
class MockHcclCommAicpu {
public:
    MOCK_METHOD0(GetCommInfoStatus, bool());
    MOCK_METHOD0(StreamTaskMonitor, int());
};

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
    // Create mock objects
    MockHcclCommAicpu mockHcclCommAicpu;
    
    // Set up mock behavior
    MOCKER(&MockHcclCommAicpu::GetCommInfoStatus)
        .stubs()
        .will(returnValue(true));
    
    MOCKER(&MockHcclCommAicpu::StreamTaskMonitor)
        .stubs()
        .will(returnValue(0));
    
    // Mock AicpuHcclProcess static methods
    ReadWriteLockBase mockMutex;
    MOCKER(AicpuHcclProcess::AicpuGetCommMutex)
        .stubs()
        .will(returnValueByRef(mockMutex));
    
    std::vector<std::pair<std::string, hccl::HcclCommAicpu *>> mockCommInfo;
    mockCommInfo.push_back(std::make_pair("test_group", reinterpret_cast<hccl::HcclCommAicpu *>(&mockHcclCommAicpu)));
    
    MOCKER(AicpuHcclProcess::AicpuGetCommAll)
        .stubs()
        .will(doAnswer(SetArgumentPointee<0>(mockCommInfo)));
    
    // Execute TaskMonitor
    ExecutorTracer::TaskMonitor();
    
    // Verify StreamTaskMonitor was called
    MOCKER(&MockHcclCommAicpu::StreamTaskMonitor).verify();
}
