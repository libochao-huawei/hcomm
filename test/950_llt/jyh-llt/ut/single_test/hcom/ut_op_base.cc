#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <runtime/rt.h>
#include <assert.h>
#include <securec.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netdb.h>

#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "externalinput.h"
#include "comm_configer.h"

#include <nlohmann/json.hpp>


#define private public
#define protected public
#include "topoinfo_detect.h"
#include "hccl_alg.h"
#include "hccl_impl.h"
#include "hccl_communicator.h"
#include "hccl_comm_pub.h"
#include "coll_batch_send_recv_executor.h"
#include "coll_reduce_scatter_v_executor.h"
#include "coll_all_gather_v_executor.h"
#include "hccl_network.h"
#include "preempt_port_manager.h"

#include "common/src/topo/hccl_whitelist.h"
#include "profiling_manager.h"
#include <hccl/hccl.h>
#include <hccl/hccl_ex.h>
#include "llt_hccl_stub_pub.h"
#include <iostream>
#include <fstream>
#include "../inc/hccl/base.h"
#include "../inc/hccl/hccl_ex.h"
#include <hccl/hccl_types.h>
#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"
#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include "externalinput_pub.h"
#include "op_base.h"
#include "adapter_rts.h"
#include "param_check_pub.h"
#include "hcom_pub.h"
#include "comm_config_pub.h"
#include "kernel_tiling/kernel_tiling.h"
#include "exception_handler.h"
#include "plugin_runner_pub.h"
#include "task_exception_handler_pub.h"
#include "topoinfo_exchange_dispatcher.h"
#include "heartbeat.h"
#include "hccl_ctrl_plane.h"
#undef protected
#undef private

using namespace std;
using namespace hccl;

class OpbaseTest : public testing::TestWithParam<bool>
{
protected:
    // static void SetUpTestCase()
    // {
    //     std::cout << "OpbaseTest SetUP" << std::endl;
    // }
    // static void TearDownTestCase()
    // {
    //     std::cout << "OpbaseTest TearDown" << std::endl;
    // }
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        static s32  call_cnt = 0;
        string name = std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        DlTdtFunction::GetInstance().DlTdtFunctionInit();
        TsdOpen(1,2);
        ra_set_shm_name(name .c_str());
        ResetInitState();
        MOCKER_CPP(&Heartbeat::Init)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&Heartbeat::RegisterRanks)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&Heartbeat::UnRegisterRanks)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        TsdClose(1);
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
};

class OpbaseUT : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--OpbaseUT SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--OpbaseUT TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};

HcclResult HcclScatter(void *sendBuf, void *recvBuf, uint64_t recvCount, HcclDataType dataType, uint32_t root,
    HcclComm comm, aclrtStream stream);
INSTANTIATE_TEST_CASE_P(FFTSSwitch, OpbaseTest, testing::Bool());

#if 1
#define HCCL_COM_DATA_SIZE 1024
TEST_P(OpbaseTest, ut_hcclBroadcast)
{   
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    // 走1910 4pring
    char* rank_table_file = "./st_opbase_test.json";
    u32 rank_ID = 0;
    setenv("PROFILING_MODE", "true", 1);
    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sendbuf = (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendbuf, count * sizeof(s8) , 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcclBroadcast(sendbuf, count, HCCL_DATA_TYPE_INT8, 0, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (sendbuf[j] != 2)
        {
            HCCL_ERROR("\n rank:%d sendbuf[%d]:%f", rank, j, sendbuf[j] );
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    setenv("PROFILING_MODE", "false", 1);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);

    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
}

TEST_P(OpbaseTest, ut_hcclBroadcast_capture)
{   
    rtStreamCaptureStatus captureStatus = rtStreamCaptureStatus::RT_STREAM_CAPTURE_STATUS_INVALIDATED;
    int mockModel = 0;
    void *pmockModel = &mockModel;    
    MOCKER(rtStreamGetCaptureInfo)
    .stubs()
    .with(any(), outBoundP(&captureStatus, sizeof(captureStatus)), outBoundP(&pmockModel, sizeof(pmockModel)))
    .will(returnValue(207000));

    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));

    MOCKER_CPP(&HcclCommunicator::StreamIsCapture)
    .stubs()
    .with(any())
    .will(returnValue(true));

    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    // 走1910 4pring
    char* rank_table_file = "./st_opbase_test.json";
    u32 rank_ID = 0;
    setenv("PROFILING_MODE", "true", 1);
    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sendbuf = (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendbuf, count * sizeof(s8) , 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcclBroadcast(sendbuf, count, HCCL_DATA_TYPE_INT8, 0, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (sendbuf[j] != 2)
        {
            HCCL_ERROR("\n rank:%d sendbuf[%d]:%f", rank, j, sendbuf[j] );
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    setenv("PROFILING_MODE", "false", 1);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);

    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
}
#endif

TEST_F(OpbaseTest, ut_HcclConfig_deterministic)
{
    union HcclConfigValue hcclConfigValue;
    hcclConfigValue.value = 1;
    HcclResult ret = HcclSetConfig(HCCL_DETERMINISTIC, hcclConfigValue);
    union HcclConfigValue hcclConfigValueRet;
    ret = HcclGetConfig(HCCL_DETERMINISTIC, &hcclConfigValueRet);
    EXPECT_EQ(hcclConfigValue.value, hcclConfigValueRet.value);

    hcclConfigValue.value = 0;
    ret = HcclSetConfig(HCCL_DETERMINISTIC, hcclConfigValue);
    ret = HcclGetConfig(HCCL_DETERMINISTIC, &hcclConfigValueRet);
    EXPECT_EQ(hcclConfigValue.value, hcclConfigValueRet.value);

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));
    hcclConfigValue.value = 2;
    ret = HcclSetConfig(HCCL_DETERMINISTIC, hcclConfigValue);
    ret = HcclGetConfig(HCCL_DETERMINISTIC, &hcclConfigValueRet);
    EXPECT_EQ(hcclConfigValue.value, hcclConfigValueRet.value);
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_HcclConfig_deterministic_fail)
{
    union HcclConfigValue hcclConfigValue;
    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));
    hcclConfigValue.value = 2;
    HcclResult ret = HcclSetConfig(HCCL_DETERMINISTIC, hcclConfigValue);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
    GlobalMockObject::verify();
}

#if 1
#define HCCL_COM_DATA_SIZE 1024
#define DEV_NUM_8 8

