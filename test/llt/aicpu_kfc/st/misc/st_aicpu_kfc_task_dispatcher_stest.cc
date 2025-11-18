#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#ifndef private
#define private public
#define protected public
#endif
#include "common/aicpu_kfc_def.h"
#include "algorithm/task_orchestrator.h"
#include "aicpu_kfc/aicpu_kfc_interface.h"
#include "env_config.h"

#include "llt_aicpu_kfc_stub_mc2.h"
#include "llt_aicpu_kfc_stub.h"
#undef private
#undef protected

using namespace std;
using namespace HcclApi;

class MC2TaskDispatcher_ST : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "MC2TaskDispatcher_ST SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MC2TaskDispatcher_ST TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        g_stubDevType = DevType::DEV_TYPE_910B;
        MockGetSendRecvCnt();
        MOCKER(halGetDeviceInfo)
        .stubs()
        .with(any())
        .will(invoke(StubhalGetDeviceInfo));
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        setenv("HCCL_DFS_CONFIG", "connection_fault_detection_time:0", 1);
        InitEnvParam();
        std::cout << "MC2TaskDispatcher_ST Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        ResetMC2Context();
        GlobalMockObject::verify();
        std::cout << "MC2TaskDispatcher_ST Test TearDown" << std::endl;
    }
};

TEST_F(MC2TaskDispatcher_ST, ipcCpyWin2WinEx_errPara)
{
    EXPECT_EQ(TaskOrchestrator::IpcCpyWin2WinEx(0, 10, 0, HCCL_REDUCE_SUM, HCCL_DATA_TYPE_FP16, 0), HCCL_E_PARA);
}

TEST_F(MC2TaskDispatcher_ST, selfCpySnd2WinEx_errPara)
{
    EXPECT_EQ(TaskOrchestrator::SelfCpySnd2WinEx(0, nullptr, 10, 0, 0, HCCL_REDUCE_SUM, HCCL_DATA_TYPE_FP16, 0),
        HCCL_E_PARA);
}

TEST_F(MC2TaskDispatcher_ST, selfCpySnd2WinEx1_errPara)
{
    EXPECT_EQ(TaskOrchestrator::SelfCpySnd2WinEx1(nullptr, 10, 0, 0, HCCL_REDUCE_SUM, HCCL_DATA_TYPE_FP16, 0),
        HCCL_E_PARA);
}

TEST_F(MC2TaskDispatcher_ST, selfCpyWin2RcvEx_errPara)
{
    EXPECT_EQ(TaskOrchestrator::SelfCpyWin2RcvEx(0, nullptr, 10, 0, 0, HCCL_REDUCE_SUM, HCCL_DATA_TYPE_FP16, 0),
        HCCL_E_PARA);
}

TEST_F(MC2TaskDispatcher_ST, selfCpySnd2Rcv)
{
    StubHccCommRes commRes;
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    AicpuKfcRpcServer::RpcMsgBody msgBody;
    paramTask.mc2WorkSpace.workSpace = uint64_t(&msgBody);

    KFCResInitTask initTask;
    initTask.context = uint64_t(&paramTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));

    StubSqeBuffer sqeBufferStub;
    int snd, rcv;
    EXPECT_EQ(TaskOrchestrator::SelfCpySnd2Rcv(&snd, &rcv, 0, 0, 10, HCCL_REDUCE_SUM, HCCL_DATA_TYPE_FP16),
        HCCL_SUCCESS);
}

TEST_F(MC2TaskDispatcher_ST, ipcCpySnd2Win)
{
    StubHccCommRes commRes;
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    AicpuKfcRpcServer::RpcMsgBody msgBody;
    paramTask.mc2WorkSpace.workSpace = uint64_t(&msgBody);

    KFCResInitTask initTask;
    initTask.context = uint64_t(&paramTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));

    StubSqeBuffer sqeBufferStub;
    uint64_t offset = 0;
    int snd, rcv;
    EXPECT_EQ(TaskOrchestrator::IpcCpySnd2Win(&snd, 10, offset, offset, HCCL_REDUCE_SUM, HCCL_DATA_TYPE_FP16),
        HCCL_SUCCESS);
}

