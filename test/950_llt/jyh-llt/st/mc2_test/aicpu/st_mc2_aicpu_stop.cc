#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#ifndef private
#define private public
#define protected public
#endif
#include "common/aicpu_kfc_def.h"
#include "framework/aicpu_hccl_process.h"
#include "framework/aicpu_communicator.h"
#include "framework/aicpu_kfc_rpc_server.h"
#include "framework/aicpu_hdc.h"
#include "framework/aicpu_one_side_service.h"
#include "aicpu_kfc/aicpu_kfc_interface.h"
#include "utils/aicpu_hdc_utils.h"
#include "aicpu_hccl_sqcq.h"
#include "executor_tracer.h"
#include "../stub/llt_hccl_stub_mc2.h"
#include "../stub/llt_hccl_stub.h"
#include "utils/hccl_aicpu_utils.h"
#include "hccl_communicator.h"
#include "stream_pub.h"
#include "dlhal_function.h"
#undef private
#undef protected

using namespace std;
using namespace HcclApi;

constexpr u32 h2dBufferSize = sizeof(KfcExecControl);
constexpr u32 d2hBufferSize = sizeof(KfcExecStatus);

class NsRecovery_ST : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "NsRecovery_ST SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "NsRecovery_ST TearDown" << std::endl;
    }
        virtual void SetUp()
    {
        g_stubDevType = DevType::DEV_TYPE_910B;
        MOCKER(halGetDeviceInfo)
            .stubs()
            .with(any())
            .will(invoke(StubhalGetDeviceInfo));
        DlHalFunction::GetInstance().DlHalFunctionInit();
        hrtSetDevice(0);
        set_chip_type_stub(0, static_cast<s32>(DevType::DEV_TYPE_910B));
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "NsRecovery_ST Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        ResetMC2Context();
        GlobalMockObject::verify();
        std::cout << "NsRecovery_ST Test TearDown" << std::endl;
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
    h2dTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));        \
    d2hTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));        \
    h2dTransfer->InitHost();                                                                \
    d2hTransfer->InitHost();                                                                \
    paramTask.kfcControlTransferH2DParams = h2dTransfer->GetCommunicateParams();                      \
    paramTask.kfcStatusTransferD2HParams = d2hTransfer->GetCommunicateParams();                      \
    paramTask.config.retryEnable = 1;                                                       \
    initTask.context = uint64_t(&paramTask);                                                \


TEST_F(NsRecovery_ST, NsCommStop_NsCommClean_for_a3_mc2) {
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->devId_ = 0;
    hcclCommAicpu->SetIsDeviceMode(true);
    uint8_t hcclMsgArea1[COMM_MAX_WORK_SPACE_SIZE];

    AicpuKfcRpcServerV2 rpcServer;
    rpcServer.Init({(u64)hcclMsgArea1, sizeof(HcclMsgAreaForTest)});
    hcclCommAicpu->SetAicpuRpcServer(reinterpret_cast<u64>(&rpcServer));

    MOCKER(StreamsKill).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(DeviceQuery).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AicpuHdc::SetOpExecStatus, HcclResult (AicpuHdc::*)(std::shared_ptr<HDCommunicate> d2hTransfer, KfcStatus state, KfcError errorCode, u32 retryCount)).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    hcclCommAicpu->NsCommStop();

    MOCKER_CPP(&HcclCommAicpu::ResetSqBuff).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::CleanStreamFunc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    hcclCommAicpu->NsCommClean();

    GlobalMockObject::verify();
    delete hcclCommAicpu;
}

TEST_F(NsRecovery_ST, NsCommStop_for_HcommApi){
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->devId_ = 0;
    // mock拿到的就是cmd就是NsStopExec。但是Stream是错的。
    MOCKER(StreamsKill).stubs().will(returnValue(1));
    MOCKER_CPP(&AicpuHdc::SetOpExecStatus, HcclResult (AicpuHdc::*)(std::shared_ptr<HDCommunicate> d2hTransfer, KfcStatus state, KfcError errorCode, u32 retryCount)).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    hcclCommAicpu->NsCommStop();
    GlobalMockObject::verify();
    // mock拿到的cmd是NsStopExec, 然后DeviceQuery是错误的

    MOCKER(StreamsKill).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(DeviceQuery).stubs().will(returnValue(HCCL_E_DRV));
    MOCKER_CPP(&AicpuHdc::SetOpExecStatus, HcclResult (AicpuHdc::*)(std::shared_ptr<HDCommunicate> d2hTransfer, KfcStatus state, KfcError errorCode, u32 retryCount)).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    hcclCommAicpu->NsCommStop();
    GlobalMockObject::verify();
    // mock拿到的cmd是NsStopExec，然后成功清理完成。并且设置结果。

    MOCKER(StreamsKill).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(DeviceQuery).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AicpuHdc::SetOpExecStatus, HcclResult (AicpuHdc::*)(std::shared_ptr<HDCommunicate> d2hTransfer, KfcStatus state, KfcError errorCode, u32 retryCount)).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    hcclCommAicpu->NsCommStop();

    GlobalMockObject::verify();
    delete hcclCommAicpu;
}