TEST_F(OpbaseTest, ut_hcclSend_hcclReceive)
{
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
    nlohmann::json rank_table = rank_table_1server_8rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;
    s32 ndev = DEV_NUM_8;
    // 走1910 8pring
    char* rank_table_file = "./ut_opbase_test.json";

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));
    ret = HcclCommInitClusterInfo(rank_table_file, 0, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
    MOCKER_CPP(&hcclComm::SendOutPlace)
    .stubs()
    .with(any())
    .will(returnValue(0));
    ret = HcclSend(sendbuf, count, HCCL_DATA_TYPE_INT8, 1, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER_CPP(&hcclComm::ReceiveOutPlace)
    .stubs()
    .with(any())
    .will(returnValue(0));
    ret = HcclRecv(recvbuf, count, HCCL_DATA_TYPE_INT8, 0, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_hcclSend_hcclReceive_capture)
{   
    rtStreamCaptureStatus captureStatus = rtStreamCaptureStatus::RT_STREAM_CAPTURE_STATUS_ACTIVE;
    int mockModel = 0;
    void *pmockModel = &mockModel;    
    MOCKER(rtStreamGetCaptureInfo)
    .stubs()
    .with(any(), outBoundP(&captureStatus, sizeof(captureStatus)), outBoundP(&pmockModel, sizeof(pmockModel)))
    .will(returnValue(0));

    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));

    MOCKER_CPP(&HcclCommunicator::StreamIsCapture)
    .stubs()
    .with(any())
    .will(returnValue(true));

    nlohmann::json rank_table = rank_table_1server_8rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;
    s32 ndev = DEV_NUM_8;
    // 走1910 8pring
    char* rank_table_file = "./ut_opbase_test.json";

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));
    ret = HcclCommInitClusterInfo(rank_table_file, 0, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
    MOCKER_CPP(&hcclComm::SendOutPlace)
    .stubs()
    .with(any())
    .will(returnValue(0));
    ret = HcclSend(sendbuf, count, HCCL_DATA_TYPE_INT8, 1, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER_CPP(&hcclComm::ReceiveOutPlace)
    .stubs()
    .with(any())
    .will(returnValue(0));
    ret = HcclRecv(recvbuf, count, HCCL_DATA_TYPE_INT8, 0, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    GlobalMockObject::verify();
}
#endif

TEST_P(OpbaseTest, ut_hcclAllReduce)
{
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    } 

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    // 走1910 4pring
    char* rank_table_file = "./st_opbase_test.json";
    u32 rank_ID = 0;
    string tmpOptions = "";
    HcomSetProfilingMode(HcomProfilingMode::PROFILING_OPEN, tmpOptions);
    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
    unsigned int rankSize = 0;
    ret = HcclGetRankSize(comm, &rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("HCCL TEST get rank size[%u] success.", rankSize);

    unsigned int rankId = 0;
    ret = HcclGetRankId(comm, &rankId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("HCCL TEST get rank id[%u] success.", rankId);

    ret = HcclBarrier(comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    auto &profilingManager = hccl::ProfilingManager::Instance();
    profilingManager.StartFftsLaunchSubscribe();
    profilingManager.StartHostApiSubscribe();
    profilingManager.StartTaskApiSubscribe();
    profilingManager.StartHostHcclOpSubscribe();
    ret = HcclAllReduce(sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    ret = HcclAllReduce(sendbuf, sendbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, comm, stream);
    // //ret = HcclAllReduce(sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, comm, stream);
    // EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    HCCL_ERROR("count %d",count);
    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            HCCL_ERROR("j %d",j);
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
}

TEST_P(OpbaseTest, ut_hcclAllReduce_capture)
{   
    rtStreamCaptureStatus captureStatus = rtStreamCaptureStatus::RT_STREAM_CAPTURE_STATUS_ACTIVE;
    int mockModel = 0;
    void *pmockModel = &mockModel;    
    MOCKER(rtStreamGetCaptureInfo)
    .stubs()
    .with(any(), outBoundP(&captureStatus, sizeof(captureStatus)), outBoundP(&pmockModel, sizeof(pmockModel)))
    .will(returnValue(0));

    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));

    MOCKER_CPP(&HcclCommunicator::StreamIsCapture)
    .stubs()
    .with(any())
    .will(returnValue(true));

    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    } 
    
    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    // 走1910 4pring
    char* rank_table_file = "./st_opbase_test.json";
    u32 rank_ID = 0;
    string tmpOptions = "";
    HcomSetProfilingMode(HcomProfilingMode::PROFILING_OPEN, tmpOptions);
    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
    unsigned int rankSize = 0;
    ret = HcclGetRankSize(comm, &rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("HCCL TEST get rank size[%u] success.", rankSize);

    unsigned int rankId = 0;
    ret = HcclGetRankId(comm, &rankId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("HCCL TEST get rank id[%u] success.", rankId);

    ret = HcclBarrier(comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    auto &profilingManager = hccl::ProfilingManager::Instance();
    profilingManager.StartFftsLaunchSubscribe();
    profilingManager.StartHostApiSubscribe();
    profilingManager.StartTaskApiSubscribe();
    profilingManager.StartHostHcclOpSubscribe();
    ret = HcclAllReduce(sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    ret = HcclAllReduce(sendbuf, sendbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, comm, stream);
    // //ret = HcclAllReduce(sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, comm, stream);
    // EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    HCCL_ERROR("count %d",count);
    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            HCCL_ERROR("j %d",j);
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
}

TEST_P(OpbaseTest, ut_hcclAllReduce_capture_rdma)
{   
    rtStreamCaptureStatus captureStatus = rtStreamCaptureStatus::RT_STREAM_CAPTURE_STATUS_ACTIVE;
    int mockModel = 0;
    void *pmockModel = &mockModel;    
    MOCKER(rtStreamGetCaptureInfo)
    .stubs()
    .with(any(), outBoundP(&captureStatus, sizeof(captureStatus)), outBoundP(&pmockModel, sizeof(pmockModel)))
    .will(returnValue(0));

    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));

    MOCKER_CPP(&HcclCommunicator::StreamIsCapture)
    .stubs()
    .with(any())
    .will(returnValue(true));

    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&HcclCommunicator::ExecOp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
    
    nlohmann::json rank_table = rank_table_910_2server_4rank;

    char file_name_t[] = "./st_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    // 走1910 4pring
    char* rank_table_file = "./st_opbase_test.json";
    u32 rank_ID = 0;
    string tmpOptions = "";
    HcomSetProfilingMode(HcomProfilingMode::PROFILING_OPEN, tmpOptions);
    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
    unsigned int rankSize = 0;
    ret = HcclGetRankSize(comm, &rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("HCCL TEST get rank size[%u] success.", rankSize);

    unsigned int rankId = 0;
    ret = HcclGetRankId(comm, &rankId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("HCCL TEST get rank id[%u] success.", rankId);

    ret = HcclBarrier(comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    auto &profilingManager = hccl::ProfilingManager::Instance();
    profilingManager.StartFftsLaunchSubscribe();
    profilingManager.StartHostApiSubscribe();
    profilingManager.StartTaskApiSubscribe();
    profilingManager.StartHostHcclOpSubscribe();
    ret = HcclAllReduce(sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    ret = HcclAllReduce(sendbuf, sendbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, comm, stream);
    // //ret = HcclAllReduce(sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, comm, stream);
    // EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    HCCL_ERROR("count %d",count);

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }

    GlobalMockObject::verify();
}

TEST_P(OpbaseTest, ut_hcclScatter_Ring_1rank)
{
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));

    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    // 走1910 4pring
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;

    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcclScatter(sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, 0, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
    GlobalMockObject::verify();
}


TEST_P(OpbaseTest, ut_hcclReducescatter)
{
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    } else {
        SetFftsSwitch(false);
    }

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    // 走1910 4pring
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;

    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcclReduceScatter(sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclReduceScatter(sendbuf, sendbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, comm, stream);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
    SetFftsSwitch(true);
}

TEST_P(OpbaseTest, ut_hcclReducescatter_capture_rdma)
{
    rtStreamCaptureStatus captureStatus = rtStreamCaptureStatus::RT_STREAM_CAPTURE_STATUS_ACTIVE;
    int mockModel = 0;
    void *pmockModel = &mockModel;    
    MOCKER(rtStreamGetCaptureInfo)
    .stubs()
    .with(any(), outBoundP(&captureStatus, sizeof(captureStatus)), outBoundP(&pmockModel, sizeof(pmockModel)))
    .will(returnValue(0));

    MOCKER_CPP(&HcclCommunicator::StreamIsCapture)
    .stubs()
    .with(any())
    .will(returnValue(true));

    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&HcclCommunicator::ExecOp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    nlohmann::json rank_table = rank_table_910_2server_4rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    u32 rankSize = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    // 走1910 4pring
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;

    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    ret = hcclComm->GetRankSize(rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(rankSize * count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    for (int j = 0; j < rankSize * count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcclReduceScatter(sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclReduceScatter(sendbuf, sendbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, comm, stream);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    if (fftsSwitch) {
        SetFftsSwitch(true);
    }

    GlobalMockObject::verify();
}

TEST_P(OpbaseTest, ut_hcclAllGather)
{
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    // 走1910 4pring
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;

    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcclAllGather(sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclAllGather(sendbuf, sendbuf, count, HCCL_DATA_TYPE_INT8, comm, stream);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
}

TEST_P(OpbaseTest, ut_hcclAllGather_capture)
{   
    rtStreamCaptureStatus captureStatus = rtStreamCaptureStatus::RT_STREAM_CAPTURE_STATUS_ACTIVE;
    int mockModel = 0;
    void *pmockModel = &mockModel;    
    MOCKER(rtStreamGetCaptureInfo)
    .stubs()
    .with(any(), outBoundP(&captureStatus, sizeof(captureStatus)), outBoundP(&pmockModel, sizeof(pmockModel)))
    .will(returnValue(0));

    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));

    MOCKER_CPP(&HcclCommunicator::StreamIsCapture)
    .stubs()
    .with(any())
    .will(returnValue(true));

    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    } else {
        SetFftsSwitch(false);
    }

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    // 走1910 4pring
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;

    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcclAllGather(sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclAllGather(sendbuf, sendbuf, count, HCCL_DATA_TYPE_INT8, comm, stream);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
}

TEST_P(OpbaseTest, ut_hcclAllGather_capture_rdma)
{   
    rtStreamCaptureStatus captureStatus = rtStreamCaptureStatus::RT_STREAM_CAPTURE_STATUS_ACTIVE;
    int mockModel = 0;
    void *pmockModel = &mockModel;    
    MOCKER(rtStreamGetCaptureInfo)
    .stubs()
    .with(any(), outBoundP(&captureStatus, sizeof(captureStatus)), outBoundP(&pmockModel, sizeof(pmockModel)))
    .will(returnValue(0));

    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));

    MOCKER_CPP(&HcclCommunicator::StreamIsCapture)
    .stubs()
    .with(any())
    .will(returnValue(true));

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&HcclCommunicator::ExecOp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    } else {
        SetFftsSwitch(false);
    }

    nlohmann::json rank_table = rank_table_910_2server_4rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    u32 rankSize = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    // 走1910 4pring
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;

    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    ret = hcclComm->GetRankSize(rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(rankSize * count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcclAllGather(sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclAllGather(sendbuf, sendbuf, count, HCCL_DATA_TYPE_INT8, comm, stream);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }

    GlobalMockObject::verify();
}

#define HCCL_COM_BIG_DATA_SIZE (300 * 1024 *1024) //300M
TEST_F(OpbaseTest, ut_BighcclAllReduce)
{

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_BIG_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    // 走1910 4pring
    char* rank_table_file = "./st_opbase_test.json";
    u32 rank_ID = 0;

    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcclAllReduce(sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}


TEST_F(OpbaseTest, ut_BighcclReducescatter)
{

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_BIG_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    // 走1910 4pring
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;

    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcclReduceScatter(sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}

TEST_F(OpbaseTest, ut_hcomReduceScatterV)
{
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
 
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&HcclCommunicator::ExecOp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    nlohmann::json rank_table = rank_table_910_1server_2rank;
 
    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);
 
    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }
 
    outfile.close();
 
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    u64* sendCounts;
    u64* sendDispls;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    u32 rankSize = 4;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;
 
    // 走1910 4pring
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;
 
    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));
    sendbuf= (s8*)sal_malloc(rankSize * count * sizeof(s8));
     sal_memset(sendbuf, rankSize * count * sizeof(s8), 0, rankSize * count * sizeof(s8));
 
    sendCounts= (u64*)sal_malloc(rankSize * sizeof(u64));
     sal_memset(sendCounts, rankSize * sizeof(u64), 0, rankSize * sizeof(u64));
    sendDispls= (u64*)sal_malloc(rankSize * sizeof(u64));
     sal_memset(sendDispls, rankSize * sizeof(u64), 0, rankSize * sizeof(u64));
 
    for (int j = 0; j < count; j++)
    {
        recvbuf[j] = 2;
    }
 
    for (int i = 0; i < rankSize; i++)
    {
        sendCounts[i] = count;
        if (i > 0) {
            sendDispls[i] = sendDispls[i-1] + sendDispls[i-1];
        }
    }
 
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    string strTag = "reducescatterv";
    ret = hcclComm->ReduceScatterV(strTag, static_cast<void *>(sendbuf), static_cast<void *>(sendCounts),
        static_cast<void *>(sendDispls), static_cast<void *>(recvbuf), 2, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    sal_free(sendbuf);
    sal_free(recvbuf);
    sal_free(sendCounts);
    sal_free(sendDispls);
    rt_ret = rtStreamDestroy(stream);
 
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_BighcclAllGather)
{

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_BIG_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    // 走1910 4pring
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;

    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcclAllGather(sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}

TEST_F(OpbaseTest, ut_BIGhcclBroadcast)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_BIG_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    // 走1910 4pring
    char* rank_table_file = "./st_opbase_test.json";
    u32 rank_ID = 0;

    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sendbuf = (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendbuf, count * sizeof(s8) , 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcclBroadcast(sendbuf, count, HCCL_DATA_TYPE_INT8, 0, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (sendbuf[j] != 2)
        {
            HCCL_ERROR("\n rank:%d sendbuf[%d]:%f", rank, j, sendbuf[j] );
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}

TEST_F(OpbaseTest, ut_initWithConfig)
{

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    // 走1910 4pring
    char* rank_table_file = "./st_opbase_test.json";
    u32 rank_ID = 0;

    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    commConfig.hcclBufferSize=400;
    commConfig.hcclBufferSize=1;

    ret = HcclCommInitClusterInfoConfig(rank_table_file, rank_ID, &commConfig, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
}

TEST_F(OpbaseTest, ut_initSubComm)
{

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* global_comm;
    void* comm;

    // 走1910 4pring
    char* rank_table_file = "./st_opbase_test.json";
    u32 rank_ID = 0;

    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    commConfig.hcclBufferSize=400;
    commConfig.hcclBufferSize=1;

    ret = HcclCommInitClusterInfoConfig(rank_table_file, rank_ID, &commConfig, &global_comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    uint32_t rankIds[1] = {0};
    ret = HcclCreateSubCommConfig(&global_comm, 1, rankIds, 2, rank_ID, &commConfig, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclCommDestroy(global_comm);
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
}

TEST_F(OpbaseTest, ut_HcclCommInitClusterInfoConfig_user_config_configer)
{

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    // 走1910 4pring
    char* rank_table_file = "./st_opbase_test.json";
    u32 rank_ID = 0;

    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    commConfig.hcclBufferSize=400;
    commConfig.hcclDeterministic = 1;
    commConfig.hcclOpExpansionMode = 3;
    commConfig.hcclExecTimeOut = 2000;
    strcpy_s(commConfig.hcclCommName, COMM_NAME_MAX_LENGTH, "comm1");
    strcpy_s(commConfig.hcclAlgo, HCCL_COMM_ALGO_MAX_LENGTH, "level0:NA;level1:ring");
    strcpy_s(commConfig.hcclRetryEnable, HCCL_COMM_RETRY_ENABLE_MAX_LENGTH, "L1:1, L2:1");
    strcpy_s(commConfig.hcclRetryParams, HCCL_COMM_RETRY_PARAMS_MAX_LENGTH, "MaxCnt:5, HoldTime:5000, IntervalTime:5000");

    ret = HcclCommInitClusterInfoConfig(rank_table_file, rank_ID, &commConfig, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcclComm *pComm = static_cast<hcclComm *>(comm);
    EXPECT_EQ(pComm->GetConfigInCCLbufferSize(), 400 * 1024 * 1024);
    EXPECT_EQ(pComm->GetConfigOutCCLbufferSize(), 400 * 1024 * 1024);
    EXPECT_EQ(pComm->communicator_->GetDeterministicConfig(), 1);
    EXPECT_EQ(pComm->GetIdentifier(), "comm1");
    EXPECT_EQ(pComm->communicator_->SetDeterministicConfig(commConfig.hcclDeterministic), HCCL_SUCCESS);

    EXPECT_EQ(pComm->communicator_->commConfig_.GetConfigCommName(), "comm1");

    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigAlgoConfig("comm1")[0], HcclAlgoType::HCCL_ALGO_TYPE_NA);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigAlgoConfig("comm1")[1], HcclAlgoType::HCCL_ALGO_TYPE_RING);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigAlgoConfig("comm1")[2], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigAlgoConfig("comm1")[3], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigInterServerRetryEnable("comm1"), 1);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigInterSuperPodRetryEnable("comm1"), 1);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigRetryMaxCnt("comm1"), 5);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigRetryHoldTime("comm1"), 5000);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigRetryIntervalTime("comm1"), 5000);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
}

TEST_F(OpbaseTest, ut_hcclGetRootInfo_single_server_success_normal_1)
{

    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
    HcclRootInfo id;
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER(InitExternalInput)
    .expects(atMost(10))
    .will(returnValue(1));
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, 1);
    GlobalMockObject::verify();
    unsetenv("HCCL_WHITELIST_DISABLE");
}

TEST_F(OpbaseTest, ut_hcclGetRootInfo_hierarchical_rank_0)
{
 
    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
    MOCKER(GetExternalInputHcclLinkTimeOut)
    .stubs()
    .will(returnValue(5));
 
    MOCKER_CPP(&HcclSocket::Listen, HcclResult (HcclSocket::*)(u32 port))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
     MOCKER_CPP(&TopoInfoExchangeAgent::Connect)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_INTERNAL));

    MOCKER(GetExternalInputHcclLinkTimeOut)
    .stubs()
    .will(returnValue(1));

    HcclRootInfo id;
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclComm newcomm;
    ret = HcclCommInitRootInfo(128*1024, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
 
    GlobalMockObject::verify();
    unsetenv("HCCL_CONNECT_TIMEOUT");
    unsetenv("HCCL_WHITELIST_DISABLE");
}

TEST_F(OpbaseTest, ut_hcclGetRootInfo_hierarchical_rank_1)
{
 
    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
    MOCKER(GetExternalInputHcclLinkTimeOut)
    .stubs()
    .will(returnValue(5));
 
    GroupLeader_t GrpLeaderInfo;
    
    nlohmann::json leaderListJson;
    nlohmann::json leaderJson;
    nlohmann::json GroupLeaderJson;
 
    leaderJson[PROP_NETWORK_IPADDR] = "127.0.0.1";
    leaderJson[PROP_NETWORK_NETWORKPORT] = 60009;
    leaderJson[PROP_NETWORK_IDENTIFIER] = "test";
    leaderJson[PROP_DEPLOY_MODE] = NICDeployment::NIC_DEPLOYMENT_HOST;
    leaderJson[PROP_RANK_ID] = 1;
    leaderListJson.push_back(leaderJson);
 
    GroupLeaderJson[PROP_RANK_NUM] = 1;
    GroupLeaderJson[PROP_STEP] = 0;
    GroupLeaderJson[PROP_GROUP_LEADER_LIST] = leaderListJson;
 
    MOCKER_CPP(&TopoInfoExchangeBase::RecvClusterJson)
    .stubs()
    .with(any(), outBound(GroupLeaderJson))
    .will(returnValue(0));

    MOCKER(GetExternalInputHcclLinkTimeOut)
    .stubs()
    .will(returnValue(1));
 
    HcclRootInfo id;
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclComm newcomm;
    ret = HcclCommInitRootInfo(128*1024, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
 
    GlobalMockObject::verify();
    unsetenv("HCCL_CONNECT_TIMEOUT");
    unsetenv("HCCL_WHITELIST_DISABLE");
}

TEST_F(OpbaseTest, ut_hcclGetRootInfo_hierarchical_root)
{
 
    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
 
    HcclIpAddress hostIP("127.0.0.1");
    u32 hostPort = 61111;
    std::vector<HcclIpAddress> whitelist ;
    HcclNetDevCtx netDevCtx;
    std::shared_ptr<HcclSocket> listenSocket(new (std::nothrow)HcclSocket("test", nullptr,
        hostIP, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
 
    const std::string identifier = "releaseSocket";
    TopoInfoExchangeServer topoExServer(hostIP, hostPort, whitelist, netDevCtx, listenSocket, identifier);
 
    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    std::map<std::string, std::shared_ptr<HcclSocket>> connectSockets;
    connectSockets.insert(std::pair<std::string, std::shared_ptr<HcclSocket>>("001", listenSocket));
    GroupLeader_t groupLeader;
 
    HcclResult ret = topoExServer.RecvGroupLeaderInfo(connectSockets, groupLeader);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    GlobalMockObject::verify();
    unsetenv("HCCL_WHITELIST_DISABLE");
}

TEST_F(OpbaseTest, ut_HcclCommInitRootInfo_SetupGroupMember)
{
    TopoInfoDetect topoDetectAgent;
    HcclResult ret;
 
    // 初始化环境变量，just for ut，防止用例间影响
    ResetInitState();
    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
 
    HcclRootHandle rootHandle;
    std::vector<HcclIpAddress> whitelist;
    std::shared_ptr<HcclSocket> socket;
    HcclRootInfo id;
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    s32 sRet = memcpy_s(&rootHandle, HCCL_ROOT_INFO_BYTES, id.internal, sizeof(HcclRootHandle));
    EXPECT_EQ(sRet, 0);
 
    u32 nRanks = 1;
    u32 myRank = 0;
 
    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    // 用例本身走超时分支 由于超时时间过长影响线上运行时长且超时时间无法修改只能选择打桩规避
    MOCKER_CPP(&TopoInfoExchangeAgent::GetConnection, HcclResult(TopoInfoExchangeAgent::*)(HcclIpAddress &, u32, std::shared_ptr<HcclSocket> &))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_TIMEOUT));
 
    ret = topoDetectAgent.SetupGroupMember(nRanks, myRank, rootHandle);
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_HcclCommInitRootInfo_SetupTopoGroupLeader_error0)
{
    TopoInfoDetect topoDetectAgent;
    HcclResult ret;
 
    MOCKER_CPP(&TopoInfoExchangeServer::SetupGroupLeader)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_INTERNAL));
 
    MOCKER(hrtResetDevice)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_INTERNAL));
 
    std::vector<HcclIpAddress> whitelist;
    std::shared_ptr<HcclSocket> socket;
    HcclRankHandle groupLeader;
    HcclIpAddress hostIP("127.0.0.1");
    HcclNetDevCtx netDevCtx;
 
    topoDetectAgent.SetupTopoGroupLeader(0, 0, hostIP, 61111, whitelist, netDevCtx, socket, socket, false);
 
    GlobalMockObject::verify();
}
 
TEST_F(OpbaseTest, ut_HcclCommInitRootInfo_SetupTopoGroupLeader_error1)
{
    TopoInfoDetect topoDetectAgent;
    HcclResult ret;
 
    MOCKER(hrtSetDevice)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_INTERNAL));
    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    std::vector<HcclIpAddress> whitelist;
    std::shared_ptr<HcclSocket> socket;
    HcclRankHandle groupLeader;
    HcclIpAddress hostIP("127.0.0.1");
    HcclNetDevCtx netDevCtx;
 
    topoDetectAgent.SetupTopoGroupLeader(0, 0, hostIP, 61111, whitelist, netDevCtx, socket, socket, false);
 
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_HcclCommInitRootInfo_GroupLeaderListen)
{
    TopoInfoDetect topoDetectGroupLeader;
    HcclResult ret;
 
    // 初始化环境变量，just for ut，防止用例间影响
    ResetInitState();
    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
 
    HcclRootHandle rootHandle;
    std::vector<HcclIpAddress> whitelist;
    std::shared_ptr<HcclSocket> socket;
    HcclRootInfo id;
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    s32 sRet = memcpy_s(&rootHandle, HCCL_ROOT_INFO_BYTES, id.internal, sizeof(HcclRootHandle));
    EXPECT_EQ(sRet, 0);
 
    u32 nRanks = 1;
    u32 myRank = 0;
 
    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    
     DevType deviceType = DevType::DEV_TYPE_910B;
     MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
 
    bool portSwitch = 0;
    MOCKER(GetExternalInputHostPortSwitch)
    .stubs()
    .will(returnValue(portSwitch));
 
     MOCKER_CPP(&HcclSocket::Listen, HcclResult (HcclSocket::*)(u32 port))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    topoDetectGroupLeader.GroupLeaderListen(rootHandle, whitelist);
    
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_GroupLeaderSendPort)
{
    TopoInfoDetect topoDetectGroupLeader;
    HcclResult ret;
 
    // 初始化环境变量，just for ut，防止用例间影响
    ResetInitState();
    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
 
    HcclRootHandle rootHandle;
    std::vector<HcclIpAddress> whitelist;
    std::shared_ptr<HcclSocket> socket;
    HcclRootInfo id;
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    s32 sRet = memcpy_s(&rootHandle, HCCL_ROOT_INFO_BYTES, id.internal, sizeof(HcclRootHandle));
    EXPECT_EQ(sRet, 0);
 
    u32 nRanks = 1;
    u32 myRank = 0;
 
    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS)); 

    MOCKER_CPP(&TopoInfoExchangeAgent::SendGroupLeaderPortInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    bool portSwitch = 0;
    MOCKER(GetExternalInputHostPortSwitch)
    .stubs()
    .will(returnValue(portSwitch));

    topoDetectGroupLeader.SendGroupLeaderPort(socket, rootHandle);
    
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_GroupLeaderAccept_timeOut)
{
    MOCKER(GetExternalInputHcclLinkTimeOut)
    .stubs()
    .will(returnValue(2));
    MOCKER_CPP(&HcclSocket::Accept)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_TIMEOUT));
    MOCKER_CPP(&TopoInfoExchangeServer::DisplayConnectionedRank)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress hostIP("127.0.0.1");
    u32 hostPort = 61111;
    std::vector<HcclIpAddress> whitelist ;
    HcclNetDevCtx netDevCtx;
    std::shared_ptr<HcclSocket> listenSocket;
    const std::string identifier = "TOPPINFOEXCHANGE_TEST";
    TopoInfoExchangeServer topoExServer(hostIP, hostPort, whitelist, netDevCtx, listenSocket, identifier);
    std::map<std::string, std::shared_ptr<HcclSocket>> connectSockets;
    topoExServer.GroupLeaderConnect(connectSockets);

    std::shared_ptr<HcclSocket> socket;
    connectSockets.insert({"0001", socket});
    topoExServer.DisplayConnectingStatus(4, 3, connectSockets);
    unsetenv("HCCL_CONNECT_TIMEOUT");

    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_HierarchicalSendRecv)
{
    MOCKER_CPP(&TopoInfoExchangeServer::RecvGroupLeaderInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TopoInfoExchangeDispather::BroadcastGroupLeaderInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TopoInfoExchangeDispather::BroadcastRankTable)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TopoInfoExchangeServer:: RecvGroupLeaderPortInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TopoInfoExchangeServer:: GetRanksBasicInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcclIpAddress hostIP("127.0.0.1");
    u32 hostPort = 61111;
    std::vector<HcclIpAddress> whitelist ;
    HcclNetDevCtx netDevCtx;
    std::shared_ptr<HcclSocket> listenSocket;
    const std::string identifier = "TOPPINFOEXCHANGE_TEST";
    TopoInfoExchangeServer topoExServer(hostIP, hostPort, whitelist, netDevCtx, listenSocket, identifier);
    std::map<std::string, std::shared_ptr<HcclSocket>> connectSockets;
    topoExServer.HierarchicalSendRecv();

    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_RecvGroupLeaderPortInfo)
{
     MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress hostIP("127.0.0.1");
    u32 hostPort = 61111;
    std::vector<HcclIpAddress> whitelist ;
    HcclNetDevCtx netDevCtx;
    std::shared_ptr<HcclSocket> listenSocket;
    const std::string identifier = "TOPPINFOEXCHANGE_TEST";
    TopoInfoExchangeServer topoExServer(hostIP, hostPort, whitelist, netDevCtx, listenSocket, identifier);
    std::map<std::string, std::shared_ptr<HcclSocket>> connectSockets;
    GroupLeader_t groupLeader;
    topoExServer.RecvGroupLeaderPortInfo(connectSockets, groupLeader);

    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_HcclGetCommName_1)
{
    char *group;
    HcclRootInfo id;
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclGetCommName(newcomm, group);
    EXPECT_EQ(ret, HCCL_E_PTR);
    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(OpbaseTest, ut_HcclGetCommName_3)
{
    const char *group = HCCL_WORLD_GROUP;
    HcclRootInfo id;
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::shared_ptr<hccl::hcclComm> comm;
    ret = HcclGetCommHandle(group, comm);
    EXPECT_EQ(ret, HCCL_E_PARA);
    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_HcclGetCommName_2)
{
    char *group;
    HcclComm newcomm;
    HcclResult ret = HcclGetCommName(&newcomm, group);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(OpbaseTest, ut_hcclGetRootInfo_single_server_success_1)
{

    nlohmann::json whitelist =
    {
        {
            "host_ip",
            {
                "127.0.0.1",
            }
        }
    };

    char *buffer;
    //也可以将buffer作为输出参数
    if((buffer = getcwd(NULL, 0)) != NULL)
    {
        std::string dirPath = buffer;
        std::string fileName = "/ut_hccl_host_allowlist.json";
        std::string realPath = dirPath.c_str() + fileName;
        setenv("HCCL_WHITELIST_FILE", realPath.c_str(), 1);
        ofstream outfile(realPath);
        // outfile << "127.0.0.1" << endl;
        outfile << std::setw(4) << whitelist << std::endl;
        outfile.close();

        HcclRootInfo id;
        HcclResult ret = HcclGetRootInfo(&id);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HcclComm newcomm;
        ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = HcclCommDestroy(newcomm);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        unsetenv("HCCL_WHITELIST_FILE");
        remove(realPath.c_str());
    }
    free(buffer);
}
#if 1
TEST_F(OpbaseTest, ut_hcclGetRootInfo_single_server_success_2)
{
    nlohmann::json whitelist =
    {
        {
            "host_ip",
            {
                "127.0.0.1",
            }
        }
    };
    char *buffer;
    //也可以将buffer作为输出参数
    if((buffer = getcwd(NULL, 0)) != NULL)
    {
        std::string dirPath = buffer;
        std::string fileName = "/ut_hccl_host_allowlist";
        std::string realPath = dirPath.c_str() + fileName;
        setenv("HCCL_WHITELIST_FILE", realPath.c_str(), 1);
        setenv("HCCL_IF_IP", "127.0.0.1", 1);
        ofstream outfile(realPath);
        outfile << std::setw(4) << whitelist << std::endl;
        outfile.close();

        HcclRootInfo id;
        HcclResult ret = HcclGetRootInfo(&id);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HcclComm newcomm;
        ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = HcclCommDestroy(newcomm);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        unsetenv("HCCL_WHITELIST_FILE");
        unsetenv("HCCL_IF_IP");
        remove(realPath.c_str());
    }
    free(buffer);
}

TEST_F(OpbaseTest, ut_hcclGetRootInfo_single_server_success_3)
{
    nlohmann::json whitelist =
    {
        {
            "host_ip",
            {
                "127.0.0.2",
            }
        }
    };
    char *buffer;
    //也可以将buffer作为输出参数
    if((buffer = getcwd(NULL, 0)) != NULL)
    {
        std::string dirPath = buffer;
        std::string fileName = "/ut_hccl_host_allowlist";
        std::string realPath = dirPath.c_str() + fileName;
        setenv("HCCL_WHITELIST_FILE", realPath.c_str(), 1);
        setenv("HCCL_IF_IP", "127.0.0.2", 1);
        ofstream outfile(realPath);
        outfile << std::setw(4) << whitelist << std::endl;
        outfile.close();

        HcclRootInfo id;
        HcclResult ret = HcclGetRootInfo(&id);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HcclComm newcomm;
        ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = HcclCommDestroy(newcomm);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        unsetenv("HCCL_WHITELIST_FILE");
        unsetenv("HCCL_IF_IP");
        remove(realPath.c_str());
    }
    free(buffer);
}

TEST_F(OpbaseTest, ut_hcclGetRootInfo_single_server_success_4)
{
    nlohmann::json whitelist =
    {
        {
            "host_ip",
            {
                "127.0.0.3",
            }
        }
    };
    char *buffer;
    //也可以将buffer作为输出参数
    if((buffer = getcwd(NULL, 0)) != NULL)
    {
        std::string dirPath = buffer;
        std::string fileName = "/ut_hccl_host_allowlist";
        std::string realPath = dirPath.c_str() + fileName;
        setenv("HCCL_WHITELIST_FILE", realPath.c_str(), 1);
        setenv("HCCL_IF_IP", "127.0.0.3", 1);
        ofstream outfile(realPath);
        outfile << std::setw(4) << whitelist << std::endl;
        outfile.close();

        HcclRootInfo id;
        HcclResult ret = HcclGetRootInfo(&id);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HcclComm newcomm;
        ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = HcclCommDestroy(newcomm);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        unsetenv("HCCL_WHITELIST_FILE");
        unsetenv("HCCL_IF_IP");
        remove(realPath.c_str());
    }
    free(buffer);
}

TEST_F(OpbaseTest, ut_hcclGetRootInfo_single_server_success_normal_2)
{

    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
    setenv("HCCL_IF_IP", "127.0.0.1", 1);

    HcclRootInfo id;
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_IF_IP");
}

TEST_F(OpbaseTest, ut_hcclGetRootInfo_single_server_success_normal_3)
{

    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
    setenv("HCCL_IF_IP", "127.0.0.2", 1);

    HcclRootInfo id;
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_IF_IP");
}

TEST_F(OpbaseTest, ut_hcclGetRootInfo_single_server_success_normal_4)
{

    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
    setenv("HCCL_IF_IP", "127.0.0.2", 1);
    HcclRootInfo id;
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_IF_IP");
}
TEST_F(OpbaseTest, ut_hcclGetRootInfo_single_server_success_normal_5)
{
    // 初始化环境变量，just for ut，防止用例间影响
    ResetInitState();
    setenv("HCCL_WHITELIST_DISABLE", "1", 1);

    // 网卡信息在ra_get_ifaddrs接口已初始化（eth0,docker,lo）
    // 匹配eth，enp前缀的网卡
    setenv("HCCL_SOCKET_IFNAME", "eth,enp", 1);
    HcclRootInfo id;
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_SOCKET_IFNAME");
}

TEST_F(OpbaseTest, ut_hcclGetRootInfo_single_server_success_normal_6)
{
    // 初始化环境变量，just for ut，防止用例间影响
    ResetInitState();
    setenv("HCCL_WHITELIST_DISABLE", "1", 1);

    // 网卡信息在ra_get_ifaddrs接口已初始化（eth0,docker,lo）
    // 匹配eth0，enp名称的网卡
    setenv("HCCL_SOCKET_IFNAME", "=eth0,enp", 1);
    HcclRootInfo id;
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_SOCKET_IFNAME");
}

TEST_F(OpbaseTest, ut_hcclGetRootInfo_single_server_success_normal_7)
{
    // 初始化环境变量，just for ut，防止用例间影响
    ResetInitState();
    setenv("HCCL_WHITELIST_DISABLE", "1", 1);

    // 网卡信息在ra_get_ifaddrs接口已初始化（eth0,docker,lo）
    // 不匹配enp，env前缀的网卡
    setenv("HCCL_SOCKET_IFNAME", "^enp,env", 1);
    HcclRootInfo id;
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_SOCKET_IFNAME");
}

TEST_F(OpbaseTest, ut_hcclGetRootInfo_single_server_success_normal_8)
{
    // 初始化环境变量，just for ut，防止用例间影响
    ResetInitState();
    setenv("HCCL_WHITELIST_DISABLE", "1", 1);

    // 网卡信息在ra_get_ifaddrs接口已初始化（eth0,docker,lo）
    // 不匹配enp，env名称的网卡
    setenv("HCCL_SOCKET_IFNAME", "^=enp,env", 1);
    HcclRootInfo id;
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_SOCKET_IFNAME");
}

TEST_F(OpbaseTest, ut_hcclGetRootInfo_single_server_success_normal_9)
{
    // 初始化环境变量，just for ut，防止用例间影响
    ResetInitState();
    setenv("HCCL_WHITELIST_DISABLE", "1", 1);

    // 网卡信息在ra_get_ifaddrs接口已初始化（eth0,docker,lo）
    // 配置匹配eth1，enp前缀的网卡，无法找到Host网卡
    setenv("HCCL_SOCKET_IFNAME", "=eth1,enp", 1);
    HcclRootInfo id;
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);

    // 配置匹配enp，env前缀的网卡，无法找到Host网卡
    ResetInitState();
    setenv("HCCL_SOCKET_IFNAME", "enp,env", 1);
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);

    // 配置不匹配,eth0，lo，docker网卡，无法找到Host网卡
    ResetInitState();
    setenv("HCCL_SOCKET_IFNAME", "^=eth0,lo,docker", 1);
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
    // 配置不匹配,eth，dock，和lo前缀的网卡，无法找到Host网卡
    ResetInitState();
    setenv("HCCL_SOCKET_IFNAME", "^eth,dock,lo", 1);
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);

    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_SOCKET_IFNAME");
}