TEST_F(MC2TaskDispatcher_ST, ipcCpySnd2Win_vector)
{
    StubHccCommRes commRes;
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    AicpuKfcRpcServer::RpcMsgBody msgBody;
    paramTask.mc2WorkSpace.workSpace = uint64_t(&msgBody);

    KFCResInitTask initTask;
    initTask.context = uint64_t(&paramTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));

    StubSqeBuffer sqeBufferStub;
    std::vector<u64> dataSizes(8, 10);
    std::vector<u64> sndOffsets(8, 0);
    int snd, rcv;
    EXPECT_EQ(TaskOrchestrator::IpcCpySnd2Win(&snd, dataSizes, sndOffsets, nullptr, HCCL_REDUCE_SUM,
        HCCL_DATA_TYPE_FP16), HCCL_SUCCESS);
}

TEST_F(MC2TaskDispatcher_ST, ipcCpyWin2RcvEx)
{
    StubHccCommRes commRes;
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    AicpuKfcRpcServer::RpcMsgBody msgBody;
    paramTask.mc2WorkSpace.workSpace = uint64_t(&msgBody);

    KFCResInitTask initTask;
    initTask.context = uint64_t(&paramTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));

    StubSqeBuffer sqeBufferStub;
    int snd, rcv;
    EXPECT_EQ(TaskOrchestrator::IpcCpyWin2RcvEx(&rcv, 10, nullptr, 0, HCCL_REDUCE_SUM, HCCL_DATA_TYPE_FP16),
        HCCL_SUCCESS);
}

TEST_F(MC2TaskDispatcher_ST, ipcCpyWin2Rcv_vector)
{
    StubHccCommRes commRes;
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    AicpuKfcRpcServer::RpcMsgBody msgBody;
    paramTask.mc2WorkSpace.workSpace = uint64_t(&msgBody);

    KFCResInitTask initTask;
    initTask.context = uint64_t(&paramTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));

    StubSqeBuffer sqeBufferStub;
    std::vector<u64> dataSizes(8, 10);
    std::vector<u64> rcvOffsets(8, 0);
    int snd, rcv;
    EXPECT_EQ(TaskOrchestrator::IpcCpyWin2Rcv(&rcv, dataSizes, nullptr, rcvOffsets, HCCL_REDUCE_SUM,
        HCCL_DATA_TYPE_FP16), HCCL_SUCCESS);
}

TEST_F(MC2TaskDispatcher_ST, ipcCpyWin2Rcv)
{
    StubHccCommRes commRes;
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    AicpuKfcRpcServer::RpcMsgBody msgBody;
    paramTask.mc2WorkSpace.workSpace = uint64_t(&msgBody);

    KFCResInitTask initTask;
    initTask.context = uint64_t(&paramTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));

    StubSqeBuffer sqeBufferStub;
    uint64_t offset = 0;
    int snd, rcv;
    EXPECT_EQ(TaskOrchestrator::IpcCpyWin2Rcv(&rcv, 10, offset, nullptr, HCCL_REDUCE_SUM, HCCL_DATA_TYPE_FP16),
        HCCL_SUCCESS);
}

TEST_F(MC2TaskDispatcher_ST, ipcCpySnd2WinEx_errPara)
{
    int snd, rcv;
    EXPECT_EQ(TaskOrchestrator::IpcCpySnd2WinEx(&snd, 10, nullptr, nullptr, HCCL_REDUCE_SUM, HCCL_DATA_TYPE_FP16,
        0, 7, 0, true), HCCL_E_PARA);
}

TEST_F(MC2TaskDispatcher_ST, ipcCpyWin2RcvEx_errPara)
{
    int snd, rcv;
    EXPECT_EQ(TaskOrchestrator::IpcCpyWin2RcvEx(&rcv, 10, nullptr, nullptr, HCCL_REDUCE_SUM, HCCL_DATA_TYPE_FP16,
        0, 7, 0, true), HCCL_E_PARA);
}

TEST_F(MC2TaskDispatcher_ST, ipcCpySnd2WinSliceEx_errPara)
{
    std::vector<Slice> dataSlice;
    int snd, rcv;
    EXPECT_EQ(TaskOrchestrator::IpcCpySnd2WinSliceEx(&snd, dataSlice, nullptr, HCCL_REDUCE_SUM, HCCL_DATA_TYPE_FP16,
        0, 7, 0, true), HCCL_E_PARA);
}

