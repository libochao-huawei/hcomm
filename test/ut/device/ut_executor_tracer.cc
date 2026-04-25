#include "gtest/gtest.h"
#include "executor_tracer.h"
#include "framework/aicpu_hccl_process.h"
#include "framework/aicpu_communicator.h"
#include "dfx/aicpu_executor_tracer.h"
#include "read_write_lock_base.h"

#ifndef private
#define private public
#define protected public
#endif

using namespace testing;
using namespace dfx_tracer;
using namespace hccl;

// 声明全局变量，用于访问AicpuHcclProcess中的通信域映射
extern "C" {
    struct hcclCommAicpuInfo {
        ReadWriteLockBase commAicpuMapMutex;
        std::unordered_map<std::string, std::pair<std::shared_ptr<hccl::HcclCommAicpu>, bool>> commMap;
    };
    extern hcclCommAicpuInfo g_commAicpuInfo;
}

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
        // 清空通信域映射
        g_commAicpuInfo.commMap.clear();
    }

    void TearDown() override {
        // 清空通信域映射
        g_commAicpuInfo.commMap.clear();
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
    // 创建测试通信域对象
    auto testComm = std::make_shared<TestHcclCommAicpu>();
    testComm->m_commInfoStatus = true;
    testComm->m_streamTaskMonitorCalled = false;

    // 将测试对象添加到全局通信域映射中
    g_commAicpuInfo.commMap["test_group"] = std::make_pair(testComm, true);

    // 执行TaskMonitor函数
    ExecutorTracer::TaskMonitor();

    // 验证StreamTaskMonitor被调用
    EXPECT_TRUE(testComm->m_streamTaskMonitorCalled);
}

TEST_F(ExecutorTracerTest, Ut_TaskMonitor_CommInfoStatus_False)
{
    // 创建测试通信域对象
    auto testComm = std::make_shared<TestHcclCommAicpu>();
    testComm->m_commInfoStatus = false;
    testComm->m_streamTaskMonitorCalled = false;

    // 将测试对象添加到全局通信域映射中
    g_commAicpuInfo.commMap["test_group"] = std::make_pair(testComm, true);

    // 执行TaskMonitor函数
    ExecutorTracer::TaskMonitor();

    // 验证StreamTaskMonitor未被调用
    EXPECT_FALSE(testComm->m_streamTaskMonitorCalled);
}