TEST_F(OpbaseTest, ut_hcclGetRootInfo_invaild_env_01)
{

    setenv("HCCL_WHITELIST_DISABLE", "2", 1);
    HcclRootInfo id;
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_E_PARA);

    unsetenv("HCCL_WHITELIST_DISABLE");
}

TEST_F(OpbaseTest, ut_hcclGetRootInfo_single_server_fail_empty_allowlist)
{
    char *buffer;
    //也可以将buffer作为输出参数
    if((buffer = getcwd(NULL, 0)) != NULL)
    {
        std::string dirPath = buffer;
        std::string fileName = "/ut_hccl_host_allowlist";
        std::string realPath = dirPath.c_str() + fileName;
        setenv("HCCL_WHITELIST_FILE", realPath.c_str(), 1);
        setenv("HCCL_WHITELIST_DISABLE", "0", 1);
        ofstream outfile(realPath);
        outfile << "" << endl;
        outfile.close();

        HcclRootInfo id;
        HcclResult ret = HcclGetRootInfo(&id);
        EXPECT_NE(ret, HCCL_SUCCESS);

        unsetenv("HCCL_WHITELIST_FILE");
        unsetenv("HCCL_WHITELIST_DISABLE");
        remove(realPath.c_str());
    }
    free(buffer);
}

TEST_F(OpbaseTest, ut_hcclGetRootInfo_single_server_fail_none_allowlist)
{
    char *buffer;
    //也可以将buffer作为输出参数
    if((buffer = getcwd(NULL, 0)) != NULL)
    {
        std::string dirPath = buffer;
        std::string fileName = "/ut_hccl_host_allowlist_none";
        std::string realPath = dirPath.c_str() + fileName;
        setenv("HCCL_WHITELIST_FILE", realPath.c_str(), 1);
        setenv("HCCL_WHITELIST_DISABLE", "0", 1);

        HcclRootInfo id;
        HcclResult ret = HcclGetRootInfo(&id);
        EXPECT_NE(ret, HCCL_SUCCESS);

        unsetenv("HCCL_WHITELIST_FILE");
        unsetenv("HCCL_WHITELIST_DISABLE");
        remove(realPath.c_str());
    }
    free(buffer);
}

TEST_F(OpbaseTest, ut_hcclGetRootInfo_single_server_fail_invaild_allowlist)
{
    nlohmann::json whitelist =
    {
        {
            "host_ip",
            {
                "127.0.0.1234",
            }
        }
    };
    char *buffer;
    //也可以将buffer作为输出参数
    if((buffer = getcwd(NULL, 0)) != NULL)
    {
        std::string dirPath = buffer;
        std::string fileName = "/ut_hccl_host_allowlist";
        std::string realPath = dirPath.c_str() + fileName;
        setenv("HCCL_WHITELIST_FILE", realPath.c_str(), 1);
        setenv("HCCL_WHITELIST_DISABLE", "0", 1);
        ofstream outfile(realPath);
        outfile << std::setw(4) << whitelist << std::endl;
        outfile.close();

        HcclRootInfo id;
        HcclResult ret = HcclGetRootInfo(&id);
        EXPECT_NE(ret, HCCL_SUCCESS);

        unsetenv("HCCL_WHITELIST_FILE");
        unsetenv("HCCL_WHITELIST_DISABLE");
        remove(realPath.c_str());
    }
    free(buffer);
}

TEST_F(OpbaseTest, ut_whitelist_multi)
{
    nlohmann::json whitelist =
    {
        {
            "host_ip",
            {
                "127.0.0.1",
                "127.0.0.2",
                "127.0.0.3",
                "127.0.0.4",
            }
        }
    };
    char *buffer;
    //也可以将buffer作为输出参数
    if((buffer = getcwd(NULL, 0)) != NULL)
    {
        std::string dirPath = buffer;
        std::string fileName = "/ut_hccl_host_allowlist";
        std::string realPath = dirPath.c_str() + fileName;
        setenv("HCCL_WHITELIST_FILE", realPath.c_str(), 1);
        setenv("HCCL_WHITELIST_DISABLE", "0", 1);
        ofstream outfile(realPath);
        outfile << std::setw(4) << whitelist << std::endl;
        outfile.close();

        HcclResult ret = InitExternalInput();
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = HcclWhitelist::GetInstance().LoadConfigFile(GetExternalInputHcclWhiteListFile());
        EXPECT_EQ(ret, HCCL_SUCCESS);
        std::vector<HcclIpAddress> allowList;
        ret = HcclWhitelist::GetInstance().GetHostWhiteList(allowList);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        EXPECT_EQ(allowList.size(), 4);

        unsetenv("HCCL_WHITELIST_FILE");
        unsetenv("HCCL_WHITELIST_DISABLE");
        remove(realPath.c_str());
    }
    free(buffer);
}
#endif

TEST_F(OpbaseTest, ut_HcclAlltoAllV_1rank_910A)
{
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }
    outfile.close();
    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 910A 
    DevType deviceType = DevType::DEV_TYPE_910 ;  
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
    void* comm;
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;

    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    Stream stream(StreamType::STREAM_TYPE_ONLINE);
    DeviceMem sendMem = DeviceMem::alloc(128);
    DeviceMem recvMem = DeviceMem::alloc(128);
    HostMem hostSendMem = HostMem::alloc(128);
    memset_s(hostSendMem.ptr(), 128, 1, 128);
    ret = hrtMemSyncCopy(sendMem.ptr(), 128, hostSendMem.ptr(), 128, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u64 sendCounts[1] = {128};
    u64 recvCounts[1] = {128};
    u64 sdispls[1] = {0};
    u64 rdispls[1] = {0};
    ret = HcclAlltoAllV(sendMem.ptr(), sendCounts, sdispls, HCCL_DATA_TYPE_INT32, recvMem.ptr(), recvCounts, rdispls,
                        HCCL_DATA_TYPE_INT32, comm, stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hcclStreamSynchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    GlobalMockObject::verify();
}


TEST_F(OpbaseTest, ut_HcclAlltoAllV_1rank_910B)
{
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 910B
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    void* comm;
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;

    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);

    EXPECT_EQ(ret, HCCL_SUCCESS);

    Stream stream(StreamType::STREAM_TYPE_ONLINE);
    DeviceMem sendMem = DeviceMem::alloc(128);
    DeviceMem recvMem = DeviceMem::alloc(128);
    HostMem hostSendMem = HostMem::alloc(128);
    memset_s(hostSendMem.ptr(), 128, 1, 128);
    ret = hrtMemSyncCopy(sendMem.ptr(), 128, hostSendMem.ptr(), 128, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u64 sendCounts[1] = {128};
    u64 recvCounts[1] = {128};
    u64 sdispls[1] = {0};
    u64 rdispls[1] = {0};
    ret = HcclAlltoAllV(sendMem.ptr(), sendCounts, sdispls, HCCL_DATA_TYPE_INT32, recvMem.ptr(), recvCounts, rdispls,
                        HCCL_DATA_TYPE_INT32, comm, stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hcclStreamSynchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    GlobalMockObject::verify();
}

TEST_P(OpbaseTest, ut_HcclAlltoAllV_1rank_ffts)
{   
    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 910B
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    void* comm;
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;

    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    Stream stream(StreamType::STREAM_TYPE_ONLINE);
    DeviceMem sendMem = DeviceMem::alloc(128);
    DeviceMem recvMem = DeviceMem::alloc(128);
    HostMem hostSendMem = HostMem::alloc(128);
    memset_s(hostSendMem.ptr(), 128, 1, 128);
    ret = hrtMemSyncCopy(sendMem.ptr(), 128, hostSendMem.ptr(), 128, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u64 sendCounts[1] = {128};
    u64 recvCounts[1] = {128};
    u64 sdispls[1] = {0};
    u64 rdispls[1] = {0};
    ret = HcclAlltoAllV(sendMem.ptr(), sendCounts, sdispls, HCCL_DATA_TYPE_INT32, recvMem.ptr(), recvCounts, rdispls,
                        HCCL_DATA_TYPE_INT32, comm, stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hcclStreamSynchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);

    if (fftsSwitch) {
        SetFftsSwitch(true);
    }

    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_PluginRunner) {
    rtStreamCaptureStatus captureStatus = rtStreamCaptureStatus::RT_STREAM_CAPTURE_STATUS_ACTIVE;
    int mockModel = 0;
    void *pmockModel = &mockModel;    
    MOCKER(rtStreamGetCaptureInfo)
    .stubs()
    .with(any(), outBoundP(&captureStatus, sizeof(captureStatus)), outBoundP(&pmockModel, sizeof(pmockModel)))
    .will(returnValue(0));
 
    MOCKER(GetExternalInputHcclEnableFfts)
    .stubs()
    .with(any())
    .will(returnValue(true));   
 
    MOCKER(GetWorkflowMode)
    .stubs()
    .with(any())
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));  

    MOCKER(GetExternalInputTaskExceptionSwitch)
    .stubs()
    .will(returnValue(1));

    u32 deviceLogicId = 0;
    TaskExceptionHandler taskExceptionHandler(deviceLogicId);

    HcclResult ret = taskExceptionHandler.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    PluginRunner pluginRunner(&taskExceptionHandler);

    rtStream_t steam;
    rtStreamCreate(&steam, 5);

    TaskParaDMA dma;
    TaskParaReduce reduce;
    TaskParaNotify notify;
    
    pluginRunner(steam, hccl::TaskType::TASK_SDMA, dma);
    pluginRunner(steam, hccl::TaskType::TASK_REDUCE_INLINE, reduce);
    pluginRunner(steam, hccl::TaskType::TASK_NOTIFY_RECORD, notify);
    rtStreamDestroy(steam);
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_PluginRunner_not_support) {
    rtStreamCaptureStatus captureStatus = rtStreamCaptureStatus::RT_STREAM_CAPTURE_STATUS_ACTIVE;
    int mockModel = 0;
    void *pmockModel = &mockModel;    
    MOCKER(rtStreamGetCaptureInfo)
    .stubs()
    .with(any(), outBoundP(&captureStatus, sizeof(captureStatus)), outBoundP(&pmockModel, sizeof(pmockModel)))
    .will(returnValue(207000));
 
    MOCKER(GetExternalInputHcclEnableFfts)
    .stubs()
    .with(any())
    .will(returnValue(true));   
 
    MOCKER(GetWorkflowMode)
    .stubs()
    .with(any())
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));  

    MOCKER(GetExternalInputTaskExceptionSwitch)
    .stubs()
    .will(returnValue(1));

    u32 deviceLogicId = 0;
    TaskExceptionHandler taskExceptionHandler(deviceLogicId);

    HcclResult ret = taskExceptionHandler.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    PluginRunner pluginRunner(&taskExceptionHandler);

    rtStream_t steam;
    rtStreamCreate(&steam, 5);

    TaskParaDMA dma;
    TaskParaReduce reduce;
    TaskParaNotify notify;
    
    pluginRunner(steam, hccl::TaskType::TASK_SDMA, dma);
    pluginRunner(steam, hccl::TaskType::TASK_REDUCE_INLINE, reduce);
    pluginRunner(steam, hccl::TaskType::TASK_NOTIFY_RECORD, notify);
    rtStreamDestroy(steam);
    GlobalMockObject::verify();
}

