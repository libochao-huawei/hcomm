#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include "env_config.h"

#ifndef private
#define private public
#define protected public
#endif
#include "common/aicpu_kfc_def.h"
#include "framework/aicpu_kfc_rpc_server.h"
#include "aicpu_kfc/aicpu_kfc_interface.h"
#include "algorithm/aicpu_reduce_scatter.h"
#include "dlhal_function.h"

#include "llt_aicpu_kfc_stub_mc2.h"
#include "llt_aicpu_kfc_stub.h"
#undef private
#undef protected

using namespace std;
using namespace HcclApi;

class MC2Reducescatter_ST : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "MC2Reducescatter_ST SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MC2Reducescatter_ST TearDown" << std::endl;
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
        MOCKER(QuerySqStatusByType)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));
        DlHalFunction::GetInstance().DlHalFunctionInit();
        hrtSetDevice(0);
        set_chip_type_stub(0, static_cast<s32>(DevType::DEV_TYPE_910B));
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        setenv("HCCL_DFS_CONFIG", "connection_fault_detection_time:0", 1);
        InitEnvParam();
        std::cout << "MC2Reducescatter_ST Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        ResetMC2Context();
        GlobalMockObject::verify();
        std::cout << "MC2Reducescatter_ST Test TearDown" << std::endl;
    }
};

#define init_kfc_args(initTask)                                                             \
    StubHccCommRes commRes;                                                                 \
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();                      \
    AicpuKfcRpcServer::RpcMsgBody msgBody;                                                     \
    (void)memset_s(&msgBody, sizeof(msgBody), 0, sizeof(msgBody));                          \
    paramTask.mc2WorkSpace.workSpace = uint64_t(&msgBody);                                  \
                                                                                            \
    std::shared_ptr<hccl::HDCommunicate> h2dTransfer;                                       \
    std::shared_ptr<hccl::HDCommunicate> d2hTransfer;                                       \
    h2dTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, sizeof(KfcExecControl)));        \
    d2hTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, sizeof(KfcExecStatus)));        \
    h2dTransfer->InitHost();                                                                \
    d2hTransfer->InitHost();                                                                \
    paramTask.kfcControlTransferH2DParams = h2dTransfer->GetCommunicateParams();                      \
    paramTask.kfcStatusTransferD2HParams = d2hTransfer->GetCommunicateParams();                      \
    paramTask.config.retryEnable = 0;                                                       \
    initTask.context = uint64_t(&paramTask);                                                \

TEST_F(MC2Reducescatter_ST, reducescatter_fp16)
{
    MOCKER_CPP(&AicpuKfcRpcServerV2::ReadAddrMsg).stubs().will(returnValue(true));
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);

    StubHccCommRes commRes;
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    AicpuKfcRpcServer::RpcMsgBody msgBody;
    paramTask.mc2WorkSpace.workSpace = uint64_t(&msgBody);

    std::shared_ptr<hccl::HDCommunicate> h2dTransfer;
    std::shared_ptr<hccl::HDCommunicate> d2hTransfer;
    h2dTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, sizeof(KfcExecControl)));
    d2hTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, sizeof(KfcExecStatus)));
    h2dTransfer->InitHost();
    d2hTransfer->InitHost();
    paramTask.kfcControlTransferH2DParams = h2dTransfer->GetCommunicateParams();
    paramTask.kfcStatusTransferD2HParams = d2hTransfer->GetCommunicateParams();
    paramTask.config.retryEnable = 0;

    KFCResInitTask initTask;
    initTask.context = uint64_t(&paramTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));

    HcclKFCTilingData tilingData = {0};
    tilingData.commOrder = 1;
    tilingData.commType = HCCL_CMD_REDUCE_SCATTER;
    tilingData.dataType = HCCL_DATA_TYPE_FP16;
    tilingData.turnNum = 2;
    tilingData.sendCnt = 1024;
    tilingData.rspPolicy = 1;
    tilingData.waitPolicy = 1;
    tilingData.preparePosition = TASK_PREPARE_HOST;

    KFCTask kfcTask;
    u64* a = (u64*)malloc(1024*1024);
    kfcTask.inputA = uint64_t(a);
    kfcTask.outputC = uint64_t(a);
    kfcTask.commOut = uint64_t(a);
    kfcTask.workSpace = uint64_t(a);
    kfcTask.context = uint64_t(&paramTask);
    kfcTask.tilingData = uint64_t(&tilingData);

    MOCKER(memcpy_s)
    .stubs()
    .will(returnValue(EOK));

    StubSqeBuffer sqeBufferStub;
    EXPECT_EQ(0, RunAicpuRpcSrvLaunch(&kfcTask));
    free(a);
}

