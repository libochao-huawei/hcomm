#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <mpi.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <runtime/rt.h>


#include <assert.h>
#include <semaphore.h>
#include <signal.h>
#include <syscall.h>
#include <sys/prctl.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <cce/dnn.h>
#include <securec.h>

#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <driver/ascend_hal.h>
#include <runtime/rt.h>

#include "externalinput.h"

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "hcom_pub.h"
#include "hcom.h"
#include "../inc/hccl/hcom_executor.h"
#include "llt_hccl_stub_pub.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "hccl_comm_pub.h"
#include "sal.h"
#include "dlra_function.h"
#include "hccl_comm_pub.h"
#include "config.h"
#include "topo/topoinfo_ranktableParser_pub.h"
#include "externalinput_pub.h"
#include "v80_rank_table.h"
#include "dltdt_function.h"
#include <hccl/hccl.h>
#include <iostream>
#include <fstream>
#include "opexecounter_pub.h"
#include "param_check_pub.h"

using namespace std;
using namespace hccl;

class MPI_HostNic_Test : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_HostNic_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_HostNic_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        MPI_Barrier(MPI_COMM_WORLD);
        static s32  call_cnt = 0;
        DlTdtFunction::GetInstance().DlTdtFunctionInit();
        TsdOpen(1, 2);
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "UT_MPI_TEST");
        ra_set_work_mode(0);
        std::cout << "A MPI_HostNic_Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        TsdClose(1);
        MPI_Barrier(MPI_COMM_WORLD);
        std::cout << "A MPI_HostNic_Test TearDown" << std::endl;
    }
};


#define MPI_ALLREDUCE_SLICE_DATA_SIZE 1024*1024*2 +5
#define MPI_ALLREDUCE_DATA_SIZE 1256
#define MPI_ALLREDUCE_ALIGN_DATA_SIZE 215
#define GOUP_DEV_NUM 8

typedef struct para_struct
{
    HcclRootInfo rootInfo;
    std::string identify;
    s32 comm_num;
    s32 device_id;
    s32 ranks_local; //本服务器内的rank数

    char* file_name;
    void* sendbuff;
    void* recvbuff;
    s32 count;
    HcclDataType datatype;
    HcclReduceOp op;
    rtStream_t stream;
    rtStream_t stream1;
    int id;
    bool flag;
    volatile s32* sync_addr;
    const char* tag;
    char* groupName;
    u32 groupRanksNum;
    u32 *pGroupRanks;
} para_t;

void* inter_all_reduce_task_for_hostnic(void* parg)
{
    HcclResult ret = HCCL_SUCCESS;
    para_t* para_info = (para_t*)parg;
    s32 rank_num_tmp;

    HcomInfo hcom_info;
    std::string ranktable_file = para_info->file_name;
    std::string rankTableM;
    std::string realFilePath;

    hrtSetDevice(para_info->device_id);
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomLoadRanktableFile(ranktable_file.c_str(), rankTableM, realFilePath);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, para_info->identify, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sal_memcpy(hcom_info.params.id.internal, sizeof(HcclRootInfo), &para_info->rootInfo, sizeof(HcclRootInfo));

    hcom_info.pComm.reset(new(std::nothrow) hccl::hcclComm());
    char* charModel = new char;
    rtModel_t model = (void*)charModel;
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);
    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task all_reduce falis", para_info->device_id);
    }
    hcom_info.pComm->CreateCommCCLbuffer();
    //-----------------Set Workspace Resource Start------------------//
    u64 stream_list_size = 0;
    ret = hcom_info.pComm->GetWorkspaceSubStreamNum(stream_list_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("get stream_list_size[%d] success", stream_list_size);
    vector<HcclRtStream> streamList(stream_list_size);

    rtError_t rt_ret;
    //生成从stream
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtStreamCreateWithFlags(&streamList[i], 0, (RT_STREAM_PERSISTENT | RT_STREAM_FORCE_COPY));
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        //从流bind到model
        rt_ret = rtModelBindStream(model, streamList[i], RT_MODEL_WAIT_ACTIVE_STREAM);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    u32 rankSize = 0;
    ret = hcom_info.pComm->GetRankSize(rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 memSize = 0;
    ret = hcom_info.pComm->GetWorkspaceMemSize("HcomAllReduce", para_info->count, para_info->datatype, rankSize, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource("tag_inter_all_reduce_task_for", memptr, memSize, streamList);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//
    bool swapped;
    rank_num_tmp = *(para_info->sync_addr) - 1;

    do
    {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr /** &rank_num */, rank_num_tmp, rank_num_tmp + 1);
    }
    while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local)
    {
        sched_yield(); // linux提供一个系统调用运行进程主动让出执行权
    }

    ret =  hcom_info.pComm->AllReduce("tag_inter_all_reduce_task_for",
                               para_info->sendbuff,
                               para_info->recvbuff,
                               para_info->count,
                               para_info->datatype,
                               para_info->op,
                               para_info->stream);

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task HcclAllReduce falis", para_info->device_id);
    }

    rt_ret = rtStreamSynchronize(para_info->stream);
    if ( rt_ret != RT_ERROR_NONE)
    {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }

    //--------------Resource destroy----------------//
    
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtModelUnbindStream(model, streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        rt_ret = rtStreamDestroy(streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    hrtFree(memptr);
    hcom_info.pComm = nullptr;
    delete charModel;
    charModel = nullptr;
    return (NULL);
}


#define DEV_NUM_1 1
#define DEV_NUM_2 2
#define DEV_NUM_4 4
#define DEV_NUM_8 8

#if 1
TEST_F(MPI_HostNic_Test, ut_mpi_hcclMasterInfo_single_server_2p_success_normal)
{
    HcclResult ret;
    s32 nnode, rank = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    hrtSetDevice(rank);

    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
    setenv("HCCL_IF_IP", "127.0.0.1", 1);
    setenv("HCCL_BUFFSIZE", "200", 1);

    string masterIp = "127.0.0.1";
    string masterPort = "6000";
    string masterID = "0";
    string rankIp = "127.0.0.1";
    string rankSize = "2";
    ret = HcomInitByMasterInfo(masterIp, masterPort, masterID, rankSize, rankIp);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    MPI_Barrier(MPI_COMM_WORLD);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ResetInitState();
    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_IF_IP");
    unsetenv("HCCL_BUFFSIZE");
}
TEST_F(MPI_HostNic_Test, ut_mpi_hcclMasterInfo_single_server_2p_timeout)
{
    HcclResult ret;
    s32 nnode, rank = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    hrtSetDevice(rank);
    MOCKER(GetExternalInputHcclLinkTimeOut)
    .stubs()
    .will(returnValue(2));
    string masterIp = "127.0.0.1";
    string masterPort = "6000";
    string masterID = "0";
    string rankIp = "127.0.0.1";
    string rankSize = "2";
    if(rank == 0) {
        ret = HcomInitByMasterInfo(masterIp, masterPort, masterID, rankSize, rankIp);
        EXPECT_EQ(ret, HCCL_E_INTERNAL);
        HcomDestroy();
        SaluSleep(SAL_SECOND_USEC * 2); // 需要额外sleep下，等待server背景线程把host nic释放，防止后续用例受影响
    }
    GlobalMockObject::verify();
    MPI_Barrier(MPI_COMM_WORLD);

    ResetInitState();
    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_IF_IP");
}
#endif

#if 1
TEST_F(MPI_HostNic_Test, ut_hrtGetHostIf)
{
    HcclResult ret = HCCL_SUCCESS;
    u32 ifAddrNum = 3;
    std::vector<std::pair<std::string, HcclIpAddress>> hostIf;
/*
    MOCKER(hrtGetIfNum)
    .stubs()
    .with(any(), outBound(ifAddrNum))
    .will(returnValue(0));

    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    MOCKER(malloc)
    .stubs()
    .will(returnValue((void *)nullptr));

    ret = hrtGetHostIf(hostIf);
    EXPECT_EQ(ret, 3);
    GlobalMockObject::verify();
*/
    MOCKER(hrtGetIfNum)
    .stubs()
    .with(any(), outBound(ifAddrNum))
    .will(returnValue(0));
    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));
    MOCKER(hrtGetIfAddress)
    .stubs()
    .will(returnValue(1));

    ret = hrtGetHostIf(hostIf);
    EXPECT_EQ(ret, 1);
    GlobalMockObject::verify();
  
    ifAddrNum = 0;
    MOCKER(hrtGetIfNum)
    .stubs()
    .with(any(), outBound(ifAddrNum))
    .will(returnValue(0));
    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    ret = hrtGetHostIf(hostIf);
    EXPECT_EQ(ret, 0);
    GlobalMockObject::verify();
}
#endif

