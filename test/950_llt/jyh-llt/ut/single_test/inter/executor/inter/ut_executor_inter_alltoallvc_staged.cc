#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include <vector>
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

#include "dlra_function.h"

#define private public
#define protected public
#include "hccl_alg.h"
#include "hccl_impl.h"
#include "hccl_communicator.h"
#include "hccl_comm_pub.h"
#include "comm_impl.h"
#include "coll_all_to_all_executor.h"
#undef private
#undef protected

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "hccl_comm_pub.h"
#include "externalinput_pub.h"
#include "externalinput.h"
#include "externalinput.h"
#include "workflow_pub.h"
#include "rank_consistentcy_checker.h"
#define private public
#include "alltoallv_staged_pairwise.h"
#include "hcom_ops_kernel_info_store.h"
#include "hcom_graph_optimizer.h"
#include "hvd_ops_kernel_info_store.h"
#undef private
#include "op_base.h"
#include "sal.h"
#include "hccl_impl.h"
#include "hccl_comm_pub.h"
#include "topoinfo_parse.h"
#include "config.h"
#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"
#include "param_check_pub.h"

#include "tsd/tsd_client.h"
#include "dltdt_function.h"

#include <iostream>
#include <fstream>

using namespace std;
using namespace hccl;

class Inter_AlltoAllVC_Staged_Test : public testing::TestWithParam<bool> {
protected:
    // static void SetUpTestCase()
    // {
    //     std::cout << "Inter_AlltoAllVC_Staged_Test SetUP" << std::endl;
    // }
    // static void TearDownTestCase()
    // {
    //     std::cout << "Inter_AlltoAllVC_Staged_Test TearDown" << std::endl;
    // }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&Heartbeat::Init)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&Heartbeat::RegisterRanks)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&Heartbeat::UnRegisterRanks)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
        DlTdtFunction::GetInstance().DlTdtFunctionInit();
        TsdOpen(1, 2);

        static s32 call_cnt = 0;
        string name = std::to_string(call_cnt++) + "_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name.c_str());

        std::cout << "A Inter_AlltoAllVC_Staged_Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        TsdClose(1);
        GlobalMockObject::verify();
        std::cout << "A Inter_AlltoAllVC_Staged_Test TearDown" << std::endl;
    }
};

#define GOUP_DEV_NUM 8

typedef struct para_struct {
    HcclRootInfo rootInfo;
    std::string identify;
    s32 userRankSize;
    s32 device_id;
    s32 ranks_local;

    char *file_name;
    void *sendbuff;
    void *sendCounts;
    void *sdispls;
    void *recvbuff;
    void *recvCounts;
    void *rdispls;
    void *sendCountMatrix;
    HcclDataType sendDataType;
    HcclDataType recvDataType;

    rtStream_t stream;

    int id;
    bool flag;
    volatile s32 *sync_addr;
    const char *tag;
    char *groupName;
    u32 groupRanksNum;
    u32 *pGroupRanks;
} para_t;

typedef struct para_alltoall_struct {
    HcclRootInfo rootInfo;
    std::string identify;
    s32 userRankSize;
    s32 device_id;
    s32 ranks_local;

    char *file_name;
    void *sendbuff;
    u64 sendCount;
    void *recvbuff;
    u64 recvCount;
    HcclDataType sendDataType;
    HcclDataType recvDataType;

    rtStream_t stream;

    int id;
    bool flag;
    volatile s32 *sync_addr;
    const char *tag;
    char *groupName;
    u32 groupRanksNum;
    u32 *pGroupRanks;
} para_alltoall_t;

#define CHK_BBIT_RET(call)                                 \
    do {                                              \
        s32 ret = call;                        \
        EXPECT_EQ(ret, 0);                                     \
    } while (0)

#define CHK_UT_PTR_NULL(ptr)   \
    do {                       \
        EXPECT_NE(ptr, nullptr);     \
    } while (0)

// alltoallv graph
void *inter_alltoallv_graph_staged_task(void *parg)
{
    HcclResult ret = HCCL_SUCCESS;
    para_t *para_info = (para_t *)parg;
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

    hcom_info.pComm.reset(new (std::nothrow) hccl::hcclComm());
    rtModel_t model = (void *)1;
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);
    HcclWorkflowMode mode = GetWorkflowMode();
    InitWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB);

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("dev[%d] task all_gather falis", para_info->device_id);
    }

    //-----------------Set Workspace Resource Start------------------//
    u64 stream_list_size = 0;
    ret = hcom_info.pComm->GetWorkspaceSubStreamNum(stream_list_size);
    HCCL_INFO("==TMP== stream_list_size=%d", stream_list_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("get stream_list_size[%d] success", stream_list_size);
    vector<HcclRtStream> streamList(stream_list_size);
    rtError_t rt_ret;
    // stream
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtStreamCreateWithFlags(&streamList[i], 0, (RT_STREAM_PERSISTENT | RT_STREAM_FORCE_COPY));
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        // bind model
        rt_ret = rtModelBindStream(model, streamList[i], RT_MODEL_WAIT_ACTIVE_STREAM);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    // workspace
    string tag = "tag-alltoallv-staged";
    u64 memSize = 0;
    ret = hcom_info.pComm->GetAlltoAllStagedWorkSpaceMemSize(
       static_cast<u64*>(para_info->sendCounts), static_cast<u64*>(para_info->sdispls), para_info->sendDataType,
       static_cast<u64*>(para_info->recvCounts), static_cast<u64*>(para_info->rdispls), para_info->recvDataType,
        memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HCCL_INFO("==TMP==  workspace memSize[%llu]", memSize);

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource(tag, memptr, memSize, streamList);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//

    bool swapped;
    rank_num_tmp = *(para_info->sync_addr) - 1;

    do {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1);
    } while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local) {
        sched_yield();
    }  // linux

    ret = hcom_info.pComm->AlltoAllV(para_info->sendbuff,
        para_info->sendCounts,
        para_info->sdispls,
        para_info->sendDataType,
        para_info->recvbuff,
        para_info->recvCounts,
        para_info->rdispls,
        para_info->recvDataType,
        para_info->stream,
        tag);

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }

    rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamSynchronize(para_info->stream);

    if (rt_ret != RT_ERROR_NONE) {
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

    ret = hcom_info.pComm->ClearOpResource(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hrtFree(memptr);
    SetWorkflowMode(mode);

    return (nullptr);
}

// opbase
void *inter_alltoallvc_staged_task1(void *parg)
{
    HcclResult ret = HCCL_SUCCESS;
    para_t *para_info = (para_t *)parg;
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

    hcom_info.pComm.reset(new (std::nothrow) hccl::hcclComm());
    rtModel_t model = (void *)1;
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("dev[%d] task all_gather falis", para_info->device_id);
    }

    ret = hcom_info.pComm->CreateCommCCLbuffer();
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("dev[%d] task all_gather falis", para_info->device_id);
    }

    HcclWorkflowMode mode = GetWorkflowMode();
    InitWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);

    //-----------------Set Workspace Resource Start------------------//
    std::vector<rtStream_t> subStreams;
    string tag = "tag-alltoallv-staged-opbase";
    u64 memSize = 0;
    ret = hcom_info.pComm->GetAlltoAllStagedWorkSpaceMemSize(
       static_cast<u64*>(para_info->sendCounts), static_cast<u64*>(para_info->sdispls), para_info->sendDataType,
       static_cast<u64*>(para_info->recvCounts), static_cast<u64*>(para_info->rdispls), para_info->recvDataType,
        memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    HCCL_INFO("==TMP==  workspace[%p] memSize[%llu]", memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource(tag, memptr, memSize, subStreams);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//

    bool swapped;
    rank_num_tmp = *(para_info->sync_addr) - 1;

    do {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1);
    } while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local) {
        sched_yield();
    }  // linux提供一个系统调用运行进程主动让出执行权

    ret = hcom_info.pComm->AlltoAllVC(para_info->sendbuff,
        para_info->sendCountMatrix,
        para_info->sendDataType,
        para_info->recvbuff,
        para_info->recvDataType,
        para_info->stream,
        tag);

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }

    rtError_t rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamSynchronize(para_info->stream);

    if (rt_ret != RT_ERROR_NONE) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }
    //--------------Resource destroy----------------//

    hrtFree(memptr);
    SetWorkflowMode(mode);

    return (nullptr);
}

void *inter_alltoall_staged_task1(void *parg)
{
    HcclResult ret = HCCL_SUCCESS;
    para_alltoall_t *para_info = (para_alltoall_t *)parg;
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

    hcom_info.pComm.reset(new (std::nothrow) hccl::hcclComm());
    rtModel_t model = (void *)1;
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("dev[%d] task alltoall failed", para_info->device_id);
    }

    ret = hcom_info.pComm->CreateCommCCLbuffer();
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("dev[%d] task alltoall failed", para_info->device_id);
    }

    HcclWorkflowMode mode = GetWorkflowMode();
    InitWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);

    //-----------------Set Workspace Resource Start------------------//
    string tag = "tag-alltoall-staged-opbase";

    //-----------------Set Workspace Resource End------------------//

    bool swapped;
    rank_num_tmp = *(para_info->sync_addr) - 1;

    do {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1);
    } while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local) {
        sched_yield();
    }  // linux提供一个系统调用运行进程主动让出执行权

    ret = hcom_info.pComm->AlltoAll(para_info->sendbuff,
        para_info->sendCount,
        para_info->sendDataType,
        para_info->recvbuff,
        para_info->recvCount,
        para_info->recvDataType,
        para_info->stream,
        tag);

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }

    rtError_t rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamSynchronize(para_info->stream);

    if (rt_ret != RT_ERROR_NONE) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }
    //--------------Resource destroy----------------//
    SetWorkflowMode(mode);

    return (nullptr);
}