TEST_F(MC2Reducescatter_ST, reducescatter_bf16)
{
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);

    KFCResInitTask initTask;
    init_kfc_args(initTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));

    HcclKFCTilingData tilingData = {0};
    tilingData.commOrder = 1;
    tilingData.commType = HCCL_CMD_REDUCE_SCATTER;
    tilingData.dataType = HCCL_DATA_TYPE_BFP16;
    tilingData.turnNum = 2;
    tilingData.sendCnt = 1024;
    tilingData.rspPolicy = 1;
    tilingData.waitPolicy = 1;
    tilingData.preparePosition = TASK_PREPARE_HOST;

    KFCTask kfcTask;
    u64* a = (u64*)malloc(1024*1024);
    kfcTask.inputA = uint64_t(a);
    kfcTask.outputC = uint64_t(a);
    kfcTask.commOut = uint64_t(a);
    kfcTask.workSpace = uint64_t(a);
    kfcTask.context = uint64_t(&paramTask);
    kfcTask.tilingData = uint64_t(&tilingData);

    MOCKER(memcpy_s)
    .stubs()
    .will(returnValue(EOK));

    StubSqeBuffer sqeBufferStub;
    EXPECT_EQ(0, RunAicpuRpcSrvLaunch(&kfcTask));
    free(a);
}

TEST_F(MC2Reducescatter_ST, reducescatter_fp16Determinisitic_less_winsize)
{
    MOCKER_CPP(&AicpuKfcRpcServerV2::ReadAddrMsg).stubs().will(returnValue(true));
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);

    KFCResInitTask initTask;
    init_kfc_args(initTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));

    HcclKFCTilingData tilingData = {0};
    tilingData.commOrder = 1;
    tilingData.commType = HCCL_CMD_REDUCE_SCATTER;
    tilingData.dataType = HCCL_DATA_TYPE_FP16;
    tilingData.turnNum = 2;
    tilingData.sendCnt = 2048;
    tilingData.rspPolicy = 1;
    tilingData.waitPolicy = 1;
    tilingData.preparePosition = TASK_PREPARE_HOST;

    KFCTask kfcTask;
    u64* a = (u64*)malloc(2048*2048);
    kfcTask.inputA = uint64_t(a);
    kfcTask.outputC = uint64_t(a);
    kfcTask.commOut = uint64_t(a);
    kfcTask.workSpace = uint64_t(a);
    kfcTask.context = uint64_t(&paramTask);
    kfcTask.tilingData = uint64_t(&tilingData);

    MOCKER(memcpy_s)
    .stubs()
    .will(returnValue(EOK));

    AicpuComContext *ctx = AicpuGetComContext();
    ctx->determinism = true;
    StubSqeBuffer sqeBufferStub;
    EXPECT_EQ(0, RunAicpuRpcSrvLaunch(&kfcTask));
    ctx->determinism = false;
    free(a);
}

TEST_F(MC2Reducescatter_ST, reducescatter_fp16Determinisitic_test_equal_winsize)
{
    MOCKER_CPP(&AicpuKfcRpcServerV2::ReadAddrMsg).stubs().will(returnValue(true));
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);

    KFCResInitTask initTask;
    init_kfc_args(initTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));

    HcclKFCTilingData tilingData = {0};
    tilingData.commOrder = 1;
    tilingData.commType = HCCL_CMD_REDUCE_SCATTER;
    tilingData.dataType = HCCL_DATA_TYPE_FP16;
    tilingData.turnNum = 2;
    tilingData.sendCnt = 104857600;
    tilingData.rspPolicy = 1;
    tilingData.waitPolicy = 1;
    tilingData.preparePosition = TASK_PREPARE_HOST;

    KFCTask kfcTask;
    u64* a = (u64*)malloc(2048*2048);
    kfcTask.inputA = uint64_t(a);
    kfcTask.outputC = uint64_t(a);
    kfcTask.commOut = uint64_t(a);
    kfcTask.workSpace = uint64_t(a);
    kfcTask.context = uint64_t(&paramTask);
    kfcTask.tilingData = uint64_t(&tilingData);

    MOCKER(memcpy_s)
    .stubs()
    .will(returnValue(EOK));

    AicpuComContext *ctx = AicpuGetComContext();
    ctx->determinism = true;
    StubSqeBuffer sqeBufferStub;
    EXPECT_EQ(0, RunAicpuRpcSrvLaunch(&kfcTask));
    ctx->determinism = false;
    free(a);
}