#if 1
TEST_F(MPI_HostNic_Test, ut_hrtGetIfNum)
{
    HcclResult ret = HCCL_SUCCESS;
    struct RaGetIfattr config = {0};
    u32 ifAddrNum = 3;

    config.phyId = 0;
    config.nicPosition = static_cast<u32>(NICDeployment::NIC_DEPLOYMENT_DEVICE);
    DlRaFunction::GetInstance().DlRaFunctionInit();

    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(0));
    MOCKER(hrtRaGetInterfaceVersion)
    .stubs()
    .will(returnValue(1));
    ret = hrtGetIfNum(config, ifAddrNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}
#endif

#if 1
TEST_F(MPI_HostNic_Test, ut_hrtRaGetDeviceIP)
{
    HcclResult ret = HCCL_SUCCESS;
    u32 ifAddrNum = 0;


    MOCKER(hrtGetIfNum)
    .stubs()
    .with(any(), outBound(ifAddrNum))
    .will(returnValue(0));
    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(0));

    std::vector<HcclIpAddress> ipAddr;
    ret = hrtRaGetDeviceIP(0, ipAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}
#endif

HcclResult stub_hrtGetDeviceInfo(u32 deviceId, HcclRtDeviceModuleType moduleType, HcclRtDeviceInfoType infoType, s64 &val)
{
    val = deviceId;
    return HCCL_SUCCESS;
}

#if 1
TEST_F(MPI_HostNic_Test, ut_mpi_91093_hcclMasterInfo_single_server_2p_success_normal)
{
    HcclResult ret;
    s32 nnode, rank = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    DevType type91093 = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(type91093))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDeviceInfo)
    .stubs()
	.will(invoke(stub_hrtGetDeviceInfo));

	MOCKER(hrtRaGetSingleSocketVnicIpInfo)
	.stubs()
	.with(any())
	.will(invoke(stub_hrtRaGetSingleSocketVnicIpInfo));
    
    hrtSetDevice(rank);

    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
    setenv("HCCL_IF_IP", "127.0.0.1", 1);
    setenv("HCCL_BUFFSIZE", "200", 1);

    string masterIp = "127.0.0.1";
    string masterPort = "6000";
    string masterID = "0";
    string rankIp = "127.0.0.1";
    string rankSize = "2";
    ret = HcomInitByMasterInfo(masterIp, masterPort, masterID, rankSize, rankIp);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    MPI_Barrier(MPI_COMM_WORLD);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ResetInitState();
    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_IF_IP");
    unsetenv("HCCL_BUFFSIZE");
    GlobalMockObject::verify();
}
#endif