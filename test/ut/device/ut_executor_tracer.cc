#include "gtest/gtest.h"
#include "executor_tracer.h"
#include "framework/aicpu_hccl_process.h"
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

TEST_F(ExecutorTracerTest, Ut_TaskMonitor_CommInfoStatus_True)
{
    // 创建一个HcclCommAicpu对象
    HcclCommAicpu *hcclAicpu = new HcclCommAicpu();
    
    // Mock GetCommInfoStatus方法，返回true
    MOCKER(&HcclCommAicpu::GetCommInfoStatus)
        .stubs()
        .will(returnValue(true));
    
    // Mock StreamTaskMonitor方法，记录调用
    bool streamTaskMonitorCalled = false;
    MOCKER(&HcclCommAicpu::StreamTaskMonitor)
        .stubs()
        .will(doAction([&streamTaskMonitorCalled]() {
            streamTaskMonitorCalled = true;
            return HCCL_SUCCESS;
        }));
    
    // 创建一个包含测试通信域的向量
    std::vector<std::pair<std::string, hccl::HcclCommAicpu *>> aicpuCommInfo;
    aicpuCommInfo.push_back(std::make_pair("test_group", hcclAicpu));
    
    // Mock AicpuGetCommAll方法，返回测试通信域
    MOCKER(AicpuHcclProcess::AicpuGetCommAll)
        .stubs()
        .will(invoke([&aicpuCommInfo](std::vector<std::pair<std::string, hccl::HcclCommAicpu *>> &info) {
            info = aicpuCommInfo;
            return HCCL_SUCCESS;
        }));

    // 执行TaskMonitor函数
    ExecutorTracer::TaskMonitor();

    // 验证StreamTaskMonitor被调用
    EXPECT_TRUE(streamTaskMonitorCalled);
    
    // 清理
    delete hcclAicpu;
}

TEST_F(ExecutorTracerTest, Ut_TaskMonitor_CommInfoStatus_False)
{
    // 创建一个HcclCommAicpu对象
    HcclCommAicpu *hcclAicpu = new HcclCommAicpu();
    
    // Mock GetCommInfoStatus方法，返回false
    MOCKER(&HcclCommAicpu::GetCommInfoStatus)
        .stubs()
        .will(returnValue(false));
    
    // Mock StreamTaskMonitor方法，记录调用
    bool streamTaskMonitorCalled = false;
    MOCKER(&HcclCommAicpu::StreamTaskMonitor)
        .stubs()
        .will(doAction([&streamTaskMonitorCalled]() {
            streamTaskMonitorCalled = true;
            return HCCL_SUCCESS;
        }));
    
    // 创建一个包含测试通信域的向量
    std::vector<std::pair<std::string, hccl::HcclCommAicpu *>> aicpuCommInfo;
    aicpuCommInfo.push_back(std::make_pair("test_group", hcclAicpu));
    
    // Mock AicpuGetCommAll方法，返回测试通信域
    MOCKER(AicpuHcclProcess::AicpuGetCommAll)
        .stubs()
        .will(invoke([&aicpuCommInfo](std::vector<std::pair<std::string, hccl::HcclCommAicpu *>> &info) {
            info = aicpuCommInfo;
            return HCCL_SUCCESS;
        }));

    // 执行TaskMonitor函数
    ExecutorTracer::TaskMonitor();

    // 验证StreamTaskMonitor未被调用
    EXPECT_FALSE(streamTaskMonitorCalled);
    
    // 清理
    delete hcclAicpu;
}