TEST_F(MC2Reducescatter_ST, reducescatter_fp16Determinisitic_test_greater_winsize)
{
    MOCKER_CPP(&AicpuKfcRpcServerV2::ReadAddrMsg).stubs().will(returnValue(true));
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);

    KFCResInitTask initTask;
    init_kfc_args(initTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));

    HcclKFCTilingData tilingData = {0};
    tilingData.commOrder = 1;
    tilingData.commType = HCCL_CMD_REDUCE_SCATTER;
    tilingData.dataType = HCCL_DATA_TYPE_FP16;
    tilingData.turnNum = 2;
    tilingData.sendCnt = 209715200;
    tilingData.rspPolicy = 1;
    tilingData.waitPolicy = 1;
    tilingData.preparePosition = TASK_PREPARE_HOST;

    KFCTask kfcTask;
    u64* a = (u64*)malloc(2048*2048);
    kfcTask.inputA = uint64_t(a);
    kfcTask.outputC = uint64_t(a);
    kfcTask.commOut = uint64_t(a);
    kfcTask.workSpace = uint64_t(a);
    kfcTask.context = uint64_t(&paramTask);
    kfcTask.tilingData = uint64_t(&tilingData);

    MOCKER(memcpy_s)
    .stubs()
    .will(returnValue(EOK));

    AicpuComContext *ctx = AicpuGetComContext();
    ctx->determinism = true;
    StubSqeBuffer sqeBufferStub;
    EXPECT_EQ(0, RunAicpuRpcSrvLaunch(&kfcTask));
    ctx->determinism = false;
    free(a);
}

TEST_F(MC2Reducescatter_ST, reducescatter_doublering)
{
    MOCKER_CPP(&AicpuKfcRpcServerV2::ReadAddrMsg).stubs().will(returnValue(true));
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);
    g_stubDevType = DevType::DEV_TYPE_910_93;

    halChipInfo info = {"Ascend", "910_9381", "0"};
    MOCKER(halGetChipInfo)
    .stubs()
    .with(any(), outBoundP(&info, sizeof(halChipInfo)))
    .will(returnValue(DRV_ERROR_NONE));

    KFCResInitTask initTask;
    init_kfc_args(initTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));

    HcclKFCTilingData tilingData = {0};
    tilingData.commOrder = 1;
    tilingData.commType = HCCL_CMD_REDUCE_SCATTER;
    tilingData.dataType = HCCL_DATA_TYPE_FP16;
    tilingData.turnNum = 2;
    tilingData.sendCnt = 1024;
    tilingData.rspPolicy = 1;
    tilingData.waitPolicy = 1;
    tilingData.commAlg = 2;
    tilingData.preparePosition = TASK_PREPARE_HOST;

    KFCTask kfcTask;
    u64* a = (u64*)malloc(1024*1024);
    kfcTask.inputA = uint64_t(a);
    kfcTask.outputC = uint64_t(a);
    kfcTask.commOut = uint64_t(a);
    kfcTask.workSpace = uint64_t(a);
    kfcTask.context = uint64_t(&paramTask);
    kfcTask.tilingData = uint64_t(&tilingData);

    MOCKER(memcpy_s)
    .stubs()
    .will(returnValue(EOK));

    StubSqeBuffer sqeBufferStub;
    EXPECT_EQ(0, RunAicpuRpcSrvLaunch(&kfcTask));
    free(a);
}

