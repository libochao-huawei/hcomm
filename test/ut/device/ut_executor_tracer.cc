#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "executor_tracer.h"
#include "framework/aicpu_hccl_process.h"
#include "framework/aicpu_communicator.h"
#include "dfx/aicpu_executor_tracer.h"

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