//ffts
void *inter_alltoallvc_staged_task1_ffts(void *parg)
{
    HcclResult ret = HCCL_SUCCESS;
    para_t *para_info = (para_t *)parg;
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

    hcom_info.pComm.reset(new (std::nothrow) hccl::hcclComm());
    rtModel_t model = (void *)1;
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("dev[%d] task all_gather falis", para_info->device_id);
    }

    ret = hcom_info.pComm->CreateCommCCLbuffer();
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("dev[%d] task all_gather falis", para_info->device_id);
    }

    HcclWorkflowMode mode = GetWorkflowMode();
    InitWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);

    //-----------------Set Workspace Resource Start------------------//
    std::vector<rtStream_t> subStreams;
    string tag = "tag-alltoallv-staged-opbase";
    u64 memSize = 0;
    ret = hcom_info.pComm->GetAlltoAllStagedWorkSpaceMemSize(
       static_cast<u64*>(para_info->sendCounts), static_cast<u64*>(para_info->sdispls), para_info->sendDataType,
       static_cast<u64*>(para_info->recvCounts), static_cast<u64*>(para_info->rdispls), para_info->recvDataType,
        memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    HCCL_INFO("==TMP==  workspace[%p] memSize[%llu]", memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource(tag, memptr, memSize, subStreams);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//

    bool swapped;
    rank_num_tmp = *(para_info->sync_addr) - 1;

    do {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1);
    } while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local) {
        sched_yield();
    }  // linux提供一个系统调用运行进程主动让出执行权


    ret = hcom_info.pComm->AlltoAllVCOutPlace(para_info->sendbuff,
        para_info->sendCountMatrix,
        para_info->sendDataType,
        para_info->recvbuff,
        para_info->recvDataType,
        para_info->stream,
        tag);

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }

    rtError_t rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamSynchronize(para_info->stream);

    if (rt_ret != RT_ERROR_NONE) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }
    //--------------Resource destroy----------------//

    hrtFree(memptr);
    SetWorkflowMode(mode);

    return (nullptr);
}

void *inter_alltoallv_staged_task_ffts(void *parg)
{
    HcclResult ret = HCCL_SUCCESS;
    para_t *para_info = (para_t *)parg;
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

    hcom_info.pComm.reset(new (std::nothrow) hccl::hcclComm());
    rtModel_t model = (void *)1;
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);
    HcclWorkflowMode mode = GetWorkflowMode();
    InitWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB);

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("dev[%d] task all_gather falis", para_info->device_id);
    }

    //-----------------Set Workspace Resource Start------------------//
    u64 stream_list_size = 0;
    ret = hcom_info.pComm->GetWorkspaceSubStreamNum(stream_list_size);
    HCCL_INFO("==TMP== stream_list_size=%d", stream_list_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("get stream_list_size[%d] success", stream_list_size);
    vector<HcclRtStream> streamList(stream_list_size);
    rtError_t rt_ret;
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtStreamCreateWithFlags(&streamList[i], 0, (RT_STREAM_PERSISTENT | RT_STREAM_FORCE_COPY));
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        // bind model
        rt_ret = rtModelBindStream(model, streamList[i], RT_MODEL_WAIT_ACTIVE_STREAM);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    string tag = "tag-alltoallv-staged";
    u64 memSize = 0;
    ret = hcom_info.pComm->GetAlltoAllStagedWorkSpaceMemSize(
       static_cast<u64*>(para_info->sendCounts), static_cast<u64*>(para_info->sdispls), para_info->sendDataType,
       static_cast<u64*>(para_info->recvCounts), static_cast<u64*>(para_info->rdispls), para_info->recvDataType,
        memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HCCL_INFO("==TMP==  workspace memSize[%llu]", memSize);

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource(tag, memptr, memSize, streamList);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//

    bool swapped;
    rank_num_tmp = *(para_info->sync_addr) - 1;

    do {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1);
    } while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local) {
        sched_yield();
    }

    ret = hcom_info.pComm->AlltoAllVOutPlace(para_info->sendbuff,
        para_info->sendCounts,
        para_info->sdispls,
        para_info->sendDataType,
        para_info->recvbuff,
        para_info->recvCounts,
        para_info->rdispls,
        para_info->recvDataType,
        para_info->stream,
        tag);

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }

    rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamSynchronize(para_info->stream);

    if (rt_ret != RT_ERROR_NONE) {
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

    ret = hcom_info.pComm->ClearOpResource(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hrtFree(memptr);
    SetWorkflowMode(mode);

    return (nullptr);
}


#define DEV_NUM_2 2
#define DEV_NUM_4 4
#define DEV_NUM_8 8

INSTANTIATE_TEST_CASE_P(FFTSSwitch, Inter_AlltoAllVC_Staged_Test, testing::Bool());
#if 1
TEST_P(Inter_AlltoAllVC_Staged_Test, ut_inter_opbase_alltoallvc_fullmesh_4ranks_1server_int64)
{
    bool fftsSwitch = GetParam();
    HcclResult ret;
    // 属于单MeshAggregation场景，会走全连接实现
    std::string algo = "level0:fullmesh;level1:pairwise";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();

    nlohmann::json rank_table = rank_table_910_1server_4rank;
    char file_name_t[] = "./ut_inter_alltoallvc_fullmesh_zcopy_4ranks_1server_int64.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    s32 nnode, rank, errors = 0;
    nnode = 1;
    ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s64* result_buff[DEV_NUM_4];
    s64* sendbuff[DEV_NUM_4];
    s64* recvbuff[DEV_NUM_4];
    u64* sendCounts[DEV_NUM_4];
    u64* sdispls[DEV_NUM_4];
    u64* recvCounts[DEV_NUM_4];
    u64* rdispls[DEV_NUM_4];
    u64* sendCountMatrix = (u64*)sal_malloc(DEV_NUM_4 * DEV_NUM_4 * sizeof(u64));

    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];
    HcclDataType sendDataType = HCCL_DATA_TYPE_INT64;
    HcclDataType recvDataType = HCCL_DATA_TYPE_INT64;

    s32 ndev = DEV_NUM_4;
    set_board_id(0x0000);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 totalRecvCounts[DEV_NUM_4];
    u64 ctotalRecvCounts[DEV_NUM_4];

    /** 初始化输入输出缓存 */
    u64 count = 1048576;
    for (u32 i = 0; i < ndev; i++ ) {
        for (u32 j = 0; j < ndev; j++) {
            sendCountMatrix[i * ndev + j] = (j + 1) * count;
        }
    }
    for (u32 i = 0; i < ndev; i++)
    {
        u64 totalSendCounts = 0;
        u64 ctotalSendCounts = 0;
        sendCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            sendCounts[i][j] = (j + 1) * count;
            totalSendCounts += sendCounts[i][j];
            ctotalSendCounts += sendCountMatrix[i * ndev + j];
        }

        HCCL_INFO("==TMP== rank%u totalSendCounts[%llu], ctotalSendCounts[%llu]", i, totalSendCounts, ctotalSendCounts);

        sdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curSDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            sdispls[i][j] = curSDispl;
            curSDispl += sendCounts[i][j];
        }

        totalRecvCounts[i] = 0;
        ctotalRecvCounts[i] = 0;
        recvCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            recvCounts[i][j] = (1 + i) * count;
            totalRecvCounts[i] += recvCounts[i][j];
            ctotalRecvCounts[i] += sendCountMatrix[i + ndev * j];
        }
        HCCL_INFO("==TMP== rank%u totalRecvCounts[%llu], ctotalRecvCounts[%llu]", i, totalRecvCounts[i], ctotalRecvCounts[i]);

        rdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curRDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            rdispls[i][j] = curRDispl;
            curRDispl += recvCounts[i][j];
        }

        ret = hrtMalloc((void**)&sendbuff[i], totalSendCounts * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        for (u32 j = 0; j < totalSendCounts; j++) {
            sendbuff[i][j] = 1;
        }

        ret = hrtMalloc((void**)&recvbuff[i], totalRecvCounts[i] * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuff[i], totalRecvCounts[i] * sizeof(s64), 0, totalRecvCounts[i] * sizeof(s64));

        result_buff[i] = (s64*)sal_malloc(totalRecvCounts[i] * sizeof(s64));
        for (u32 j = 0; j < totalRecvCounts[i]; j++) {
            result_buff[i][j] = 1;
        }
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = rtStreamCreate(&stream[i], 5);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        sal_memcpy(&para_info[i].rootInfo, sizeof(HcclRootInfo), &rootInfo, sizeof(HcclRootInfo));
        std::ostringstream identify("");
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].userRankSize = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;


        para_info[i].sendbuff = sendbuff[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuff[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;

        para_info[i].sendCounts = sendCounts[i];
        para_info[i].sdispls = sdispls[i];

        para_info[i].recvCounts = recvCounts[i];
        para_info[i].rdispls = rdispls[i];

        para_info[i].sendCountMatrix = sendCountMatrix;

        para_info[i].sendDataType = sendDataType;
        para_info[i].recvDataType = recvDataType;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        if (fftsSwitch) {
            tid[i] = sal_thread_create("thread", inter_alltoallvc_staged_task1_ffts, (void*)&para_info[i]);
        } else {
            tid[i] = sal_thread_create("thread", inter_alltoallvc_staged_task1, (void*)&para_info[i]);
        }

        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //获取stream的操作的同步信号量
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < totalRecvCounts[j]; i++)
        {
            s64 res = result_buff[j][i];
            s64 recv = recvbuff[j][i];

            if (res != recv)
            {
                HCCL_ERROR("node:%d result[%d][%d]:%llu recvbuff:%llu\n", noderank, j, i, result_buff[j][i] , recvbuff[j][i]);
                errors++;
                break;
            }
        }
    }

    if (errors)
    {
        HCCL_ERROR("%d errors. Test FAILED.\n", errors);
    }
    else
    {
        HCCL_INFO("Test PASSED.\n");
    }

    SaluSleep(SAL_SECOND_USEC);

    for (s32 j = 0; j < ndev; j++)
    {
        hrtFree(sendbuff[j]);
        hrtFree(recvbuff[j]);
        sal_free(result_buff[j]);

        sal_free(sendCounts[j]);
        sal_free(sdispls[j]);
        sal_free(recvCounts[j]);
        sal_free(rdispls[j]);

        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    sal_free(sendCountMatrix);

    set_board_id(0x0000);
    remove(file_name_t);
    EXPECT_EQ(errors, 0);

    ResetInitState();

    algo = "level0:ring;level1:H-D_R";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

void *inter_alltoallvc_staged_task_ffts(void *parg)
{
    HcclResult ret = HCCL_SUCCESS;
    para_t *para_info = (para_t *)parg;
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

    hcom_info.pComm.reset(new (std::nothrow) hccl::hcclComm(1*1024*1024, 1*1024*1024, HCCL_WORLD_GROUP));
    rtModel_t model = (void *)1;
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("dev[%d] task all_gather falis", para_info->device_id);
    }

    HcclWorkflowMode mode = GetWorkflowMode();
    InitWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);

    //-----------------Set Workspace Resource Start------------------//
    // alltoallv 不需要申请从stream
    std::vector<rtStream_t> subStreams;
    // 获取workspace内存大小
    string tag = "tag-alltoallvc-ffts-staged-opbase";
    u64 memSize = 0;
    ret = hcom_info.pComm->GetAlltoAllStagedWorkSpaceMemSize(
       static_cast<u64*>(para_info->sendCounts), static_cast<u64*>(para_info->sdispls), para_info->sendDataType,
       static_cast<u64*>(para_info->recvCounts), static_cast<u64*>(para_info->rdispls), para_info->recvDataType,
        memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcom_info.pComm.reset(new (std::nothrow) hccl::hcclComm(1*1024*1024, 1*1024*1024, HCCL_WORLD_GROUP));
    hcom_info.params.deviceType = DevType::DEV_TYPE_910B;
    SetFftsSwitch(true);
    InitEnvVarParam();
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    HCCL_INFO("==TMP==  workspace[%p] memSize[%llu]", memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource(tag, memptr, memSize, subStreams);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//

    bool swapped;
    rank_num_tmp = *(para_info->sync_addr) - 1;

    do {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1);
    } while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local) {
        sched_yield();
    }  // linux提供一个系统调用运行进程主动让出执行权

    ret = hcom_info.pComm->AlltoAllVCOutPlace(para_info->sendbuff,
        para_info->sendCountMatrix,
        para_info->sendDataType,
        para_info->recvbuff,
        para_info->recvDataType,
        para_info->stream,
        tag);

    SetFftsSwitch(true);
    InitEnvVarParam();

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }

    rtError_t rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamSynchronize(para_info->stream);

    if (rt_ret != RT_ERROR_NONE) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }
    //--------------Resource destroy----------------//

    hrtFree(memptr);
    SetWorkflowMode(mode);

    return (nullptr);
}

