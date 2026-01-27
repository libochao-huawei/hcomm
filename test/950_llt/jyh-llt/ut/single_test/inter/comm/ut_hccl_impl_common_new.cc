#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <runtime/rt.h>
#include <cassert>
#include <semaphore.h>
#include <csignal>
#include <syscall.h>
#include <sys/prctl.h>
#include <syslog.h>
#include <unistd.h>
#include <cerrno>
#include <cce/dnn.h>
#include <securec.h>

#include <sys/types.h>
#include <cstddef>
#include <sys/mman.h>
#include <fcntl.h>
#include <driver/ascend_hal.h>
#include <runtime/rt.h>

#define private public
#define protected public

#include "opexecounter_pub.h"
#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "hccl_communicator.h"
#include "hccl_communicator_attrs.h"
#include "llt_hccl_stub_pub.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "sal.h"
#include "dlra_function.h"
#include "externalinput_pub.h"
#include "config.h"
#include "topoinfo_ranktableParser_pub.h"
#include "rank_consistentcy_checker.h"
#include "workflow_pub.h"
#include "workflow_pub.h"
#include "../misc/ut_rank_table.h"
#include "dltdt_function.h"
#include "param_check_pub.h"
#include "externalinput.h"
#include <iostream>
#include <fstream>
#include "stream_utils.h"
#include "order_launch/order_launch.h"

#undef protected
#undef private

using namespace std;
using namespace hccl;

class HcclImplCommonNewTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "HcclImplCommonNewTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "HcclImplCommonNewTest TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        DlTdtFunction::GetInstance().DlTdtFunctionInit();
        TsdOpen(1, 2);
        static s32  call_cnt = 0;
        string name = std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(0, "UT_TEST");
        set_board_id(0x2000);
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        TsdClose(1);
        set_board_id(0x0000);
        std::cout << "A Test TearDown" << std::endl;
    }
};

TEST_F(HcclImplCommonNewTest, ut_hccl_impl_comm_error)
{
    HcclCommParams params;
    string commId = "AllReduce";
    memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
    params.rank = 0;
    params.totalRanks = 2;
    params.isHeterogComm = false;
    params.logicDevId = 0;
    params.deviceType = DevType::DEV_TYPE_310P3;

    RankTable_t rankTable;
    rankTable.collectiveId = "192.168.0.101-8000-8001";
    vector<RankInfo_t> rankVec(2);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";
    rankVec[1].rankId = 1;
    rankVec[1].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr2(1711319232);
    rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2); // 101.0.168.192
    rankVec[1].serverIdx = 1;
    rankVec[1].serverId = "192.168.0.102";
    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.deviceNum = 2;
    rankTable.serverNum = 1;
    aclrtSetDevice(0);

    HcclRtStream opStream;
    rtStream_t stream;
    HcclCommunicator communicator;
    HcclResult ret = communicator.Init(params, rankTable);

    bool bret = communicator.GetCommResource(" ", nullptr);
    EXPECT_EQ(bret, false);

    ret = communicator.AlltoAllVOutPlace(nullptr, nullptr, nullptr, HCCL_DATA_TYPE_FP32, nullptr, nullptr, nullptr,
        HCCL_DATA_TYPE_FP32, stream, " ");
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = communicator.AlltoAllVCOutPlace(nullptr, nullptr, HCCL_DATA_TYPE_FP32, nullptr, HCCL_DATA_TYPE_FP32, stream, " ");
    EXPECT_EQ(ret, HCCL_SUCCESS);

    setenv("HCCL_ALGO", "level0:NA;level1:H-D_R", 1);
    ResetInitState();
    InitExternalInput();
    ret = communicator.BroadcastOutPlace(" ", nullptr, 0, HCCL_DATA_TYPE_FP32, 0, opStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    unsetenv("HCCL_ALGO");
    ResetInitState();
    InitExternalInput();

    ret = communicator.ReduceOutPlace(" ", nullptr, nullptr, 0, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_RESERVED, 0, opStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    /* DEV_TYPE_310P3场景下不支持Send */
    ret = communicator.SendOutPlace(" ", nullptr, 0, HCCL_DATA_TYPE_FP32, 0, opStream);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    /* DEV_TYPE_310P3场景下不支持Receive */
    ret = communicator.ReceiveOutPlace(" ", nullptr, 0, HCCL_DATA_TYPE_FP32, 0, opStream);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}

TEST_F(HcclImplCommonNewTest, ut_HcclCommunicator_postSync)
{
    u64 one = 1;
    MOCKER_CPP(&HcclCommunicator::CalcOpTilingDynamicDataSize)
        .stubs()
        .will(returnValue(one));
    MOCKER_CPP(&HcclCommunicator::AicpuInitOpTilingDataBuf)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::AicpuKfcTilingDataLaunchIn)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    HcclCommParams params;
    string commId = "AllReduce";
    memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
    params.rank = 0;
    params.totalRanks = 2;
    params.isHeterogComm = false;
    params.logicDevId = 0;
    params.deviceType = DevType::DEV_TYPE_910_93;

    RankTable_t rankTable;
    rankTable.collectiveId = "192.168.0.101-8000-8001";
    vector<RankInfo_t> rankVec(2);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";
    rankVec[1].rankId = 1;
    rankVec[1].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr2(1711319232);
    rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2); // 101.0.168.192
    rankVec[1].serverIdx = 1;
    rankVec[1].serverId = "192.168.0.102";
    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.deviceNum = 2;
    rankTable.serverNum = 2;
    aclrtSetDevice(0);

    HcclRtStream opStream;
    rtStream_t stream;
    HcclCommunicator communicator;
    HcclResult ret = communicator.Init(params, rankTable);

    bool bret = communicator.GetCommResource(" ", nullptr);
    EXPECT_EQ(bret, false);

    OpParam opParam;
    communicator.retryEnable_ = true;
    HcclCMDType opType = HcclCMDType::HCCL_CMD_ALLTOALLV;
    DeviceMem deviceContext;
    std::string kernelName = "";
    AicpuOpTiling opTilingInfo;
    ret = communicator.AicpuKfcTilingDataLaunchExt(
        opParam, opType, deviceContext, kernelName, opTilingInfo);
}