TEST_P(OpbaseTest, ut_HcclAlltoAllV_1rank_ffts_capture)
{   
    rtStreamCaptureStatus captureStatus = rtStreamCaptureStatus::RT_STREAM_CAPTURE_STATUS_MAX;
    int mockModel = 0;
    void *pmockModel = &mockModel;    
    MOCKER(rtStreamGetCaptureInfo)
    .stubs()
    .with(any(), outBoundP(&captureStatus, sizeof(captureStatus)), outBoundP(&pmockModel, sizeof(pmockModel)))
    .will(returnValue(0));
    
    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 910B
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    void* comm;
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;

    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    Stream stream(StreamType::STREAM_TYPE_ONLINE);
    DeviceMem sendMem = DeviceMem::alloc(128);
    DeviceMem recvMem = DeviceMem::alloc(128);
    HostMem hostSendMem = HostMem::alloc(128);
    memset_s(hostSendMem.ptr(), 128, 1, 128);
    ret = hrtMemSyncCopy(sendMem.ptr(), 128, hostSendMem.ptr(), 128, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u64 sendCounts[1] = {128};
    u64 recvCounts[1] = {128};
    u64 sdispls[1] = {0};
    u64 rdispls[1] = {0};
    ret = HcclAlltoAllV(sendMem.ptr(), sendCounts, sdispls, HCCL_DATA_TYPE_INT32, recvMem.ptr(), recvCounts, rdispls,
                        HCCL_DATA_TYPE_INT32, comm, stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hcclStreamSynchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);

    if (fftsSwitch) {
        SetFftsSwitch(true);
    }

    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_check_alltoallv_external_mem)
{
    constexpr s32 rankSize = 2;
    u64 sendCounts[rankSize] = {10, INVALID_U64};
    u64 recvCounts[rankSize] = {10, INVALID_U64};
    void *addr = static_cast<void *>(sendCounts);
    HcclResult ret = HcomCheckAlltoAllVExternalMem(addr, sendCounts, addr, recvCounts, rankSize);
    EXPECT_EQ(ret, HCCL_E_PARA);

    sendCounts[1] = 10;
    ret = HcomCheckAlltoAllVExternalMem(addr, sendCounts, addr, recvCounts, rankSize);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(OpbaseTest, ut_gather_alltoallv)
{
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s32 rankSize = 1;
    s32 rank = 0;
    s32 count = 200;
    u32 countsNum = 200;

    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    char* rank_table_file = "./ut_opbase_test.json";

    ret = HcclCommInitClusterInfo(rank_table_file, rank, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    u64 memSize = count * rankSize * sizeof(s32) * countsNum;
    DeviceMem recvMem = DeviceMem::alloc(memSize);
    DeviceMem gatherMem = DeviceMem::alloc(memSize);
    vector<u64> addrInfo;
    vector<u64> addrInfoCountPerRank(rankSize, countsNum);
    HostMem hostSendMem = HostMem::alloc(memSize);
    for (u32 i = 0; i < count * rankSize * countsNum; i++) {
        *((s32 *)hostSendMem.ptr() + i) = rank + 1;
        if (i % count == 0) {
            addrInfo.push_back((uintptr_t)((s32 *)hostSendMem.ptr() + i));
            addrInfo.push_back(count * sizeof(s32));
        }
    }

    DeviceMem devAddrInfo = DeviceMem::alloc(addrInfo.size() * sizeof(u64));
    ret = hrtMemSyncCopy(devAddrInfo.ptr(), addrInfo.size() * sizeof(u64), addrInfo.data(), addrInfo.size() * sizeof(u64), HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem devCountPerRank = DeviceMem::alloc(addrInfoCountPerRank.size() * sizeof(u64));
    ret = hrtMemSyncCopy(devCountPerRank.ptr(), addrInfoCountPerRank.size() * sizeof(u64), addrInfoCountPerRank.data(),
                         addrInfoCountPerRank.size() * sizeof(u64), HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    vector<u64> recvCounts(rankSize, count);
    vector<u64> rdispls(rankSize, 0);
    for (int i = 0; i < rankSize; i++) {
        rdispls[i] = count * i;
        HCCL_INFO("num[%d] displs[%d]", i, count * i);
    }
    DeviceMem devRecvCounts = DeviceMem::alloc(rankSize * sizeof(u64));
    ret = hrtMemSyncCopy(devRecvCounts.ptr(), rankSize * sizeof(u64), recvCounts.data(), rankSize * sizeof(u64), HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem devRdispls = DeviceMem::alloc(rankSize * sizeof(u64));
    ret = hrtMemSyncCopy(devRdispls.ptr(), rankSize * sizeof(u64), rdispls.data(), rankSize * sizeof(u64), HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcomGatherAllToAllVParams params;
    params.addrInfo = devAddrInfo.ptr();
    params.addrInfoCountPerRank = devCountPerRank.ptr();
    params.recvbuf = recvMem.ptr();
    params.recvcounts = devRecvCounts.ptr();
    params.rdispls = devRdispls.ptr();
    params.recvtype = HCCL_DATA_TYPE_INT32;
    params.gatheredbuf = gatherMem.ptr();
    params.group = nullptr;
    params.addrLength = -1;

    MOCKER(HcclAlltoAllV)
    .expects(atMost(10))
    .will(returnValue(0));

    ret = HcclGatherAlltoAllV(params, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    (void)aclrtResetDevice(0);
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_heterog_initComm_error)
{
    nlohmann::json rank_table =
    {
	    {"collective_id", "192.168.3.3-9527-0001"},
        {"master_ip", "192.168.0.100"},
        {"master_port", "18000"},
        {"status", "completed"},
	    {"version","1.1"},
        {"node_list", {
            {
                {"node_addr", "192.168.0.101"},
                {"ranks", {
                    {
                        {"rank_id", "0"}
                    }
                }}
            },
            {
                {"node_addr", "192.168.1.101"},
                {"ranks", {
                    {
                        {"rank_id", "1"}
                    }
                }}
            },
            {
                {"node_addr", "192.168.2.101"},
                {"ranks", {
                    {
                        {"rank_id", "2"}
                    }
                }}
            },
            {
                {"node_addr", "192.168.3.101"},
                {"ranks", {
                    {
                        {"rank_id", "3"}
                    }
                }}
            }
        }
        }
    };

    // MOCKER_CPP(&HcclCommunicator::InitClusterCommMgmt)
    // .stubs()
    // .with(any())
    // .will(returnValue(HCCL_SUCCESS));

    // MOCKER_CPP(&HcclCommunicator::InitAsyncScheduler)
    // .stubs()
    // .with(any())
    // .will(returnValue(HCCL_SUCCESS));

    HcclResult ret;
    HcclComm comm;
    u32 rank_ID = 5;
    std::string rank_table_string = rank_table.dump();
    CommAttr attr;
    attr.deviceId = 0;
    attr.mode = HCCL_MODE_NORMAL;
    ret = HcclInitComm(rank_table_string.c_str(), rank_ID, &attr, &comm);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}


TEST_F(OpbaseTest, ut_GetLocalHostIP)
{
    MOCKER(FindLocalHostIP)
    .stubs()
    .will(returnValue(HCCL_E_INTERNAL));
    std::string serverid;
    std::string ret = GetLocalServerId(serverid);
    EXPECT_EQ(ret, "0.0.0.0");
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_GetLocalHostIP_2)
{
    MOCKER(GetLocalHostIP)
    .stubs()
    .will(returnValue(HCCL_E_NOT_FOUND));
    std::string serverid;
    std::string ret = GetLocalServerId(serverid);
    EXPECT_EQ(ret, "0.0.0.0");
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_GetLocalHostIP_3)
{
    std::vector<std::pair<std::string, HcclIpAddress>> ifInfos;
    HcclResult ret;
    HcclIpAddress ip;
    ip.SetReadableAddress("127.0.0.1");
    ifInfos.push_back({"docker", ip});
    HcclIpAddress hostip;
    ret = FindLocalHostIP(ifInfos, hostip);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(OpbaseTest, ut_GetLocalHostIP_4)
{
    std::vector<std::pair<std::string, HcclIpAddress>> ifInfos;
    HcclResult ret;
    HcclIpAddress ip;
    ip.SetReadableAddress("127.0.0.1");
    ifInfos.push_back({"lo", ip});

    HcclIpAddress hostip;
    ret = FindLocalHostIP(ifInfos, hostip);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(OpbaseTest, ut_HcclCommGetAsyncError)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();
    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;

    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER_CPP(&hcclComm::CommCheckErrorCqe)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclResult result = HCCL_SUCCESS;
    HcclGetCommAsyncError((HcclComm)comm, &result);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);

}

TEST_F(OpbaseTest, ut_HcclCommInitAll)
{
    uint32_t ndev = 8;
    int32_t devices[ndev] = {0, 1, 2, 3, 4, 5, 6, 7};
    HcclComm comms[ndev];
    HcclResult ret = HcclCommInitAll(ndev, devices, comms);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    char commName[128];
    ret = HcclGetCommName(comms[0], commName);
    for (int i = 0; i < ndev; i++) {
        ret = hrtSetDevice(i);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        ret = HcclCommDestroy(comms[i]);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
}

TEST_F(OpbaseTest, ut_HcclGetRootInfo_Exception)
{
    MOCKER(HcclCommInitRootInfo)
        .stubs()
        .will(returnValue(HCCL_E_INTERNAL));

    hrtSetDevice(0);
    HcclRootInfo rootHandle;
    // HcclResult ret = HcclGetRootInfo(&rootHandle);
    // EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclComm comm;
    HcclResult ret = GetDeviceComm(1, rootHandle, 0, 0, comm);

    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}



extern int ra_get_ifaddrs_ipv4(struct RaGetIfattr *config, struct InterfaceInfo ifaddr_infos[], unsigned int *num);
extern int ra_get_ifaddrs_ipv6(struct RaGetIfattr *config, struct InterfaceInfo ifaddr_infos[], unsigned int *num);


HcclResult HrtRaGetDeviceIPStub(u32 devicePhyId, vector<HcclIpAddress> &ipAddr)
{
    ipAddr.push_back(HcclIpAddress{"127.0.0.1"});
    return HCCL_SUCCESS;
}

TEST_F(OpbaseTest, ut_GenerateLocalRankInfo)
{
    TopoInfoDetect topoDetectAgent;
    HcclResult ret;

    MOCKER(RaGetIfaddrs)
    .stubs()
    .will(invoke(ra_get_ifaddrs_ipv4));

    MOCKER(HcclNetDevGetTlsStatus)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    HcclBasicRankInfo localRankInfo;
    setenv("HCCL_SOCKET_FAMILY", "AF_INET", 1);
    InitExternalInput();
    ret = topoDetectAgent.GenerateLocalRankInfo(2, 0, localRankInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(localRankInfo.deviceIP.size(), 1);
    EXPECT_EQ(localRankInfo.deviceIP[0].IsIPv6(), false);
    ResetInitState();
    GlobalMockObject::verify();

    MOCKER(RaGetIfaddrs)
    .stubs()
    .will(invoke(ra_get_ifaddrs_ipv6));

    HcclBasicRankInfo localRankInfo1;
    setenv("HCCL_SOCKET_FAMILY", "AF_INET6", 1);
    InitExternalInput();
    ret = topoDetectAgent.GenerateLocalRankInfo(2, 0, localRankInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(localRankInfo1.deviceIP.size(), 1);
    EXPECT_EQ(localRankInfo1.deviceIP[0].IsIPv6(), true);
    ResetInitState();
    GlobalMockObject::verify();

    HcclBasicRankInfo localRankInfo2;
    setenv("HCCL_SOCKET_FAMILY", "AF_INET6", 1);
    InitExternalInput();
    ret = topoDetectAgent.GenerateLocalRankInfo(2, 0, localRankInfo2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(localRankInfo2.deviceIP.size(), 1);
    EXPECT_EQ(localRankInfo2.deviceIP[0].IsIPv6(), true);
    ResetInitState();
    GlobalMockObject::verify();

    HcclBasicRankInfo localRankInfo3;
    setenv("HCCL_SOCKET_FAMILY", "AF_INET", 1);
    InitExternalInput();
    ret = topoDetectAgent.GenerateLocalRankInfo(2, 0, localRankInfo3);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(localRankInfo3.deviceIP.size(), 1);
    EXPECT_EQ(localRankInfo3.deviceIP[0].IsIPv6(), false);
    ResetInitState();
    GlobalMockObject::verify();

    HcclBasicRankInfo localRankInfo4;
    InitExternalInput();
    ret = topoDetectAgent.GenerateLocalRankInfo(2, 0, localRankInfo4);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(localRankInfo4.deviceIP.size(), 1);
    EXPECT_EQ(localRankInfo4.deviceIP[0].IsIPv6(), false);
    ResetInitState();
    GlobalMockObject::verify();

    setenv("HCCL_SOCKET_FAMILY", "AF_INET", 1);
    MOCKER_CPP(&TopoInfoDetect::PreemptDeviceNicPort).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TopoInfoDetect::PreemptDeviceVnicPort).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    
    MOCKER(GetExternalInputNpuPortSwitch).stubs().will(returnValue(true));
    MOCKER(hrtRaGetDeviceIP).stubs().will(invoke(HrtRaGetDeviceIPStub));
 
    HcclBasicRankInfo localRankInfo5;
    InitExternalInput();
    localRankInfo5.deviceType = DevType::DEV_TYPE_910B;
    
    ret = topoDetectAgent.GenerateLocalRankInfo(2, 0, localRankInfo5);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ResetInitState();
    GlobalMockObject::verify();
}

TEST_P(OpbaseTest, ut_HcclReduce)
{
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    } 

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    // 走1910 4pring
    char* rank_table_file = "./st_opbase_test.json";
    u32 rank_ID = 0;
    string tmpOptions = "";
    HcomSetProfilingMode(HcomProfilingMode::PROFILING_OPEN, tmpOptions);
    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2; 
        recvbuf[j] = -1;
    }
    unsigned int rankSize = 0;
    ret = HcclGetRankSize(comm, &rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("HCCL TEST get rank size[%u] success.", rankSize);

    unsigned int rankId = 0;
    ret = HcclGetRankId(comm, &rankId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("HCCL TEST get rank id[%u] success.", rankId);

    ret = HcclBarrier(comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    ret = HcclReduce(sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, 0, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    HCCL_ERROR("count %d",count);
    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            HCCL_ERROR("j %d : %d",j, recvbuf[j]);
            errors++;
            break;
        }
    }

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
}


TEST_P(OpbaseTest, ut_HcclReduce_inptr_EQ_outPtr)
{
    bool fftsSwitch = GetParam();
   if (fftsSwitch) {
        SetFftsSwitch(true);
    } else {
        SetFftsSwitch(false);
    }

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    char* rank_table_file = "./st_opbase_test.json";
    u32 rank_ID = 0;
    string tmpOptions = "";
    HcomSetProfilingMode(HcomProfilingMode::PROFILING_OPEN, tmpOptions);
    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf = sendbuf ;

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
    unsigned int rankSize = 0;
    ret = HcclGetRankSize(comm, &rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("HCCL TEST get rank size[%u] success.", rankSize);

    unsigned int rankId = 0;
    ret = HcclGetRankId(comm, &rankId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("HCCL TEST get rank id[%u] success.", rankId);

    ret = HcclBarrier(comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclReduce(sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, 0, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    HCCL_ERROR("count %d",count);
    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            HCCL_ERROR("j %d",j);
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);

    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
    SetFftsSwitch(true);
}


TEST_P(OpbaseTest, ut_hcclAllReduce_rankSize_1)
{
    // 单算子 rankSize 为 1 时，不进行 CCLBuffer 分配, 这个用例需要被废弃掉
    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    } else {
        SetFftsSwitch(false);
    }

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    // 走1910 4pring
    char* rank_table_file = "./st_opbase_test.json";
    u32 rank_ID = 0;
    string tmpOptions = "";
    HcomSetProfilingMode(HcomProfilingMode::PROFILING_OPEN, tmpOptions);
    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
    unsigned int rankSize = 0;
    ret = HcclGetRankSize(comm, &rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("HCCL TEST get rank size[%u] success.", rankSize);

    unsigned int rankId = 0;
    ret = HcclGetRankId(comm, &rankId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("HCCL TEST get rank id[%u] success.", rankId);

    ret = HcclBarrier(comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    void *commInputPtr = nullptr;
    u64 commInputSize = 0;
    void *commOutputPtr = nullptr;
    u64 commOutputSize = 0;
    ret = hcclComm->GetInCCLbuffer(commInputPtr, commInputSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hcclComm->GetOutCCLbuffer(commOutputPtr, commOutputSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclAllReduce(sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    HCCL_ERROR("count %d",count);
    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            HCCL_ERROR("j %d",j);
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
    SetFftsSwitch(true);
}

TEST_F(OpbaseTest, ut_HcclGetErrorString)
{
    EXPECT_EQ("no error", std::string(HcclGetErrorString(HCCL_SUCCESS)));
    EXPECT_EQ("parameter error", std::string(HcclGetErrorString(HCCL_E_PARA)));
    EXPECT_EQ("empty pointer", std::string(HcclGetErrorString(HCCL_E_PTR)));
    EXPECT_EQ("memory error", std::string(HcclGetErrorString( HCCL_E_MEMORY)));
    EXPECT_EQ("internal error", std::string(HcclGetErrorString(HCCL_E_INTERNAL)));
    EXPECT_EQ("not support feature", std::string(HcclGetErrorString(HCCL_E_NOT_SUPPORT)));
    EXPECT_EQ("not found specific resource", std::string(HcclGetErrorString(HCCL_E_NOT_FOUND)));
    EXPECT_EQ("resource unavailable", std::string(HcclGetErrorString(HCCL_E_UNAVAIL)));
    EXPECT_EQ("call system interface error", std::string(HcclGetErrorString(HCCL_E_SYSCALL)));
    EXPECT_EQ("timeout", std::string(HcclGetErrorString(HCCL_E_TIMEOUT)));
    EXPECT_EQ("open file fail", std::string(HcclGetErrorString(HCCL_E_OPEN_FILE_FAILURE)));
    EXPECT_EQ("tcp connect fail", std::string(HcclGetErrorString(HCCL_E_TCP_CONNECT)));
    EXPECT_EQ("roce connect fail", std::string(HcclGetErrorString(HCCL_E_ROCE_CONNECT)));
    EXPECT_EQ("tcp transfer fail", std::string(HcclGetErrorString(HCCL_E_TCP_TRANSFER)));
    EXPECT_EQ("roce transfer fail", std::string(HcclGetErrorString(HCCL_E_ROCE_TRANSFER)));
    EXPECT_EQ("call runtime api fail", std::string(HcclGetErrorString(HCCL_E_RUNTIME)));
    EXPECT_EQ("call profiling api fail", std::string(HcclGetErrorString(HCCL_E_PROFILING)));
    EXPECT_EQ("call cce api fail", std::string(HcclGetErrorString(HCCL_E_CCE)));
    EXPECT_EQ("call network api fail", std::string(HcclGetErrorString(HCCL_E_NETWORK)));
    EXPECT_EQ("error cqe", std::string(HcclGetErrorString(HCCL_E_REMOTE)));
    EXPECT_EQ("call network api fail", std::string(HcclGetErrorString(HCCL_E_NETWORK)));
    EXPECT_EQ("unknow error", std::string(HcclGetErrorString(HCCL_E_RESERVED)));
}

void ExecReduce(int ndev, HcclComm comm, uint32_t deviceLogicID, uint32_t rankId)
{

    int ret = HCCL_SUCCESS;
    /* 1. 申请相关资源 */
    ret = hrtSetDevice(deviceLogicID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rtStream_t stream;
    rtError_t rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    s32 count = 10;

    float* sendbuf;
    sendbuf= (float*)sal_malloc(count * sizeof(float));
    sal_memset(sendbuf, count * sizeof(float), 0, count * sizeof(float));

    float* recvbuf;
    recvbuf= (float*)sal_malloc(count * sizeof(float));
    sal_memset(recvbuf, count * sizeof(float), 0, count * sizeof(float));

    for (int j = 0; j < count; j++) {
        sendbuf[j] = 2;
        recvbuf[j] = 2 * ndev;
    }

    uint32_t rankSize = 0;
    ret = HcclGetRankSize(comm, &rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankSize, ndev);

    uint32_t rankID = 0;
    ret = HcclGetRankId(comm, &rankID);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankID, rankId);

    /* 2. 执行 HcclReduce */
    ret = HcclReduce(sendbuf, recvbuf, count, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM, 0, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    /* 3. 校验执行结果准确性 */
    s32 errors = 0;
    if (rankId == 0) {
        for (int j = 0; j < count; j++) {
            recvbuf[j] = 2 * ndev;
            if (recvbuf[j] != 2 * ndev) {
                printf("rankId : %d, deviceLogicID: %d, j : %d, val : %d \n", rankId, deviceLogicID, j, recvbuf[j]);
                errors ++;
                break;
            }
        }
    }
    EXPECT_EQ(errors, 0);

    /* 4. 执行allreduce */
    sal_memset(recvbuf, count * sizeof(float), 0, count * sizeof(float));

    ret = HcclAllReduce(sendbuf, recvbuf, count, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    /* 5. 校验执行结果准确性 */
    for (int j = 0; j < count; j++) {
        recvbuf[j] = 2 * ndev;
        if (recvbuf[j] != 2 * ndev) {
            printf("rankId : %d, deviceLogicID: %d, j : %d, val : %d \n", rankId, deviceLogicID, j, recvbuf[j]);
            errors ++;
            break;
        }
    }
    EXPECT_EQ(errors, 0);

    /* 6. 释放相关资源 */
    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    ret = hrtResetDevice(deviceLogicID);
    EXPECT_EQ(ret, 0);
    return;
}

TEST_P(OpbaseTest, ut_hcclReducescatterV)
{
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
 
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&HcclCommunicator::ExecOp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
 
    nlohmann::json rank_table = rank_table_910_1server_2rank;
 
    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);
 
    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }
 
    outfile.close();
 
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    u64* sendCounts;
    u64* sendDispls;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    u32 rankSize = 0;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;
 
    // 走1910 4pring
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;
 
    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    ret = hcclComm->GetRankSize(rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    sendbuf= (s8*)sal_malloc(rankSize * count * sizeof(s8));
     sal_memset(sendbuf, rankSize * count * sizeof(s8), 0, rankSize * count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));
 
    sendCounts= (u64*)sal_malloc(rankSize * sizeof(u64));
     sal_memset(sendCounts, rankSize * sizeof(u64), 0, rankSize * sizeof(u64));
    sendDispls= (u64*)sal_malloc(rankSize * sizeof(u64));
     sal_memset(sendDispls, rankSize * sizeof(u64), 0, rankSize * sizeof(u64));
 
    for (int j = 0; j < rankSize * count; j++)
    {
        sendbuf[j] = 2;
    }
 
    for (int i = 0; i < rankSize; i++)
    {
        sendCounts[i] = count;
        if (i > 0) {
            sendDispls[i] = sendDispls[i-1] + sendCounts[i-1];
        }
    }
 
    ret = HcclReduceScatterV(sendbuf, sendCounts, sendDispls, recvbuf, count, HCCL_DATA_TYPE_INT8,
        HCCL_REDUCE_SUM, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclReduceScatterV(sendbuf, sendCounts, sendDispls, recvbuf, count, HCCL_DATA_TYPE_INT8,
        HCCL_REDUCE_SUM, comm, stream);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    sal_free(sendbuf);
    sal_free(recvbuf);
    sal_free(sendCounts);
    sal_free(sendDispls);
    rt_ret = rtStreamDestroy(stream);
 
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
    GlobalMockObject::verify();
}

TEST_P(OpbaseTest, ut_hcclReducescatterV_capture)
{   
    rtStreamCaptureStatus captureStatus = rtStreamCaptureStatus::RT_STREAM_CAPTURE_STATUS_INVALIDATED;
    int mockModel = 0;
    void *pmockModel = &mockModel;    
    MOCKER(rtStreamGetCaptureInfo)
    .stubs()
    .with(any(), outBoundP(&captureStatus, sizeof(captureStatus)), outBoundP(&pmockModel, sizeof(pmockModel)))
    .will(returnValue(0));

    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
 
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&HcclCommunicator::ExecOp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclCommunicator::StreamIsCapture)
    .stubs()
    .with(any())
    .will(returnValue(true));
 
    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
 
    nlohmann::json rank_table = rank_table_910_1server_2rank;
 
    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);
 
    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }
 
    outfile.close();
 
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    u64* sendCounts;
    u64* sendDispls;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    u32 rankSize = 0;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;
 
    // 走1910 4pring
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;
 
    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    ret = hcclComm->GetRankSize(rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    sendbuf= (s8*)sal_malloc(rankSize * count * sizeof(s8));
     sal_memset(sendbuf, rankSize * count * sizeof(s8), 0, rankSize * count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));
 
    sendCounts= (u64*)sal_malloc(rankSize * sizeof(u64));
     sal_memset(sendCounts, rankSize * sizeof(u64), 0, rankSize * sizeof(u64));
    sendDispls= (u64*)sal_malloc(rankSize * sizeof(u64));
     sal_memset(sendDispls, rankSize * sizeof(u64), 0, rankSize * sizeof(u64));
 
    for (int j = 0; j < rankSize * count; j++)
    {
        sendbuf[j] = 2;
    }
 
    for (int i = 0; i < rankSize; i++)
    {
        sendCounts[i] = count;
        if (i > 0) {
            sendDispls[i] = sendDispls[i-1] + sendCounts[i-1];
        }
    }
 
    ret = HcclReduceScatterV(sendbuf, sendCounts, sendDispls, recvbuf, count, HCCL_DATA_TYPE_INT8,
        HCCL_REDUCE_SUM, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclReduceScatterV(sendbuf, sendCounts, sendDispls, recvbuf, count, HCCL_DATA_TYPE_INT8,
        HCCL_REDUCE_SUM, comm, stream);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    sal_free(sendbuf);
    sal_free(recvbuf);
    sal_free(sendCounts);
    sal_free(sendDispls);
    rt_ret = rtStreamDestroy(stream);
 
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
    GlobalMockObject::verify();
}
 
TEST_P(OpbaseTest, ut_hcclAllGatherV)
{
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
 
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&HcclCommunicator::ExecOp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
 
    nlohmann::json rank_table = rank_table_910_1server_2rank;
 
    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);
 
    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }
 
    outfile.close();
 
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    u64* recvCounts;
    u64* recvDispls;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    u32 rankSize = 0;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;
 
    // 走1910 4pring
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;
 
    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    ret = hcclComm->GetRankSize(rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(rankSize * count * sizeof(s8));
     sal_memset(recvbuf, rankSize * count * sizeof(s8), 0, rankSize * count * sizeof(s8));
 
    recvCounts= (u64*)sal_malloc(rankSize * sizeof(u64));
     sal_memset(recvCounts, rankSize * sizeof(u64), 0, rankSize * sizeof(u64));
    recvDispls= (u64*)sal_malloc(rankSize * sizeof(u64));
     sal_memset(recvDispls, rankSize * sizeof(u64), 0, rankSize * sizeof(u64));
 
    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
 
    for (int i = 0; i < rankSize; i++)
    {
        recvCounts[i] = count;
        if (i > 0) {
            recvDispls[i] = recvDispls[i-1] + recvCounts[i-1];
        }
    }
 
    ret = HcclAllGatherV(sendbuf, count, recvbuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclAllGatherV(sendbuf, count, recvbuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    sal_free(sendbuf);
    sal_free(recvbuf);
    sal_free(recvCounts);
    sal_free(recvDispls);
    rt_ret = rtStreamDestroy(stream);
 
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
 
    GlobalMockObject::verify();
}

TEST_P(OpbaseTest, ut_hcclAllGatherV_capture_rdma)
{
    rtStreamCaptureStatus captureStatus = rtStreamCaptureStatus::RT_STREAM_CAPTURE_STATUS_INVALIDATED;
    int mockModel = 0;
    void *pmockModel = &mockModel;    
    MOCKER(rtStreamGetCaptureInfo)
    .stubs()
    .with(any(), outBoundP(&captureStatus, sizeof(captureStatus)), outBoundP(&pmockModel, sizeof(pmockModel)))
    .will(returnValue(207000));

    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));

    MOCKER_CPP(&HcclCommunicator::StreamIsCapture)
    .stubs()
    .with(any())
    .will(returnValue(true));

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&HcclCommunicator::ExecOp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
 
    nlohmann::json rank_table = rank_table_910_2server_4rank;
 
    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);
 
    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }
 
    outfile.close();
 
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    u64* recvCounts;
    u64* recvDispls;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    u32 rankSize = 0;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;
 
    // 走1910 4pring
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;
 
    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    ret = hcclComm->GetRankSize(rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(rankSize * count * sizeof(s8));
     sal_memset(recvbuf, rankSize * count * sizeof(s8), 0, rankSize * count * sizeof(s8));
 
    recvCounts= (u64*)sal_malloc(rankSize * sizeof(u64));
     sal_memset(recvCounts, rankSize * sizeof(u64), 0, rankSize * sizeof(u64));
    recvDispls= (u64*)sal_malloc(rankSize * sizeof(u64));
     sal_memset(recvDispls, rankSize * sizeof(u64), 0, rankSize * sizeof(u64));
 
    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
 
    for (int i = 0; i < rankSize; i++)
    {
        recvCounts[i] = count;
        if (i > 0) {
            recvDispls[i] = recvDispls[i-1] + recvCounts[i-1];
        }
    }
 
    ret = HcclAllGatherV(sendbuf, count, recvbuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclAllGatherV(sendbuf, count, recvbuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    sal_free(sendbuf);
    sal_free(recvbuf);
    sal_free(recvCounts);
    sal_free(recvDispls);
    rt_ret = rtStreamDestroy(stream);
 
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
 
    GlobalMockObject::verify();
}

TEST_P(OpbaseTest, ut_hcclReducescatterVFor310P3)
{
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
 
    DevType deviceType = DevType::DEV_TYPE_310P3;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&HcclCommunicator::ExecOp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
 
    nlohmann::json rank_table = rank_table_910_1server_2rank;
 
    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);
 
    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }
 
    outfile.close();
 
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    u64* sendCounts;
    u64* sendDispls;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    u32 rankSize = 0;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;
 
    // 走1910 4pring
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;
 
    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    ret = hcclComm->GetRankSize(rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    sendbuf= (s8*)sal_malloc(rankSize * count * sizeof(s8));
     sal_memset(sendbuf, rankSize * count * sizeof(s8), 0, rankSize * count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));
 
    sendCounts= (u64*)sal_malloc(rankSize * sizeof(u64));
     sal_memset(sendCounts, rankSize * sizeof(u64), 0, rankSize * sizeof(u64));
    sendDispls= (u64*)sal_malloc(rankSize * sizeof(u64));
     sal_memset(sendDispls, rankSize * sizeof(u64), 0, rankSize * sizeof(u64));
 
    for (int j = 0; j < rankSize * count; j++)
    {
        sendbuf[j] = 2;
    }
 
    for (int i = 0; i < rankSize; i++)
    {
        sendCounts[i] = count;
        if (i > 0) {
            sendDispls[i] = sendDispls[i-1] + sendCounts[i-1];
        }
    }
 
    ret = HcclReduceScatterV(sendbuf, sendCounts, sendDispls, recvbuf, count, HCCL_DATA_TYPE_INT8,
        HCCL_REDUCE_SUM, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclReduceScatterV(sendbuf, sendCounts, sendDispls, recvbuf, count, HCCL_DATA_TYPE_INT8,
        HCCL_REDUCE_SUM, comm, stream);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    sal_free(sendbuf);
    sal_free(recvbuf);
    sal_free(sendCounts);
    sal_free(sendDispls);
    rt_ret = rtStreamDestroy(stream);
 
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
    GlobalMockObject::verify();
}
 
TEST_P(OpbaseTest, ut_hcclAllGatherVFor310P3)
{
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
 
    DevType deviceType = DevType::DEV_TYPE_310P3;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&HcclCommunicator::ExecOp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
 
    nlohmann::json rank_table = rank_table_910_1server_2rank;
 
    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);
 
    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }
 
    outfile.close();
 
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    u64* recvCounts;
    u64* recvDispls;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    u32 rankSize = 0;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;
 
    // 走1910 4pring
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;
 
    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    ret = hcclComm->GetRankSize(rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(rankSize * count * sizeof(s8));
     sal_memset(recvbuf, rankSize * count * sizeof(s8), 0, rankSize * count * sizeof(s8));
 
    recvCounts= (u64*)sal_malloc(rankSize * sizeof(u64));
     sal_memset(recvCounts, rankSize * sizeof(u64), 0, rankSize * sizeof(u64));
    recvDispls= (u64*)sal_malloc(rankSize * sizeof(u64));
     sal_memset(recvDispls, rankSize * sizeof(u64), 0, rankSize * sizeof(u64));
 
    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
 
    for (int i = 0; i < rankSize; i++)
    {
        recvCounts[i] = count;
        if (i > 0) {
            recvDispls[i] = recvDispls[i-1] + recvCounts[i-1];
        }
    }
 
    ret = HcclAllGatherV(sendbuf, count, recvbuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclAllGatherV(sendbuf, count, recvbuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    sal_free(sendbuf);
    sal_free(recvbuf);
    sal_free(recvCounts);
    sal_free(recvDispls);
    rt_ret = rtStreamDestroy(stream);
 
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
 
    GlobalMockObject::verify();
}

TEST_P(OpbaseTest, ut_hcclAllGatherVCheckCounts)
{
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
 
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&HcclCommunicator::ExecOp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
 
    nlohmann::json rank_table = rank_table_910_1server_2rank;
 
    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);
 
    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }
 
    outfile.close();
 
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    u64* recvCounts;
    u64* recvDispls;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    u32 rankSize = 0;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;
 
    // 走1910 4pring
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;
 
    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    ret = hcclComm->GetRankSize(rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(rankSize * count * sizeof(s8));
     sal_memset(recvbuf, rankSize * count * sizeof(s8), 0, rankSize * count * sizeof(s8));
 
    recvCounts= (u64*)sal_malloc(rankSize * sizeof(u64));
     sal_memset(recvCounts, rankSize * sizeof(u64), 0, rankSize * sizeof(u64));
    recvDispls= (u64*)sal_malloc(rankSize * sizeof(u64));
     sal_memset(recvDispls, rankSize * sizeof(u64), 0, rankSize * sizeof(u64));
 
    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
 
    for (int i = 0; i < rankSize; i++)
    {
        recvCounts[i] = count;
        if (i > 0) {
            recvDispls[i] = recvDispls[i-1] + recvCounts[i-1] + 100;
        }
    }
 
    ret = HcclAllGatherV(sendbuf, count, recvbuf, recvCounts, recvDispls, HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    sal_free(sendbuf);
    sal_free(recvbuf);
    sal_free(recvCounts);
    sal_free(recvDispls);
    rt_ret = rtStreamDestroy(stream);
 
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
 
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_HcclReduceOverFlow)
{
    int ret = HCCL_SUCCESS;
    uint32_t ndev = 2;
    int32_t devices[ndev] = {1,2};
    HcclComm comms[ndev];

    DevType deviceType = DevType::DEV_TYPE_910;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtReduceAsync)
    .expects(atMost(0))
    .will(returnValue(HCCL_SUCCESS));

    for (int i = 0; i < ndev; i++) {
        ret = hrtSetDevice(devices[i]);
        EXPECT_EQ(ret, 0);
    }
    ret = HcclCommInitAll(ndev, devices, comms);
    EXPECT_EQ(ret, 0);

    std::vector<std::thread> threads;
    threads.resize(ndev);
    for (uint32_t i = 0; i < ndev; i++) {
        threads[i] = std::thread(ExecReduce, ndev, comms[i], devices[i], i);
    }

    for (uint32_t i = 0; i < ndev; ++i) {
        threads[i].join();
    }

    for (uint32_t i = 0; i < ndev; i++) {
        ret = hrtResetDevice(devices[i]);
        EXPECT_EQ(ret, 0);
        ret = HcclCommDestroy(comms[i]);
        EXPECT_EQ(ret, 0);
    }
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_HcclAiCpuResource91093)
{
    HcclRootInfo id;
    char group[ROOTINFO_INDENTIFIER_MAX_LENGTH] = {0};
    void *commContext = nullptr;
    void *aicpuNotify = nullptr;
    rtStream_t Opstream;

    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
	MOCKER(hrtRaGetSingleSocketVnicIpInfo)
	.stubs()
	.with(any())
	.will(invoke(stub_hrtRaGetSingleSocketVnicIpInfo));
    MOCKER_CPP(&HcclCommunicator::HcclGetCmdTimeout)
    .stubs()
    .will(returnValue(50));
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclGetCommName(newcomm, group);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    printf("commName:%s", group);

    ret = HcclCreateComResource(group, 1, &commContext);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclGetAicpuOpStreamNotify(group, &Opstream, &aicpuNotify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(aicpuNotify, nullptr);

    HcclComm commHandle;
    ret = HcomGetCommHandleByGroup(group, &commHandle);

    ret = HcclCreateComResourceByComm(commHandle, 1, true, &commContext);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclGetAicpuOpStreamAndNotify(commHandle, &Opstream, 1, &aicpuNotify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(aicpuNotify, nullptr);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_HcclAiCpuResource)
{
    HcclRootInfo id;
    char group[ROOTINFO_INDENTIFIER_MAX_LENGTH] = {0};
    void *commContext = nullptr;
    void *aicpuNotify = nullptr;
    rtStream_t Opstream;

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::HcclGetCmdTimeout)
    .stubs()
    .will(returnValue(50));

    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclGetCommName(newcomm, group);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    printf("commName:%s", group);

    ret = HcclCreateComResource(group, 1, &commContext);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclGetAicpuOpStreamNotify(group, &Opstream, &aicpuNotify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(aicpuNotify, nullptr);

    HcclComm commHandle;
    ret = HcomGetCommHandleByGroup(group, &commHandle);

    ret = HcclCreateComResourceByComm(commHandle, 1, true, &commContext);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(commContext, nullptr);

    ret = HcclGetAicpuOpStreamAndNotify(commHandle, &Opstream, 1, &aicpuNotify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(aicpuNotify, nullptr);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

#pragma pack(4)
struct MC2Tiling {
    uint32_t version = 1;
    uint32_t mc2HcommCnt = 2;
    Mc2ServerCfg serverCfg;
    Mc2HcommCfg cfg1;
    Mc2HcommCfg cfg2;
};
#pragma pack()

TEST_F(OpbaseTest, ut_HcclAiCpuResourceByTiling)
{
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));

    MOCKER_CPP(&HcclCommunicator::HcclGetCmdTimeout)
    .stubs()
    .will(returnValue(50));

    HcclRootInfo id;
    char group[ROOTINFO_INDENTIFIER_MAX_LENGTH] = {0};
    void *commContext = nullptr;
    void *aicpuNotify = nullptr;
    rtStream_t Opstream = nullptr;
 
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
 
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = HcclGetCommName(newcomm, group);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    printf("commName:%s\n", group);
 
    HcclComm commHandle;
    ret = HcomGetCommHandleByGroup(group, &commHandle);
 
    MC2Tiling mc2Tiling;
    mc2Tiling.version = 2;
    mc2Tiling.mc2HcommCnt = 2;
 
    mc2Tiling.cfg1.opType = 8;
    mc2Tiling.cfg1.reduceType = 0;
    strcpy_s(mc2Tiling.cfg1.groupName, 128, group);
    strcpy_s(mc2Tiling.cfg1.algConfig, 128, "AlltoAll=level0:fullmesh;level1:pairwise");

    mc2Tiling.cfg2.opType = 6;
    mc2Tiling.cfg2.reduceType = 0;
    strcpy_s(mc2Tiling.cfg2.groupName, 128, group);
    strcpy_s(mc2Tiling.cfg2.algConfig, 128, "AllGather=level0:ring");

    rtStream_t stream;
    rtError_t rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamCreate(&stream, 0);
 
    ret = HcclAllocComResourceByTiling(commHandle, stream, &mc2Tiling, &commContext);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(commContext, nullptr);
 
    ret = HcclGetAicpuOpStreamAndNotify(commHandle, &Opstream, 1, &aicpuNotify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(Opstream, nullptr);
 
    mc2Tiling.version = 0;

    ret = HcclAllocComResourceByTiling(commHandle, stream, &mc2Tiling, &commContext);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(commContext, nullptr);
 
    ret = HcclGetAicpuOpStreamAndNotify(commHandle, &Opstream, 1, &aicpuNotify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(Opstream, nullptr);

    rt_ret = rtStreamDestroy(stream);
    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

#pragma pack(4)
struct MC2TilingV2 {
    uint32_t version = 1;
    uint32_t mc2HcommCnt = 2;
    uint32_t offset[8];
    Mc2HcommCfg cfg1;
    uint32_t tmp;
    Mc2HcommCfg cfg2;
};
#pragma pack()

TEST_F(OpbaseTest, ut_HcclAiCpuResourceByTiling_A3)
{
    HcclRootInfo id;
    char group[ROOTINFO_INDENTIFIER_MAX_LENGTH] = {0};
    void *commContext = nullptr;
    void *aicpuNotify = nullptr;
    rtStream_t Opstream = nullptr;
 
    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
 
     MOCKER_CPP(&HcclCommunicator::HcclGetCmdTimeout)
    .stubs()
    .will(returnValue(50));

    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = HcclGetCommName(newcomm, group);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    printf("commName:%s\n", group);
 
    HcclComm commHandle;
    ret = HcomGetCommHandleByGroup(group, &commHandle);
 
    MC2TilingV2 mc2Tiling;
    mc2Tiling.version = 2;
    mc2Tiling.mc2HcommCnt = 2;
    mc2Tiling.offset[0] = reinterpret_cast<uint8_t *>(&(mc2Tiling.cfg1)) - reinterpret_cast<uint8_t *>(&mc2Tiling);
    mc2Tiling.offset[1] = reinterpret_cast<uint8_t *>(&(mc2Tiling.cfg2)) - reinterpret_cast<uint8_t *>(&mc2Tiling);
 
    mc2Tiling.cfg1.opType = 8;
    mc2Tiling.cfg1.reduceType = 0;
    strcpy_s(mc2Tiling.cfg1.groupName, 128, group);
    strcpy_s(mc2Tiling.cfg1.algConfig, 128, "AlltoAll=level0:fullmesh;level1:pairwise");

    mc2Tiling.cfg2.opType = 6;
    mc2Tiling.cfg2.reduceType = 0;
    strcpy_s(mc2Tiling.cfg2.groupName, 128, group);
    strcpy_s(mc2Tiling.cfg2.algConfig, 128, "AllGather=level0:ring");

    rtStream_t stream;
    rtError_t rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamCreate(&stream, 0);
 
    ret = HcclAllocComResourceByTiling(commHandle, stream, &mc2Tiling, &commContext);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(commContext, nullptr);
 
    ret = HcclGetAicpuOpStreamAndNotify(commHandle, &Opstream, 1, &aicpuNotify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(Opstream, nullptr);
 
    mc2Tiling.version = 3;

    ret = HcclAllocComResourceByTiling(commHandle, stream, &mc2Tiling, &commContext);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(commContext, nullptr);
 
    ret = HcclGetAicpuOpStreamAndNotify(commHandle, &Opstream, 1, &aicpuNotify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(Opstream, nullptr);

    rt_ret = rtStreamDestroy(stream);
    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_hcclimpl_AiCpuInitMultiStreamResource)
{
    level1StreamInfo_t streamInfo;
    HcclRootInfo id;

    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(newcomm);
    HcclCommunicator* impl = hcclComm->communicator_.get();
    std::string tag = "CreatecomResource_" + hcclComm->GetIdentifier();
    HcclRtStream stream = nullptr;
    void *commInputPtr = nullptr;
    void *commOutputPtr = nullptr;
    u64 commInputSize, commOutputSize;
    ret = hcclComm->GetInCCLbuffer(commInputPtr, commInputSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hcclComm->GetOutCCLbuffer(commOutputPtr, commOutputSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem inputMem = DeviceMem::create(commInputPtr, commInputSize);
    DeviceMem outputMem = DeviceMem::create(commOutputPtr, commOutputSize);
    ret = impl->notifyPool_->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    AlgType algType;
    algType.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    algType.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    ret = impl->implAlg_->CreateComm(tag, inputMem, outputMem, algType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    impl->implAlg_->SetAlgType(algType, HcclCMDType::HCCL_CMD_ALL);
    streamInfo.ringNum = 8;

    ret = impl->implAlg_->pimpl_->InitMultiStreamResource(tag, streamInfo, algType, true);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(streamInfo.ringDeviceSignal.size(), streamInfo.ringNum - 1);
    EXPECT_EQ(streamInfo.ringDeviceSignalAux.size(), streamInfo.ringNum - 1);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(OpbaseTest, ut_hcclimpl_AiCpuCreateMutiStreamRes)
{
    level1StreamInfo_t streamInfo;
    HcclRootInfo id;

    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(newcomm);
    HcclCommunicator* impl = hcclComm->communicator_.get();
    std::string tag = "CreatecomResource_" + hcclComm->GetIdentifier();
    HcclRtStream stream = nullptr;
    void *commInputPtr = nullptr;
    void *commOutputPtr = nullptr;
    u64 commInputSize, commOutputSize;
    ret = hcclComm->GetInCCLbuffer(commInputPtr, commInputSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hcclComm->GetOutCCLbuffer(commOutputPtr, commOutputSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem inputMem = DeviceMem::create(commInputPtr, commInputSize);
    DeviceMem outputMem = DeviceMem::create(commOutputPtr, commOutputSize);
    ret = impl->notifyPool_->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    AlgType algType;
    algType.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    algType.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_NHR;
    ret = impl->implAlg_->CreateComm(tag, inputMem, outputMem, algType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    impl->implAlg_->SetAlgType(algType, HcclCMDType::HCCL_CMD_ALL);
    streamInfo.ringNum = 8;

    Stream masterStream(StreamType::STREAM_TYPE_ONLINE);
    masterStream.SetMode(12);

    ret = impl->implAlg_->pimpl_->CreateMutiStreamRes(tag, masterStream, streamInfo, algType, true);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

extern thread_local s32 g_hcclDeviceId;
extern HcclOpInfoCtx g_opHcomInfos[MAX_MODULE_DEVICE_NUM + 1];
TEST_F(OpbaseTest, ut_GetHcclOpInfoCtx_cover_bottom_false)
{
    for (int i = 0; i < MAX_MODULE_DEVICE_NUM + 1; i++) {
        g_opHcomInfos[i].isUsed = false;
    }
    g_hcclDeviceId = INVALID_INT;
    HcclResult ret = HCCL_SUCCESS;
    MOCKER(hrtGetDevice)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_INTERNAL));
    const char *group = "group";
    std::shared_ptr<hccl::hcclComm> comm;
    ret = HcclGetCommHandle(group, comm);
    EXPECT_EQ(ret, HCCL_E_PARA);

    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_GetHcclOpInfoCtx_cover_bottom_true)
{
    for (int i = 0; i < MAX_MODULE_DEVICE_NUM; i++) {
        g_opHcomInfos[i].isUsed = false;
    }
    g_opHcomInfos[MAX_MODULE_DEVICE_NUM].isUsed = true;
    g_hcclDeviceId = INVALID_INT;
    HcclResult ret = HCCL_SUCCESS;
    MOCKER(hrtGetDevice)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    const char *group = "group";
    std::shared_ptr<hccl::hcclComm> comm;
    g_hcclDeviceId = 0;
    ret = HcclGetCommHandle(group, comm);
    EXPECT_EQ(ret, HCCL_E_PARA);

    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_opbaseinit_hcomstream)
{
    int ret = HCCL_SUCCESS;
    uint32_t ndev = 1;
    int32_t devices[ndev] = {0};
    HcclComm comms[ndev];
    for (int i = 0; i < ndev; i++) {
        ret = hrtSetDevice(devices[i]);
        EXPECT_EQ(ret, 0);
    }
    ret = HcclCommInitAll(ndev, devices, comms);
    EXPECT_EQ(ret, 0);

    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comms[0]);
    string groupName = hcclComm->GetIdentifier();
    u64 memSize = 0;
    ret = HcomGetWorkspaceMemSize("HcomReduceScatter", 1, HCCL_DATA_TYPE_INT8, groupName.c_str(), memSize);
    EXPECT_EQ(ret, 0);

    for (uint32_t i = 0; i < ndev; i++) {
        ret = hrtResetDevice(devices[i]);
        EXPECT_EQ(ret, 0);
        ret = HcclCommDestroy(comms[i]);
        EXPECT_EQ(ret, 0);
    }
    GlobalMockObject::verify();
}

#if 1
#define HCCL_COM_DATA_SIZE 1024
#define DEV_NUM_8 8

TEST_F(OpbaseTest, ut_BatchSendRecv_self)
{
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
    
    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;
    s32 ndev = DEV_NUM_8;

    char* rank_table_file = "./ut_opbase_test.json";

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));
    ret = HcclCommInitClusterInfo(rank_table_file, 0, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    HcclSendRecvItem data[] = {{HcclSendRecvType::HCCL_SEND, sendbuf, HCCL_COM_DATA_SIZE, HcclDataType::HCCL_DATA_TYPE_INT8, 0},
                               {HcclSendRecvType::HCCL_RECV, recvbuf, HCCL_COM_DATA_SIZE, HcclDataType::HCCL_DATA_TYPE_INT8, 0}};
    HcclSendRecvItem* dataPtr = data;
    ret = HcclBatchSendRecv(dataPtr, 2, comm, stream);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_BatchSendRecv_2rank_send)
{

    nlohmann::json rank_table = rank_table_910_1server_2rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;
    s32 ndev = DEV_NUM_8;
    // 走1910 8pring
    char* rank_table_file = "./ut_opbase_test.json";

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));
    ret = HcclCommInitClusterInfo(rank_table_file, 0, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::ProcessSendDataSlice)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::ProcessRecvDataSlice)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::GetSendTargetLink)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::GetRecvTargetLink)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::RunLoopInHostUnfoldMode)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    HcclSendRecvItem data[] = {{HcclSendRecvType::HCCL_SEND, sendbuf, HCCL_COM_DATA_SIZE, HcclDataType::HCCL_DATA_TYPE_INT8, 1}};
    HcclSendRecvItem* dataPtr = data;
    ret = HcclBatchSendRecv(dataPtr, 1, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_BatchSendRecv_2rank_recv)
{
    nlohmann::json rank_table = rank_table_910_1server_2rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;
    s32 ndev = DEV_NUM_8;
    // 走1910 8pring
    char* rank_table_file = "./ut_opbase_test.json";

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));
    ret = HcclCommInitClusterInfo(rank_table_file, 0, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::ProcessSendDataSlice)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::ProcessRecvDataSlice)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::GetSendTargetLink)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::GetRecvTargetLink)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::RunLoopInHostUnfoldMode)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    HcclSendRecvItem data[] = {{HcclSendRecvType::HCCL_RECV, recvbuf, HCCL_COM_DATA_SIZE, HcclDataType::HCCL_DATA_TYPE_INT8, 1}};
    HcclSendRecvItem* dataPtr = data;
    ret = HcclBatchSendRecv(dataPtr, 1, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);

    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_BatchSendRecv_2rank_send_91093_aicpu_unflod)
{
    setenv("HCCL_OP_EXPANSION_MODE", "AI_CPU", 1);
    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;
    s32 ndev = DEV_NUM_8;

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    HcclComm newcomm;
    HcclRootInfo id;
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::ProcessSendDataSlice)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::ProcessRecvDataSlice)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::GetSendTargetLink)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::GetRecvTargetLink)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::RunLoopInHostUnfoldMode)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclCommunicator::IsAtomicInit)
    .stubs()
    .will(returnValue(true));

    HcclCommunicator impl;
    HcclCommParams params;
    string commId = "BatchSendRecv";
    memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
    params.rank = 0;
    params.totalRanks = 1;
    params.isHeterogComm = false;
    params.logicDevId = 0;
    params.deviceType = DevType::DEV_TYPE_910_93;

    RankTable_t rankTable;
    rankTable.collectiveId = "192.168.0.101-8000-8001";
    vector<RankInfo_t> rankVec(1);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";
    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.deviceNum = 1;
    rankTable.serverNum = 1;
    aclrtSetDevice(0);

    ret = impl.Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    unsetenv("HCCL_OP_EXPANSION_MODE");
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_BatchSendRecv_2rank_send_91093_aicpu_bigsize)
{
    setenv("HCCL_OP_EXPANSION_MODE", "AI_CPU", 1);
    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s32 rank = 0;

    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;
    s32 ndev = DEV_NUM_8;

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    HcclComm newcomm;
    HcclRootInfo id;
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&CollBatchSendRecvExecutor::ProcessSendDataSlice)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::ProcessRecvDataSlice)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::GetSendTargetLink)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::GetRecvTargetLink)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::RunLoopInHostUnfoldMode)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclCommunicator::IsAtomicInit)
    .stubs()
    .will(returnValue(true));
    MOCKER(aclrtLaunchKernelWithConfig).stubs().with(any()).will(returnValue(ACL_SUCCESS));

    HcclCommunicator impl;
    HcclCommParams params;
    string commId = "BatchSendRecv";
    memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
    params.rank = 0;
    params.totalRanks = 1;
    params.isHeterogComm = false;
    params.logicDevId = 0;
    params.deviceType = DevType::DEV_TYPE_910_93;

    RankTable_t rankTable;
    rankTable.collectiveId = "192.168.0.101-8000-8001";
    vector<RankInfo_t> rankVec(1);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";
    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.deviceNum = 1;
    rankTable.serverNum = 1;
    aclrtSetDevice(0);

    ret = impl.Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    impl.userRankSize_ = 2;
    impl.InitCCLbuffer(1024, 1024);

    void* inputPtr = nullptr;
    void* outputPtr = nullptr;
    u64 addr = 0U;
    void* tilingDataPtr = nullptr;
    u32 tilingDataSize = 32 * 1024;
    std::string kernelName = "RunAicpuRpcSrvLaunchV2" ;
    tilingDataPtr= (void*)sal_malloc(tilingDataSize * sizeof(s8));
    sal_memset(tilingDataPtr, tilingDataSize * sizeof(s8), 0, tilingDataSize * sizeof(s8));
    ret  = impl.AicpuUnfoldKernelLaunchV2(inputPtr, outputPtr, stream, addr, tilingDataPtr, tilingDataSize, kernelName,
        HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, commId, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sal_free(tilingDataPtr);
    rt_ret = rtStreamDestroy(stream);
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    unsetenv("HCCL_OP_EXPANSION_MODE");
    GlobalMockObject::verify();
}


TEST_P(OpbaseTest, ut_BatchSendRecv_2rank_send_recv)
{

    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
    nlohmann::json rank_table = rank_table_910_1server_2rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;
    s32 ndev = DEV_NUM_8;

    char* rank_table_file = "./ut_opbase_test.json";

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));
    ret = HcclCommInitClusterInfo(rank_table_file, 0, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));\

    MOCKER_CPP(&CollBatchSendRecvExecutor::ProcessSendDataSlice)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::ProcessRecvDataSlice)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::GetSendTargetLink)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::GetRecvTargetLink)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::RunLoopInHostUnfoldMode)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    HcclSendRecvItem data[] = {{HcclSendRecvType::HCCL_RECV, recvbuf, HCCL_COM_DATA_SIZE, HcclDataType::HCCL_DATA_TYPE_INT8, 1},
                                {HcclSendRecvType::HCCL_SEND, sendbuf, HCCL_COM_DATA_SIZE, HcclDataType::HCCL_DATA_TYPE_INT8, 1}};
    HcclSendRecvItem* dataPtr = data;
    ret = HcclBatchSendRecv(dataPtr, 2, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);

    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
    GlobalMockObject::verify();
}
#endif