#if 1
TEST_F(Inter_AlltoAllVC_Staged_Test, st_inter_opbase_alltoallvc_ffts_fullmesh_4ranks_1server_int64)
{
    HcclResult ret = HCCL_SUCCESS;
    // 属于单MeshAggregation场景，会走全连接实现
    std::string algo = "level0:pairwise;level1:pairwise";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();

    nlohmann::json rank_table = rank_table_910_1server_4rank;
    char file_name_t[] = "./st_inter_alltoallvc_ffts_fullmesh_4ranks_1server_int64.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    s32 nnode, rank, errors = 0;
    nnode = 1;
    ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s64* result_buff[DEV_NUM_4];
    s64* sendbuff[DEV_NUM_4];
    s64* recvbuff[DEV_NUM_4];
    u64* sendCounts[DEV_NUM_4];
    u64* sdispls[DEV_NUM_4];
    u64* recvCounts[DEV_NUM_4];
    u64* rdispls[DEV_NUM_4];
    u64* sendCountMatrix = (u64*)sal_malloc(DEV_NUM_4 * DEV_NUM_4 * sizeof(u64));

    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];
    HcclDataType sendDataType = HCCL_DATA_TYPE_INT64;
    HcclDataType recvDataType = HCCL_DATA_TYPE_INT64;

    s32 ndev = DEV_NUM_4;
    set_board_id(0x0000);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 totalRecvCounts[DEV_NUM_4];
    u64 ctotalRecvCounts[DEV_NUM_4];

    /** 初始化输入输出缓存 */
    u64 count = 1048576;
    for (u32 i = 0; i < ndev; i++ ) {
        for (u32 j = 0; j < ndev; j++) {
            sendCountMatrix[i * ndev + j] = (j + 1) * count;
        }
    }
    for (u32 i = 0; i < ndev; i++ )
    {
        u64 totalSendCounts = 0;
        u64 ctotalSendCounts = 0;
        sendCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            sendCounts[i][j] = (j + 1) * count;
            totalSendCounts += sendCounts[i][j];
            ctotalSendCounts += sendCountMatrix[i * ndev + j];
        }

        HCCL_INFO("==TMP== rank%u totalSendCounts[%llu], ctotalSendCounts[%llu]", i, totalSendCounts, ctotalSendCounts);

        sdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curSDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            sdispls[i][j] = curSDispl;
            curSDispl += sendCounts[i][j];
        }

        totalRecvCounts[i] = 0;
        ctotalRecvCounts[i] = 0;
        recvCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            recvCounts[i][j] = (1 + i) * count;
            totalRecvCounts[i] += recvCounts[i][j];
            ctotalRecvCounts[i] += sendCountMatrix[i + ndev * j];
        }
        HCCL_INFO("==TMP== rank%u totalRecvCounts[%llu], ctotalRecvCounts[%llu]", i, totalRecvCounts[i], ctotalRecvCounts[i]);

        rdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curRDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            rdispls[i][j] = curRDispl;
            curRDispl += recvCounts[i][j];
        }

        ret = hrtMalloc((void**)&sendbuff[i], totalSendCounts * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        for (u32 j = 0; j < totalSendCounts; j++) {
            sendbuff[i][j] = 1;
        }

        ret = hrtMalloc((void**)&recvbuff[i], totalRecvCounts[i] * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuff[i], totalRecvCounts[i] * sizeof(s64), 0, totalRecvCounts[i] * sizeof(s64));

        result_buff[i] = (s64*)sal_malloc(totalRecvCounts[i] * sizeof(s64));
        for (u32 j = 0; j < totalRecvCounts[i]; j++) {
            result_buff[i][j] = 1;
        }
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = rtStreamCreate(&stream[i], 5);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        sal_memcpy(&para_info[i].rootInfo, sizeof(HcclRootInfo), &rootInfo, sizeof(HcclRootInfo));
        std::ostringstream identify("");
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].userRankSize = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;


        para_info[i].sendbuff = sendbuff[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuff[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;

        para_info[i].sendCounts = sendCounts[i];
        para_info[i].sdispls = sdispls[i];

        para_info[i].recvCounts = recvCounts[i];
        para_info[i].rdispls = rdispls[i];

        para_info[i].sendCountMatrix = sendCountMatrix;

        para_info[i].sendDataType = sendDataType;
        para_info[i].recvDataType = recvDataType;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread_alltoallvc_ffts", inter_alltoallvc_staged_task_ffts, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //获取stream的操作的同步信号量
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    if (errors)
    {
        HCCL_ERROR("%d errors. Test FAILED.\n", errors);
    }
    else
    {
        HCCL_INFO("Test PASSED.\n");
    }

    SaluSleep(SAL_SECOND_USEC);

    for (s32 j = 0; j < ndev; j++)
    {
        hrtFree(sendbuff[j]);
        hrtFree(recvbuff[j]);
        sal_free(result_buff[j]);

        sal_free(sendCounts[j]);
        sal_free(sdispls[j]);
        sal_free(recvCounts[j]);
        sal_free(rdispls[j]);

        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    sal_free(sendCountMatrix);

    set_board_id(0x0000);
    remove(file_name_t);
    EXPECT_EQ(errors, 0);

    algo = "level0:ring;level1:H-D_R";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ResetInitState();
}
#endif


#if 1
TEST_F(Inter_AlltoAllVC_Staged_Test, ut_inter_opbase_alltoallvc_staged_zcopy_4ranks_1server_int64)
{
    MOCKER_CPP(&TopoInfoParse::IsSingleMeshAggregation)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    HcclResult ret;
    std::string algo = "level0:fullmesh;level1:pairwise";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();

    nlohmann::json rank_table = rank_table_910_1server_4rank;
    char file_name_t[] = "./ut_inter_alltoallvc_staged_zcopy_4ranks_1server_int64.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    s32 nnode, rank, errors = 0;
    nnode = 1;
    ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s64* result_buff[DEV_NUM_4];
    s64* sendbuff[DEV_NUM_4];
    s64* recvbuff[DEV_NUM_4];
    u64* sendCounts[DEV_NUM_4];
    u64* sdispls[DEV_NUM_4];
    u64* recvCounts[DEV_NUM_4];
    u64* rdispls[DEV_NUM_4];
    u64* sendCountMatrix = (u64*)sal_malloc(DEV_NUM_4 * DEV_NUM_4 * sizeof(u64));

    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];
    HcclDataType sendDataType = HCCL_DATA_TYPE_INT64;
    HcclDataType recvDataType = HCCL_DATA_TYPE_INT64;

    s32 ndev = DEV_NUM_4;
    set_board_id(0x0000);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 totalRecvCounts[DEV_NUM_4];
    u64 ctotalRecvCounts[DEV_NUM_4];

    u64 count = 1048576;
    for (u32 i = 0; i < ndev; i++ ) {
        for (u32 j = 0; j < ndev; j++) {
            sendCountMatrix[i * ndev + j] = (j + 1) * count;
        }
    }
    for (u32 i = 0; i < ndev; i++)
    {
        u64 totalSendCounts = 0;
        u64 ctotalSendCounts = 0;
        sendCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            sendCounts[i][j] = (j + 1) * count;
            totalSendCounts += sendCounts[i][j];
            ctotalSendCounts += sendCountMatrix[i * ndev + j];
        }

        HCCL_INFO("==TMP== rank%u totalSendCounts[%llu], ctotalSendCounts[%llu]", i, totalSendCounts, ctotalSendCounts);

        sdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curSDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            sdispls[i][j] = curSDispl;
            curSDispl += sendCounts[i][j];
        }

        totalRecvCounts[i] = 0;
        ctotalRecvCounts[i] = 0;
        recvCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            recvCounts[i][j] = (1 + i) * count;
            totalRecvCounts[i] += recvCounts[i][j];
            ctotalRecvCounts[i] += sendCountMatrix[i + ndev * j];
        }
        HCCL_INFO("==TMP== rank%u totalRecvCounts[%llu], ctotalRecvCounts[%llu]", i, totalRecvCounts[i], ctotalRecvCounts[i]);

        rdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curRDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            rdispls[i][j] = curRDispl;
            curRDispl += recvCounts[i][j];
        }

        ret = hrtMalloc((void**)&sendbuff[i], totalSendCounts * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        for (u32 j = 0; j < totalSendCounts; j++) {
            sendbuff[i][j] = 1;
        }

        ret = hrtMalloc((void**)&recvbuff[i], totalRecvCounts[i] * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuff[i], totalRecvCounts[i] * sizeof(s64), 0, totalRecvCounts[i] * sizeof(s64));

        result_buff[i] = (s64*)sal_malloc(totalRecvCounts[i] * sizeof(s64));
        for (u32 j = 0; j < totalRecvCounts[i]; j++) {
            result_buff[i][j] = 1;
        }
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = rtStreamCreate(&stream[i], 5);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        sal_memcpy(&para_info[i].rootInfo, sizeof(HcclRootInfo), &rootInfo, sizeof(HcclRootInfo));
        std::ostringstream identify("");
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].userRankSize = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;


        para_info[i].sendbuff = sendbuff[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuff[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;

        para_info[i].sendCounts = sendCounts[i];
        para_info[i].sdispls = sdispls[i];

        para_info[i].recvCounts = recvCounts[i];
        para_info[i].rdispls = rdispls[i];

        para_info[i].sendCountMatrix = sendCountMatrix;

        para_info[i].sendDataType = sendDataType;
        para_info[i].recvDataType = recvDataType;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_alltoallvc_staged_task1, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < totalRecvCounts[j]; i++)
        {
            s64 res = result_buff[j][i];
            s64 recv = recvbuff[j][i];
            if (res != recv)
            {
                HCCL_ERROR("node:%d result[%d][%d]:%llu recvbuff:%llu\n", noderank, j, i, result_buff[j][i] , recvbuff[j][i]);
                errors++;
                break;
            }
        }
    }

    if (errors)
    {
        HCCL_ERROR("%d errors. Test FAILED.\n", errors);
    }
    else
    {
        HCCL_INFO("Test PASSED.\n");
    }

    SaluSleep(SAL_SECOND_USEC);

    for (s32 j = 0; j < ndev; j++)
    {
        hrtFree(sendbuff[j]);
        hrtFree(recvbuff[j]);
        sal_free(result_buff[j]);

        sal_free(sendCounts[j]);
        sal_free(sdispls[j]);
        sal_free(recvCounts[j]);
        sal_free(rdispls[j]);

        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    sal_free(sendCountMatrix);

    set_board_id(0x0000);
    remove(file_name_t);
    EXPECT_EQ(errors, 0);

    ResetInitState();

    algo = "level0:ring;level1:H-D_R";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}