TEST_F(MC2Reducescatter_ST, reducescatter_fp16_only_aicpu_commorder0)
{
    MOCKER_CPP(&AicpuKfcRpcServerV2::ReadAddrMsg).stubs().will(returnValue(true));
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);

    KFCResInitTask initTask;
    init_kfc_args(initTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));

    HcclKFCTilingData tilingData = {0};
    tilingData.commOrder = 0;
    tilingData.commType = HCCL_CMD_REDUCE_SCATTER;
    tilingData.dataType = HCCL_DATA_TYPE_FP16;
    tilingData.turnNum = 2;
    tilingData.sendCnt = 1024;
    tilingData.rspPolicy = 1;
    tilingData.waitPolicy = 1;
    tilingData.debugMode = 4;
    tilingData.useBufferType = 1;
    tilingData.preparePosition = TASK_PREPARE_HOST;

    KFCTask kfcTask;
    u64* a = (u64*)malloc(1024*1024);
    kfcTask.inputA = uint64_t(a);
    kfcTask.outputC = uint64_t(a);
    kfcTask.commOut = uint64_t(a);
    kfcTask.workSpace = uint64_t(a);
    kfcTask.context = uint64_t(&paramTask);
    kfcTask.tilingData = uint64_t(&tilingData);

    MOCKER(memcpy_s)
    .stubs()
    .will(returnValue(EOK));

    StubSqeBuffer sqeBufferStub;
    EXPECT_EQ(0, RunAicpuRpcSrvLaunch(&kfcTask));
    free(a);
}

TEST_F(MC2Reducescatter_ST, reducescatter_switch)
{
    MOCKER_CPP(&AicpuKfcRpcServerV2::ReadAddrMsg).stubs().will(returnValue(true));
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);
    g_stubDevType = DevType::DEV_TYPE_910_93;

    halChipInfo info = {"Ascend", "910_9381", "0"};
    MOCKER(halGetChipInfo)
    .stubs()
    .with(any(), outBoundP(&info, sizeof(halChipInfo)))
    .will(returnValue(DRV_ERROR_NONE));

    KFCResInitTask initTask;
    init_kfc_args(initTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));

    HcclKFCTilingData tilingData = {0};
    tilingData.commOrder = 1;
    tilingData.commType = HCCL_CMD_REDUCE_SCATTER;
    tilingData.dataType = HCCL_DATA_TYPE_FP16;
    tilingData.turnNum = 2;
    tilingData.sendCnt = 1024;
    tilingData.rspPolicy = 1;
    tilingData.waitPolicy = 1;
    tilingData.commAlg = 3;
    tilingData.preparePosition = TASK_PREPARE_HOST;

    KFCTask kfcTask;
    u64* a = (u64*)malloc(1024*1024);
    kfcTask.inputA = uint64_t(a);
    kfcTask.outputC = uint64_t(a);
    kfcTask.commOut = uint64_t(a);
    kfcTask.workSpace = uint64_t(a);
    kfcTask.context = uint64_t(&paramTask);
    kfcTask.tilingData = uint64_t(&tilingData);

    MOCKER(memcpy_s)
    .stubs()
    .will(returnValue(EOK));

    StubSqeBuffer sqeBufferStub;
    EXPECT_EQ(0, RunAicpuRpcSrvLaunch(&kfcTask));
    free(a);
}

TEST_F(MC2Reducescatter_ST, reducescatter_fp16_commorder0)
{
    MOCKER_CPP(&AicpuKfcRpcServerV2::ReadAddrMsg).stubs().will(returnValue(true));
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);

    KFCResInitTask initTask;
    init_kfc_args(initTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));

    HcclKFCTilingData tilingData = {0};
    tilingData.commOrder = 0;
    tilingData.commType = HCCL_CMD_REDUCE_SCATTER;
    tilingData.dataType = HCCL_DATA_TYPE_FP16;
    tilingData.turnNum = 2;
    tilingData.sendCnt = 1024;
    tilingData.rspPolicy = 1;
    tilingData.waitPolicy = 1;
    tilingData.useBufferType = 1;
    tilingData.preparePosition = TASK_PREPARE_HOST;

    KFCTask kfcTask;
    u64* a = (u64*)malloc(1024*1024);
    kfcTask.inputA = uint64_t(a);
    kfcTask.outputC = uint64_t(a);
    kfcTask.commOut = uint64_t(a);
    kfcTask.workSpace = uint64_t(a);
    kfcTask.context = uint64_t(&paramTask);
    kfcTask.tilingData = uint64_t(&tilingData);

    MOCKER(memcpy_s)
    .stubs()
    .will(returnValue(EOK));

    StubSqeBuffer sqeBufferStub;
    EXPECT_EQ(0, RunAicpuRpcSrvLaunch(&kfcTask));
    free(a);
}