#if 1
TEST_F(OpbaseTest, ut_BatchSendRecv_4rank_send_multitimes)
{

    nlohmann::json rank_table = rank_table_910_1server_4rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;
    s32 ndev = DEV_NUM_8;
    // 走1910 8pring
    char* rank_table_file = "./ut_opbase_test.json";

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));
    ret = HcclCommInitClusterInfo(rank_table_file, 0, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportManager::IncreAlloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::ProcessSendDataSlice)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::ProcessRecvDataSlice)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::GetSendTargetLink)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::GetRecvTargetLink)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CollBatchSendRecvExecutor::RunLoopInHostUnfoldMode)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    HcclSendRecvItem data[] = {{HcclSendRecvType::HCCL_SEND, sendbuf, HCCL_COM_DATA_SIZE, HcclDataType::HCCL_DATA_TYPE_INT8, 1}};
    HcclSendRecvItem* dataPtr = data;
    ret = HcclBatchSendRecv(dataPtr, 1, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclSendRecvItem data1[] = {{HcclSendRecvType::HCCL_SEND, sendbuf, HCCL_COM_DATA_SIZE, HcclDataType::HCCL_DATA_TYPE_INT8, 2}};
    HcclSendRecvItem* dataPtr1 = data1;
    ret = HcclBatchSendRecv(dataPtr1, 1, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    GlobalMockObject::verify();
}
#endif

#if 1
TEST_F(OpbaseTest, ut_hcclGetRootInfo_91093_single_server_success_normal_3)
{
    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
    setenv("HCCL_IF_IP", "127.0.0.2", 1);

    DevType type91093 = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(type91093))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDeviceInfo)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    HcclRootInfo id;
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_IF_IP");
    GlobalMockObject::verify();
}
#endif