TEST_F(NsRecovery_ST, NsCommClean_for_HcommApi){ //OK

    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    KfcCommand cmd;
    //这里就是拿到了clean命令字
    MOCKER(DeviceQuery).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::ResetSqBuff).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::CleanStreamFunc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AicpuHdc::SetOpExecStatus, HcclResult (AicpuHdc::*)(std::shared_ptr<HDCommunicate> d2hTransfer, KfcStatus state, KfcError errorCode, u32 retryCount)).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    hcclCommAicpu->NsCommClean();
    GlobalMockObject::verify();
    MOCKER(DeviceQuery).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclCommAicpu::CleanStreamFunc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::ResetSqBuff).stubs().with(any()).will(returnValue(HCCL_E_MEMORY));
    MOCKER_CPP(&AicpuHdc::SetOpExecStatus, HcclResult (AicpuHdc::*)(std::shared_ptr<HDCommunicate> d2hTransfer, KfcStatus state, KfcError errorCode, u32 retryCount)).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    hcclCommAicpu->NsCommClean();
    GlobalMockObject::verify();
    MOCKER_CPP(&HcclCommAicpu::CleanStreamFunc).stubs().with(any()).will(returnValue(HCCL_E_SUSPENDING));
    MOCKER_CPP(&AicpuHdc::SetOpExecStatus, HcclResult (AicpuHdc::*)(std::shared_ptr<HDCommunicate> d2hTransfer, KfcStatus state, KfcError errorCode, u32 retryCount)).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    hcclCommAicpu->NsCommClean();
    GlobalMockObject::verify();
    MOCKER(DeviceQuery).stubs().with(any()).will(returnValue(HCCL_E_DRV));
    MOCKER_CPP(&AicpuHdc::SetOpExecStatus, HcclResult (AicpuHdc::*)(std::shared_ptr<HDCommunicate> d2hTransfer, KfcStatus state, KfcError errorCode, u32 retryCount)).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    hcclCommAicpu->NsCommClean();
    GlobalMockObject::verify();
    delete hcclCommAicpu;
}