TEST_F(HcclImplCommonNewTest, ut_HcclCommunicator_AicpuKfcTilingDataLaunchExt_Reduce_PostSync)
{
    u64 one = 1;
    MOCKER_CPP(&HcclCommunicator::CalcOpTilingDynamicDataSize)
        .stubs()
        .will(returnValue(one));
    MOCKER_CPP(&HcclCommunicator::AicpuInitOpTilingDataBuf)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::AicpuKfcTilingDataLaunchIn)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    HcclCommParams params;
    string commId = "Reduce";
    memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
    params.rank = 0;
    params.totalRanks = 2;
    params.isHeterogComm = false;
    params.logicDevId = 0;
    params.deviceType = DevType::DEV_TYPE_910_93;

    RankTable_t rankTable;
    rankTable.collectiveId = "192.168.0.101-8000-8001";
    vector<RankInfo_t> rankVec(2);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";
    rankVec[1].rankId = 1;
    rankVec[1].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr2(1711319232);
    rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2); // 101.0.168.192
    rankVec[1].serverIdx = 1;
    rankVec[1].serverId = "192.168.0.102";
    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.deviceNum = 2;
    rankTable.serverNum = 2;
    aclrtSetDevice(0);

    HcclRtStream opStream;
    rtStream_t stream;
    HcclCommunicator communicator;
    HcclResult ret = communicator.Init(params, rankTable);
    communicator.superPodNum_ = 2;
    bool bret = communicator.GetCommResource(" ", nullptr);
    EXPECT_EQ(bret, false);

    OpParam opParam;
    communicator.retryEnable_ = true;
    HcclCMDType opType = HcclCMDType::HCCL_CMD_REDUCE;
    DeviceMem deviceContext;
    std::string kernelName = "";
    AicpuOpTiling opTilingInfo;
    ret = communicator.AicpuKfcTilingDataLaunchExt(
        opParam, opType, deviceContext, kernelName, opTilingInfo);
}