TEST_F(OpbaseTest, ut_HcclAiCpuUnfold310P)
{

    s8* sendBuf;
    s8* recvBuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    HcclRootInfo id;
    char group[ROOTINFO_INDENTIFIER_MAX_LENGTH] = {0};
    void *commContext = nullptr;
    void *aicpuNotify = nullptr;
    rtStream_t stream;

    DevType deviceType = DevType::DEV_TYPE_310P3;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Heartbeat::Init)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::RegisterToHeartBeat, HcclResult(HcclCommunicator::*)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sendBuf = (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendBuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvBuf = (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(recvBuf, count * sizeof(s8), 0, count * sizeof(s8));

    HcclCommunicator impl;
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
    rankTable.serverNum = 2;
    aclrtSetDevice(0);

    ret = impl.Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rtError_t rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    impl.userRankSize_ = 2;
    impl.InitCCLbuffer(1024, 1024);

    impl.AllReduceOutPlace(commId, sendBuf, recvBuf, count, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM, stream, SyncMode::DEFAULT_TIMEWAITSYNCMODE);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rtStreamDestroy(stream);
    sal_free(sendBuf);
    sal_free(recvBuf);
    impl.ReleaseCommCCLbuffer();
    unsetenv("HCCL_OP_EXPANSION_MODE");
    GlobalMockObject::verify();
}

#if 1
TEST_F(OpbaseTest, ut_HcclAllReduceOutPlace310P)
{
    s8* sendBuf;
    s8* recvBuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    HcclRootInfo id;
    char group[ROOTINFO_INDENTIFIER_MAX_LENGTH] = {0};
    void *commContext = nullptr;
    void *aicpuNotify = nullptr;
    rtStream_t stream;

    DevType deviceType = DevType::DEV_TYPE_310P3;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    // MOCKER_CPP(&HcclCommunicator::RegisterToHeartBeat, HcclResult(HcclCommunicator::*)())
    // .stubs()
    // .with(any())
    // .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sendBuf = (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendBuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvBuf = (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(recvBuf, count * sizeof(s8), 0, count * sizeof(s8));

    HcclCommunicator impl;
    impl.AtomicInitSet();
    HcclCommParams params;
    string commId = "AllReduce";
    memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
    params.rank = 0;
    params.totalRanks = 1;
    params.isHeterogComm = false;
    params.logicDevId = 0;
    params.deviceType = DevType::DEV_TYPE_310P3;

    RankTable_t rankTable;
    rankTable.collectiveId = "192.168.0.101-8000-8001";
    vector<RankInfo_t> rankVec(1);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";
    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.deviceNum = 1;
    rankTable.serverNum = 1;
    aclrtSetDevice(0);

    ret = impl.Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rtError_t rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    impl.userRankSize_ = 2;
    impl.InitCCLbuffer(1024, 1024);

    ret = impl.AllReduceOutPlace(commId, sendBuf, recvBuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, stream, SyncMode::DEFAULT_TIMEWAITSYNCMODE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rtStreamSynchronize(stream);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rtStreamDestroy(stream);
    sal_free(sendBuf);
    sal_free(recvBuf);
    impl.ReleaseCommCCLbuffer();
    GlobalMockObject::verify();
}
TEST_F(OpbaseTest, ut_HcclAllGatherOutPlace310P_ranksize_1)
{
    MOCKER_CPP(&NetworkManager::CheckAutoListenVersion).stubs().will(returnValue(HCCL_SUCCESS));
    s8* sendBuf;
    s8* recvBuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    HcclRootInfo id;
    char group[ROOTINFO_INDENTIFIER_MAX_LENGTH] = {0};
    void *commContext = nullptr;
    void *aicpuNotify = nullptr;
    rtStream_t stream;

    DevType deviceType = DevType::DEV_TYPE_310P3;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    // MOCKER_CPP(&HcclCommunicator::RegisterToHeartBeat, HcclResult(HcclCommunicator::*)())
    // .stubs()
    // .with(any())
    // .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sendBuf = (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendBuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvBuf = (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(recvBuf, count * sizeof(s8), 0, count * sizeof(s8));

    HcclCommunicator impl;
    impl.AtomicInitSet();
    HcclCommParams params;
    string commId = "AllGather310p";
    memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
    params.rank = 0;
    params.totalRanks = 1;
    params.isHeterogComm = false;
    params.logicDevId = 0;
    params.deviceType = DevType::DEV_TYPE_310P3;

    RankTable_t rankTable;
    rankTable.collectiveId = "192.168.0.101-8000-8001";
    vector<RankInfo_t> rankVec(1);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";
    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.deviceNum = 1;
    rankTable.serverNum = 1;
    aclrtSetDevice(0);

    ret = impl.Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rtError_t rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    impl.userRankSize_ = 1;

    ret = impl.AllGatherOutPlace(commId, sendBuf, recvBuf, count, HCCL_DATA_TYPE_INT8, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rtStreamSynchronize(stream);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rtStreamDestroy(stream);
    sal_free(sendBuf);
    sal_free(recvBuf);
    impl.ReleaseCommCCLbuffer();
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_HcclAllGatherOutPlace_mix_ranksize_1_capture)
{
    setenv("HCCL_OP_EXPANSION_MODE", "HOST", 1);
    ResetInitState();
    InitExternalInput();
    SetFftsSwitch(false);

    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCallbackTask::CallbackRegStream)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    s32 portNum = -1;
    MOCKER(hrtGetHccsPortNum)
    .stubs()
    .with(any(), outBound(portNum))
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Heartbeat::Init)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::RegisterToHeartBeat, HcclResult(HcclCommunicator::*)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendBuf;
    s8* recvBuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;
    s32 ndev = 8;

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendBuf = (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendBuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvBuf = (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(recvBuf, count * sizeof(s8), 0, count * sizeof(s8));

    HcclComm newcomm;
    HcclRootInfo id;
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclCommunicator::IsAtomicInit)
    .stubs()
    .will(returnValue(true));
    rtStreamCaptureStatus captureStatus = rtStreamCaptureStatus::RT_STREAM_CAPTURE_STATUS_ACTIVE;
    int mockModel = 0;
    void *pmockModel = &mockModel;    
    MOCKER(rtStreamGetCaptureInfo)
    .stubs()
    .with(any(), outBoundP(&captureStatus, sizeof(captureStatus)), outBoundP(&pmockModel, sizeof(pmockModel)))
    .will(returnValue(0));

    HcclCommunicator impl;
    HcclCommParams params;
    string commId = "AllGatherMixOpbase";
    memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
    params.rank = 0;
    params.totalRanks = 1;
    params.isHeterogComm = false;
    params.logicDevId = 0;
    params.deviceType = DevType::DEV_TYPE_910_93;

    RankTable_t rankTable;
    rankTable.collectiveId = "192.168.10.101-8000-8001";
    vector<RankInfo_t> rankVec(1);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1695197376);  // 1,695,197,376
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.10.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";
    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.deviceNum = 1;
    rankTable.serverNum = 1;
    aclrtSetDevice(0);

    ret = impl.Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    impl.userRankSize_ = 1;

    ret = impl.AllGatherOutPlace(commId, sendBuf, recvBuf, count, HCCL_DATA_TYPE_INT8, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sal_free(sendBuf);
    sal_free(recvBuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    GlobalMockObject::verify();

    unsetenv("HCCL_OP_EXPANSION_MODE");
    ResetInitState();
    InitExternalInput();
}
#endif
TEST_F(OpbaseTest, ut_HcclGetTopoDesc)
{
    HcclRootInfo id;
    char group[ROOTINFO_INDENTIFIER_MAX_LENGTH] = {0};

    DevType deviceType = DevType::DEV_TYPE_310P3;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclGetCommName(newcomm, group);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclTopoDescs topoDescs[2];
    u32 topoSize = 2;
    ret = HcclGetTopoDesc(newcomm, topoDescs, topoSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(topoDescs[0].algSets, HCCL_ALG_RING);
    EXPECT_EQ(topoDescs[1].algSets, 0);

    hcclComm *pComm = static_cast<hcclComm *>(newcomm);
    pComm->communicator_->deviceType_ = DevType::DEV_TYPE_910_93;
    ret = HcclGetTopoDesc(newcomm, topoDescs, topoSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(topoDescs[0].algSets, HCCL_ALG_SWITCH | HCCL_ALG_RING);
    EXPECT_EQ(topoDescs[1].algSets, HCCL_ALG_RING);

    pComm->communicator_->deviceType_ = DevType::DEV_TYPE_910B;
    ret = HcclGetTopoDesc(newcomm, topoDescs, topoSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(topoDescs[0].algSets, HCCL_ALG_MESH);
    EXPECT_EQ(topoDescs[1].algSets, 0);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_HcclCommInitRootInfoConfig_default)
{
    HcclRootInfo id;

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclCommConfig config;
    HcclCommConfigInit(&config);

    HcclComm newcomm;
    ret = HcclCommInitRootInfoConfig(1, &id, 0, &config, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcclComm *pComm = static_cast<hcclComm *>(newcomm);
    EXPECT_EQ(pComm->GetConfigInCCLbufferSize(), 200 * 1024 * 1024);
    EXPECT_EQ(pComm->GetConfigOutCCLbufferSize(), 200 * 1024 * 1024);
    EXPECT_EQ(pComm->communicator_->GetDeterministicConfig(), 0);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}

void InitCommonTest(HcclRootInfo* id, HcclCommConfig* config, int aclGraphZeroCopyEnable)
{
    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType)
        .stubs()
        .with(outBound(deviceType))
        .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HcclGetRootInfo(id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclCommConfigInit(config);
    config->hcclBufferSize = 300;
    config->hcclDeterministic = 1;
    config->aclGraphZeroCopyEnable = aclGraphZeroCopyEnable;
    strcpy_s(config->hcclCommName, 128, HCCL_WORLD_GROUP);
}

HcclComm CreateAndVerifyComm(int aclGraphZeroCopyEnable, HcclResult retExpect, int getExpact)
{
    HcclRootInfo id;
    HcclCommConfig config;
    InitCommonTest(&id, &config, aclGraphZeroCopyEnable);

    HcclComm newcomm;
    HcclResult ret = HcclCommInitRootInfoConfig(1, &id, 0, &config, &newcomm);
    EXPECT_EQ(ret, retExpect);

    if (ret == HCCL_SUCCESS) {
        hcclComm* pComm = static_cast<hcclComm*>(newcomm);
        EXPECT_EQ(pComm->communicator_->GetConfigAclGraphZeroCopyEnable(), getExpact);
    }

    return newcomm;
}

TEST_F(OpbaseTest, Ut_HcclCommInitRootInfoConfig_When_aclGraphZeroCopyEnable_Set0_Expect_Get0)
{
    HcclComm newcomm = CreateAndVerifyComm(0, HCCL_SUCCESS, 0);
    HcclResult ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, Ut_HcclCommInitRootInfoConfig_When_aclGraphZeroCopyEnable_Set1_Expect_Get1)
{
    HcclComm newcomm = CreateAndVerifyComm(1, HCCL_SUCCESS, 1);
    HcclResult ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_HcclCommInitRootInfoConfig_user_config)
{
    HcclRootInfo id;

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclCommConfig config;
    HcclCommConfigInit(&config);

    config.hcclBufferSize = 300;
    config.hcclDeterministic = 1;

    HcclComm newcomm;
    ret = HcclCommInitRootInfoConfig(1, &id, 0, &config, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcclComm *pComm = static_cast<hcclComm *>(newcomm);
    EXPECT_EQ(pComm->GetConfigInCCLbufferSize(), 300 * 1024 * 1024);
    EXPECT_EQ(pComm->GetConfigOutCCLbufferSize(), 300 * 1024 * 1024);
    EXPECT_EQ(pComm->communicator_->GetDeterministicConfig(), 1);
    EXPECT_EQ(pComm->communicator_->SetDeterministicConfig(config.hcclDeterministic), HCCL_SUCCESS);
    EXPECT_EQ(pComm->communicator_->SetExecTimeOutConfig(1000), HCCL_SUCCESS);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_HcclCommInitRootInfoConfig_user_config_world_group)
{
    HcclRootInfo id;

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclCommConfig config;
    HcclCommConfigInit(&config);

    config.hcclBufferSize = 300;
    config.hcclDeterministic = 1;

    strcpy_s(config.hcclCommName, 128, HCCL_WORLD_GROUP);

    HcclComm newcomm;
    ret = HcclCommInitRootInfoConfig(1, &id, 0, &config, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcclComm *pComm = static_cast<hcclComm *>(newcomm);
    EXPECT_EQ(pComm->GetConfigInCCLbufferSize(), 300 * 1024 * 1024);
    EXPECT_EQ(pComm->GetConfigOutCCLbufferSize(), 300 * 1024 * 1024);
    EXPECT_EQ(pComm->communicator_->GetDeterministicConfig(), 1); 
    EXPECT_EQ(pComm->GetIdentifier(), HCCL_WORLD_GROUP);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_HcclCommInitRootInfoConfig_user_config_configer)
{
    HcclRootInfo id;

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclCommConfig config;
    HcclCommConfigInit(&config);

    config.hcclBufferSize = 300;
    config.hcclDeterministic = 1;
    config.hcclOpExpansionMode = 3;
    config.hcclExecTimeOut = 2000;
    strcpy_s(config.hcclCommName, COMM_NAME_MAX_LENGTH, "comm1");
    strcpy_s(config.hcclAlgo, HCCL_COMM_ALGO_MAX_LENGTH, "level0:NA;level1:ring");
    strcpy_s(config.hcclRetryEnable, HCCL_COMM_RETRY_ENABLE_MAX_LENGTH, "L1:1, L2:1");
    strcpy_s(config.hcclRetryParams, HCCL_COMM_RETRY_PARAMS_MAX_LENGTH, "MaxCnt:5, HoldTime:5000, IntervalTime:5000");

    HcclComm newcomm;
    ret = HcclCommInitRootInfoConfig(1, &id, 0, &config, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcclComm *pComm = static_cast<hcclComm *>(newcomm);
    EXPECT_EQ(pComm->GetConfigInCCLbufferSize(), 300 * 1024 * 1024);
    EXPECT_EQ(pComm->GetConfigOutCCLbufferSize(), 300 * 1024 * 1024);
    EXPECT_EQ(pComm->communicator_->GetDeterministicConfig(), 1);
    EXPECT_EQ(pComm->GetIdentifier(), "comm1");
    EXPECT_EQ(pComm->communicator_->SetDeterministicConfig(config.hcclDeterministic), HCCL_SUCCESS);

    EXPECT_EQ(pComm->communicator_->commConfig_.GetConfigCommName(), "comm1");
    EXPECT_EQ(pComm->communicator_->commConfig_.GetConfigExecTimeOut(), 2000);

    s32 execTimeOut = CommConfiger::GetInstance().GetCommConfigExecTimeOut("comm1");
    EXPECT_EQ(execTimeOut, 2000);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigAlgoConfig("comm1")[0], HcclAlgoType::HCCL_ALGO_TYPE_NA);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigAlgoConfig("comm1")[1], HcclAlgoType::HCCL_ALGO_TYPE_RING);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigAlgoConfig("comm1")[2], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigAlgoConfig("comm1")[3], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigInterServerRetryEnable("comm1"), 1);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigInterSuperPodRetryEnable("comm1"), 1);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigRetryMaxCnt("comm1"), 5);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigRetryHoldTime("comm1"), 5000);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigRetryIntervalTime("comm1"), 5000);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_HcclCommInitRootInfoConfig_user_config_world_group_configer)
{
    HcclRootInfo id;

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclCommConfig config;
    HcclCommConfigInit(&config);

    config.hcclBufferSize = 300;
    config.hcclDeterministic = 1;
    config.hcclOpExpansionMode = 3;
    config.hcclExecTimeOut = 2000;
    strcpy_s(config.hcclCommName, COMM_NAME_MAX_LENGTH, "comm1");
    strcpy_s(config.hcclAlgo, HCCL_COMM_ALGO_MAX_LENGTH, "level0:NA;level1:ring");
    strcpy_s(config.hcclRetryEnable, HCCL_COMM_RETRY_ENABLE_MAX_LENGTH, "L0:1, L1:1, L2:1");
    strcpy_s(config.hcclRetryParams, HCCL_COMM_RETRY_PARAMS_MAX_LENGTH, "MaxCnt:5, HoldTime:5000, IntervalTime:5000");

    strcpy_s(config.hcclCommName, 128, HCCL_WORLD_GROUP);

    HcclComm newcomm;
    ret = HcclCommInitRootInfoConfig(1, &id, 0, &config, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcclComm *pComm = static_cast<hcclComm *>(newcomm);
    EXPECT_EQ(pComm->GetConfigInCCLbufferSize(), 300 * 1024 * 1024);
    EXPECT_EQ(pComm->GetConfigOutCCLbufferSize(), 300 * 1024 * 1024);
    EXPECT_EQ(pComm->communicator_->GetDeterministicConfig(), 1);
    EXPECT_EQ(pComm->GetIdentifier(), HCCL_WORLD_GROUP);

    EXPECT_EQ(pComm->communicator_->commConfig_.GetConfigCommName(), HCCL_WORLD_GROUP);
    EXPECT_EQ(pComm->communicator_->commConfig_.GetConfigExecTimeOut(), 2000);

    s32 execTimeOut = CommConfiger::GetInstance().GetCommConfigExecTimeOut(HCCL_WORLD_GROUP);
    EXPECT_EQ(execTimeOut, 2000);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigAlgoConfig(HCCL_WORLD_GROUP)[0], HcclAlgoType::HCCL_ALGO_TYPE_NA);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigAlgoConfig(HCCL_WORLD_GROUP)[1], HcclAlgoType::HCCL_ALGO_TYPE_RING);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigAlgoConfig(HCCL_WORLD_GROUP)[2], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigAlgoConfig(HCCL_WORLD_GROUP)[3], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigInterServerRetryEnable(HCCL_WORLD_GROUP), 1);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigInterSuperPodRetryEnable(HCCL_WORLD_GROUP), 1);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigRetryMaxCnt(HCCL_WORLD_GROUP), 5);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigRetryHoldTime(HCCL_WORLD_GROUP), 5000);
    EXPECT_EQ(CommConfiger::GetInstance().GetCommConfigRetryIntervalTime(HCCL_WORLD_GROUP), 5000);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_hccl_comm_suspend_test)
{
 
    hcclComm test;
    HcclComm newcomm = &test;
    MOCKER_CPP(&hcclComm::Suspend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    auto ret = HcclCommSuspend(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
 
TEST_F(OpbaseTest, ut_hccl_comm_resume_test)
{
    hcclComm test;
    HcclComm newcomm = &test;
    MOCKER_CPP(&hcclComm::Resume)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    auto ret = HcclCommResume(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}



TEST_P(OpbaseTest, ut_CollScatterSingleRankExecutor)
{
    /*  
     * (1)cclbuffer 适配后的测试用例:  
     * (1)用例目标:
     *    coll_scatter_single_rank_executor.cc
     * (1)功能:
     *    能够进行单卡 Scatter 算子的正常数据传送测试
    */
    bool fftsSwitch = GetParam();
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);
    if (outfile.is_open()) {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    } else {
        HCCL_ERROR("open %s failed", file_name_t);
    }
    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = 1024;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;

    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf = (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8)); ;

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
        recvbuf[j] = 0;
    }

    ret = HcclScatter(sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, 0, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            errors ++;
            break;
        }
    }
    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
    if (fftsSwitch) {
        SetFftsSwitch(true);
    }
}