TEST_F(MC2TaskDispatcher_ST, ipcCpyWin2RcvSliceEx_errPara)
{
    std::vector<Slice> dataSlice;
    EXPECT_EQ(TaskOrchestrator::IpcCpyWin2RcvSliceEx(nullptr, dataSlice, nullptr, HCCL_REDUCE_SUM, HCCL_DATA_TYPE_FP16,
        0, 7, 0, true), HCCL_E_PARA);
}

TEST_F(MC2TaskDispatcher_ST, launchTasksEx_errPara)
{
    EXPECT_EQ(TaskOrchestrator::LaunchTasksEx(0, 7, 0), HCCL_E_PARA);
}

TEST_F(MC2TaskDispatcher_ST, ipcPreRecordEx_errPara)
{
    EXPECT_EQ(TaskOrchestrator::IpcPreRecordEx(0, 7, 0, true), HCCL_E_PARA);
}

TEST_F(MC2TaskDispatcher_ST, ipcPreWaitEx_errPara)
{
    EXPECT_EQ(TaskOrchestrator::IpcPreWaitEx(0, 7, 0, true), HCCL_E_PARA);
}

TEST_F(MC2TaskDispatcher_ST, ipcPreSyncEx_errPara)
{
    EXPECT_EQ(TaskOrchestrator::IpcPreSyncEx(0, 7, 0, true), HCCL_E_PARA);
}

TEST_F(MC2TaskDispatcher_ST, ipcPreSyncOnMainStream)
{
    StubHccCommRes commRes;
    MOCKER_CPP(&HcclTraceInfo::Flush).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    AicpuKfcRpcServer::RpcMsgBody msgBody;
    paramTask.mc2WorkSpace.workSpace = uint64_t(&msgBody);
    KFCResInitTask initTask;
    initTask.context = uint64_t(&paramTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));

    StubSqeBuffer sqeBufferStub;
    EXPECT_EQ(TaskOrchestrator::IpcPreSyncOnMainStream(), 0);
}

TEST_F(MC2TaskDispatcher_ST, ipcPostRecordEx_errPara)
{
    EXPECT_EQ(TaskOrchestrator::IpcPostRecordEx(0, 7, 0, true), HCCL_E_PARA);
}

TEST_F(MC2TaskDispatcher_ST, ipcPostWaitEx_errPara)
{
    EXPECT_EQ(TaskOrchestrator::IpcPostWaitEx(0, 7, 0, true), HCCL_E_PARA);
}

TEST_F(MC2TaskDispatcher_ST, ipcPostSyncEx_errPara)
{
    EXPECT_EQ(TaskOrchestrator::IpcPostSyncEx(0, 7, 0, true), HCCL_E_PARA);
}

TEST_F(MC2TaskDispatcher_ST, ut_CheckTaskTimeout)
{
    AicpuComContext *ctx = AicpuGetComContext();
 
    ctx->rankNum = 2; // 设置rank数量为2
    ctx->devId = 0;
    ctx->rankId = 0;
    for (uint32_t i = 0U; i < ctx->rankNum; i++) {
        ctx->streamInfo[i].sqDepth = 2048;
    }
    ctx->streamInfo[0].sqId = 0;
    ctx->streamInfo[1].sqId = 1;

    auto ret = TaskOrchestrator::CheckTaskTimeout(ctx, 0);
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);
}

TEST_F(MC2TaskDispatcher_ST, ut_OverflowAddrCheck)
{
    AicpuComContext *ctx = AicpuGetComContext();
    uint32_t overflowFlag = 0;
    ctx->devType = DevType::DEV_TYPE_310P1;

    // rts地址溢出后，会在该地址位置填写0x11
    uint32_t overflowContent = 17; // 0x11
    ctx->overflowAddr = reinterpret_cast<u64>(&overflowContent);
    TaskOrchestrator::OverflowAddrCheck(ctx, overflowFlag, 50, 51);
    EXPECT_EQ(overflowFlag, 1);

    // reset ctx
    overflowContent = 0;
    ctx->devType = DevType::DEV_TYPE_910B;
}

TEST_F(MC2TaskDispatcher_ST, ut_RdmaSend)
{
    uint16_t streamId = 0;
    u64 dbInfo = 0x11;
    u64 dbAddr = 0x11;
    u32 userRank = 0;
    EXPECT_EQ(AicpuDispatcher::RdmaSend(streamId, dbInfo, dbAddr, userRank), HCCL_SUCCESS);
}