TEST_F(HcclImplCommonNewTest, ut_HcclCommunicator_AicpuKfcTilingDataLaunchExt_ReduceScatter_PostSync)
{
    u64 one = 1;
    MOCKER_CPP(&HcclCommunicator::CalcOpTilingDynamicDataSize)
        .stubs()
        .will(returnValue(one));
    MOCKER_CPP(&HcclCommunicator::AicpuInitOpTilingDataBuf)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::AicpuKfcTilingDataLaunchIn)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    HcclCommParams params;
    string commId = "ReduceScatter";
    memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
    params.rank = 0;
    params.totalRanks = 2;
    params.isHeterogComm = false;
    params.logicDevId = 0;
    params.deviceType = DevType::DEV_TYPE_910_93;

    RankTable_t rankTable;
    rankTable.collectiveId = "192.168.0.101-8000-8001";
    vector<RankInfo_t> rankVec(2);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";
    rankVec[1].rankId = 1;
    rankVec[1].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr2(1711319232);
    rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2); // 101.0.168.192
    rankVec[1].serverIdx = 1;
    rankVec[1].serverId = "192.168.0.102";
    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.deviceNum = 2;
    rankTable.serverNum = 2;
    aclrtSetDevice(0);

    HcclRtStream opStream;
    rtStream_t stream;
    HcclCommunicator communicator;
    HcclResult ret = communicator.Init(params, rankTable);
    communicator.superPodNum_ = 2;

    bool bret = communicator.GetCommResource(" ", nullptr);
    EXPECT_EQ(bret, false);

    OpParam opParam;
    communicator.retryEnable_ = true;
    communicator.inPlaceSupportRetryStatus_ = InplaceSupportRetryStatus::USER_LARGER_THAN_CCL;

    HcclCMDType opType = HcclCMDType::HCCL_CMD_REDUCE_SCATTER ;
    DeviceMem deviceContext;
    std::string kernelName = "";
    AicpuOpTiling opTilingInfo;
    ret = communicator.AicpuKfcTilingDataLaunchExt(
        opParam, opType, deviceContext, kernelName, opTilingInfo);
}

TEST_F(HcclImplCommonNewTest, ut_HcclCommunicator_AicpuKfcTilingDataLaunchExt_Allreduce_PreSync)
{
    u64 one = 1;
    MOCKER_CPP(&HcclCommunicator::CalcOpTilingDynamicDataSize)
        .stubs()
        .will(returnValue(one));
    MOCKER_CPP(&HcclCommunicator::AicpuInitOpTilingDataBuf)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::AicpuKfcTilingDataLaunchIn)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    HcclCommParams params;
    string commId = "Allreduce";
    memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
    params.rank = 0;
    params.totalRanks = 2;
    params.isHeterogComm = false;
    params.logicDevId = 0;
    params.deviceType = DevType::DEV_TYPE_910_93;

    RankTable_t rankTable;
    rankTable.collectiveId = "192.168.0.101-8000-8001";
    vector<RankInfo_t> rankVec(2);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";
    rankVec[1].rankId = 1;
    rankVec[1].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr2(1711319232);
    rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2); // 101.0.168.192
    rankVec[1].serverIdx = 1;
    rankVec[1].serverId = "192.168.0.102";
    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.deviceNum = 2;
    rankTable.serverNum = 2;
    aclrtSetDevice(0);

    HcclRtStream opStream;
    rtStream_t stream;
    HcclCommunicator communicator;
    HcclResult ret = communicator.Init(params, rankTable);
    communicator.superPodNum_ = 2;

    bool bret = communicator.GetCommResource(" ", nullptr);
    EXPECT_EQ(bret, false);

    OpParam opParam;
    communicator.retryEnable_ = true;
    communicator.inPlaceSupportRetryStatus_ = InplaceSupportRetryStatus::USER_LARGER_THAN_CCL;

    HcclCMDType opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    DeviceMem deviceContext;
    std::string kernelName = "";
    AicpuOpTiling opTilingInfo;
    ret = communicator.AicpuKfcTilingDataLaunchExt(
        opParam, opType, deviceContext, kernelName, opTilingInfo);
}