TEST_P(OpbaseTest, ut_CollAlltoAllSingleRankExecutor_ALLTOALL)
{
    /*  
     * (2)cclbuffer 适配后的测试用例:  
     * (2)用例目标:
     *    coll_all_to_all_single_rank_executor.cc
     * (2)功能:
     *    能够进行单卡 alltoall 算子的正常数据传送测试
    */
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
    bool fftsSwitch = GetParam();

    if (fftsSwitch) {
        SetFftsSwitch(true);
    }else{
        SetFftsSwitch(false);
    }

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = 1024;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    char* rank_table_file = "./st_opbase_test.json";
    u32 rank_ID = 0;
    string tmpOptions = "";
    HcomSetProfilingMode(HcomProfilingMode::PROFILING_OPEN, tmpOptions);
    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
        recvbuf[j] = 0;
    }
    unsigned int rankSize = 0;
    ret = HcclGetRankSize(comm, &rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("HCCL TEST get rank size[%u] success.", rankSize);

    unsigned int rankId = 0;
    ret = HcclGetRankId(comm, &rankId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("HCCL TEST get rank id[%u] success.", rankId);
    ret = HcclAlltoAll(sendbuf, count, HCCL_DATA_TYPE_INT8,
                       recvbuf, count, HCCL_DATA_TYPE_INT8,
                       comm, stream);


    EXPECT_EQ(ret, HCCL_SUCCESS);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    HCCL_ERROR("count %d",count);
    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            HCCL_ERROR("j %d",j);
            errors ++;
            break;
        }
    }
    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
    SetFftsSwitch(true);
}

TEST_P(OpbaseTest, ut_CollAlltoAllSingleRankExecutor_ALLTOALLVC)
{
    /*  
     * (3)cclbuffer 适配后的测试用例:  
     * (3)用例目标:
     *    coll_all_to_all_single_rank_executor.cc
     * (3)功能:
     *    能够进行单卡 alltoallVC 算子的正常数据传送测试
    */
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
    bool fftsSwitch = GetParam();

    if (fftsSwitch) {
        SetFftsSwitch(true);
    }else{
        SetFftsSwitch(false);
    }

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = 1024;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;

    char* rank_table_file = "./st_opbase_test.json";
    u32 rank_ID = 0;
    string tmpOptions = "";
    HcomSetProfilingMode(HcomProfilingMode::PROFILING_OPEN, tmpOptions);
    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    u64 sendCountMatrix[1] = {1024};  
    
    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
        recvbuf[j] = 0;
    }
    unsigned int rankSize = 0;
    ret = HcclGetRankSize(comm, &rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("HCCL TEST get rank size[%u] success.", rankSize);

    unsigned int rankId = 0;
    ret = HcclGetRankId(comm, &rankId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("HCCL TEST get rank id[%u] success.", rankId);
    ret = HcclAlltoAllVC(sendbuf, static_cast<void *>(sendCountMatrix), HCCL_DATA_TYPE_INT8,
                       recvbuf, HCCL_DATA_TYPE_INT8,
                       comm, stream);


    EXPECT_EQ(ret, HCCL_SUCCESS);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    HCCL_ERROR("count %d",count);
    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            HCCL_ERROR("j %d",j);
            errors ++;
            break;
        }
    }
    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);

    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
    SetFftsSwitch(true);
}

TEST_P(OpbaseTest, ut_CollAlltoAllSingleRankExecutor_ALLTOALLV)
{
    /*  
     * (4)cclbuffer 适配后的测试用例:  
     * (4)用例目标:
     *    coll_all_to_all_single_rank_executor.cc
     * (4)功能:
     *    能够进行单卡 alltoallV 算子的正常数据传送测试
    */
    bool fftsSwitch = GetParam();

    if (fftsSwitch) {
        SetFftsSwitch(true);
    }else{
        SetFftsSwitch(false);
    }


    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }
    outfile.close();
    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void* comm;
    char* rank_table_file = "./ut_opbase_test.json";
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    u32 rank_ID = 0;
    s32 count = 1024;
    
    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));
    u64 sendCounts[1] = {1024};
    u64 recvCounts[1] = {1024};
    u64 sdispls[1] = {0};
    u64 rdispls[1] = {0};
    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
        recvbuf[j] = 0;
    }
    ret = HcclAlltoAllV(sendbuf, sendCounts, sdispls, HCCL_DATA_TYPE_INT8, recvbuf, recvCounts, rdispls,
                        HCCL_DATA_TYPE_INT8, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    int errors = 0 ;
    for (int j = 0; j < 128; j++)
    {
        if (recvbuf[j] != 2)
        {
            HCCL_ERROR("j %d",recvbuf[j]);
            errors ++;
            break;
        }
    }
    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = rtStreamDestroy(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
    SetFftsSwitch(true);
}

TEST_F(OpbaseTest, ut_HcclGetCommConfigCapability)
{
    uint32_t capability = HcclGetCommConfigCapability();
    EXPECT_EQ(capability, HCCL_COMM_CONFIG_RESERVED);
    EXPECT_EQ(capability > HCCL_COMM_CONFIG_BUFFER_SIZE, 1);
    EXPECT_EQ(capability > HCCL_COMM_CONFIG_DETERMINISTIC, 1);
    EXPECT_EQ(capability > HCCL_COMM_CONFIG_COMM_NAME, 1);
}

TEST_F(OpbaseTest, ut_SetDynamicTilingDataAlltoall)
{
    HcclCommunicator impl;
    OpParam opParam;
    opParam.All2AllDataDes.sendType = HcclDataType::HCCL_DATA_TYPE_FP32;
    opParam.All2AllDataDes.recvType = HcclDataType::HCCL_DATA_TYPE_FP32;
    opParam.All2AllDataDes.sendCount = 16;
    HostMem dynamicDataMem = HostMem::alloc(sizeof(struct OpTilingAllToAllDataDes));
    HcclResult ret = impl.SetDynamicTilingDataAlltoall(opParam, dynamicDataMem);
    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(OpbaseTest, ut_FreeScratchMemOnOpBaseMode)
{
    HcclCommunicator impl;
    OpParam opParam;
    opParam.aicpuUnfoldMode = true;
    HcclCMDType opType = HcclCMDType::HCCL_CMD_ALLTOALL;
    DeviceMem scratchMem;
    HcclResult ret = impl.FreeScratchMemOnOpBaseMode(scratchMem, opParam, opType);
    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(OpbaseTest, ut_CheckDataTypeAllBranch)
{
    MOCKER(Is310P3Common)
    .stubs()
    .with(any())
    .will(returnValue(true));

    HcclCommunicator impl;
    HcclResult ret = impl.CheckDataType(HcclDataType::HCCL_DATA_TYPE_RESERVED, false);
    EXPECT_EQ(HCCL_E_NOT_SUPPORT, ret);

    ret = impl.CheckDataType(HcclDataType::HCCL_DATA_TYPE_INT64, true);
    EXPECT_EQ(HCCL_E_NOT_SUPPORT, ret);

    ret = impl.CheckDataType(HcclDataType::HCCL_DATA_TYPE_UINT64, true);
    EXPECT_EQ(HCCL_E_NOT_SUPPORT, ret);

    GlobalMockObject::verify();
}
TEST_F(OpbaseTest, ut_310P3CommonFrontLogPrint)
{
    MOCKER(Is310P3Common)
    .stubs()
    .with(any())
    .will(returnValue(true));
    MOCKER_CPP(&HcclCommunicator::IsAtomicInit)
    .stubs()
    .with(any())
    .will(returnValue(true));

    HcclCommunicator impl;
    void *sendBuf = nullptr;
    void *recvBuf = nullptr;
    void *sendCounts = nullptr;
    void *recvCounts = nullptr;
    void *sdispls = nullptr;
    void *rdispls = nullptr;
    rtStream_t stream;
    rtStreamCreate(&stream, 0);
    void *comm = nullptr;
    HcclResult ret = HCCL_SUCCESS;
    ret = impl.AlltoAllVC(sendBuf, sendCounts, HcclDataType::HCCL_DATA_TYPE_RESERVED,
        recvBuf, HcclDataType::HCCL_DATA_TYPE_RESERVED, stream, "alltoallvc");
    EXPECT_EQ(HCCL_E_NOT_SUPPORT, ret);
    ret = impl.Scatter("scatter", sendBuf, recvBuf, 0, HcclDataType::HCCL_DATA_TYPE_RESERVED, 0, stream);
    EXPECT_EQ(HCCL_E_NOT_SUPPORT, ret);
    ret = impl.ScatterOutPlace("ScatterOutPlace", sendBuf, recvBuf, 0,
            HcclDataType::HCCL_DATA_TYPE_RESERVED, 0, stream);
    EXPECT_EQ(HCCL_E_NOT_SUPPORT, ret);
    ret = impl.Reduce("Reduce", sendBuf, recvBuf, 0, HcclDataType::HCCL_DATA_TYPE_RESERVED,
            HcclReduceOp::HCCL_REDUCE_RESERVED, 0, stream);
    EXPECT_EQ(HCCL_E_NOT_SUPPORT, ret);
    ret = impl.SendOutPlace("SendOutPlace", sendBuf, 0, HcclDataType::HCCL_DATA_TYPE_RESERVED, 0, stream);
    EXPECT_EQ(HCCL_E_NOT_SUPPORT, ret);
    ret = impl.ReceiveOutPlace("ReceiveOutPlace", recvBuf, 0, HcclDataType::HCCL_DATA_TYPE_RESERVED, 1, stream);
    EXPECT_EQ(HCCL_E_NOT_SUPPORT, ret);
    u64 *sendCounts1 = nullptr;
    u64 *recvCounts1 = nullptr;
    u64 *sdispls1 = nullptr;
    u64 *rdispls1 = nullptr;
    u64 memSize = 0;
    ret = impl.GetAlltoAllStagedWorkSpaceMemSize(sendCounts1, sdispls1, HcclDataType::HCCL_DATA_TYPE_RESERVED,
            recvCounts1, rdispls1, HcclDataType::HCCL_DATA_TYPE_RESERVED, memSize);
    EXPECT_EQ(HCCL_E_NOT_SUPPORT, ret);
    rtError_t rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamDestroy(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_HcclCommInitClusterInfoMemConfig_Multiple_Comm)
{
    typedef HcclResult (*HcclOneSideServiceCallBack)(std::unique_ptr<hccl::IHcclOneSidedService> &,
    std::unique_ptr<hccl::HcclSocketManager> &, std::unique_ptr<hccl::NotifyPool> &);

    nlohmann::json rank_table_one = rank_table_910_1server_2rank;
    nlohmann::json rank_table_two = rank_table_910_1server_4rank;
    std::string clusterString_one = rank_table_one.dump();
    std::string clusterString_two = rank_table_two.dump();
 
    int ret = HCCL_SUCCESS;
    u32 rank_ID = 0;
    void* commOne;
    void* commTwo;
    
    HcclCommConfig commConfigOne;
    HcclCommConfigInit(&commConfigOne);
    commConfigOne.hcclBufferSize=800;
    strcpy_s(commConfigOne.hcclCommName, COMM_NAME_MAX_LENGTH, "comm1");

    HcclCommConfig commConfigTwo;
    HcclCommConfigInit(&commConfigTwo);
    commConfigTwo.hcclBufferSize=800;
    strcpy_s(commConfigTwo.hcclCommName, COMM_NAME_MAX_LENGTH, "comm2");

    unsetenv("HCCL_INTRA_PCIE_ENABLE");
    setenv("HCCL_INTRA_ROCE_ENABLE", "1", 1);
    ret = HcclCommInitClusterInfoMemConfig(const_cast<char*>(clusterString_one.c_str()), rank_ID, &commConfigOne, &commOne);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclCommInitClusterInfoMemConfig(const_cast<char*>(clusterString_two.c_str()), rank_ID, &commConfigTwo, &commTwo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hccl::hcclComm* hcclCommOne = static_cast<hccl::hcclComm *>(commOne);
    hccl::hcclComm* hcclCommTwo = static_cast<hccl::hcclComm *>(commTwo);
    IHcclOneSidedService *iServiceOne = nullptr;
    ret = hcclCommOne->GetOneSidedService(&iServiceOne);
    IHcclOneSidedService *iServiceTwo = nullptr;
    ret = hcclCommTwo->GetOneSidedService(&iServiceTwo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = HcclCommDestroy(commOne);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclCommDestroy(commTwo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_INTRA_ROCE_ENABLE");
}

TEST_F(OpbaseTest, ut_HcclCommInitClusterInfoMemConfig_multiple_comm_configer)
{
    typedef HcclResult (*HcclOneSideServiceCallBack)(std::unique_ptr<hccl::IHcclOneSidedService> &,
    std::unique_ptr<hccl::HcclSocketManager> &, std::unique_ptr<hccl::NotifyPool> &);

    nlohmann::json rank_table_one = rank_table_910_1server_2rank;
    nlohmann::json rank_table_two = rank_table_910_1server_4rank;
    std::string clusterString_one = rank_table_one.dump();
    std::string clusterString_two = rank_table_two.dump();
 
    int ret = HCCL_SUCCESS;
    u32 rank_ID = 0;
    void* commOne;
    void* commTwo;
    
    HcclCommConfig commConfigOne;
    HcclCommConfigInit(&commConfigOne);
    commConfigOne.hcclBufferSize=800;
    strcpy_s(commConfigOne.hcclCommName, COMM_NAME_MAX_LENGTH, "comm1");
    commConfigOne.hcclExecTimeOut = 2000;

    HcclCommConfig commConfigTwo;
    HcclCommConfigInit(&commConfigTwo);
    commConfigTwo.hcclBufferSize=800;
    strcpy_s(commConfigTwo.hcclCommName, COMM_NAME_MAX_LENGTH, "comm2");
    commConfigTwo.hcclExecTimeOut = 1000;

    unsetenv("HCCL_INTRA_PCIE_ENABLE");
    setenv("HCCL_INTRA_ROCE_ENABLE", "1", 1);
    ret = HcclCommInitClusterInfoMemConfig(const_cast<char*>(clusterString_one.c_str()), rank_ID, &commConfigOne, &commOne);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclCommInitClusterInfoMemConfig(const_cast<char*>(clusterString_two.c_str()), rank_ID, &commConfigTwo, &commTwo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hccl::hcclComm* hcclCommOne = static_cast<hccl::hcclComm *>(commOne);
    hccl::hcclComm* hcclCommTwo = static_cast<hccl::hcclComm *>(commTwo);
    IHcclOneSidedService *iServiceOne = nullptr;
    ret = hcclCommOne->GetOneSidedService(&iServiceOne);
    IHcclOneSidedService *iServiceTwo = nullptr;
    ret = hcclCommTwo->GetOneSidedService(&iServiceTwo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    s32 execTimeOut1 = CommConfiger::GetInstance().GetCommConfigExecTimeOut("comm1");
    EXPECT_EQ(execTimeOut1, 1972);

    s32 execTimeOut2 = CommConfiger::GetInstance().GetCommConfigExecTimeOut("comm2");
    EXPECT_EQ(execTimeOut2, 952);
 
    ret = HcclCommDestroy(commOne);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclCommDestroy(commTwo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_INTRA_ROCE_ENABLE");
}

TEST_F(OpbaseTest, ut_HcclCommInitClusterInfoMemConfig_Single_Comm)
{
    typedef HcclResult (*HcclOneSideServiceCallBack)(std::unique_ptr<hccl::IHcclOneSidedService> &,
    std::unique_ptr<hccl::HcclSocketManager> &, std::unique_ptr<hccl::NotifyPool> &);

    nlohmann::json rank_table = rank_table_910_1server_2rank;
    std::string clusterString = rank_table.dump();
 
    int ret = HCCL_SUCCESS;
    void* comm;
    u32 rank_ID = 0;

    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    commConfig.hcclBufferSize=800;
    strcpy_s(commConfig.hcclCommName, COMM_NAME_MAX_LENGTH, "comm1");

    unsetenv("HCCL_INTRA_PCIE_ENABLE");
    setenv("HCCL_INTRA_ROCE_ENABLE", "1", 1);
    ret = HcclCommInitClusterInfoMemConfig(const_cast<char*>(clusterString.c_str()), rank_ID, &commConfig, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    IHcclOneSidedService *iService = nullptr;
    ret = hcclComm->GetOneSidedService(&iService);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_INTRA_ROCE_ENABLE");
}

TEST_F(OpbaseTest, ut_HcclCommInitClusterInfoMemConfig_single_comm_configer)
{
    typedef HcclResult (*HcclOneSideServiceCallBack)(std::unique_ptr<hccl::IHcclOneSidedService> &,
    std::unique_ptr<hccl::HcclSocketManager> &, std::unique_ptr<hccl::NotifyPool> &);

    nlohmann::json rank_table = rank_table_910_1server_2rank;
    std::string clusterString = rank_table.dump();
 
    int ret = HCCL_SUCCESS;
    void* comm;
    u32 rank_ID = 0;

    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    commConfig.hcclBufferSize=800;
    commConfig.hcclExecTimeOut = 2000;
    strcpy_s(commConfig.hcclCommName, COMM_NAME_MAX_LENGTH, "comm1");

    unsetenv("HCCL_INTRA_PCIE_ENABLE");
    setenv("HCCL_INTRA_ROCE_ENABLE", "1", 1);
    ret = HcclCommInitClusterInfoMemConfig(const_cast<char*>(clusterString.c_str()), rank_ID, &commConfig, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    IHcclOneSidedService *iService = nullptr;
    ret = hcclComm->GetOneSidedService(&iService);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    s32 execTimeOut = CommConfiger::GetInstance().GetCommConfigExecTimeOut("comm1");
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(execTimeOut, 1972);
 
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_INTRA_ROCE_ENABLE");
}

TEST_F(OpbaseTest, ut_HcclCommInitClusterInfoMemConfig_No_Commname)
{
    nlohmann::json rank_table = rank_table_910_1server_2rank;
    std::string clusterString = rank_table.dump();
 
    int ret = HCCL_SUCCESS;
    void* comm;
    u32 rank_ID = 0;

    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    commConfig.hcclBufferSize=800;

    unsetenv("HCCL_INTRA_PCIE_ENABLE");
    setenv("HCCL_INTRA_ROCE_ENABLE", "1", 1);
    ret = HcclCommInitClusterInfoMemConfig(const_cast<char*>(clusterString.c_str()), rank_ID, &commConfig, &comm);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(OpbaseTest, ut_HcclGetRootInfo_setdevice_fail)
{
    MOCKER(hrtSetDevice)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_INTERNAL));

    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
    setenv("HCCL_IF_IP", "127.0.0.1", 1);

    HcclRootInfo id;
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_IF_IP");

    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_hcclGetRootInfo_resetdevice_fail)
{
    MOCKER(hrtResetDevice)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_PARA));

    MOCKER_CPP(&TopoInfoExchangeServer::Setup)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
    setenv("HCCL_IF_IP", "127.0.0.1", 1);

    HcclRootInfo id;
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_IF_IP");

    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_hcclGetRootInfo_setup_fail)
{
    MOCKER_CPP(&TopoInfoExchangeServer::Setup)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_INTERNAL));

    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
    setenv("HCCL_IF_IP", "127.0.0.1", 1);

    HcclRootInfo id;
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_IF_IP");

    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_failedConnectionAgentIdString_RankSizeIsZero)
{
    u32 rankSize = 0;
    std::string failedAgentIdList = "initial";

    HcclIpAddress hostIP("127.0.0.1");
    u32 hostPort = 61111;
    std::vector<HcclIpAddress> whitelist ;
    HcclNetDevCtx netDevCtx;
    std::shared_ptr<HcclSocket> listenSocket(new (std::nothrow)HcclSocket("test", nullptr,
        hostIP, 0, HcclSocketRole::SOCKET_ROLE_SERVER));

    const std::string identifier = "releaseSocket";
    TopoInfoExchangeServer topoExServer(hostIP, hostPort, whitelist, netDevCtx, listenSocket, identifier);
    HcclResult result = topoExServer.FailedConnectionAgentIdString(rankSize, failedAgentIdList);

    EXPECT_EQ(result, HCCL_E_NOT_FOUND);
    EXPECT_EQ(failedAgentIdList, "initial");
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_failedConnectionAgentIdString_WhenAllRanksExist)
{
    u32 rankSize = 3;

    HcclIpAddress hostIP("127.0.0.1");
    u32 hostPort = 61111;
    std::vector<HcclIpAddress> whitelist ;
    HcclNetDevCtx netDevCtx;
    std::shared_ptr<HcclSocket> listenSocket(new (std::nothrow)HcclSocket("test", nullptr,
        hostIP, 0, HcclSocketRole::SOCKET_ROLE_SERVER));

    const std::string identifier = "releaseSocket";
    TopoInfoExchangeServer topoExServer(hostIP, hostPort, whitelist, netDevCtx, listenSocket, identifier);

    topoExServer.connectSocketsWithRankID_[0] = nullptr;
    topoExServer.connectSocketsWithRankID_[1] = nullptr;
    topoExServer.connectSocketsWithRankID_[2] = nullptr;
    std::string failedAgentIdList = "initial";

    HcclResult result = topoExServer.FailedConnectionAgentIdString(rankSize, failedAgentIdList);

    EXPECT_EQ(result, HCCL_E_NOT_FOUND);
    EXPECT_EQ(failedAgentIdList, "initial");
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_failedConnectionAgentIdString_WhenSomeRanksDoNotExist)
{
    u32 rankSize = 5;

    HcclIpAddress hostIP("127.0.0.1");
    u32 hostPort = 61111;
    std::vector<HcclIpAddress> whitelist ;
    HcclNetDevCtx netDevCtx;
    std::shared_ptr<HcclSocket> listenSocket(new (std::nothrow)HcclSocket("test", nullptr,
        hostIP, 0, HcclSocketRole::SOCKET_ROLE_SERVER));

    const std::string identifier = "releaseSocket";
    TopoInfoExchangeServer topoExServer(hostIP, hostPort, whitelist, netDevCtx, listenSocket, identifier);
    topoExServer.connectSocketsWithRankID_[1] = nullptr;
    topoExServer.connectSocketsWithRankID_[3] = nullptr;
    std::string failedAgentIdList = "";

    HcclResult result = topoExServer.FailedConnectionAgentIdString(rankSize, failedAgentIdList);

    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_EQ(failedAgentIdList, "0,2,4,");
    GlobalMockObject::verify();
}


TEST_F(OpbaseTest, ut_failedConnectionAgentIdString_WhenRankIdExceedsRange)
{
    u32 rankSize = 3;

    HcclIpAddress hostIP("127.0.0.1");
    u32 hostPort = 61111;
    std::vector<HcclIpAddress> whitelist ;
    HcclNetDevCtx netDevCtx;
    std::shared_ptr<HcclSocket> listenSocket(new (std::nothrow)HcclSocket("test", nullptr,
        hostIP, 0, HcclSocketRole::SOCKET_ROLE_SERVER));

    const std::string identifier = "releaseSocket";
    TopoInfoExchangeServer topoExServer(hostIP, hostPort, whitelist, netDevCtx, listenSocket, identifier);
    topoExServer.connectSocketsWithRankID_[3] = nullptr;
    std::string failedAgentIdList = "";

    HCCL_INFO("rankSize[%d], topoExServer.connectSocketsWithRankID_.size[%u]", rankSize, topoExServer.connectSocketsWithRankID_.size());
    HcclResult result = topoExServer.FailedConnectionAgentIdString(rankSize, failedAgentIdList);

    EXPECT_EQ(result, HCCL_E_INTERNAL);
    EXPECT_EQ(failedAgentIdList, "");
    GlobalMockObject::verify();
}

TEST_F(OpbaseUT, ut_topoInfoExchangeAgent_Setup_Fail)
{
    HcclIpAddress localIp(1694542016);
    HcclNetDevCtx netDevCtx;
    HcclBasicRankInfo localRankInfo;
    localRankInfo.deviceType = DevType::DEV_TYPE_910_93;
    u32 serverPort = 60000;
    string identifier = "test";
    TopoInfoExchangeAgent agent(localIp, serverPort, identifier, netDevCtx, localRankInfo);
    MOCKER_CPP(&TopoInfoExchangeAgent::Connect)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TopoInfoExchangeAgent::DetectClusterTopoInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    agent.isByMasterInfo_ = true;
    MOCKER_CPP(&TopoInfoExchangeAgent::VerifyClusterInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_INTERNAL));
    
    g_broadcastStage.store(BroadcastStage::Completed);
    EXPECT_EQ(agent.Setup(), HCCL_E_INTERNAL);
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_topoInfoExchangeServer_timeout)
{
    setenv("HCCL_CONNECT_TIMEOUT", "5", 1);
    MOCKER(GetExternalInputHcclLinkTimeOut)
    .stubs()
    .will(returnValue(5));
    MOCKER_CPP(&HcclSocket::Accept)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_TIMEOUT));
    MOCKER_CPP(&TopoInfoExchangeServer::DisplayConnectionedRank)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress hostIP("127.0.0.1");
    u32 hostPort = 61111;
    std::vector<HcclIpAddress> whitelist ;
    HcclNetDevCtx netDevCtx;
    std::shared_ptr<HcclSocket> listenSocket;
    const std::string identifier = "TOPPINFOEXCHANGE_TEST";
    TopoInfoExchangeServer topoExServer(hostIP, hostPort, whitelist, netDevCtx, listenSocket, identifier);
    std::map<std::string, std::shared_ptr<HcclSocket>> connectSockets;
    u32 rankSize = 0;
    topoExServer.Connect(connectSockets, rankSize);

    std::shared_ptr<HcclSocket> socket;
    connectSockets.insert({"0001", socket});
    topoExServer.DisplayConnectingStatus(4, 3, connectSockets);

    unsetenv("HCCL_CONNECT_TIMEOUT");
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_topoInfoExchangeServer_ErrTCP)
{
    setenv("HCCL_CONNECT_TIMEOUT", "5", 1);
    MOCKER(GetExternalInputHcclLinkTimeOut)
    .stubs()
    .will(returnValue(5));
    MOCKER_CPP(&HcclSocket::Accept)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_TCP_CONNECT));
    MOCKER_CPP(&TopoInfoExchangeServer::DisplayConnectionedRank)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress hostIP("127.0.0.1");
    u32 hostPort = 61111;
    std::vector<HcclIpAddress> whitelist ;
    HcclNetDevCtx netDevCtx;
    std::shared_ptr<HcclSocket> listenSocket;
    const std::string identifier = "TOPPINFOEXCHANGE_TEST";
    TopoInfoExchangeServer topoExServer(hostIP, hostPort, whitelist, netDevCtx, listenSocket, identifier);
    std::map<std::string, std::shared_ptr<HcclSocket>> connectSockets;
    u32 rankSize = 0;
    topoExServer.Connect(connectSockets, rankSize);

    unsetenv("HCCL_CONNECT_TIMEOUT");
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_hcomAllGatherV)
{
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
 
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&HcclCommunicator::ExecOp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
 
    nlohmann::json rank_table = rank_table_910_1server_2rank;
 
    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);
 
    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }
 
    outfile.close();
 
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    u64* recvCounts;
    u64* recvDispls;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    u32 rankSize = 4;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;
 
    // 走1910 4pring
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;
    setenv("HCCL_DEBUG_CONFIG", "alg", 1);
    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(rankSize * count * sizeof(s8));
     sal_memset(recvbuf, rankSize * count * sizeof(s8), 0, rankSize * count * sizeof(s8));
 
    recvCounts= (u64*)sal_malloc(rankSize * sizeof(u64));
     sal_memset(recvCounts, rankSize * sizeof(u64), 0, rankSize * sizeof(u64));
    recvDispls= (u64*)sal_malloc(rankSize * sizeof(u64));
     sal_memset(recvDispls, rankSize * sizeof(u64), 0, rankSize * sizeof(u64));
 
    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
 
    for (int i = 0; i < rankSize; i++)
    {
        recvCounts[i] = count;
        if (i > 0) {
            recvDispls[i] = recvDispls[i-1] + recvCounts[i-1];
        }
    }
 
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    string strTag = "allgatherv";
    ret = hcclComm->AllGatherV(strTag, static_cast<void *>(sendbuf), 2, static_cast<void *>(recvbuf), static_cast<void *>(recvCounts), static_cast<void *>(recvDispls), HCCL_DATA_TYPE_INT8, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    sal_free(sendbuf);
    sal_free(recvbuf);
    sal_free(recvCounts);
    sal_free(recvDispls);
    rt_ret = rtStreamDestroy(stream);

    unsetenv("HCCL_DEBUG_CONFIG");
 
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_topoInfoExchangeServer_PreemptPortManager_releaseSocket)
{
    HcclIpAddress hostIP("127.0.0.1");
    u32 hostPort = 61111;
    std::vector<HcclIpAddress> whitelist ;
    HcclNetDevCtx netDevCtx;
    std::shared_ptr<HcclSocket> listenSocket(new (std::nothrow)HcclSocket("test", nullptr,
        hostIP, 0, HcclSocketRole::SOCKET_ROLE_SERVER));

    const std::string identifier = "releaseSocket";
    TopoInfoExchangeServer topoExServer(hostIP, hostPort, whitelist, netDevCtx, listenSocket, identifier);
    MOCKER_CPP(&PreemptPortManager::Release).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    bool portSwitch = 1;
    MOCKER(GetExternalInputHostPortSwitch).stubs().will(returnValue(portSwitch));

    EXPECT_EQ(topoExServer.StopSocketListen(whitelist, hostIP, hostPort), HCCL_SUCCESS);
    GlobalMockObject::verify();
}

HcclResult hrtRaGetDeviceAllNicIPStub(vector<vector<HcclIpAddress>> &chipDeviceIPs)
{
    std::vector<HcclIpAddress> chipDeviceIP1;
    std::vector<HcclIpAddress> chipDeviceIP2;
    chipDeviceIP1.push_back(HcclIpAddress("127.0.0.1"));
    chipDeviceIP2.push_back(HcclIpAddress("127.0.0.1"));
    chipDeviceIPs.push_back(chipDeviceIP1);
    chipDeviceIPs.push_back(chipDeviceIP2);
    return HCCL_SUCCESS;
}

TEST_F(OpbaseTest, ut_GetDeviceBackupNicInfo)
{
    setenv("HCCL_SOCKET_FAMILY", "AF_INET", 1);
    TopoInfoDetect topoDetectAgent;
    HcclResult ret;

    MOCKER(hrtRaGetDeviceAllNicIP).stubs().will(invoke(hrtRaGetDeviceAllNicIPStub));
    MOCKER(GetExternalInputNpuPortSwitch).stubs().with(any()).will(returnValue(true));
    MOCKER(hrtGetPairDevicePhyId).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtGetDeviceIndexByPhyId).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TopoInfoDetect::PreemptBackupDeviceNicPort).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    HcclBasicRankInfo localRankInfo;
    localRankInfo.devicePhysicID = 1;
    InitExternalInput();
  
    ret = topoDetectAgent.GetDeviceBackupNicInfo(localRankInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ResetInitState();
    GlobalMockObject::verify();
}