TEST_F(NsRecovery_ST,KfcCommandHandle_StopExec_clear){ //OK
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);
    AicpuComContext* ctx = AicpuGetComContext();
    ctx->commOpenStatus = false;
    HcclOpResParam context1;
    memset(&context1, 0, sizeof(HcclOpResParam));
    std::string hcomId1 = "hcom_aicpu_comm11";
    context1.winSize = 4096;
    context1.localWindowsIn = reinterpret_cast<u64>(malloc(4096));
    context1.localWindowsOut = reinterpret_cast<u64>(malloc(4096));
    context1.hostStateInfo = reinterpret_cast<u64>(malloc(4096));
    context1.aicpuStateInfo = reinterpret_cast<u64>(malloc(4096));
    strcpy(context1.hcomId, hcomId1.c_str());
    context1.config.retryEnable =1;
    context1.config.retryHoldTime = 10;
    context1.config.retryIntervalTime = 3;
    StubHccCommRes commRes;
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    AicpuKfcRpcServer::RpcMsgBody msgBody;
    paramTask.mc2WorkSpace.workSpace = uint64_t(&msgBody);
    std::shared_ptr<hccl::HDCommunicate> kfcControlTransferH2D_;
    std::shared_ptr<hccl::HDCommunicate> kfcStatusTransferD2H_;
    kfcControlTransferH2D_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    kfcStatusTransferD2H_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    kfcControlTransferH2D_->InitHost();
    kfcStatusTransferD2H_->InitHost();
    context1.kfcControlTransferH2DParams = kfcControlTransferH2D_->GetCommunicateParams();
    context1.kfcStatusTransferD2HParams = kfcStatusTransferD2H_->GetCommunicateParams();
    DeviceMem aicpuCustomDev = DeviceMem::alloc(sizeof(AicpuCustomParam));
    context1.aicpuCustomParamAddr = reinterpret_cast<u64>(aicpuCustomDev.ptr());
    context1.aicpuCustomParamSize = sizeof(AicpuCustomParam);
    SqCqeContext sqeCqeContext;
    context1.localRes.mainStreamParam.sqCqContextAddr = reinterpret_cast<u64>(&sqeCqeContext);
    context1.localRes.mainStreamParam.sqCqContextSize = sizeof(SqCqeContext);
    context1.localRes.streamNum = LOCAL_STREAM_MAX_NUM;
    for (u32 i = 0; i < LOCAL_STREAM_MAX_NUM; i++) {
        context1.localRes.streamParam[i].sqCqContextAddr = reinterpret_cast<u64>(&sqeCqeContext);
        context1.localRes.streamParam[i].sqCqContextSize = sizeof(SqCqeContext);
    }
    MOCKER_CPP(&HcclCommAicpu::SetHrtWorkMode).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitConfigInfo).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitCclbuffer).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitTopoInfo).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitOpNotifyObj).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitTimeOutConfig).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclDispatcherAicpuInit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitLocalNotifyObj).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitMainStreamObj).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitSlaveStreamObjs).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitLocalTagRes).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RefreshTransportsResForRank).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitHostDeviceLock).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitTopoMatcher).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RegisterDispatcherCallback).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitOrderStreamObj).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RecordHostOrder).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    AicpuHcclProcess::AicpuRpcResInitV2(&context1, false);

    KfcExecControl request;
    memset_s(&request, sizeof(KfcExecControl), 0, sizeof(KfcExecControl));
    KfcExecStatus response;
    memset_s(&response, sizeof(KfcExecStatus), 0, sizeof(KfcExecStatus));
    MOCKER_CPP(&HcclCommAicpu::GetCommRecoveryFlag).stubs().will(returnValue(true));
    MOCKER_CPP(&HcclCommAicpu::CleanStreamFunc).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(DeviceQuery).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(StreamsKill).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::ResetSqBuff).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    request.kfcCmd = KfcCommand::NsStopExec;
    kfcControlTransferH2D_->Put(0, sizeof(KfcExecControl), (u8 *)&request);
    dfx_tracer::ExecutorTracer::KfcCommandHandle(ctx);
    kfcStatusTransferD2H_->Get(0, sizeof(KfcExecStatus), (u8 *)&response);
    EXPECT_EQ(response.execStatus.kfcStatus, KfcStatus::kStopExec);

    request.kfcCmd = KfcCommand::NsClear;
    kfcControlTransferH2D_->Put(0, sizeof(KfcExecControl), (u8 *)&request);
    dfx_tracer::ExecutorTracer::KfcCommandHandle(ctx);
    kfcStatusTransferD2H_->Get(0, sizeof(KfcExecStatus), (u8 *)&response);
    EXPECT_EQ(response.execStatus.kfcStatus, KfcStatus::kClear);

    AicpuHcclProcess::AicpuDestoryCommbyGroup(hcomId1);
    free(reinterpret_cast<void *>(context1.localWindowsIn));
    free(reinterpret_cast<void *>(context1.localWindowsOut));
    free(reinterpret_cast<void *>(context1.hostStateInfo));
    free(reinterpret_cast<void *>(context1.aicpuStateInfo));
    aicpuCustomDev.free();
}