TEST_F(HcclImplCommonNewTest, ut_HcclCommunicator_AicpuKDataLaunch)
{
    GlobalMockObject::verify();

    MOCKER(LocalNotify::Post)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclCommParams params;
    string commId = "Allreduce";
    memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
    params.rank = 0;
    params.totalRanks = 2;
    params.isHeterogComm = false;
    params.logicDevId = 0;
    params.deviceType = DevType::DEV_TYPE_910_93;

    RankTable_t rankTable;
    rankTable.collectiveId = "192.168.0.101-8000-8001";
    vector<RankInfo_t> rankVec(2);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";
    rankVec[1].rankId = 1;
    rankVec[1].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr2(1711319232);
    rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2); // 101.0.168.192
    rankVec[1].serverIdx = 1;
    rankVec[1].serverId = "192.168.0.102";
    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.deviceNum = 2;
    rankTable.serverNum = 2;
    aclrtSetDevice(0);

    HcclRtStream opStream;
    rtStream_t stream;
    HcclCommunicator communicator;
    HcclResult ret = communicator.Init(params, rankTable);
    communicator.superPodNum_ = 2;

    MOCKER_CPP_VIRTUAL(communicator, &HcclCommunicator::AicpuUnfoldKernelLaunchV2)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    OrderLaunch& orderLaunch = OrderLaunch::GetInstance(0);
    MOCKER_CPP(&OrderLaunch::LaunchInOrder).stubs().will(returnValue(HCCL_SUCCESS));

    bool bret = communicator.GetCommResource(" ", nullptr);
    EXPECT_EQ(bret, false);

    OpParam opParam;
    communicator.retryEnable_ = true;
    communicator.inPlaceSupportRetryStatus_ = InplaceSupportRetryStatus::USER_LARGER_THAN_CCL;

    HcclCMDType opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    DeviceMem deviceContext;
    std::string kernelName = "";
    AicpuOpTiling opTilingInfo;

    ret = communicator.AicpuKfcTilingDataLaunchIn(
        opParam, deviceContext, kernelName, opTilingInfo, sizeof(struct OpTilingData));
    GlobalMockObject::verify();
}