HcclResult IsSuperPodModeStub(bool &useSuperPodMode)
{
    useSuperPodMode = true;
    return HCCL_SUCCESS;
}

TEST_F(OpbaseTest, ut_PreemptDeviceVnicPort)
{
    TopoInfoDetect topoDetectAgent;
    HcclResult ret;
    MOCKER(IsSuperPodMode).stubs().will(invoke(IsSuperPodModeStub));
    MOCKER(hrtRaGetSingleSocketVnicIpInfo).stubs().will(returnValue(HCCL_E_PARA));

    HcclBasicRankInfo localRankInfo;
    localRankInfo.superDeviceId = 1;
    InitExternalInput();

    ret = topoDetectAgent.PreemptDeviceVnicPort(localRankInfo);
    EXPECT_EQ(ret, HCCL_E_PARA);

    ResetInitState();
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_PreemptDeviceVnicPortFail)
{
    TopoInfoDetect topoDetectAgent;
    HcclResult ret;
    MOCKER(hrtRaGetSingleSocketVnicIpInfo).stubs().will(returnValue(HCCL_E_PARA));

    HcclBasicRankInfo localRankInfo;
    InitExternalInput();

    localRankInfo.superDeviceId = INVALID_UINT;
    ret = topoDetectAgent.PreemptDeviceVnicPort(localRankInfo);
    EXPECT_EQ(ret, HCCL_E_PARA);

    ResetInitState();
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_PreemptDevSocket)
{
    TopoInfoDetect topoDetectAgent;
    MOCKER_CPP(&PreemptPortManager::ListenPreempt)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress ipAddr(1694542016);
    u32 port = 0;
    HcclResult ret = topoDetectAgent.PreemptDeviceNicPort(0, 0, ipAddr, port);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hccl::NetDevContext* pNetDevCtx =
        static_cast<hccl::NetDevContext *>(topoDetectAgent.commPortConfig_.devNicListen.second);
    delete pNetDevCtx;
    topoDetectAgent.commPortConfig_.devNicListen.second = nullptr;

    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_PreemptBackupNicSocket)
{
    TopoInfoDetect topoDetectAgent;
    MOCKER_CPP(&PreemptPortManager::ListenPreempt)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress ipAddr(1694542016);
    u32 port = 0;
    HcclResult ret = topoDetectAgent.PreemptBackupDeviceNicPort(0, 0, ipAddr, ipAddr, port);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hccl::NetDevContext* pNetDevCtx =
        static_cast<hccl::NetDevContext *>(topoDetectAgent.commPortConfig_.backupDevNicListen.second);
    delete pNetDevCtx;
    topoDetectAgent.commPortConfig_.backupDevNicListen.second = nullptr;

    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_hcomAllGatherV_91093)
{
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));

    MOCKER(GetExternalInputHcclAicpuUnfold)
    .stubs()
    .with(any())
    .will(returnValue(true));

    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&HcclCommunicator::ExecOp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
 
    nlohmann::json rank_table = rank_table_910_1server_2rank;
 
    char file_name_t[] = "./ut_opbase_test.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);
 
    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }
 
    outfile.close();
 
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    u64* recvCounts;
    u64* recvDispls;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    u32 rankSize = 4;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void* comm;
 
    // 走1910 4pring
    char* rank_table_file = "./ut_opbase_test.json";
    u32 rank_ID = 0;
 
    ret = HcclCommInitClusterInfo(rank_table_file, rank_ID, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(rankSize * count * sizeof(s8));
     sal_memset(recvbuf, rankSize * count * sizeof(s8), 0, rankSize * count * sizeof(s8));
 
    recvCounts= (u64*)sal_malloc(rankSize * sizeof(u64));
     sal_memset(recvCounts, rankSize * sizeof(u64), 0, rankSize * sizeof(u64));
    recvDispls= (u64*)sal_malloc(rankSize * sizeof(u64));
     sal_memset(recvDispls, rankSize * sizeof(u64), 0, rankSize * sizeof(u64));
 
    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
 
    for (int i = 0; i < rankSize; i++)
    {
        recvCounts[i] = count;
        if (i > 0) {
            recvDispls[i] = recvDispls[i-1] + recvCounts[i-1];
        }
    }
 
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    string strTag = "allgatherv";
    ret = hcclComm->AllGatherV(strTag, static_cast<void *>(sendbuf), 2, static_cast<void *>(recvbuf), static_cast<void *>(recvCounts), static_cast<void *>(recvDispls), HCCL_DATA_TYPE_INT8, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    sal_free(sendbuf);
    sal_free(recvbuf);
    sal_free(recvCounts);
    sal_free(recvDispls);
    rt_ret = rtStreamDestroy(stream);
 
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_ExchangeCommUserMem_When_Mem_Valid_Expect_ReturnSuccess)
{
    HcclRootInfo id;
 
    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclCommConfig config;
    HcclCommConfigInit(&config);
 
    config.hcclBufferSize = 300;
    config.hcclDeterministic = 1;
    config.hcclOpExpansionMode = 3;
    strcpy_s(config.hcclCommName, COMM_NAME_MAX_LENGTH, "comm1");
 
    HcclComm comm;
    ret = HcclCommInitRootInfoConfig(1, &id, 0, &config, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    u64 size = 1024 * 1024;
    DeviceMem windowMem = DeviceMem::alloc(size);
    void *windowHandle = nullptr;
    uint32_t ranks[] = {0};
    ret = HcclCommRegister(comm, windowMem.ptr(), size, &windowHandle, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = HcclCommExchangeMem(comm, windowHandle, ranks, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = HcclCommDeregister(comm, windowHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    windowMem.free();
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_ExchangeCommUserMem_When_ranksNum_Valid_Expect_ReturnE_PARA)
{
    HcclRootInfo id;
 
    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclCommConfig config;
    HcclCommConfigInit(&config);
 
    config.hcclBufferSize = 300;
    config.hcclDeterministic = 1;
    config.hcclOpExpansionMode = 3;
    strcpy_s(config.hcclCommName, COMM_NAME_MAX_LENGTH, "comm1");
 
    HcclComm comm;
    ret = HcclCommInitRootInfoConfig(1, &id, 0, &config, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    u64 size = 1024 * 1024;
    DeviceMem windowMem = DeviceMem::alloc(size);
    void *windowHandle = nullptr;
    uint32_t ranks[] = {1, 2};
    ret = HcclCommRegister(comm, windowMem.ptr(), size, &windowHandle, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclCommExchangeMem(comm, windowHandle, ranks, 2);
    EXPECT_EQ(ret, HCCL_E_PARA);

    ret = HcclCommDeregister(comm, windowHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    windowMem.free();
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_ExchangeCommUserMem_When_usedRDMA_Expect_ReturnE_NOT_SUPPORT)
{
    HcclRootInfo id;
 
    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(GetExternalInputInterHccsDisable)
    .stubs()
    .will(returnValue(true));
 
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclCommConfig config;
    HcclCommConfigInit(&config);
 
    config.hcclBufferSize = 300;
    config.hcclDeterministic = 1;
    config.hcclOpExpansionMode = 3;
    strcpy_s(config.hcclCommName, COMM_NAME_MAX_LENGTH, "comm1");
 
    HcclComm comm;
    ret = HcclCommInitRootInfoConfig(1, &id, 0, &config, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    u64 size = 1024 * 1024;
    DeviceMem windowMem = DeviceMem::alloc(size);
    void *windowHandle = nullptr;
    uint32_t ranks[] = {1, 2};
    ret = HcclCommRegister(comm, windowMem.ptr(), size, &windowHandle, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = HcclCommExchangeMem(comm, windowHandle, ranks, 2);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
 
    ret = HcclCommDeregister(comm, windowHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    windowMem.free();
    GlobalMockObject::verify();
}

TEST_F(OpbaseTest, ut_cclbuf_op)
{
    hcclComm comm;
    comm.communicator_ = make_unique<HcclCommunicator>();
    HcclComm commoPtr = &comm;
 
    void *addr;
    uint64_t size = 0;
 
    auto ret = CommGetLocalCCLBuf(commoPtr, &addr, &size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    auto ret1 = CommGetRemoteCCLBuf(commoPtr, 0, &addr, &size);
    EXPECT_EQ(ret1, HCCL_SUCCESS);
 
    auto ret2 = CommGetKFCWorkSpace(commoPtr, &addr, &size);
    EXPECT_EQ(ret2, HCCL_SUCCESS);
 
    auto ret3 = CommGetCCLBufSizeCfg(commoPtr, &size);
    EXPECT_EQ(ret3, HCCL_SUCCESS);
}

TEST_F(OpbaseTest, ut_cclbuf_op_check_size)
{
    hcclComm comm;
    comm.communicator_ = make_unique<HcclCommunicator>();
    HcclComm commoPtr = &comm;
    
    setenv("HCCL_BUFFSIZE", "3000", 1);\
    InitExternalInput();
    void *addr;
    uint64_t size = 0;
    comm.communicator_->CreateCommCCLbuffer();
    auto ret = CommGetLocalCCLBuf(commoPtr, &addr, &size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(size, 6292504576);
}

TEST_F(OpbaseTest, ut_topo_getNetLayer)
{
    hcclComm comm;
    comm.communicator_ = make_unique<HcclCommunicator>();
    HcclComm commoPtr = &comm;
    uint32_t *netLayer = nullptr;
    uint32_t netLayerNum = 0;
    uint32_t topo = 0;
    uint32_t size = 0;

    comm.communicator_->deviceType_ = DevType::DEV_TYPE_910_93;
    auto ret = CommGetNetLayers(commoPtr, &netLayer, &netLayerNum);
    EXPECT_EQ(netLayerNum, 2);
    EXPECT_EQ(netLayer[0], 0);
    EXPECT_EQ(netLayer[1], 1);

    ret = CommGetInstTopoTypeByNetLayer(commoPtr, 0, &topo);
    EXPECT_EQ(topo, HCCL_ALG_SWITCH | HCCL_ALG_RING);
    ret = CommGetInstTopoTypeByNetLayer(commoPtr, 1, &topo);
    EXPECT_EQ(topo, HCCL_ALG_RING);

    comm.communicator_->userRankSize_ = 32;
    ret = CommGetInstSizeByNetLayer(commoPtr, 0, &size);
    EXPECT_EQ(size, 32);
    ret = CommGetInstSizeByNetLayer(commoPtr, 1, &size);
    EXPECT_EQ(size, 32);

    comm.communicator_->deviceType_ = DevType::DEV_TYPE_910B;
    ret = CommGetNetLayers(commoPtr, &netLayer, &netLayerNum);
    EXPECT_EQ(netLayerNum, 1);
    EXPECT_EQ(netLayer[0], 0);

    ret = CommGetInstTopoTypeByNetLayer(commoPtr, 0, &topo);
    EXPECT_EQ(topo, HCCL_ALG_MESH);

    comm.communicator_->deviceType_ = DevType::DEV_TYPE_310P3;
    ret = CommGetNetLayers(commoPtr, &netLayer, &netLayerNum);
    EXPECT_EQ(netLayerNum, 1);
    EXPECT_EQ(netLayer[0], 0);

    ret = CommGetInstTopoTypeByNetLayer(commoPtr, 0, &topo);
    EXPECT_EQ(topo, HCCL_ALG_RING);
}

TEST_F(OpbaseTest, ut_GroupLeaderAccept_When_Accept_E_Connect_Then_ReturnE_Connect)
{
    HcclResult ret = HCCL_SUCCESS;
    MOCKER_CPP(&HcclSocket::Accept)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_TCP_CONNECT));
    MOCKER_CPP(&TopoInfoExchangeServer::DisplayConnectionedRank)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    HcclIpAddress hostIP("127.0.0.1");
    u32 hostPort = 61111;
    std::vector<HcclIpAddress> whitelist ;
    HcclNetDevCtx netDevCtx;
    std::shared_ptr<HcclSocket> listenSocket;
    const std::string identifier = "TOPPINFOEXCHANGE_TEST";
    TopoInfoExchangeServer topoExServer(hostIP, hostPort, whitelist, netDevCtx, listenSocket, identifier);
    std::map<std::string, std::shared_ptr<HcclSocket>> connectSockets;
    ret = topoExServer.GroupLeaderConnect(connectSockets);
    EXPECT_EQ(ret, HCCL_E_TCP_CONNECT);
 
    GlobalMockObject::verify();
}