TEST_F(NsRecovery_ST,KfcCommandHandle_StopExec_clear_ERROR){ //ok
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);
    AicpuComContext* ctx = AicpuGetComContext();
    ctx->commOpenStatus = false;

    HcclOpResParam context1;
    memset(&context1, 0, sizeof(HcclOpResParam));
    std::string hcomId1 = "hcom_aicpu_comm4";
    context1.winSize = 4096;
    context1.localWindowsIn = reinterpret_cast<u64>(malloc(4096));
    context1.localWindowsOut = reinterpret_cast<u64>(malloc(4096));
    context1.hostStateInfo = reinterpret_cast<u64>(malloc(4096));
    context1.aicpuStateInfo = reinterpret_cast<u64>(malloc(4096));
    strcpy(context1.hcomId, hcomId1.c_str());
    context1.config.retryEnable =1;
    context1.config.retryHoldTime = 10;
    context1.config.retryIntervalTime = 3;
    DeviceMem aicpuCustomDev = DeviceMem::alloc(sizeof(AicpuCustomParam));
    context1.aicpuCustomParamAddr = reinterpret_cast<u64>(aicpuCustomDev.ptr());
    context1.aicpuCustomParamSize = sizeof(AicpuCustomParam);
    SqCqeContext sqeCqeContext;
    context1.localRes.mainStreamParam.sqCqContextAddr = reinterpret_cast<u64>(&sqeCqeContext);
    context1.localRes.mainStreamParam.sqCqContextSize = sizeof(SqCqeContext);
    context1.localRes.streamNum = LOCAL_STREAM_MAX_NUM;
    for (u32 i = 0; i < LOCAL_STREAM_MAX_NUM; i++) {
        context1.localRes.streamParam[i].sqCqContextAddr = reinterpret_cast<u64>(&sqeCqeContext);
        context1.localRes.streamParam[i].sqCqContextSize = sizeof(SqCqeContext);
    }


    std::shared_ptr<hccl::HDCommunicate> kfcControlTransferH2D_;
    std::shared_ptr<hccl::HDCommunicate> kfcStatusTransferD2H_;
    kfcControlTransferH2D_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    kfcStatusTransferD2H_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    kfcControlTransferH2D_->InitHost();
    kfcStatusTransferD2H_->InitHost();

    context1.kfcControlTransferH2DParams = kfcControlTransferH2D_->GetCommunicateParams();
    context1.kfcStatusTransferD2HParams = kfcStatusTransferD2H_->GetCommunicateParams();

    MOCKER_CPP(&HcclCommAicpu::SetHrtWorkMode).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitConfigInfo).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitCclbuffer).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitTopoInfo).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitOpNotifyObj).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitTimeOutConfig).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclDispatcherAicpuInit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitLocalNotifyObj).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitMainStreamObj).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitSlaveStreamObjs).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitLocalTagRes).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RefreshTransportsResForRank).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitHostDeviceLock).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitTopoMatcher).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RegisterDispatcherCallback).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitOrderStreamObj).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RecordHostOrder).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    AicpuHcclProcess::AicpuRpcResInitV2(&context1, false);

    KfcExecControl request;
    memset_s(&request, sizeof(KfcExecControl), 0, sizeof(KfcExecControl));
    KfcExecStatus response;
    memset_s(&response, sizeof(KfcExecStatus), 0, sizeof(KfcExecStatus));
    MOCKER_CPP(&HcclCommAicpu::GetCommRecoveryFlag).stubs().will(returnValue(true));
    MOCKER(DeviceQuery).stubs().with(any()).will(returnValue(HCCL_E_DRV));

    request.kfcCmd = KfcCommand::NsStopExec;
    kfcControlTransferH2D_->Put(0, sizeof(KfcExecControl), (u8 *)&request);
    dfx_tracer::ExecutorTracer::KfcCommandHandle(ctx);
    kfcStatusTransferD2H_->Get(0, sizeof(KfcExecStatus), (u8 *)&response);
    EXPECT_EQ(response.execStatus.kfcStatus, KfcStatus::kError);

    request.kfcCmd = KfcCommand::NsClear;
    kfcControlTransferH2D_->Put(0, sizeof(KfcExecControl), (u8 *)&request);
    dfx_tracer::ExecutorTracer::KfcCommandHandle(ctx);
    kfcStatusTransferD2H_->Get(0, sizeof(KfcExecStatus), (u8 *)&response);
    EXPECT_EQ(response.execStatus.kfcStatus, KfcStatus::kError);
    AicpuHcclProcess::AicpuDestoryCommbyGroup(hcomId1);
    free(reinterpret_cast<void *>(context1.localWindowsIn));
    free(reinterpret_cast<void *>(context1.localWindowsOut));
    free(reinterpret_cast<void *>(context1.hostStateInfo));
    free(reinterpret_cast<void *>(context1.aicpuStateInfo));
    aicpuCustomDev.free();
}

TEST_F(NsRecovery_ST, HandleHcclCommAicpu){ //ok

    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->SetCommRecoveryFlag(true);
    hcclCommAicpu->SetNsStopLaunchStatus(true);
    hcclCommAicpu->GetCommRecoveryFlag();
    hcclCommAicpu->GetNsStopLaunchStatus();
    hcclCommAicpu->GetCommInfoStatus();
    hcclCommAicpu->InitCommInfoStatus(true);
    hcclCommAicpu->BackGroundGetOpStatus();
    delete hcclCommAicpu;
}