// 补充覆盖率
TEST_F(MC2TaskDispatcher_ST, ut_AicpuUnfoldSignalWait)
{
    AicpuComContext *ctx = AicpuGetComContext();
    ctx->devType = DevType::DEV_TYPE_910B;

    u16 streamId = 0;
    u16 notifyId = 0;
    bool innerChip = true;

    AicpuDispatcher::AicpuUnfoldSignalWait(streamId, notifyId, innerChip);

    innerChip = false;
    ctx->devType = DevType::DEV_TYPE_310P1;
    AicpuDispatcher::AicpuUnfoldSignalWait(streamId, notifyId, innerChip);

    ctx->aicpuOpNotify[notifyId].actualNotifyId = (1 << 15U);
    AicpuDispatcher::AicpuUnfoldSignalWait(streamId, notifyId, innerChip);

    AicpuAddOneEventWaitSqe g_addOneEventWaitSqe = AicpuGetAddOneEventWaitSqe();
    g_addOneEventWaitSqe = AddOneEventWaitSqeV2;
    AicpuDispatcher::AicpuUnfoldSignalWait(streamId, notifyId, innerChip);
}

// 补充覆盖率
TEST_F(MC2TaskDispatcher_ST, ut_dispatcher_cpy)
{
    AicpuComContext *ctx = AicpuGetComContext();
    void *buff = nullptr;
    u64 dataSize = 0;
    u64 offset = 0;
    u64 winOffset = 0;
    HcclReduceOp opType = HCCL_REDUCE_SUM;
    HcclDataType dataType = HCCL_DATA_TYPE_FP16;

    HcclResult ret = HCCL_SUCCESS;
    ret = TaskOrchestrator::SelfCpySnd2Win(buff, dataSize, offset, winOffset, opType, dataType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = TaskOrchestrator::SelfCpyRcv2Win(buff, dataSize, offset, winOffset, opType, dataType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ctx->rankNum = 2;
    ret = TaskOrchestrator::IpcCpyWin2Win(&dataSize, &winOffset, opType, (u64)buff, dataType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::vector<u64> dataSizes(10, 0);
    std::vector<u64> winOffsets;
    ret = TaskOrchestrator::IpcCpyWin2Win(dataSizes, offset, winOffsets, opType, dataType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u32 mainRankId = 0;
    u32 maxStreamNum = 1;
    ret = TaskOrchestrator::IpcCpyWin2WinEx(mainRankId, dataSize, winOffset, opType, dataType, maxStreamNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = TaskOrchestrator::SelfCpyWin2RcvEx1(buff, dataSize, offset, winOffset, opType, dataType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = TaskOrchestrator::IpcCpySnd2Win(buff, dataSize, nullptr, winOffset, opType, dataType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = TaskOrchestrator::IpcCpySnd2Win(buff, dataSizes.data(), nullptr, nullptr, opType, dataType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = TaskOrchestrator::IpcCpyWin2WinP2P(0, 0, 0, 0, opType, dataType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = TaskOrchestrator::IpcCpyWin2RcvEx(buff, dataSizes.data(), nullptr, winOffset, opType, dataType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = TaskOrchestrator::IpcCpyWin2RcvEx(buff, dataSizes, winOffsets, 0, opType, dataType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::vector<u64> recvOffsets;
    ret = TaskOrchestrator::IpcCpyWin2Rcv(buff, dataSizes, winOffsets.data(), recvOffsets, opType, dataType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = TaskOrchestrator::IpcCpyWin2RcvP2PMainStream(nullptr, 0, 0, 0, 0, opType, dataType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = TaskOrchestrator::IpcCpySnd2WinEx(nullptr, 0, nullptr, nullptr, opType, dataType, 0, 1, 1, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = TaskOrchestrator::IpcCpyWin2RcvEx(nullptr, 0, nullptr, nullptr, opType, dataType, 0, 1, 1, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = TaskOrchestrator::IpcCpyWin2Rcv(nullptr, dataSizes.data(), nullptr, nullptr, opType, dataType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = TaskOrchestrator::IpcCpyWin2RcvP2P(nullptr, 0, 0, 0, 0, opType, dataType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::vector<Slice> dataSlice(10);
    ret = TaskOrchestrator::IpcCpyWin2RcvSliceEx(nullptr, dataSlice, nullptr, opType, dataType, 0, 1, 1, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}