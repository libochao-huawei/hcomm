#include "gtest/gtest.h"
#include "executor_tracer.h"
#include "framework/aicpu_hccl_process.h"
#include "framework/aicpu_communicator.h"
#include "dfx/aicpu_executor_tracer.h"
#include "read_write_lock_base.h"
#include "mockcpp/mockcpp.hpp"

#ifndef private
#define private public
#define protected public
#endif

using namespace testing;
using namespace dfx_tracer;
using namespace hccl;

class TestHcclCommAicpu : public hccl::HcclCommAicpu {
public:
    TestHcclCommAicpu();
    bool GetCommInfoStatus() const
    {
        return m_commInfoStatus;
    }
    HcclResult StreamTaskMonitor()
    {
        m_streamTaskMonitorCalled = true;
        return HCCL_SUCCESS;
    }
    bool m_commInfoStatus;
    bool m_streamTaskMonitorCalled;
};

TestHcclCommAicpu::TestHcclCommAicpu()
    : hccl::HcclCommAicpu(), m_commInfoStatus(true), m_streamTaskMonitorCalled(false)
{
}

class ExecutorTracerTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
        GlobalMockObject::verify();
        GlobalMockObject::reset();
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

// 测试用的AicpuGetCommAll stub函数
TestHcclCommAicpu g_testComm;
HcclResult AicpuGetCommAll_stub(std::vector<std::pair<std::string, hccl::HcclCommAicpu *>> &aicpuCommInfo)
{
    aicpuCommInfo.push_back(std::make_pair("test_group", &g_testComm));
    return HCCL_SUCCESS;
}

TEST_F(ExecutorTracerTest, Ut_TaskMonitor_CommInfoStatus_True)
{
    // 设置测试通信域对象状态
    g_testComm.m_commInfoStatus = true;
    g_testComm.m_streamTaskMonitorCalled = false;

    // Mock AicpuGetCommAll方法
    MOCKER(AicpuHcclProcess::AicpuGetCommAll)
        .stubs()
        .will(invoke(AicpuGetCommAll_stub));

    // 执行TaskMonitor函数
    ExecutorTracer::TaskMonitor();

    // 验证StreamTaskMonitor被调用
    EXPECT_TRUE(g_testComm.m_streamTaskMonitorCalled);
}

TEST_F(ExecutorTracerTest, Ut_TaskMonitor_CommInfoStatus_False)
{
    // 设置测试通信域对象状态
    g_testComm.m_commInfoStatus = false;
    g_testComm.m_streamTaskMonitorCalled = false;

    // Mock AicpuGetCommAll方法
    MOCKER(AicpuHcclProcess::AicpuGetCommAll)
        .stubs()
        .will(invoke(AicpuGetCommAll_stub));

    // 执行TaskMonitor函数
    ExecutorTracer::TaskMonitor();

    // 验证StreamTaskMonitor未被调用
    EXPECT_FALSE(g_testComm.m_streamTaskMonitorCalled);
}