TEST_F(NsRecovery_ST, StopBackGround){ //ok
    AicpuComContext* ctx = AicpuGetComContext();
    ctx->commOpenStatus = true;
    bool isStop = false;
    dfx_tracer::ExecutorTracer::StopBackGround(ctx, isStop);
    EXPECT_EQ(isStop, true);

    ctx->commOpenStatus = false;
     HcclOpResParam context1;
    memset(&context1, 0, sizeof(HcclOpResParam));
    std::string hcomId1 = "hcom_aicpu_comm2";
    context1.winSize = 4096;
    context1.localWindowsIn = reinterpret_cast<u64>(malloc(4096));
    context1.localWindowsOut = reinterpret_cast<u64>(malloc(4096));
    context1.hostStateInfo = reinterpret_cast<u64>(malloc(4096));
    context1.aicpuStateInfo = reinterpret_cast<u64>(malloc(4096));
    strcpy(context1.hcomId, hcomId1.c_str());
    context1.config.retryEnable =1;
    context1.config.retryHoldTime = 10;
    context1.config.retryIntervalTime = 3;


    std::shared_ptr<hccl::HDCommunicate> kfcControlTransferH2D_;
    std::shared_ptr<hccl::HDCommunicate> kfcStatusTransferD2H_;
    kfcControlTransferH2D_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    kfcStatusTransferD2H_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    kfcControlTransferH2D_->InitHost();
    kfcStatusTransferD2H_->InitHost();

    context1.kfcControlTransferH2DParams = kfcControlTransferH2D_->GetCommunicateParams();
    context1.kfcStatusTransferD2HParams = kfcStatusTransferD2H_->GetCommunicateParams();
    DeviceMem aicpuCustomDev = DeviceMem::alloc(sizeof(AicpuCustomParam));
    context1.aicpuCustomParamAddr = reinterpret_cast<u64>(aicpuCustomDev.ptr());
    context1.aicpuCustomParamSize = sizeof(AicpuCustomParam);
    SqCqeContext sqeCqeContext;
    context1.localRes.mainStreamParam.sqCqContextAddr = reinterpret_cast<u64>(&sqeCqeContext);
    context1.localRes.mainStreamParam.sqCqContextSize = sizeof(SqCqeContext);
    context1.localRes.streamNum = LOCAL_STREAM_MAX_NUM;
    for (u32 i = 0; i < LOCAL_STREAM_MAX_NUM; i++) {
        context1.localRes.streamParam[i].sqCqContextAddr = reinterpret_cast<u64>(&sqeCqeContext);
        context1.localRes.streamParam[i].sqCqContextSize = sizeof(SqCqeContext);
    }

    MOCKER_CPP(&HcclCommAicpu::SetHrtWorkMode).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitConfigInfo).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitCclbuffer).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitTopoInfo).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitOpNotifyObj).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitTimeOutConfig).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclDispatcherAicpuInit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitLocalNotifyObj).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitMainStreamObj).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitSlaveStreamObjs).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitLocalTagRes).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RefreshTransportsResForRank).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitHostDeviceLock).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitTopoMatcher).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RegisterDispatcherCallback).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitOrderStreamObj).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RecordHostOrder).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    AicpuHcclProcess::AicpuRpcResInitV2(&context1, false);

    isStop = false;
    dfx_tracer::ExecutorTracer::StopBackGround(ctx, isStop);
    EXPECT_EQ(isStop, true);
    AicpuHcclProcess::AicpuDestoryCommbyGroup(hcomId1);
    free(reinterpret_cast<void *>(context1.localWindowsIn));
    free(reinterpret_cast<void *>(context1.localWindowsOut));
    free(reinterpret_cast<void *>(context1.hostStateInfo));
    free(reinterpret_cast<void *>(context1.aicpuStateInfo));
    aicpuCustomDev.free();
}