#endif

#if 1
// graph pairwise pairwise
TEST_F(Inter_AlltoAllVC_Staged_Test, ut_inter_graph_pairwise_pairwise_alltoallv_staged_zcopy_4ranks_1server_int64)
{
    MOCKER_CPP(&TopoInfoParse::IsSingleMeshAggregation)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    HcclResult ret;
    std::string algo = "level0:pairwise;level1:pairwise";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();

    nlohmann::json rank_table = rank_table_910_1server_4rank;
    char file_name_t[] = "./ut_inter_alltoallvc_staged_zcopy_4ranks_1server_int64.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    s32 nnode, rank, errors = 0;
    nnode = 1;
    ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s64* result_buff[DEV_NUM_4];
    s64* sendbuff[DEV_NUM_4];
    s64* recvbuff[DEV_NUM_4];
    u64* sendCounts[DEV_NUM_4];
    u64* sdispls[DEV_NUM_4];
    u64* recvCounts[DEV_NUM_4];
    u64* rdispls[DEV_NUM_4];
    u64* sendCountMatrix = (u64*)sal_malloc(DEV_NUM_4 * DEV_NUM_4 * sizeof(u64));

    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];
    HcclDataType sendDataType = HCCL_DATA_TYPE_INT64;
    HcclDataType recvDataType = HCCL_DATA_TYPE_INT64;

    s32 ndev = DEV_NUM_4;
    set_board_id(0x0000);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 totalRecvCounts[DEV_NUM_4];
    u64 ctotalRecvCounts[DEV_NUM_4];

    u64 count = 1048576;
    for (u32 i = 0; i < ndev; i++ ) {
        for (u32 j = 0; j < ndev; j++) {
            sendCountMatrix[i * ndev + j] = (j + 1) * count;
        }
    }
    for (u32 i = 0; i < ndev; i++)
    {
        u64 totalSendCounts = 0;
        u64 ctotalSendCounts = 0;
        sendCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            sendCounts[i][j] = (j + 1) * count;
            totalSendCounts += sendCounts[i][j];
            ctotalSendCounts += sendCountMatrix[i * ndev + j];
        }

        sdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curSDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            sdispls[i][j] = curSDispl;
            curSDispl += sendCounts[i][j];
        }

        totalRecvCounts[i] = 0;
        ctotalRecvCounts[i] = 0;
        recvCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            recvCounts[i][j] = (1 + i) * count;
            totalRecvCounts[i] += recvCounts[i][j];
            ctotalRecvCounts[i] += sendCountMatrix[i + ndev * j];
        }

        rdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curRDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            rdispls[i][j] = curRDispl;
            curRDispl += recvCounts[i][j];
        }

        ret = hrtMalloc((void**)&sendbuff[i], totalSendCounts * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        for (u32 j = 0; j < totalSendCounts; j++) {
            sendbuff[i][j] = 1;
        }

        ret = hrtMalloc((void**)&recvbuff[i], totalRecvCounts[i] * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuff[i], totalRecvCounts[i] * sizeof(s64), 0, totalRecvCounts[i] * sizeof(s64));

        result_buff[i] = (s64*)sal_malloc(totalRecvCounts[i] * sizeof(s64));
        for (u32 j = 0; j < totalRecvCounts[i]; j++) {
            result_buff[i][j] = 1;
        }
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = rtStreamCreate(&stream[i], 5);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        sal_memcpy(&para_info[i].rootInfo, sizeof(HcclRootInfo), &rootInfo, sizeof(HcclRootInfo));
        std::ostringstream identify("");
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].userRankSize = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;


        para_info[i].sendbuff = sendbuff[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuff[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;

        para_info[i].sendCounts = sendCounts[i];
        para_info[i].sdispls = sdispls[i];

        para_info[i].recvCounts = recvCounts[i];
        para_info[i].rdispls = rdispls[i];

        para_info[i].sendCountMatrix = sendCountMatrix;

        para_info[i].sendDataType = sendDataType;
        para_info[i].recvDataType = recvDataType;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_alltoallv_graph_staged_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < totalRecvCounts[j]; i++)
        {
            s64 res = result_buff[j][i];
            s64 recv = recvbuff[j][i];
            if (res != recv)
            {
                errors++;
                break;
            }
        }
    }

    if (errors)
    {
        HCCL_ERROR("%d errors. Test FAILED.\n", errors);
    }
    else
    {
        HCCL_INFO("Test PASSED.\n");
    }

    SaluSleep(SAL_SECOND_USEC);

    for (s32 j = 0; j < ndev; j++)
    {
        hrtFree(sendbuff[j]);
        hrtFree(recvbuff[j]);
        sal_free(result_buff[j]);

        sal_free(sendCounts[j]);
        sal_free(sdispls[j]);
        sal_free(recvCounts[j]);
        sal_free(rdispls[j]);

        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    sal_free(sendCountMatrix);

    set_board_id(0x0000);
    remove(file_name_t);
    EXPECT_EQ(errors, 0);

    ResetInitState();

    algo = "level0:ring;level1:H-D_R";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}
#endif