TEST_F(MC2Reducescatter_ST, reducescatter_fp16_unfold) // 单reducescatter不带计算 aicpu展开
{
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);

    KFCResInitTask initTask;
    init_kfc_args(initTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));

    HcclKFCTilingData tilingData = {0};
    tilingData.commOrder = 0;
    tilingData.commType = HCCL_CMD_REDUCE_SCATTER;
    tilingData.dataType = HCCL_DATA_TYPE_FP16;
    tilingData.turnNum = 1;
    tilingData.sendCnt = 256;
    tilingData.totalCnt = 1;
    tilingData.rspPolicy = 0;
    tilingData.waitPolicy = 0;
    tilingData.taskType = HCCL_KFC_TASK_HCCL_ONLY_EXE;
    tilingData.useBufferType = 0;
    tilingData.preparePosition = TASK_PREPARE_HOST;

    KFCTask kfcTask;
    kfcTask.inputA = 0x124080012000;
    kfcTask.outputC = 0;
    kfcTask.commOut = 0x124080013000;
    kfcTask.context = uint64_t(&paramTask);
    kfcTask.tilingData = uint64_t(&tilingData);

    auto ctx = AicpuGetComContext();
    AicpuKfcRpcServer rpc;
    rpc.Init(ctx->workSpaceAddr, ctx->notifyOff, ctx->notifyBeginCnt, &kfcTask);
    AivAicpuOpParam msg;
    rpc.CheckRcvAddrMsg(&msg, 0);
    EXPECT_EQ(msg.sendBuffer, 0x124080012000);
    EXPECT_EQ(msg.recvBuffer, 0x124080013000);
}

TEST_F(MC2Reducescatter_ST, reducescatter_mc2Api)
{
    MOCKER_CPP(&AicpuKfcRpcServerV2::ReadAddrMsg).stubs().will(returnValue(true));
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);
 
    KFCResInitTask initTask;
    init_kfc_args(initTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));
 
    HcclKFCTilingData tilingData = {0};
    tilingData.preparePosition = TASK_PREPARE_KERNEL;
 
    KFCTask kfcTask;
    u64* a = (u64*)malloc(1024*1024);
    kfcTask.context = uint64_t(&paramTask);
    kfcTask.tilingData = uint64_t(&tilingData);
 
    AicpuComContext *ctx = AicpuGetComContext();
    u64 newAddr = ctx->workSpaceAddr;
    if (newAddr & 0x1ff) {
        newAddr = (newAddr & (~((uint64_t)0x1ff))) + 0x200;
    }
    HcclMsgAreaForTest *hcclMsgArea = reinterpret_cast<HcclMsgAreaForTest *>(newAddr);
    // commit
    hcclMsgArea->sendMsgList[0].commType = HCCL_CMD_REDUCE_SCATTER;
    hcclMsgArea->sendMsgList[0].opType = HCCL_REDUCE_SUM;
    hcclMsgArea->sendMsgList[0].hcclDataType = HCCL_DATA_TYPE_FP16;
    hcclMsgArea->sendMsgList[0].valid = HCCL_MSG_VALID_MASK;
    hcclMsgArea->sendMsgList[0].repeatCnt = 1;
    hcclMsgArea->sendMsgList[0].sendBuffer = uint64_t(a);
    hcclMsgArea->sendMsgList[0].recvBuffer = uint64_t(a);
    hcclMsgArea->sendMsgList[0].xorCheck = GenXorStub(&(hcclMsgArea->sendMsgList[0]));
    // finilze
    hcclMsgArea->sendMsgList[1].commType = HCCL_CMD_FINALIZE;
    hcclMsgArea->sendMsgList[1].valid = HCCL_MSG_VALID_MASK;
    hcclMsgArea->sendMsgList[1].xorCheck = GenXorStub(&(hcclMsgArea->sendMsgList[1]));
 
    StubSqeBuffer sqeBufferStub;
    EXPECT_EQ(0, RunAicpuRpcSrvLaunch(&kfcTask));
    EXPECT_EQ(1, ctx->msgPosForKernel);
    EXPECT_EQ(1, ctx->curTurnCntForKernel);
    free(a);
}
 