TEST_F(NsRecovery_ST,HandleBackGrond_ctx){ //ok
    KFCResInitTask initTask;
    init_kfc_args(initTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));
    AicpuComContext* ctx = AicpuGetComContext();
    ctx->commOpenStatus = true;
    KfcExecControl request;
    memset_s(&request, sizeof(KfcExecControl), 0, sizeof(KfcExecControl));
    KfcExecStatus response;
    memset_s(&response, sizeof(KfcExecStatus), 0, sizeof(KfcExecStatus));
     thread threadHandle([&] {
        while (true) {
            request.bgCmd = BackgroundCommand::kStop;
            h2dTransfer->Put(0, sizeof(KfcExecControl), (u8 *)&request);  // 从host侧拿到KstopLaunch的命令字
            usleep(10000);
            dfx_tracer::ExecutorTracer::HandleBackGround(ctx);
            d2hTransfer->Get(0, sizeof(KfcExecStatus), (u8 *)&response);  // device就会把状态改为KStopLaunch
            if (response.execStatus.backgroundStatus != BackgroundStatus::kNone) {
                break;
            }
        }
        EXPECT_EQ(response.execStatus.backgroundStatus, BackgroundStatus ::kStop);  // 这个时候就是希望从host侧拿到的命令字NsStopLaunch
    });
    threadHandle.join();
}


TEST_F(NsRecovery_ST, HandleBackGrond_aicpu) //ok
{
    AicpuComContext* ctx = AicpuGetComContext();
    ctx->commOpenStatus = false;
    ctx->isRunning = true;

    HcclOpResParam context1;
    memset(&context1, 0, sizeof(HcclOpResParam));
    std::string hcomId1 = "hcom_aicpu_comm3";
    context1.winSize = 4096;
    context1.localWindowsIn = reinterpret_cast<u64>(malloc(4096));
    context1.localWindowsOut = reinterpret_cast<u64>(malloc(4096));
    context1.hostStateInfo = reinterpret_cast<u64>(malloc(4096));
    context1.aicpuStateInfo = reinterpret_cast<u64>(malloc(4096));
    strcpy(context1.hcomId, hcomId1.c_str());
    context1.config.retryEnable =1;
    context1.config.retryHoldTime = 10;
    context1.config.retryIntervalTime = 3;


    std::shared_ptr<hccl::HDCommunicate> kfcControlTransferH2D_;
    std::shared_ptr<hccl::HDCommunicate> kfcStatusTransferD2H_;
    kfcControlTransferH2D_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    kfcStatusTransferD2H_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    kfcControlTransferH2D_->InitHost();
    kfcStatusTransferD2H_->InitHost();

    context1.kfcControlTransferH2DParams = kfcControlTransferH2D_->GetCommunicateParams();
    context1.kfcStatusTransferD2HParams = kfcStatusTransferD2H_->GetCommunicateParams();
    DeviceMem aicpuCustomDev = DeviceMem::alloc(sizeof(AicpuCustomParam));
    context1.aicpuCustomParamAddr = reinterpret_cast<u64>(aicpuCustomDev.ptr());
    context1.aicpuCustomParamSize = sizeof(AicpuCustomParam);
    SqCqeContext sqeCqeContext;
    context1.localRes.mainStreamParam.sqCqContextAddr = reinterpret_cast<u64>(&sqeCqeContext);
    context1.localRes.mainStreamParam.sqCqContextSize = sizeof(SqCqeContext);
    context1.localRes.streamNum = LOCAL_STREAM_MAX_NUM;
    for (u32 i = 0; i < LOCAL_STREAM_MAX_NUM; i++) {
        context1.localRes.streamParam[i].sqCqContextAddr = reinterpret_cast<u64>(&sqeCqeContext);
        context1.localRes.streamParam[i].sqCqContextSize = sizeof(SqCqeContext);
    }

    MOCKER_CPP(&HcclCommAicpu::SetHrtWorkMode).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitConfigInfo).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitCclbuffer).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitTopoInfo).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitOpNotifyObj).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitTimeOutConfig).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclDispatcherAicpuInit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitLocalNotifyObj).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitMainStreamObj).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitSlaveStreamObjs).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitLocalTagRes).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RefreshTransportsResForRank).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitHostDeviceLock).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitTopoMatcher).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RegisterDispatcherCallback).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitOrderStreamObj).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RecordHostOrder).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    AicpuHcclProcess::AicpuRpcResInitV2(&context1, false);
    AicpuHcclProcess::AicpuDestoryCommbyGroup(hcomId1);
    free(reinterpret_cast<void *>(context1.localWindowsIn));
    free(reinterpret_cast<void *>(context1.localWindowsOut));
    free(reinterpret_cast<void *>(context1.hostStateInfo));
    free(reinterpret_cast<void *>(context1.aicpuStateInfo));
    aicpuCustomDev.free();
}