#if 1
// alltoallvc NA+pairwise opbase
TEST_F(Inter_AlltoAllVC_Staged_Test, ut_inter_alltoallvc_fullmesh_2ranks_1server_int64)
{
    HcclResult ret;
    std::string algo = "level0:NA;level1:pairwise";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();

    nlohmann::json rank_table = rank_table_910_1server_2rank;
    char file_name_t[] = "./ut_inter_alltoallvc_staged_2ranks_1server_int64.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    s32 nnode, rank, errors = 0;
    nnode = 1;
    ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s64* result_buff[DEV_NUM_2];
    s64* sendbuff[DEV_NUM_2];
    s64* recvbuff[DEV_NUM_2];
    u64* sendCounts[DEV_NUM_2];
    u64* sdispls[DEV_NUM_2];
    u64* recvCounts[DEV_NUM_2];
    u64* rdispls[DEV_NUM_2];
    u64* sendCountMatrix = (u64*)sal_malloc(DEV_NUM_2 * DEV_NUM_2 * sizeof(u64));

    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_2];
    sal_thread_t tid[DEV_NUM_2];
    para_t para_info[DEV_NUM_2];
    HcclDataType sendDataType = HCCL_DATA_TYPE_INT64;
    HcclDataType recvDataType = HCCL_DATA_TYPE_INT64;

    s32 ndev = DEV_NUM_2;
    set_board_id(0x0000);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 totalRecvCounts[DEV_NUM_2];
    u64 ctotalRecvCounts[DEV_NUM_2];

    /* 初始化输入输出缓存 */
    u64 count = 1048576;
    for (u32 i = 0; i < ndev; i++ ) {
        for (u32 j = 0; j < ndev; j++) {
            sendCountMatrix[i * ndev + j] = (j + 1) * count;
        }
    }
    for (u32 i = 0; i < ndev; i++)
    {
        u64 totalSendCounts = 0;
        u64 ctotalSendCounts = 0;
        sendCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            sendCounts[i][j] = (j + 1) * count;
            totalSendCounts += sendCounts[i][j];
            ctotalSendCounts += sendCountMatrix[i * ndev + j];
        }

        sdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curSDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            sdispls[i][j] = curSDispl;
            curSDispl += sendCounts[i][j];
        }

        totalRecvCounts[i] = 0;
        ctotalRecvCounts[i] = 0;
        recvCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            recvCounts[i][j] = (1 + i) * count;
            totalRecvCounts[i] += recvCounts[i][j];
            ctotalRecvCounts[i] += sendCountMatrix[i + ndev * j];
        }

        rdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curRDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            rdispls[i][j] = curRDispl;
            curRDispl += recvCounts[i][j];
        }

        ret = hrtMalloc((void**)&sendbuff[i], totalSendCounts * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        for (u32 j = 0; j < totalSendCounts; j++) {
            sendbuff[i][j] = 1;
        }

        ret = hrtMalloc((void**)&recvbuff[i], totalRecvCounts[i] * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuff[i], totalRecvCounts[i] * sizeof(s64), 0, totalRecvCounts[i] * sizeof(s64));

        result_buff[i] = (s64*)sal_malloc(totalRecvCounts[i] * sizeof(s64));
        for (u32 j = 0; j < totalRecvCounts[i]; j++) {
            result_buff[i][j] = 1;
        }
    }


    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = rtStreamCreate(&stream[i], 5);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        sal_memcpy(&para_info[i].rootInfo, sizeof(HcclRootInfo), &rootInfo, sizeof(HcclRootInfo));
        std::ostringstream identify("");
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].userRankSize = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;


        para_info[i].sendbuff = sendbuff[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuff[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;

        para_info[i].sendCounts = sendCounts[i];
        para_info[i].sdispls = sdispls[i];

        para_info[i].recvCounts = recvCounts[i];
        para_info[i].rdispls = rdispls[i];

        para_info[i].sendCountMatrix = sendCountMatrix;

        para_info[i].sendDataType = sendDataType;
        para_info[i].recvDataType = recvDataType;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_alltoallvc_staged_task1, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    // stream
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < totalRecvCounts[j]; i++)
        {
            s64 res = result_buff[j][i];
            s64 recv = recvbuff[j][i];

            if (res != recv)
            {
                errors++;
                break;
            }
        }
    }

    if (errors)
    {
        HCCL_ERROR("%d errors. Test FAILED.\n", errors);
    }
    else
    {
        HCCL_INFO("Test PASSED.\n");
    }

    for (s32 j = 0; j < ndev; j++)
    {
        hrtFree(sendbuff[j]);
        hrtFree(recvbuff[j]);
        sal_free(result_buff[j]);

        sal_free(sendCounts[j]);
        sal_free(sdispls[j]);
        sal_free(recvCounts[j]);
        sal_free(rdispls[j]);

        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    sal_free(sendCountMatrix);

    set_board_id(0x0000);
    remove(file_name_t);
    EXPECT_EQ(errors, 0);

    algo = "level0:ring;level1:H-D_R";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(Inter_AlltoAllVC_Staged_Test, ut_HcomAlltoAllVCOpKernel_HcomAlltoAllVC)
{
    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"deploy_mode", "lab"},
        {"group_count", "1"},
        {"chip_info", "910"},
        {"board_id", "0x0000"},
        {"para_plane_nic_location", "device"},
        {"para_plane_nic_num", "1"},
        {"para_plane_nic_name", {"eth0"}},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"device_num", "1"},
                    {"server_num", "1"},
                    {"instance_count", "1"},
                        {
                            "instance_list",
                            {
                                {   {"rank_id", "0"}, {"server_id", "172.17.1.120"},
                                    {
                                        "devices", {{{"device_id", "0"}, {"device_ip", "192.168.1.120"}}}
                                    }
                                }
                            }
                        },
                }
            }
        }
    };

    char file_name_t[] = "./ut_HcomAlltoAllVCOpKernel_HcomAlltoAllVC.json";
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

    char *identify = "0";
    CHK_BBIT_RET(HcomInitByFile(file_name_t, identify));
    u32 rankSize = 0;
    CHK_BBIT_RET(HcomGetRankSize(HCCL_WORLD_GROUP, &rankSize));
    u32 rankID = 0;
    CHK_BBIT_RET(HcomGetRankId(HCCL_WORLD_GROUP, &rankID));

    u64 count = 10;
    u64 memSize = rankSize * count * sizeof(int32_t);
    HostMem inputHostMem = HostMem::alloc(memSize);
    CHK_UT_PTR_NULL(inputHostMem.ptr());
    HostMem comparaMem = HostMem::alloc(memSize);
    for (u32 i = 0; i < count * rankSize; i++) {
        *(reinterpret_cast<int32_t *>(inputHostMem.ptr()) + i) = i / count;
        *(reinterpret_cast<int32_t *>(comparaMem.ptr()) + i) = rankID;
    }
    DeviceMem inputDevMem = DeviceMem::alloc(memSize);
    CHK_UT_PTR_NULL(inputDevMem.ptr());
    CHK_BBIT_RET(hrtMemSyncCopy(inputDevMem.ptr(), memSize, inputHostMem.ptr(), memSize, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));

    DeviceMem outputDevMem =  DeviceMem::alloc(memSize);
    CHK_UT_PTR_NULL(outputDevMem.ptr());

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    CHK_UT_PTR_NULL(stream.ptr());

    HCCL_KERNEL_INFO_PRIVATE_DEF privateDefBuf;
    strcpy_s((char *)privateDefBuf.group, 128, "hccl_world_group");
    privateDefBuf.nodeNameHash = 1234567890;
    privateDefBuf.graphId = 1;
    privateDefBuf.destRank = 1;
    privateDefBuf.originalGraphShapeType = 0;
    privateDefBuf.originalGraphShapeType = ORIGINAL_GRAPH_UNKNOWNSHAPE_TYPE;
    privateDefBuf.aivCoreLimit = 48;

    HCCL_ALLTOALLV_KERNEL_INFO_PRIVATE_DEF alltoallvPrivateDefBuf(privateDefBuf);
    alltoallvPrivateDefBuf.cparamsInfo.sendCountMatrix[0] = count;
    alltoallvPrivateDefBuf.cparamsInfo.sendType = HCCL_DATA_TYPE_FP32;
    alltoallvPrivateDefBuf.cparamsInfo.recvType = HCCL_DATA_TYPE_FP32;

    u64 wsMemSize = HCCL_WORKSPACE_MEM_32_KB;
    DeviceMem workSpaceMem = DeviceMem::alloc(wsMemSize);

    ge::GETaskKernelHcclInfo hcclInfo;
    hcclInfo.dataType = HCCL_DATA_TYPE_FP32;
    hcclInfo.hccl_type = HCCL_KERNEL_OP_TYPE_ALLTOALLVC;
    hcclInfo.inputDataAddr = inputDevMem.ptr();
    hcclInfo.outputDataAddr = outputDevMem.ptr();
    hcclInfo.workSpaceAddr = workSpaceMem.ptr();
    hcclInfo.workSpaceMemSize = workSpaceMem.size();
    ge::GETaskInfo task;
    task.kernelHcclInfo.push_back(hcclInfo);
    task.stream = stream.ptr();
    task.streamID = 1;
    task.type = RT_MODEL_TASK_HCCL;
    task.privateDef = reinterpret_cast<void *>(&alltoallvPrivateDefBuf);
    task.privateDefLen = sizeof(HCCL_ALLTOALLV_KERNEL_INFO_PRIVATE_DEF);
    HcomOpsKernelInfoStore infoStore;

    MOCKER(HcomGetInitStatus)
    .stubs()
    .with(outBound(true))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(InitGroup)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtMemAsyncCopy)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    CHK_BBIT_RET(infoStore.LoadTask(task));
    HCCL_INFO("TEST000: hcom alltoallvc issue task");

    CHK_BBIT_RET(hcclStreamSynchronize(stream.ptr()));
    HCCL_INFO("TEST000: execute hcom alltoallvc finished");

    HostMem resultHostMem = HostMem::alloc(memSize);
    CHK_UT_PTR_NULL(resultHostMem.ptr());
    CHK_BBIT_RET(hrtMemSyncCopy(resultHostMem.ptr(), memSize, outputDevMem.ptr(), memSize, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_DEVICE_TO_HOST));
    CHK_BBIT_RET(HcomDestroy());
    remove(file_name_t);
    GlobalMockObject::verify();
}
#endif

#if 1
TEST_F(Inter_AlltoAllVC_Staged_Test, ut_CheckAlltoAllVCExternalMem_test)
{
    hccl::hcclComm* hcclComm = new hccl::hcclComm(0, 0, "ut");
    HcclComm comm = static_cast<void*>(hcclComm);

    MOCKER_CPP(&hcclComm::GetRankSize)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&hcclComm::GetUserRank)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&hcclComm::GetGroupRank)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&hcclComm::GetAlgType)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(HcomCheckOpParam, HcclResult(*)(const char *, const u64, const HcclDataType , const char *,
        const void *))
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(GetWorkflowMode)
        .stubs()
        .will(returnValue(0));

    MOCKER(ProfilerBase::AddTag)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetStreamId)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&RankConsistentcyChecker::RecordOpPara, HcclResult(RankConsistentcyChecker::*)(HcclCMDType,
        const std::string &, u64, HcclDataType, HcclReduceOp, u32, u64, u64, const char *, u32, u8, u32))
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&hcclComm::AlltoAllVC)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(ProfilerBase::DelTag)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(SetDefaultQosConfig)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&hcclComm::SaveTraceInfo)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    
    MOCKER_CPP(&hcclComm::GetRankTableCrc)
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetBlockDim)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    u64 count = 10;
    u32 rankSize = 4;
    u32 rank = 0;
    void *sendBuf = (u64*)sal_malloc(count * sizeof(u64));
    void *recvBuf = (u64*)sal_malloc(count * sizeof(u64));
    void *sendCountMatrix = (u64*)sal_malloc(rankSize * rankSize * sizeof(u64));
    rtStream_t stream;
    rtStreamCreate(&stream, 5);
    HcclDataType sendType = HCCL_DATA_TYPE_INT64;
    HcclDataType recvType = HCCL_DATA_TYPE_INT64;
    HcclResult ret = HcclAlltoAllVC(sendBuf, sendCountMatrix, sendType,
        recvBuf, recvType, comm, stream);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
    delete hcclComm;
    sal_free(sendBuf);
    sal_free(recvBuf);
    sal_free(sendCountMatrix);
    rtStreamDestroy(stream);
}
#endif