TEST_F(MC2Reducescatter_ST, reducescatter_mc2Api_turn2)
{
    MOCKER_CPP(&AicpuKfcRpcServerV2::ReadAddrMsg).stubs().will(returnValue(true));
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);
 
    KFCResInitTask initTask;
    init_kfc_args(initTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));
 
    HcclKFCTilingData tilingData = {0};
    tilingData.preparePosition = TASK_PREPARE_KERNEL;
 
    KFCTask kfcTask;
    u64* a = (u64*)malloc(1024*1024);
    kfcTask.context = uint64_t(&paramTask);
    kfcTask.tilingData = uint64_t(&tilingData);
 
    AicpuComContext *ctx = AicpuGetComContext();
    u64 newAddr = ctx->workSpaceAddr;
    if (newAddr & 0x1ff) {
        newAddr = (newAddr & (~((uint64_t)0x1ff))) + 0x200;
    }
    HcclMsgAreaForTest *hcclMsgArea = reinterpret_cast<HcclMsgAreaForTest *>(newAddr);
    memset_s(hcclMsgArea, sizeof(HcclMsgAreaForTest), 0, sizeof(HcclMsgAreaForTest));
    // commit
    hcclMsgArea->sendMsgList[0].commType = HCCL_CMD_REDUCE_SCATTER;
    hcclMsgArea->sendMsgList[0].opType = HCCL_REDUCE_SUM;
    hcclMsgArea->sendMsgList[0].hcclDataType = HCCL_DATA_TYPE_FP16;
    hcclMsgArea->sendMsgList[0].valid = HCCL_MSG_VALID_MASK;
    hcclMsgArea->sendMsgList[0].repeatCnt = 1;
    hcclMsgArea->sendMsgList[0].sendBuffer = uint64_t(a);
    hcclMsgArea->sendMsgList[0].recvBuffer = uint64_t(a);
    hcclMsgArea->sendMsgList[0].xorCheck = GenXorStub(&(hcclMsgArea->sendMsgList[0]));
    // commit 2
    hcclMsgArea->sendMsgList[1].commType = HCCL_CMD_REDUCE_SCATTER;
    hcclMsgArea->sendMsgList[1].opType = HCCL_REDUCE_SUM;
    hcclMsgArea->sendMsgList[1].hcclDataType = HCCL_DATA_TYPE_FP16;
    hcclMsgArea->sendMsgList[1].valid = HCCL_MSG_VALID_MASK;
    hcclMsgArea->sendMsgList[1].repeatCnt = 1;
    hcclMsgArea->sendMsgList[1].sendBuffer = uint64_t(a);
    hcclMsgArea->sendMsgList[1].recvBuffer = uint64_t(a);
    hcclMsgArea->sendMsgList[1].xorCheck = GenXorStub(&(hcclMsgArea->sendMsgList[1]));
    // finilze
    hcclMsgArea->sendMsgList[2].commType = HCCL_CMD_FINALIZE;
    hcclMsgArea->sendMsgList[2].valid = HCCL_MSG_VALID_MASK;
    hcclMsgArea->sendMsgList[2].xorCheck = GenXorStub(&(hcclMsgArea->sendMsgList[2]));
 
    StubSqeBuffer sqeBufferStub;
    EXPECT_EQ(0, RunAicpuRpcSrvLaunch(&kfcTask));
    EXPECT_EQ(2, ctx->msgPosForKernel);
    EXPECT_EQ(1, ctx->curTurnCntForKernel);
    free(a);
}
 