TEST_F(NsRecovery_ST,StopLaunchCommandHandle_ctx){
    MOCKER_CPP(&HcclCommAicpu::GenTaskExceptionInfo).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    AicpuComContext *ctx = AicpuGetComContext();
    ctx->commOpenStatus = true;
    ctx->isOpLaunch = false;
    StubHccCommRes commRes;
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    AicpuKfcRpcServer::RpcMsgBody msgBody;
    paramTask.mc2WorkSpace.workSpace = uint64_t(&msgBody);
    std::shared_ptr<hccl::HDCommunicate> kfcControlTransferH2D_;
    std::shared_ptr<hccl::HDCommunicate> kfcStatusTransferD2H_;
    kfcControlTransferH2D_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    kfcStatusTransferD2H_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    kfcControlTransferH2D_->InitHost();
    kfcStatusTransferD2H_->InitHost();
    paramTask.kfcControlTransferH2DParams = kfcControlTransferH2D_->GetCommunicateParams();
    paramTask.kfcStatusTransferD2HParams = kfcStatusTransferD2H_->GetCommunicateParams();
    ctx->kfcControlTransferH2D.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    ctx->kfcStatusTransferD2H.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    ctx->kfcControlTransferH2D->InitDevice(paramTask.kfcControlTransferH2DParams);
    ctx->kfcStatusTransferD2H->InitDevice(paramTask.kfcStatusTransferD2HParams);
    KfcExecControl request;
    memset_s(&request, sizeof(KfcExecControl), 0, sizeof(KfcExecControl));
    KfcExecStatus response;
    memset_s(&response, sizeof(KfcExecStatus), 0, sizeof(KfcExecStatus));
     thread threadHandle([&] {
        while (true) {
            request.kfcCmd = KfcCommand::NsStopLaunch;
            kfcControlTransferH2D_->Put(0, sizeof(KfcExecControl), (u8 *)&request);  // 从host侧拿到KstopLaunch的命令字
            usleep(10000);
            dfx_tracer::ExecutorTracer::StopLaunchCommandHandle(ctx);
            kfcStatusTransferD2H_->Get(0, sizeof(KfcExecStatus), (u8 *)&response);  // device就会把状态改为KStopLaunch
            if (response.execStatus.kfcStatus != KfcStatus::kNull) {
                break;
            }
        }
        EXPECT_EQ(response.execStatus.kfcStatus, KfcStatus ::kStoplaunch);  // 这个时候就是希望从host侧拿到的命令字NsStopLaunch
    });
    threadHandle.join();

}

HcclResult PutStubCmd(hccl::HDCommunicate*This,unsigned int offset, unsigned int length, unsigned char* value){
    return HCCL_SUCCESS;
}
TEST_F(NsRecovery_ST, HostMC2EnvResume){ //ok
  dlog_setlevel(HCCL, DLOG_DEBUG, 0);
  HcclCommunicator hcclCommunicator;
  hcclCommunicator.SetAicpuCommEngine(true);
  hcclCommunicator.SetMC2EnvFlag();
  hcclCommunicator.kfcControlTransferH2D_ .reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
  hcclCommunicator.kfcControlTransferH2D_->InitHost();
  MOCKER_CPP(&HDCommunicate::Put).stubs().will(invoke(PutStubCmd));
  auto ret = hcclCommunicator.HostMC2EnvResume();
  EXPECT_EQ(HCCL_SUCCESS, ret);
  hcclCommunicator.isAicpuCommEngine_ = false;
  hcclCommunicator.isNsRecovery_ = false;
}