#if 1
TEST_F(Inter_AlltoAllVC_Staged_Test, ut_inter_opbase_alltoall_staged_zcopy_4ranks_1server_int64)
{
    MOCKER_CPP(&TopoInfoParse::IsSingleMeshAggregation)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    HcclResult ret;
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();

    nlohmann::json rank_table = rank_table_910_1server_4rank;
    char file_name_t[] = "./ut_inter_alltoall_staged_zcopy_4ranks_1server_int64.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    s32 nnode, rank, errors = 0;
    nnode = 1;
    ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s64* result_buff[DEV_NUM_4];
    s64* sendbuff[DEV_NUM_4];
    s64* recvbuff[DEV_NUM_4];
    u64 sendCount;
    u64 recvCount;

    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_alltoall_t para_info[DEV_NUM_4];
    HcclDataType sendDataType = HCCL_DATA_TYPE_INT64;
    HcclDataType recvDataType = HCCL_DATA_TYPE_INT64;

    s32 ndev = DEV_NUM_4;
    set_board_id(0x0000);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 count = 1048576;
    sendCount = count;
    recvCount = count;
    u64 totalSendCounts = count * DEV_NUM_4;
    u64 totalRecvCounts = count * DEV_NUM_4;
    for (u32 i = 0; i < ndev; i++)
    {
        ret = hrtMalloc((void**)&sendbuff[i], totalSendCounts * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        for (u32 j = 0; j < totalSendCounts; j++) {
            sendbuff[i][j] = 1;
        }

        ret = hrtMalloc((void**)&recvbuff[i], totalRecvCounts * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuff[i], totalRecvCounts * sizeof(s64), 0, totalRecvCounts * sizeof(s64));

        result_buff[i] = (s64*)sal_malloc(totalRecvCounts * sizeof(s64));
        for (u32 j = 0; j < totalRecvCounts; j++) {
            result_buff[i][j] = 1;
        }
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = rtStreamCreate(&stream[i], 5);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        sal_memcpy(&para_info[i].rootInfo, sizeof(HcclRootInfo), &rootInfo, sizeof(HcclRootInfo));
        std::ostringstream identify("");
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].userRankSize = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;


        para_info[i].sendbuff = sendbuff[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuff[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;

        para_info[i].sendCount = sendCount;
        para_info[i].recvCount = recvCount;

        para_info[i].sendDataType = sendDataType;
        para_info[i].recvDataType = recvDataType;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_alltoall_staged_task1, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < totalRecvCounts; i++)
        {
            s64 res = result_buff[j][i];
            s64 recv = recvbuff[j][i];
            if (res != recv)
            {
                HCCL_ERROR("node:%d result[%d][%d]:%llu recvbuff:%llu\n", noderank, j, i, result_buff[j][i] , recvbuff[j][i]);
                errors++;
                break;
            }
        }
    }

    if (errors)
    {
        HCCL_ERROR("%d errors. Test FAILED.\n", errors);
    }
    else
    {
        HCCL_INFO("Test PASSED.\n");
    }

    SaluSleep(SAL_SECOND_USEC);

    for (s32 j = 0; j < ndev; j++)
    {
        hrtFree(sendbuff[j]);
        hrtFree(recvbuff[j]);
        sal_free(result_buff[j]);

        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    set_board_id(0x0000);
    remove(file_name_t);
    EXPECT_EQ(errors, 0);

    ResetInitState();

    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}
#endif

#if 1
// opbase
void *inter_alltoallvc_staged_task1_91093(void *parg)
{
    HcclResult ret = HCCL_SUCCESS;
    para_t *para_info = (para_t *)parg;
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

    hcom_info.pComm.reset(new (std::nothrow) hccl::hcclComm());
    CommConfig commConfig("hccl_world_group");
    rtModel_t model = (void *)1;
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("dev[%d] task all_gather falis", para_info->device_id);
    }

    ret = hcom_info.pComm->CreateCommCCLbuffer();
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("dev[%d] task all_gather falis", para_info->device_id);
    }

    HcclWorkflowMode mode = GetWorkflowMode();
    InitWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);

    //-----------------Set Workspace Resource Start------------------//
    std::vector<rtStream_t> subStreams;
    string tag = "tag-alltoallv-staged-opbase";
    u64 memSize = 0;
    ret = hcom_info.pComm->GetAlltoAllStagedWorkSpaceMemSize(
       static_cast<u64*>(para_info->sendCounts), static_cast<u64*>(para_info->sdispls), para_info->sendDataType,
       static_cast<u64*>(para_info->recvCounts), static_cast<u64*>(para_info->rdispls), para_info->recvDataType,
        memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    HCCL_INFO("==TMP==  workspace[%p] memSize[%llu]", memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource(tag, memptr, memSize, subStreams);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//

    bool swapped;
    rank_num_tmp = *(para_info->sync_addr) - 1;

    do {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1);
    } while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local) {
        sched_yield();
    }  // linux提供一个系统调用运行进程主动让出执行权

    hcom_info.pComm->communicator_->implAlg_->pimpl_->commFactory_->isUsedInterHccsMode_ = true;


    ret = hcom_info.pComm->AlltoAllVC(para_info->sendbuff,
        para_info->sendCountMatrix,
        para_info->sendDataType,
        para_info->recvbuff,
        para_info->recvDataType,
        para_info->stream,
        tag);

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }

    rtError_t rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamSynchronize(para_info->stream);

    if (rt_ret != RT_ERROR_NONE) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }
    //--------------Resource destroy----------------//

    hrtFree(memptr);
    SetWorkflowMode(mode);

    return (nullptr);
}

// alltoallvc NA+pairwise opbase
TEST_F(Inter_AlltoAllVC_Staged_Test, ut_inter_alltoallvc_fullmesh_2ranks_1server_91093)
{
    HcclResult ret;
    std::string algo = "level0:NA;level1:pairwise";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();


    nlohmann::json rank_table = rank_table_910_1server_2rank;
    char file_name_t[] = "./ut_inter_alltoallvc_staged_2ranks_1server_91093.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(2) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    s32 nnode, rank, errors = 0;
    nnode = 1;
    ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s64* result_buff[DEV_NUM_2];
    s64* sendbuff[DEV_NUM_2];
    s64* recvbuff[DEV_NUM_2];
    u64* sendCounts[DEV_NUM_2];
    u64* sdispls[DEV_NUM_2];
    u64* recvCounts[DEV_NUM_2];
    u64* rdispls[DEV_NUM_2];
    u64* sendCountMatrix = (u64*)sal_malloc(DEV_NUM_2 * DEV_NUM_2 * sizeof(u64));

    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_2];
    sal_thread_t tid[DEV_NUM_2];
    para_t para_info[DEV_NUM_2];
    HcclDataType sendDataType = HCCL_DATA_TYPE_INT64;
    HcclDataType recvDataType = HCCL_DATA_TYPE_INT64;

    s32 ndev = DEV_NUM_2;
    set_board_id(0x0000);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 totalRecvCounts[DEV_NUM_2];
    u64 ctotalRecvCounts[DEV_NUM_2];

    /* 初始化输入输出缓存 */
    u64 count = 1048576;
    for (u32 i = 0; i < ndev; i++ ) {
        for (u32 j = 0; j < ndev; j++) {
            sendCountMatrix[i * ndev + j] = (j + 1) * count;
        }
    }
    for (u32 i = 0; i < ndev; i++)
    {
        u64 totalSendCounts = 0;
        u64 ctotalSendCounts = 0;
        sendCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            sendCounts[i][j] = (j + 1) * count;
            totalSendCounts += sendCounts[i][j];
            ctotalSendCounts += sendCountMatrix[i * ndev + j];
        }

        HCCL_INFO("==TMP== rank%u totalSendCounts[%llu], ctotalSendCounts[%llu]", i, totalSendCounts, ctotalSendCounts);

        sdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curSDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            sdispls[i][j] = curSDispl;
            curSDispl += sendCounts[i][j];
        }

        totalRecvCounts[i] = 0;
        ctotalRecvCounts[i] = 0;
        recvCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            recvCounts[i][j] = (1 + i) * count;
            totalRecvCounts[i] += recvCounts[i][j];
            ctotalRecvCounts[i] += sendCountMatrix[i + ndev * j];
        }
        HCCL_INFO("==TMP== rank%u totalRecvCounts[%llu], ctotalRecvCounts[%llu]", i, totalRecvCounts[i], ctotalRecvCounts[i]);

        rdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curRDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            rdispls[i][j] = curRDispl;
            curRDispl += recvCounts[i][j];
        }

        ret = hrtMalloc((void**)&sendbuff[i], totalSendCounts * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        for (u32 j = 0; j < totalSendCounts; j++) {
            sendbuff[i][j] = 1;
        }

        ret = hrtMalloc((void**)&recvbuff[i], totalRecvCounts[i] * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuff[i], totalRecvCounts[i] * sizeof(s64), 0, totalRecvCounts[i] * sizeof(s64));

        result_buff[i] = (s64*)sal_malloc(totalRecvCounts[i] * sizeof(s64));
        for (u32 j = 0; j < totalRecvCounts[i]; j++) {
            result_buff[i][j] = 1;
        }
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = rtStreamCreate(&stream[i], 5);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        sal_memcpy(&para_info[i].rootInfo, sizeof(HcclRootInfo), &rootInfo, sizeof(HcclRootInfo));
        std::ostringstream identify("");
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].userRankSize = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].sendbuff = sendbuff[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuff[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;

        para_info[i].sendCounts = sendCounts[i];
        para_info[i].sdispls = sdispls[i];

        para_info[i].recvCounts = recvCounts[i];
        para_info[i].rdispls = rdispls[i];

        para_info[i].sendCountMatrix = sendCountMatrix;

        para_info[i].sendDataType = sendDataType;
        para_info[i].recvDataType = recvDataType;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_alltoallvc_staged_task1_91093, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    // stream
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < totalRecvCounts[j]; i++)
        {
            s64 res = result_buff[j][i];
            s64 recv = recvbuff[j][i];
        }
    }

    if (errors)
    {
        HCCL_ERROR("%d errors. Test FAILED.\n", errors);
    }
    else
    {
        HCCL_INFO("Test PASSED.\n");
    }

    for (s32 j = 0; j < ndev; j++)
    {
        hrtFree(sendbuff[j]);
        hrtFree(recvbuff[j]);
        sal_free(result_buff[j]);

        sal_free(sendCounts[j]);
        sal_free(sdispls[j]);
        sal_free(recvCounts[j]);
        sal_free(rdispls[j]);

        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    sal_free(sendCountMatrix);

    set_board_id(0x0000);
    remove(file_name_t);
    EXPECT_EQ(errors, 0);

    algo = "level0:ring;level1:H-D_R";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
// graph fullmesh pairwise
TEST_P(Inter_AlltoAllVC_Staged_Test, ut_inter_graph_fullmesh_pairwise_alltoallv_staged_zcopy_4ranks_1server_int64)
{
    MOCKER_CPP(&TopoInfoParse::IsSingleMeshAggregation)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    HcclResult ret;
    std::string algo = "level0:fullmesh;level1:pairwise";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();

    nlohmann::json rank_table = rank_table_910_1server_4rank;
    char file_name_t[] = "./ut_inter_alltoallvc_staged_zcopy_4ranks_1server_int64.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    s32 nnode, rank, errors = 0;
    nnode = 1;
    ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s64* result_buff[DEV_NUM_4];
    s64* sendbuff[DEV_NUM_4];
    s64* recvbuff[DEV_NUM_4];
    u64* sendCounts[DEV_NUM_4];
    u64* sdispls[DEV_NUM_4];
    u64* recvCounts[DEV_NUM_4];
    u64* rdispls[DEV_NUM_4];
    u64* sendCountMatrix = (u64*)sal_malloc(DEV_NUM_4 * DEV_NUM_4 * sizeof(u64));

    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];
    HcclDataType sendDataType = HCCL_DATA_TYPE_INT64;
    HcclDataType recvDataType = HCCL_DATA_TYPE_INT64;

    s32 ndev = DEV_NUM_4;
    set_board_id(0x0000);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 totalRecvCounts[DEV_NUM_4];
    u64 ctotalRecvCounts[DEV_NUM_4];

    u64 count = 1048576;
    for (u32 i = 0; i < ndev; i++ ) {
        for (u32 j = 0; j < ndev; j++) {
            sendCountMatrix[i * ndev + j] = (j + 1) * count;
        }
    }
    for (u32 i = 0; i < ndev; i++)
    {
        u64 totalSendCounts = 0;
        u64 ctotalSendCounts = 0;
        sendCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            sendCounts[i][j] = (j + 1) * count;
            totalSendCounts += sendCounts[i][j];
            ctotalSendCounts += sendCountMatrix[i * ndev + j];
        }

        sdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curSDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            sdispls[i][j] = curSDispl;
            curSDispl += sendCounts[i][j];
        }

        totalRecvCounts[i] = 0;
        ctotalRecvCounts[i] = 0;
        recvCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            recvCounts[i][j] = (1 + i) * count;
            totalRecvCounts[i] += recvCounts[i][j];
            ctotalRecvCounts[i] += sendCountMatrix[i + ndev * j];
        }

        rdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curRDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            rdispls[i][j] = curRDispl;
            curRDispl += recvCounts[i][j];
        }

        ret = hrtMalloc((void**)&sendbuff[i], totalSendCounts * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        for (u32 j = 0; j < totalSendCounts; j++) {
            sendbuff[i][j] = 1;
        }

        ret = hrtMalloc((void**)&recvbuff[i], totalRecvCounts[i] * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuff[i], totalRecvCounts[i] * sizeof(s64), 0, totalRecvCounts[i] * sizeof(s64));

        result_buff[i] = (s64*)sal_malloc(totalRecvCounts[i] * sizeof(s64));
        for (u32 j = 0; j < totalRecvCounts[i]; j++) {
            result_buff[i][j] = 1;
        }
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = rtStreamCreate(&stream[i], 5);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        sal_memcpy(&para_info[i].rootInfo, sizeof(HcclRootInfo), &rootInfo, sizeof(HcclRootInfo));
        std::ostringstream identify("");
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].userRankSize = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;


        para_info[i].sendbuff = sendbuff[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuff[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;

        para_info[i].sendCounts = sendCounts[i];
        para_info[i].sdispls = sdispls[i];

        para_info[i].recvCounts = recvCounts[i];
        para_info[i].rdispls = rdispls[i];

        para_info[i].sendCountMatrix = sendCountMatrix;

        para_info[i].sendDataType = sendDataType;
        para_info[i].recvDataType = recvDataType;
    }

    bool fftsSwitch = GetParam();
    for (s32 i = 0; i < ndev; ++i)
    {
        if (fftsSwitch) {
            tid[i] = sal_thread_create("thread", inter_alltoallv_staged_task_ffts, (void*)&para_info[i]);
        } else {
            tid[i] = sal_thread_create("thread", inter_alltoallv_graph_staged_task, (void*)&para_info[i]);
        }

        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < totalRecvCounts[j]; i++)
        {
            s64 res = result_buff[j][i];
            s64 recv = recvbuff[j][i];
            if (res != recv)
            {
                HCCL_ERROR("node:%d result[%d][%d]:%llu recvbuff:%llu\n", noderank, j, i, result_buff[j][i] , recvbuff[j][i]);
                errors++;
                break;
            }
        }
    }

    if (errors)
    {
        HCCL_ERROR("%d errors. Test FAILED.\n", errors);
    }
    else
    {
        HCCL_INFO("Test PASSED.\n");
    }

    SaluSleep(SAL_SECOND_USEC);

    for (s32 j = 0; j < ndev; j++)
    {
        hrtFree(sendbuff[j]);
        hrtFree(recvbuff[j]);
        sal_free(result_buff[j]);

        sal_free(sendCounts[j]);
        sal_free(sdispls[j]);
        sal_free(recvCounts[j]);
        sal_free(rdispls[j]);

        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    sal_free(sendCountMatrix);

    set_board_id(0x0000);
    remove(file_name_t);
    EXPECT_EQ(errors, 0);

    ResetInitState();

    algo = "level0:ring;level1:H-D_R";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}