TEST_F(MC2Reducescatter_ST, reducescatter_mc2Api_repeat)
{
    MOCKER_CPP(&AicpuKfcRpcServerV2::ReadAddrMsg).stubs().will(returnValue(true));
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);
 
    KFCResInitTask initTask;
    init_kfc_args(initTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));
 
    HcclKFCTilingData tilingData = {0};
    tilingData.preparePosition = TASK_PREPARE_KERNEL;
 
    KFCTask kfcTask;
    u64* a = (u64*)malloc(1024*1024);
    kfcTask.context = uint64_t(&paramTask);
    kfcTask.tilingData = uint64_t(&tilingData);
 
    AicpuComContext *ctx = AicpuGetComContext();
    u64 newAddr = ctx->workSpaceAddr;
    if (newAddr & 0x1ff) {
        newAddr = (newAddr & (~((uint64_t)0x1ff))) + 0x200;
    }
    HcclMsgAreaForTest *hcclMsgArea = reinterpret_cast<HcclMsgAreaForTest *>(newAddr);
    // commit
    hcclMsgArea->sendMsgList[0].commType = HCCL_CMD_REDUCE_SCATTER;
    hcclMsgArea->sendMsgList[0].opType = HCCL_REDUCE_SUM;
    hcclMsgArea->sendMsgList[0].hcclDataType = HCCL_DATA_TYPE_FP16;
    hcclMsgArea->sendMsgList[0].valid = HCCL_MSG_VALID_MASK;
    hcclMsgArea->sendMsgList[0].repeatCnt = 2;
    hcclMsgArea->sendMsgList[0].sendBuffer = uint64_t(a);
    hcclMsgArea->sendMsgList[0].recvBuffer = uint64_t(a);
    hcclMsgArea->sendMsgList[0].xorCheck = GenXorStub(&(hcclMsgArea->sendMsgList[0]));
    // finilze
    hcclMsgArea->sendMsgList[1].commType = HCCL_CMD_FINALIZE;
    hcclMsgArea->sendMsgList[1].valid = HCCL_MSG_VALID_MASK;
    hcclMsgArea->sendMsgList[1].xorCheck = GenXorStub(&(hcclMsgArea->sendMsgList[1]));
 
    StubSqeBuffer sqeBufferStub;
    EXPECT_EQ(0, RunAicpuRpcSrvLaunch(&kfcTask));
    EXPECT_EQ(1, ctx->msgPosForKernel);
    EXPECT_EQ(2, ctx->curTurnCntForKernel);
    free(a);
}

TEST_F(MC2Reducescatter_ST, reducescatter_mc2Api_repeat_fail)
{
    MOCKER_CPP(&AicpuKfcRpcServerV2::ReadAddrMsg).stubs().will(returnValue(true));
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);
 
    StubHccCommRes commRes;
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    AicpuKfcRpcServer::RpcMsgBody msgBody;
    paramTask.mc2WorkSpace.workSpace = uint64_t(&msgBody);
 
    KFCResInitTask initTask;
    initTask.context = uint64_t(&paramTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));
 
    HcclKFCTilingData tilingData = {0};
    tilingData.preparePosition = TASK_PREPARE_KERNEL;
 
    KFCTask kfcTask;
    u64* a = (u64*)malloc(1024*1024);
    kfcTask.context = uint64_t(&paramTask);
    kfcTask.tilingData = uint64_t(&tilingData);
 
    AicpuComContext *ctx = AicpuGetComContext();
    u64 newAddr = ctx->workSpaceAddr;
    if (newAddr & 0x1ff) {
        newAddr = (newAddr & (~((uint64_t)0x1ff))) + 0x200;
    }
    HcclMsgAreaForTest *hcclMsgArea = reinterpret_cast<HcclMsgAreaForTest *>(newAddr);
    // commit
    hcclMsgArea->sendMsgList[0].commType = HCCL_CMD_REDUCE_SCATTER;
    hcclMsgArea->sendMsgList[0].opType = HCCL_REDUCE_SUM;
    hcclMsgArea->sendMsgList[0].hcclDataType = HCCL_DATA_TYPE_FP16;
    hcclMsgArea->sendMsgList[0].valid = HCCL_MSG_VALID_MASK;
    hcclMsgArea->sendMsgList[0].repeatCnt = 2;
    hcclMsgArea->sendMsgList[0].strideCount = 64;
    hcclMsgArea->sendMsgList[0].dataCnt = 64;
    hcclMsgArea->sendMsgList[0].sendBuffer = uint64_t(a);
    hcclMsgArea->sendMsgList[0].recvBuffer = uint64_t(a);
    hcclMsgArea->sendMsgList[0].xorCheck = GenXorStub(&(hcclMsgArea->sendMsgList[0]));
    // finilze
    hcclMsgArea->sendMsgList[1].commType = HCCL_CMD_FINALIZE;
    hcclMsgArea->sendMsgList[1].valid = HCCL_MSG_VALID_MASK;
    hcclMsgArea->sendMsgList[1].xorCheck = GenXorStub(&(hcclMsgArea->sendMsgList[1]));
 
    StubSqeBuffer sqeBufferStub;
    EXPECT_EQ(1, RunAicpuRpcSrvLaunch(&kfcTask));
    free(a);
}