TEST_F(NsRecovery_ST, StopLaunchCommandHandle_aicpu){
    AicpuComContext *ctx = AicpuGetComContext();
    ctx->commOpenStatus = false;

    HcclOpResParam context1;
    memset(&context1, 0, sizeof(HcclOpResParam));
    std::string hcomId1 = "hcom_aicpu_comm6";
    context1.winSize = 4096;
    context1.localWindowsIn = reinterpret_cast<u64>(malloc(4096));
    context1.localWindowsOut = reinterpret_cast<u64>(malloc(4096));
    context1.hostStateInfo = reinterpret_cast<u64>(malloc(4096));
    context1.aicpuStateInfo = reinterpret_cast<u64>(malloc(4096));
    strcpy(context1.hcomId, hcomId1.c_str());
    context1.config.retryEnable =1;
    context1.config.retryHoldTime = 10;
    context1.config.retryIntervalTime = 3;


    std::shared_ptr<hccl::HDCommunicate> kfcControlTransferH2D_;
    std::shared_ptr<hccl::HDCommunicate> kfcStatusTransferD2H_;
    kfcControlTransferH2D_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    kfcStatusTransferD2H_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    kfcControlTransferH2D_->InitHost();
    kfcStatusTransferD2H_->InitHost();

    context1.kfcControlTransferH2DParams = kfcControlTransferH2D_->GetCommunicateParams();
    context1.kfcStatusTransferD2HParams = kfcStatusTransferD2H_->GetCommunicateParams();
    DeviceMem aicpuCustomDev = DeviceMem::alloc(sizeof(AicpuCustomParam));
    context1.aicpuCustomParamAddr = reinterpret_cast<u64>(aicpuCustomDev.ptr());
    context1.aicpuCustomParamSize = sizeof(AicpuCustomParam);
    SqCqeContext sqeCqeContext;
    context1.localRes.mainStreamParam.sqCqContextAddr = reinterpret_cast<u64>(&sqeCqeContext);
    context1.localRes.mainStreamParam.sqCqContextSize = sizeof(SqCqeContext);
    context1.localRes.streamNum = LOCAL_STREAM_MAX_NUM;
    for (u32 i = 0; i < LOCAL_STREAM_MAX_NUM; i++) {
        context1.localRes.streamParam[i].sqCqContextAddr = reinterpret_cast<u64>(&sqeCqeContext);
        context1.localRes.streamParam[i].sqCqContextSize = sizeof(SqCqeContext);
    }

    MOCKER_CPP(&HcclCommAicpu::SetHrtWorkMode).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitConfigInfo).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitCclbuffer).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitTopoInfo).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitOpNotifyObj).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitTimeOutConfig).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclDispatcherAicpuInit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitLocalNotifyObj).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitMainStreamObj).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitSlaveStreamObjs).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitLocalTagRes).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RefreshTransportsResForRank).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitHostDeviceLock).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitTopoMatcher).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RegisterDispatcherCallback).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitOrderStreamObj).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RecordHostOrder).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    AicpuHcclProcess::AicpuRpcResInitV2(&context1, false);

    KfcExecControl request;
    memset_s(&request, sizeof(KfcExecControl), 0, sizeof(KfcExecControl));
    KfcExecStatus response;
    memset_s(&response, sizeof(KfcExecStatus), 0, sizeof(KfcExecStatus));
    MOCKER_CPP(&HcclCommAicpu::GetNsStopLaunchStatus).stubs().with(any()).will(returnValue(false));
    MOCKER_CPP(&HcclCommAicpu::BackGroundGetOpStatus).stubs().with(any()).will(returnValue(false));
    thread threadHandle([&] {
        while (true) {
            request.kfcCmd = KfcCommand::NsStopLaunch;
            kfcControlTransferH2D_->Put(0, sizeof(KfcExecControl), (u8 *)&request);  // 从host侧拿到KstopLaunch的命令字
            usleep(10000);
            dfx_tracer::ExecutorTracer::StopLaunchCommandHandle(ctx);
            kfcStatusTransferD2H_->Get(0, sizeof(KfcExecStatus), (u8 *)&response);  // device就会把状态改为KStopLaunch
            if (response.execStatus.kfcStatus != KfcStatus::kNull) {
                break;
            }
        }
        EXPECT_EQ(response.execStatus.kfcStatus,KfcStatus ::kStoplaunch);  // 这个时候就是希望从host侧拿到的命令字NsStopLaunch
    });
    threadHandle.join();
    AicpuHcclProcess::AicpuDestoryCommbyGroup(hcomId1);
    free(reinterpret_cast<void *>(context1.localWindowsIn));
    free(reinterpret_cast<void *>(context1.localWindowsOut));
    free(reinterpret_cast<void *>(context1.hostStateInfo));
    free(reinterpret_cast<void *>(context1.aicpuStateInfo));
    aicpuCustomDev.free();
}

TEST_F(NsRecovery_ST, st_CleanStreamTrue) {
    hccl::HcclOneSideServiceAicpu *oneSideServiceAicpu = new hccl::HcclOneSideServiceAicpu;
    auto ret = oneSideServiceAicpu->CleanStreamFunc();
    EXPECT_EQ(HCCL_SUCCESS, ret);
    delete oneSideServiceAicpu;
}