#endif

#if 1
// opbase
void *inter_alltoallvc_staged_task2_91093(void *parg)
{
    HcclResult ret = HCCL_SUCCESS;
    para_t *para_info = (para_t *)parg;
    s32 rank_num_tmp;

    hrtSetDevice(para_info->device_id);

    HcomInfo hcom_info;
    std::string ranktable_file = para_info->file_name;
    std::string rankTableM;
    std::string realFilePath;

    hrtSetDevice(para_info->device_id);
    set_chip_type_stub(para_info->device_id, static_cast<s32>(DevType::DEV_TYPE_910_93));

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomLoadRanktableFile(ranktable_file.c_str(), rankTableM, realFilePath);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, para_info->identify, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sal_memcpy(hcom_info.params.id.internal, sizeof(HcclRootInfo), &para_info->rootInfo, sizeof(HcclRootInfo));

    hcom_info.pComm.reset(new (std::nothrow) hccl::hcclComm());
    CommConfig commConfig("hccl_world_group");
    rtModel_t model = (void *)1;
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("dev[%d] task all_gather falis", para_info->device_id);
    }

    ret = hcom_info.pComm->CreateCommCCLbuffer();
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("dev[%d] task all_gather falis", para_info->device_id);
    }

    HcclWorkflowMode mode = GetWorkflowMode();
    InitWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);

    //-----------------Set Workspace Resource Start------------------//
    std::vector<rtStream_t> subStreams;
    string tag = "tag-alltoallv-staged-opbase";
    u64 memSize = 0;
    ret = hcom_info.pComm->GetAlltoAllStagedWorkSpaceMemSize(
       static_cast<u64*>(para_info->sendCounts), static_cast<u64*>(para_info->sdispls), para_info->sendDataType,
       static_cast<u64*>(para_info->recvCounts), static_cast<u64*>(para_info->rdispls), para_info->recvDataType,
        memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    HCCL_INFO("==TMP==  workspace[%p] memSize[%llu]", memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource(tag, memptr, memSize, subStreams);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//

    bool swapped;
    rank_num_tmp = *(para_info->sync_addr) - 1;

    do {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1);
    } while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local) {
        sched_yield();
    }  // linux提供一个系统调用运行进程主动让出执行权

    hcom_info.pComm->communicator_->implAlg_->pimpl_->commFactory_->isUsedInterHccsMode_ = true;


    ret = hcom_info.pComm->AlltoAllVC(para_info->sendbuff,
        para_info->sendCountMatrix,
        para_info->sendDataType,
        para_info->recvbuff,
        para_info->recvDataType,
        para_info->stream,
        tag);

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }

    rtError_t rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamSynchronize(para_info->stream);

    if (rt_ret != RT_ERROR_NONE) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }
    //--------------Resource destroy----------------//

    hrtResetDevice(para_info->device_id);
    set_chip_type_stub(para_info->device_id, static_cast<s32>(DevType::DEV_TYPE_910));

    hrtFree(memptr);
    SetWorkflowMode(mode);

    return (nullptr);
}

// TEST: coll_all_to_all_v_direct_fullmesh_executor
void *inter_alltoallv_directFullmesh_task_91093(void *parg)
{
    HcclResult ret = HCCL_SUCCESS;
    para_t *para_info = (para_t *)parg;
    s32 rank_num_tmp;

    hrtSetDevice(para_info->device_id);

    HcomInfo hcom_info;
    std::string ranktable_file = para_info->file_name;
    std::string rankTableM;
    std::string realFilePath;

    hrtSetDevice(para_info->device_id);
    set_chip_type_stub(para_info->device_id, static_cast<s32>(DevType::DEV_TYPE_910_93));

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomLoadRanktableFile(ranktable_file.c_str(), rankTableM, realFilePath);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, para_info->identify, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sal_memcpy(hcom_info.params.id.internal, sizeof(HcclRootInfo), &para_info->rootInfo, sizeof(HcclRootInfo));

    hcom_info.pComm.reset(new (std::nothrow) hccl::hcclComm());
    CommConfig commConfig("hccl_world_group");
    rtModel_t model = (void *)1;
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("dev[%d] task alltoallv falis", para_info->device_id);
    }

    ret = hcom_info.pComm->CreateCommCCLbuffer();
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("dev[%d] task alltoallv falis", para_info->device_id);
    }

    HcclWorkflowMode mode = GetWorkflowMode();
    InitWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);

    //-----------------Set Workspace Resource Start------------------//
    std::vector<rtStream_t> subStreams;
    string tag = "tag-alltoallv-staged-opbase";
    u64 memSize = 0;
    ret = hcom_info.pComm->GetAlltoAllStagedWorkSpaceMemSize(
       static_cast<u64*>(para_info->sendCounts), static_cast<u64*>(para_info->sdispls), para_info->sendDataType,
       static_cast<u64*>(para_info->recvCounts), static_cast<u64*>(para_info->rdispls), para_info->recvDataType,
        memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    HCCL_INFO("==TMP==  workspace[%p] memSize[%llu]", memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource(tag, memptr, memSize, subStreams);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//

    bool swapped;
    rank_num_tmp = *(para_info->sync_addr) - 1;

    do {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1);
    } while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local) {
        sched_yield();
    }  // linux提供一个系统调用运行进程主动让出执行权

    hcom_info.pComm->communicator_->implAlg_->pimpl_->commFactory_->isUsedInterHccsMode_ = true;

    ret = hcom_info.pComm->AlltoAllV(para_info->sendbuff,
        para_info->sendCounts,
        para_info->sdispls,
        para_info->sendDataType,
        para_info->recvbuff,
        para_info->recvCounts,
        para_info->rdispls,
        para_info->recvDataType,
        para_info->stream,
        tag);

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }

    rtError_t rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamSynchronize(para_info->stream);

    if (rt_ret != RT_ERROR_NONE) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }
    //--------------Resource destroy----------------//

    hrtResetDevice(para_info->device_id);
    set_chip_type_stub(para_info->device_id, static_cast<s32>(DevType::DEV_TYPE_910));

    hrtFree(memptr);
    SetWorkflowMode(mode);

    return (nullptr);
}