TEST_F(HcclImplCommonNewTest, ut_HcclCommunicator_AicpuKDataLaunch_Capture)
{
    GlobalMockObject::verify();

    MOCKER(LocalNotify::Post)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtGetStreamId)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    rtStreamCaptureStatus captureStatus = rtStreamCaptureStatus::RT_STREAM_CAPTURE_STATUS_ACTIVE;
    int mockModel = 0;
    void *pmockModel = &mockModel;    
    MOCKER(rtStreamGetCaptureInfo)
    .stubs()
    .with(any(), outBoundP(&captureStatus, sizeof(captureStatus)), outBoundP(&pmockModel, sizeof(pmockModel)))
    .will(returnValue(0));

    MOCKER(rtStreamAddToModel)
    .stubs()
    .with(any())
    .will(returnValue(0));

    HcclCommParams params;
    string commId = "Allreduce";
    memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
    params.rank = 0;
    params.totalRanks = 2;
    params.isHeterogComm = false;
    params.logicDevId = 0;
    params.deviceType = DevType::DEV_TYPE_910_93;

    RankTable_t rankTable;
    rankTable.collectiveId = "192.168.0.101-8000-8001";
    vector<RankInfo_t> rankVec(2);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";
    rankVec[1].rankId = 1;
    rankVec[1].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr2(1711319232);
    rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2); // 101.0.168.192
    rankVec[1].serverIdx = 1;
    rankVec[1].serverId = "192.168.0.102";
    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.deviceNum = 2;
    rankTable.serverNum = 2;
    aclrtSetDevice(0);

    HcclCommunicator communicator;
    HcclResult ret = communicator.Init(params, rankTable);
    communicator.superPodNum_ = 2;

    MOCKER_CPP_VIRTUAL(communicator, &HcclCommunicator::AicpuUnfoldKernelLaunchV2)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    bool bret = communicator.GetCommResource(" ", nullptr);
    EXPECT_EQ(bret, false);

    OpParam opParam;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);
    opParam.isCapture = true;
    communicator.retryEnable_ = false;
    communicator.inPlaceSupportRetryStatus_ = InplaceSupportRetryStatus::USER_LARGER_THAN_CCL;
    communicator.opStream_ = Stream(StreamType::STREAM_TYPE_ONLINE);

    for (u32 i = 0; i < AICPU_LOCAL_NOTIFY_SIZE; ++i) {
        if (communicator.localAiCpuOpNotify_[i] == nullptr) {
            communicator.localAiCpuOpNotify_[i] = std::make_shared<hccl::LocalNotify>();
        }
    }

    HcclCMDType opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    DeviceMem deviceContext;
    std::string kernelName = "";
    AicpuOpTiling opTilingInfo;

    uint64_t streamMode = 0;
    rtStream_t aicpuStream;
    ret = communicator.Mc2AiCpuInitStreamAllocAndGet(streamMode, aicpuStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 重复调用
    ret = communicator.Mc2AiCpuInitStreamAllocAndGet(streamMode, aicpuStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = communicator.AicpuKfcTilingDataLaunchIn(
        opParam, deviceContext, kernelName, opTilingInfo, sizeof(struct OpTilingData));
    GlobalMockObject::verify();
}

TEST_F(HcclImplCommonNewTest, ut_HcclCommunicator_GetStreamCaptureInfo)
{
    GlobalMockObject::verify();
    // 非单算子场景
    MOCKER(GetWorkflowMode)
    .stubs()
    .with(any())
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB));

    bool isCapture = false;
    rtModel_t rtModel = nullptr;
    rtStream_t stream;
    HcclResult ret = GetStreamCaptureInfo(stream, rtModel, isCapture);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
    // unsupport场景
    MOCKER(GetWorkflowMode)
    .stubs()
    .with(any())
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));
    rtStreamCaptureStatus captureStatus = rtStreamCaptureStatus::RT_STREAM_CAPTURE_STATUS_ACTIVE;
    int mockModel = 0;
    void *pmockModel = &mockModel;    
    MOCKER(rtStreamGetCaptureInfo)
    .stubs()
    .with(any(), outBoundP(&captureStatus, sizeof(captureStatus)), outBoundP(&pmockModel, sizeof(pmockModel)))
    .will(returnValue(ACL_ERROR_RT_FEATURE_NOT_SUPPORT));
    ret = GetStreamCaptureInfo(stream, rtModel, isCapture);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
    // capture异常场景
    MOCKER(GetWorkflowMode)
    .stubs()
    .with(any())
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));
    captureStatus = rtStreamCaptureStatus::RT_STREAM_CAPTURE_STATUS_NONE;   
    MOCKER(rtStreamGetCaptureInfo)
    .stubs()
    .with(any(), outBoundP(&captureStatus, sizeof(captureStatus)), outBoundP(&pmockModel, sizeof(pmockModel)))
    .will(returnValue(0));
    ret = GetStreamCaptureInfo(stream, rtModel, isCapture);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    captureStatus = rtStreamCaptureStatus::RT_STREAM_CAPTURE_STATUS_MAX;   
    MOCKER(rtStreamGetCaptureInfo)
    .stubs()
    .with(any(), outBoundP(&captureStatus, sizeof(captureStatus)), outBoundP(&pmockModel, sizeof(pmockModel)))
    .will(returnValue(0));
    ret = GetStreamCaptureInfo(stream, rtModel, isCapture);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    captureStatus = rtStreamCaptureStatus::RT_STREAM_CAPTURE_STATUS_INVALIDATED;   
    MOCKER(rtStreamGetCaptureInfo)
    .stubs()
    .with(any(), outBoundP(&captureStatus, sizeof(captureStatus)), outBoundP(&pmockModel, sizeof(pmockModel)))
    .will(returnValue(0));
    ret = GetStreamCaptureInfo(stream, rtModel, isCapture);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(HcclImplCommonNewTest, ut_HcclCommunicator_AddStreamToModel)
{
    MOCKER(rtStreamAddToModel)
    .stubs()
    .with(any())
    .will(returnValue(1));
    rtModel_t rtModel = nullptr;
    rtStream_t stream;
    HcclResult ret = AddStreamToModel(rtModel, stream);
    EXPECT_EQ(ret, HCCL_E_RUNTIME);
    GlobalMockObject::verify();
}

TEST_F(HcclImplCommonNewTest, ut_HcclCommunicator_GetModelId)
{
    MOCKER(rtModelGetId)
    .stubs()
    .with(any())
    .will(returnValue(1));
    rtModel_t rtModel = nullptr;
    u32 modelId = 0;
    HcclResult ret = GetModelId(rtModel, modelId);
    EXPECT_EQ(ret, HCCL_E_RUNTIME);
    GlobalMockObject::verify();
 
    // success
    MOCKER(rtModelGetId)
    .stubs()
    .with(any())
    .will(returnValue(0));
    ret = GetModelId(rtModel, modelId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}