// alltoallvc NA+pairwise opbase
TEST_F(Inter_AlltoAllVC_Staged_Test, ut_inter_alltoallvc_fullmesh_2ranks_1server_91093_2)
{
    HcclResult ret;
    std::string algo = "level0:NA;level1:fullmesh";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();

    MOCKER(rtGetDeviceInfo)
    .stubs()
    .with()
    .will(returnValue(RT_ERROR_NONE));

    MOCKER(SatisfyIntraSuperPod)
    .stubs()
    .with()
    .will(returnValue(true));

    nlohmann::json rank_table = rank_table_910_1server_2rank;
    char file_name_t[] = "./ut_inter_alltoallvc_staged_2ranks_1server_91093.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(2) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    s32 nnode, rank, errors = 0;
    nnode = 1;
    ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s64* result_buff[DEV_NUM_2];
    s64* sendbuff[DEV_NUM_2];
    s64* recvbuff[DEV_NUM_2];
    u64* sendCounts[DEV_NUM_2];
    u64* sdispls[DEV_NUM_2];
    u64* recvCounts[DEV_NUM_2];
    u64* rdispls[DEV_NUM_2];
    u64* sendCountMatrix = (u64*)sal_malloc(DEV_NUM_2 * DEV_NUM_2 * sizeof(u64));

    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_2];
    sal_thread_t tid[DEV_NUM_2];
    para_t para_info[DEV_NUM_2];
    HcclDataType sendDataType = HCCL_DATA_TYPE_INT64;
    HcclDataType recvDataType = HCCL_DATA_TYPE_INT64;

    s32 ndev = DEV_NUM_2;
    set_board_id(0x0000);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 totalRecvCounts[DEV_NUM_2];
    u64 ctotalRecvCounts[DEV_NUM_2];

    /* 初始化输入输出缓存 */
    u64 count = 20485760;
    for (u32 i = 0; i < ndev; i++ ) {
        for (u32 j = 0; j < ndev; j++) {
            sendCountMatrix[i * ndev + j] = (j + 1) * count;
        }
    }
    for (u32 i = 0; i < ndev; i++)
    {
        u64 totalSendCounts = 0;
        u64 ctotalSendCounts = 0;
        sendCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            sendCounts[i][j] = (j + 1) * count;
            totalSendCounts += sendCounts[i][j];
            ctotalSendCounts += sendCountMatrix[i * ndev + j];
        }

        sdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curSDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            sdispls[i][j] = curSDispl;
            curSDispl += sendCounts[i][j];
        }

        totalRecvCounts[i] = 0;
        ctotalRecvCounts[i] = 0;
        recvCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            recvCounts[i][j] = (1 + i) * count;
            totalRecvCounts[i] += recvCounts[i][j];
            ctotalRecvCounts[i] += sendCountMatrix[i + ndev * j];
        }

        rdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curRDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            rdispls[i][j] = curRDispl;
            curRDispl += recvCounts[i][j];
        }

        ret = hrtMalloc((void**)&sendbuff[i], totalSendCounts * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        for (u32 j = 0; j < totalSendCounts; j++) {
            sendbuff[i][j] = 1;
        }

        ret = hrtMalloc((void**)&recvbuff[i], totalRecvCounts[i] * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuff[i], totalRecvCounts[i] * sizeof(s64), 0, totalRecvCounts[i] * sizeof(s64));

        result_buff[i] = (s64*)sal_malloc(totalRecvCounts[i] * sizeof(s64));
        for (u32 j = 0; j < totalRecvCounts[i]; j++) {
            result_buff[i][j] = 1;
        }
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = rtStreamCreate(&stream[i], 5);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        sal_memcpy(&para_info[i].rootInfo, sizeof(HcclRootInfo), &rootInfo, sizeof(HcclRootInfo));
        std::ostringstream identify("");
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].userRankSize = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].sendbuff = sendbuff[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuff[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;

        para_info[i].sendCounts = sendCounts[i];
        para_info[i].sdispls = sdispls[i];

        para_info[i].recvCounts = recvCounts[i];
        para_info[i].rdispls = rdispls[i];

        para_info[i].sendCountMatrix = sendCountMatrix;

        para_info[i].sendDataType = sendDataType;
        para_info[i].recvDataType = recvDataType;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_alltoallvc_staged_task2_91093, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    // stream
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < totalRecvCounts[j]; i++)
        {
            s64 res = result_buff[j][i];
            s64 recv = recvbuff[j][i];
        }
    }

    if (errors)
    {
        HCCL_ERROR("%d errors. Test FAILED.\n", errors);
    }
    else
    {
        HCCL_INFO("Test PASSED.\n");
    }

    for (s32 j = 0; j < ndev; j++)
    {
        hrtFree(sendbuff[j]);
        hrtFree(recvbuff[j]);
        sal_free(result_buff[j]);

        sal_free(sendCounts[j]);
        sal_free(sdispls[j]);
        sal_free(recvCounts[j]);
        sal_free(rdispls[j]);

        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    sal_free(sendCountMatrix);

    set_board_id(0x0000);
    remove(file_name_t);
    EXPECT_EQ(errors, 0);

    algo = "level0:ring;level1:H-D_R";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}
#endif

// TEST: coll_all_to_all_v_direct_fullmesh_executor
TEST_F(Inter_AlltoAllVC_Staged_Test, ut_inter_alltoallv_directFullmesh_2ranks_1server_91093)
{
    HcclResult ret;
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();

    MOCKER(rtGetDeviceInfo)
    .stubs()
    .with()
    .will(returnValue(RT_ERROR_NONE));

    MOCKER(IsSupportDirectFullmeshForAlltoallv)
    .stubs()
    .with()
    .will(returnValue(true));

    nlohmann::json rank_table = rank_table_910_1server_2rank;
    char file_name_t[] = "./ut_inter_alltoallvc_staged_2ranks_1server_91093.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open()) {
        outfile << std::setw(2) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    } else {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    s32 nnode, rank, errors = 0;
    nnode = 1;
    ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s64* result_buff[DEV_NUM_2];
    s64* sendbuff[DEV_NUM_2];
    s64* recvbuff[DEV_NUM_2];
    u64* sendCounts[DEV_NUM_2];
    u64* sdispls[DEV_NUM_2];
    u64* recvCounts[DEV_NUM_2];
    u64* rdispls[DEV_NUM_2];
    u64* sendCountMatrix = (u64*)sal_malloc(DEV_NUM_2 * DEV_NUM_2 * sizeof(u64));

    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_2];
    sal_thread_t tid[DEV_NUM_2];
    para_t para_info[DEV_NUM_2];
    HcclDataType sendDataType = HCCL_DATA_TYPE_INT64;
    HcclDataType recvDataType = HCCL_DATA_TYPE_INT64;

    s32 ndev = DEV_NUM_2;
    set_board_id(0x0000);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 totalRecvCounts[DEV_NUM_2];
    u64 ctotalRecvCounts[DEV_NUM_2];

    /* 初始化输入输出缓存 */
    u64 count = 20485760;
    for (u32 i = 0; i < ndev; i++ ) {
        for (u32 j = 0; j < ndev; j++) {
            sendCountMatrix[i * ndev + j] = (j + 1) * count;
        }
    }
    for (u32 i = 0; i < ndev; i++)
    {
        u64 totalSendCounts = 0;
        u64 ctotalSendCounts = 0;
        sendCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            sendCounts[i][j] = (j + 1) * count;
            totalSendCounts += sendCounts[i][j];
            ctotalSendCounts += sendCountMatrix[i * ndev + j];
        }

        HCCL_INFO("==TMP== rank%u totalSendCounts[%llu], ctotalSendCounts[%llu]",
            i, totalSendCounts, ctotalSendCounts);

        sdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curSDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            sdispls[i][j] = curSDispl;
            curSDispl += sendCounts[i][j];
        }

        totalRecvCounts[i] = 0;
        ctotalRecvCounts[i] = 0;
        recvCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            recvCounts[i][j] = (1 + i) * count;
            totalRecvCounts[i] += recvCounts[i][j];
            ctotalRecvCounts[i] += sendCountMatrix[i + ndev * j];
        }
        HCCL_INFO("==TMP== rank%u totalRecvCounts[%llu], ctotalRecvCounts[%llu]",
            i, totalRecvCounts[i], ctotalRecvCounts[i]);

        rdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curRDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            rdispls[i][j] = curRDispl;
            curRDispl += recvCounts[i][j];
        }

        ret = hrtMalloc((void**)&sendbuff[i], totalSendCounts * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        for (u32 j = 0; j < totalSendCounts; j++) {
            sendbuff[i][j] = 1;
        }

        ret = hrtMalloc((void**)&recvbuff[i], totalRecvCounts[i] * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuff[i], totalRecvCounts[i] * sizeof(s64), 0, totalRecvCounts[i] * sizeof(s64));

        result_buff[i] = (s64*)sal_malloc(totalRecvCounts[i] * sizeof(s64));
        for (u32 j = 0; j < totalRecvCounts[i]; j++) {
            result_buff[i][j] = 1;
        }
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = rtStreamCreate(&stream[i], 5);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        sal_memcpy(&para_info[i].rootInfo, sizeof(HcclRootInfo), &rootInfo, sizeof(HcclRootInfo));
        std::ostringstream identify("");
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].userRankSize = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].sendbuff = sendbuff[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuff[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;

        para_info[i].sendCounts = sendCounts[i];
        para_info[i].sdispls = sdispls[i];

        para_info[i].recvCounts = recvCounts[i];
        para_info[i].rdispls = rdispls[i];

        para_info[i].sendCountMatrix = sendCountMatrix;

        para_info[i].sendDataType = sendDataType;
        para_info[i].recvDataType = recvDataType;
    }

    for (s32 i = 0; i < ndev; ++i) {
        tid[i] = sal_thread_create("thread", inter_alltoallv_directFullmesh_task_91093, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i) {
        while ( sal_thread_is_running(tid[i])) {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    // stream
    for (s32 j = 0; j < ndev; j++) {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (u32 j = 0; j < ndev; j++) {
        for (u32 i = 0; i < totalRecvCounts[j]; i++) {
            s64 res = result_buff[j][i];
            s64 recv = recvbuff[j][i];
        }
    }

    if (errors) {
        HCCL_ERROR("%d errors. Test FAILED.\n", errors);
    }
    else {
        HCCL_INFO("Test PASSED.\n");
    }

    for (s32 j = 0; j < ndev; j++) {
        hrtFree(sendbuff[j]);
        hrtFree(recvbuff[j]);
        sal_free(result_buff[j]);

        sal_free(sendCounts[j]);
        sal_free(sdispls[j]);
        sal_free(recvCounts[j]);
        sal_free(rdispls[j]);

        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    sal_free(sendCountMatrix);

    set_board_id(0x0000);
    remove(file_name_t);
    EXPECT_EQ(errors, 0);

    GlobalMockObject::verify();
}