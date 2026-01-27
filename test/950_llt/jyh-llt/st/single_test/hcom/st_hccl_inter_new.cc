#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
//#include <mpi.h>

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

#include "hcom_pub.h"

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "sal.h"
#include "config.h"
#include "dlra_function.h"
#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"
#include "opexecounter_pub.h"

#include <iostream>
#include <fstream>
#include "network_manager_pub.h"
#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include "param_check_pub.h"
#include "llt_hccl_stub.h"
#include "rank_consistentcy_checker.h"
#include "env_config.h"
#include "heartbeat.h"
#include "tbe_vector_reduce.h"

using namespace std;
using namespace hccl;

typedef struct para_struct
{
    HcclRootInfo rootInfo;
    std::string identify;
    s32 comm_num;
    s32 device_id;
    s32 ranks_local;

    char* file_name;
    void* sendbuff;
    void* recvbuff;
    s32 count;
    HcclDataType datatype;
    HcclReduceOp op;
    s32 root;
    rtStream_t stream;
    int id;
    volatile s32* sync_addr;
} para_t;


void* inter_reduce_task(void* parg)
{
    s32 portNum = 7;
    MOCKER(hrtGetHccsPortNum)
        .stubs()
        .with(any(), outBound(portNum))
        .will(returnValue(HCCL_SUCCESS));
    HcclResult ret = HCCL_SUCCESS;
    para_t* para_info = (para_t*)parg;
    s32 rank_num_tmp;

    HcomInfo hcom_info;
    std::string ranktable_file = para_info->file_name;
    std::string rankTableM;
    std::string realFilePath;

    hrtSetDevice(para_info->device_id);
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = NetworkManager::GetInstance(para_info->device_id).Destroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomLoadRanktableFile(ranktable_file.c_str(), rankTableM, realFilePath);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, para_info->identify, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    sal_memcpy(hcom_info.params.id.internal, sizeof(HcclRootInfo), &para_info->rootInfo, sizeof(HcclRootInfo));

    hcom_info.pComm.reset(new(std::nothrow) hccl::hcclComm());
    rtModel_t model = (void*)1;

    hrtSetDevice(para_info->device_id);
    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task rt_set_device falis", hcom_info.params.rank);
    }
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);
    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task reduce falis", para_info->device_id);
    }

    bool swapped;
    //-----------------Get Workspace Resource Start------------------//
    u64 stream_list_size = 0;
    ret = hcom_info.pComm->GetWorkspaceSubStreamNum(stream_list_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("get stream_list_size[%d] success", stream_list_size);
    vector<HcclRtStream> streamList(stream_list_size);

    rtError_t rt_ret;
    //???stream
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtStreamCreateWithFlags(&streamList[i], 0, (RT_STREAM_PERSISTENT | RT_STREAM_FORCE_COPY));
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        //??bind?model
        rt_ret = rtModelBindStream(model, streamList[i], RT_MODEL_WAIT_ACTIVE_STREAM);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    u32 rankSize = 0;
    ret = hcom_info.pComm->GetRankSize(rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 memSize = 0;
    ret = hcom_info.pComm->GetWorkspaceMemSize("HcomReduce", para_info->count, para_info->datatype, rankSize, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource("tag_inter_reduce_task", memptr, memSize, streamList);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Get Workspace Resource End------------------//
    rank_num_tmp = *(para_info->sync_addr) - 1;

    do
    {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1);
    }
    while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local)
    { sched_yield(); } // linux鎻愪緵涓€涓郴缁熻皟鐢ㄨ繍琛岃繘绋嬩富鍔ㄨ鍑烘墽琛屾潈

    __sync_synchronize();  // 鎻掑叆鍐呭瓨灞忛殰锛屽椤哄簭鎬ф湁瑕佹眰锛屼絾鏄湁娌℃湁浣跨敤lock鎸囦护鐨勬椂鍊�

    HCCL_DEBUG("all %d  ranks init ok ,then reduce", hcom_info.params.totalRanks);

    ret = hcom_info.pComm->Reduce("tag_inter_reduce_task", para_info->sendbuff,
                                   para_info->recvbuff,
                                   para_info->count,
                                   para_info->datatype,
                                   para_info->op,
                                   para_info->root,
                                   para_info->stream);

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("rank[%d] task reduce falis", hcom_info.params.rank);
    }

    rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamSynchronize(para_info->stream);

    if ( rt_ret != RT_ERROR_NONE)
    {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtModelUnbindStream(model, streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    for (int i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtStreamDestroy(streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    hrtFree(memptr);
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    return (NULL);
}

void* inter_all_gather_task(void* parg)
{
    s32 portNum = 7;
    MOCKER(hrtGetHccsPortNum)
        .stubs()
        .with(any(), outBound(portNum))
        .will(returnValue(HCCL_SUCCESS));
    HcclResult ret = HCCL_SUCCESS;
    para_t* para_info = (para_t*)parg;
    s32 rank_num_tmp;

    HcomInfo hcom_info;
    std::string ranktable_file = para_info->file_name;
    std::string rankTableM;
    std::string realFilePath;

    hrtSetDevice(para_info->device_id);
    ret = NetworkManager::GetInstance(para_info->device_id).Destroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomLoadRanktableFile(ranktable_file.c_str(), rankTableM, realFilePath);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, para_info->identify, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    sal_memcpy(hcom_info.params.id.internal, sizeof(HcclRootInfo), &para_info->rootInfo, sizeof(HcclRootInfo));

    hcom_info.pComm.reset(new(std::nothrow) hccl::hcclComm(209715200, 209715200));
    rtModel_t model = (void*)1;
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);
    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task all_gather falis", para_info->device_id);
    }
    bool swapped;

    rank_num_tmp = *(para_info->sync_addr) - 1;

    do
    {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1);
    }
    while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local)
    { sched_yield(); } // linux鎻愪緵涓€涓郴缁熻皟鐢ㄨ繍琛岃繘绋嬩富鍔ㄨ鍑烘墽琛屾潈

    __sync_synchronize();  // 鎻掑叆鍐呭瓨灞忛殰锛屽椤哄簭鎬ф湁瑕佹眰锛屼絾鏄湁娌℃湁浣跨敤lock鎸囦护鐨勬椂鍊�

    HCCL_DEBUG("all %d  ranks init ok ,then allgather", hcom_info.params.totalRanks);
    ret = hcom_info.pComm->AllGather("tag_inter_all_gather_task",
                                       para_info->sendbuff,
                                       para_info->recvbuff,
                                       para_info->count,
                                       para_info->datatype,
                                       para_info->stream);

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }

    rtError_t rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamSynchronize(para_info->stream);

    if ( rt_ret != RT_ERROR_NONE)
    {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }

    return (NULL);
}

void* inter_broadcast_task(void* parg)
{
    s32 portNum = -1;
    MOCKER(hrtGetHccsPortNum)
        .stubs()
        .with(any(), outBound(portNum))
        .will(returnValue(HCCL_SUCCESS));
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
    ret = NetworkManager::GetInstance(para_info->device_id).Destroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomLoadRanktableFile(ranktable_file.c_str(), rankTableM, realFilePath);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, para_info->identify, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    sal_memcpy(hcom_info.params.id.internal, sizeof(HcclRootInfo), &para_info->rootInfo, sizeof(HcclRootInfo));

    hcom_info.pComm.reset(new(std::nothrow) hccl::hcclComm());
    rtModel_t model = (void*)1;

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task rt_set_device falis", hcom_info.params.rank);
    }
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);
    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task broadcast falis", para_info->device_id);
    }

    bool swapped;
    //-----------------Get Workspace Resource Start------------------//
    u64 stream_list_size = 0;
    ret = hcom_info.pComm->GetWorkspaceSubStreamNum(stream_list_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("get stream_list_size[%d] success", stream_list_size);
    vector<HcclRtStream> streamList(stream_list_size);

    rtError_t rt_ret;
    //???stream
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtStreamCreateWithFlags(&streamList[i], 0, (RT_STREAM_PERSISTENT | RT_STREAM_FORCE_COPY));
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        //??bind?model
        rt_ret = rtModelBindStream(model, streamList[i], RT_MODEL_WAIT_ACTIVE_STREAM);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    u32 rankSize = 0;
    ret = hcom_info.pComm->GetRankSize(rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 memSize = 0;
    ret = hcom_info.pComm->GetWorkspaceMemSize("HcomBroadcast", para_info->count, para_info->datatype, rankSize, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource("tag_inter_broadcast_task", memptr, memSize, streamList);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Get Workspace Resource End------------------//

    rank_num_tmp = *(para_info->sync_addr) - 1;

    do
    {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1);
    }
    while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local)
    { sched_yield(); } // linux鎻愪緵涓€涓郴缁熻皟鐢ㄨ繍琛岃繘绋嬩富鍔ㄨ鍑烘墽琛屾潈

    __sync_synchronize();  // 鎻掑叆鍐呭瓨灞忛殰锛屽椤哄簭鎬ф湁瑕佹眰锛屼絾鏄湁娌℃湁浣跨敤lock鎸囦护鐨勬椂鍊�

    HCCL_DEBUG("all %d  ranks init ok ,then broadcast", hcom_info.params.totalRanks);
    ret = hcom_info.pComm->Broadcast("tag_inter_broadcast_task",
                                      para_info->sendbuff,
                                      para_info->count,
                                      para_info->datatype,
                                      para_info->root,
                                      para_info->stream);

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("rank[%d] task broadcast falis", hcom_info.params.rank);
    }

    rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamSynchronize(para_info->stream);

    if ( rt_ret != RT_ERROR_NONE)
    {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtModelUnbindStream(model, streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    for (int i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtStreamDestroy(streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    hrtFree(memptr);
    return (NULL);
}

void* inter_all_reduce_task(void* parg)
{
    s32 portNum = 7;

    HcclResult ret = HCCL_SUCCESS;
    para_t* para_info = (para_t*)parg;
    s32 rank_num_tmp;

    HcomInfo hcom_info;
    std::string ranktable_file = para_info->file_name;
    std::string rankTableM;
    std::string realFilePath;
    u32 port = 16666;

    hrtSetDevice(para_info->device_id);
    ret = NetworkManager::GetInstance(para_info->device_id).Destroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = DlTdtFunction::GetInstance().DlTdtFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = NetworkManager::GetInstance(para_info->device_id).Init(NICDeployment::NIC_DEPLOYMENT_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = NetworkManager::GetInstance(para_info->device_id).StartVnic(HcclIpAddress(para_info->device_id), port);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomLoadRanktableFile(ranktable_file.c_str(), rankTableM, realFilePath);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, para_info->identify, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    sal_memcpy(hcom_info.params.id.internal, sizeof(HcclRootInfo), &para_info->rootInfo, sizeof(HcclRootInfo));

    hcom_info.pComm.reset(new(std::nothrow) hccl::hcclComm());
    rtModel_t model = (void*)1;
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);
    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task all_reduce falis", para_info->device_id);
    }

    bool swapped;
    //-----------------Get Workspace Resource Start------------------//
    u64 stream_list_size = 0;
    ret = hcom_info.pComm->GetWorkspaceSubStreamNum(stream_list_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("get stream_list_size[%d] success", stream_list_size);
    vector<HcclRtStream> streamList(stream_list_size);

    rtError_t rt_ret;
    //???stream
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtStreamCreateWithFlags(&streamList[i], 0, (RT_STREAM_PERSISTENT | RT_STREAM_FORCE_COPY));
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        //??bind?model
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

    ret = hcom_info.pComm->SetWorkspaceResource("tag_inter_all_reduce_task", memptr, memSize, streamList);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Get Workspace Resource End------------------//
    rank_num_tmp = *(para_info->sync_addr) - 1;

    do
    {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1);
    }
    while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local)
    { sched_yield(); } // linux鎻愪緵涓€涓郴缁熻皟鐢ㄨ繍琛岃繘绋嬩富鍔ㄨ鍑烘墽琛屾潈

    __sync_synchronize();  // 鎻掑叆鍐呭瓨灞忛殰锛屽椤哄簭鎬ф湁瑕佹眰锛屼絾鏄湁娌℃湁浣跨敤lock鎸囦护鐨勬椂鍊�

    ret =  hcom_info.pComm->AllReduce("tag_inter_all_reduce_task",
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

    rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamSynchronize(para_info->stream);

    if ( rt_ret != RT_ERROR_NONE)
    {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtModelUnbindStream(model, streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    for (int i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtStreamDestroy(streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    hrtFree(memptr);

    ret = NetworkManager::GetInstance(para_info->device_id).StopVnic();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = NetworkManager::GetInstance(para_info->device_id).DeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    return (NULL);
}
void* inter_reduce_scatter_task(void* parg)
{
    s32 portNum = 7;
    MOCKER(hrtGetHccsPortNum)
        .stubs()
        .with(any(), outBound(portNum))
        .will(returnValue(HCCL_SUCCESS));
    HcclResult ret = HCCL_SUCCESS;
    para_t* para_info = (para_t*)parg;
    s32 rank_num_tmp;

    HcomInfo hcom_info;
    std::string ranktable_file = para_info->file_name;
    std::string rankTableM;
    std::string realFilePath;
    u32 port = 16666;

    hrtSetDevice(para_info->device_id);
    
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = NetworkManager::GetInstance(para_info->device_id).Init(NICDeployment::NIC_DEPLOYMENT_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = NetworkManager::GetInstance(para_info->device_id).StartVnic(HcclIpAddress(para_info->device_id), port);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomLoadRanktableFile(ranktable_file.c_str(), rankTableM, realFilePath);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, para_info->identify, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    sal_memcpy(hcom_info.params.id.internal, sizeof(HcclRootInfo), &para_info->rootInfo, sizeof(HcclRootInfo));

    hcom_info.pComm.reset(new(std::nothrow) hccl::hcclComm());
    rtModel_t model = (void*)1;
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);
    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task reduce_scatter falis", para_info->device_id);
    }
    //-----------------Get Workspace Resource Start------------------//
    u64 stream_list_size = 0;
    ret = hcom_info.pComm->GetWorkspaceSubStreamNum(stream_list_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("get stream_list_size[%d] success", stream_list_size);
    rtError_t rt_ret;
    vector<HcclRtStream> streamList(stream_list_size);
    u32 rankSize = 0;
    u64 memSize = 0;
    void *memptr = nullptr;
    if (true) {
        vector<HcclRtStream> streamList(stream_list_size);
        //???stream
        for (s32 i = 0; i < stream_list_size; i++)
        {
            rt_ret = rtStreamCreateWithFlags(&streamList[i], 0, (RT_STREAM_PERSISTENT | RT_STREAM_FORCE_COPY));
            EXPECT_EQ(rt_ret, RT_ERROR_NONE);
            //??bind?model
            rt_ret = rtModelBindStream(model, streamList[i], RT_MODEL_WAIT_ACTIVE_STREAM);
            EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        }

        ret = hcom_info.pComm->GetRankSize(rankSize);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = hcom_info.pComm->GetWorkspaceMemSize(HCCL_KERNEL_OP_TYPE_REDUCESCATTER, para_info->count, para_info->datatype, rankSize, memSize);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        
        ret = hrtMalloc(&memptr, memSize);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("HCCL TEST T3");
        ret = hcom_info.pComm->SetWorkspaceResource("tag_inter_reduce_scatter_task", memptr, memSize, streamList);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
    //-----------------Get Workspace Resource End------------------//
    bool swapped;

    rank_num_tmp = *(para_info->sync_addr) - 1;

    do
    {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1);
    }
    while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local)
    { sched_yield(); } // linux鎻愪緵涓€涓郴缁熻皟鐢ㄨ繍琛岃繘绋嬩富鍔ㄨ鍑烘墽琛屾潈

    __sync_synchronize();  // 鎻掑叆鍐呭瓨灞忛殰锛屽椤哄簭鎬ф湁瑕佹眰锛屼絾鏄湁娌℃湁浣跨敤lock鎸囦护鐨勬椂锟�?

    ret =  hcom_info.pComm->ReduceScatter("tag_inter_reduce_scatter_task",
                               para_info->sendbuff,
                               para_info->recvbuff,
                               para_info->count,
                               para_info->datatype,
                               para_info->op,
                               para_info->stream);

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task reduce_scatter falis", para_info->device_id);
    }

    rt_ret = rtStreamSynchronize(para_info->stream);

    if ( rt_ret != RT_ERROR_NONE)
    {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }
    if (stream_list_size > 0) {
        for (s32 i = 0; i < stream_list_size; i++)
        {
            rt_ret = rtModelUnbindStream(model, streamList[i]);
            EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        }
        for (int i = 0; i < stream_list_size; i++)
        {
            rt_ret = rtStreamDestroy(streamList[i]);
            EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        }
        hrtFree(memptr);
    }    
    ret = NetworkManager::GetInstance(para_info->device_id).StopVnic();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = NetworkManager::GetInstance(para_info->device_id).DeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    return (NULL);
}

class HcclInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "HcclInterTest SetUP" << std::endl;
        setenv("HCCL_DFS_CONFIG", "connection_fault_detection_time:0", 1);
        InitEnvParam();
    }
    static void TearDownTestCase()
    {
        std::cout << "HcclInterTest TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        GlobalMockObject::verify();
        static s32  call_cnt = 0;
        DlTdtFunction::GetInstance().DlTdtFunctionInit();
        TsdOpen(1, 2);
        std::cout << "tsd open" << std::endl;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
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
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        TsdClose(1);
        GlobalMockObject::verify();
        std::cout << "tsd close" << std::endl;
        std::cout << "A Test TearDown" << std::endl;
    }
};


#define HCC_REDUCE_DATA_SIZE 10
#define HCC_REDUCE_DATA_SIZE_1024 1024
#define HCC_REDUCE_DATA_SIZE_2M (1024*1024*2)

#define DEV_NUM_4 4
#define DEV_NUM_2 2
#define DEV_NUM_8 8
#if 0
TEST_F(HcclInterTest, st_reduce_inter_sum_char)
{
    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"deploy_mode", "lab"},
        {"group_count", "1"},
        {"chip_info", "910"},
        {"board_id", "0x0000"},
        {"para_plane_nic_location", "device"},
        {"para_plane_nic_num", "4"},
        {"para_plane_nic_name", {"eth0", "eth1","eth2", "eth3"}},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"device_num", "4"},
                    {"server_num", "1"},
                    {"instance_count", "4"},
                        {
                            "instance_list",
                            {
                                {   {"rank_id", "0"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "0"}, {"device_ip", "192.168.0.12"}}}
                                    }
                                },

                                {   {"rank_id", "1"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "1"}, {"device_ip", "192.168.0.14"}}}
                                    }
                                },
                                {   {"rank_id", "2"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "2"}, {"device_ip", "192.168.0.16"}}}
                                    }
                                },

                                {   {"rank_id", "3"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "3"}, {"device_ip", "192.168.0.18"}}}
                                    }
                                },

                            }
                        },
                        {
                            "server_list",
                            {
                                {
                                    {"server_id", "192.168.10.2"},
                                    {
                                        "para_plane_info",
                                        {{
                                                {"eth1", "192.168.210.2"},
                                            },
                                            {
                                                {"eth0", "192.168.200.2"},
                                            }
                                        }
                                    }

                                },
                            }
                        }
                }
            }
        }
    };

    char file_name_t[] = "./st_reduce_inter_sum_char.json";
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

    s32 rank, errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s8* result_buff[DEV_NUM_4];
    s8* sendbuf[DEV_NUM_4];
    s8* recvbuf[DEV_NUM_4];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;

    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = HCC_REDUCE_DATA_SIZE;
    s32 ndev = DEV_NUM_4;

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(s8), 0, count * sizeof(s8));
        ret = hrtMalloc((void **)&recvbuf[i] , count * sizeof(s8));
        sal_memset(recvbuf[i], count * sizeof(s8), 0, count * sizeof(s8));
        ret = hrtMalloc((void **)&result_buff[i] ,count * sizeof(s8));
        sal_memset(result_buff[i], count * sizeof(s8), 0, count * sizeof(s8));
    }

    //sendbuf 赋值
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[j][i] = 1;
        }
    }

    //resultbuf 赋值

    for (u32 j = 0; j < count; j++)
     {
            result_buff[0][j] = ndev;
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuf[i];
        para_info[i].op = op;
        para_info[i].root = 0;
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_reduce_task, (void*)&para_info[i]);
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

     for (s32 i = 0; i < count; i++)
    {
            s32 res = result_buff[0][i];
            s32 recv = recvbuf[0][i];

            if (res != recv)
            {
                HCCL_ERROR(" root recvbuf[%d] result_buff[%d] \n", recv, res);
                errors ++;
                break;
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
    for (s32 i = 0; i < ndev; i++)
   {
     hrtFree(sendbuf[i]);
    hrtFree(recvbuf[i]);
    hrtFree(result_buff[i]);
    rt_ret = rtStreamDestroy(stream[i]);

    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    ret = NetworkManager::GetInstance(i).Destroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
   }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}
#endif

#if 1
TEST_F(HcclInterTest, st_reduce_inter_sum_char_root3)
{
    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"deploy_mode", "lab"},
        {"group_count", "1"},
        {"chip_info", "910"},
        {"board_id", "0x0000"},
        {"para_plane_nic_location", "device"},
        {"para_plane_nic_num", "8"},
        {"para_plane_nic_name", {"eth0", "eth1","eth2", "eth3","eth4", "eth5","eth6", "eth7"}},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"device_num", "8"},
                    {"server_num", "1"}, 
                    {"instance_count", "8"},
                        {
                            "instance_list",
                            {
                                {   {"rank_id", "0"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "0"}, {"device_ip", "192.168.0.12"}}}
                                    }
                                },

                                {   {"rank_id", "1"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "1"}, {"device_ip", "192.168.0.14"}}}
                                    }
                                },
                                {   {"rank_id", "2"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "2"}, {"device_ip", "192.168.0.16"}}}
                                    }
                                },

                                {   {"rank_id", "3"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "3"}, {"device_ip", "192.168.0.18"}}}
                                    }
                                },
                                {   {"rank_id", "4"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "4"}, {"device_ip", "192.168.0.20"}}}
                                    }
                                },
                                {   {"rank_id", "5"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "5"}, {"device_ip", "192.168.0.22"}}}
                                    }
                                },
                                {   {"rank_id", "6"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "6"}, {"device_ip", "192.168.0.24"}}}
                                    }
                                },
                                 {   {"rank_id", "7"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "7"}, {"device_ip", "192.168.0.26"}}}
                                    }
                                },                                                                                               
                            }
                        },
                        {
                            "server_list",
                            {
                                {
                                    {"server_id", "192.168.10.2"},
                                    {
                                        "para_plane_info",
                                        {{
                                                {"eth1", "192.168.210.2"},
                                            },
                                            {
                                                {"eth0", "192.168.200.2"},
                                            }
                                        }
                                    }

                                },
                            }
                        }
                }
            }
        }
    };

    char file_name_t[] = "./st_reduce_inter_sum_char_root3.json";
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

    s32 rank, errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s8* result_buff[DEV_NUM_8];
    s8* sendbuf[DEV_NUM_8];
    s8* recvbuf[DEV_NUM_8];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;

    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = HCC_REDUCE_DATA_SIZE_1024;
    s32 ndev = DEV_NUM_8;

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);

        sal_memset(sendbuf[i], count * sizeof(s8), 0, count * sizeof(s8));

        ret = hrtMalloc((void **)&recvbuf[i] , count * sizeof(s8));
        sal_memset(recvbuf[i], count * sizeof(s8), 0, count * sizeof(s8));

        ret = hrtMalloc((void **)&result_buff[i] ,count * sizeof(s8));
        sal_memset(result_buff[i], count * sizeof(s8), 0, count * sizeof(s8));
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[j][i] = 1;
        }
    }
    for (u32 j = 0; j < count; j++)
     {
            result_buff[3][j] = ndev;
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuf[i];
        para_info[i].op = op;
        para_info[i].root = 3;
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_reduce_task, (void*)&para_info[i]);
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
 for (s32 i = 0; i < count; i++) {

        s32 recv = recvbuf[3][i];

            if (recv != 8)
            {
                HCCL_ERROR(" root recv[%d]:%d \n",i, recv);
                //break;
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
    for (s32 i = 0; i < ndev; i++)
   {
     hrtFree(sendbuf[i]);
    hrtFree(recvbuf[i]);
    hrtFree(result_buff[i]);
    rt_ret = rtStreamDestroy(stream[i]);

    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
   }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}
#endif

#if 1
TEST_F(HcclInterTest, st_reduce_inter_sum_float_slice)
{

    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"deploy_mode", "lab"},
        {"group_count", "1"},
        {"chip_info", "910"},
        {"board_id", "0x0000"},
        {"para_plane_nic_location", "device"},
        {"para_plane_nic_num", "8"},
        {"para_plane_nic_name", {"eth0", "eth1","eth2", "eth3","eth4", "eth5","eth6", "eth7"}},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"device_num", "8"},
                    {"server_num", "1"},
                    {"instance_count", "8"},
                        {
                            "instance_list",
                            {
                                {   {"rank_id", "0"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "0"}, {"device_ip", "192.168.0.12"}}}
                                    }
                                },

                                {   {"rank_id", "1"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "1"}, {"device_ip", "192.168.0.14"}}}
                                    }
                                },
                                {   {"rank_id", "2"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "2"}, {"device_ip", "192.168.0.16"}}}
                                    }
                                },

                                {   {"rank_id", "3"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "3"}, {"device_ip", "192.168.0.18"}}}
                                    }
                                },
                                {   {"rank_id", "4"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "4"}, {"device_ip", "192.168.0.20"}}}
                                    }
                                },
                                {   {"rank_id", "5"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "5"}, {"device_ip", "192.168.0.22"}}}
                                    }
                                },
                                {   {"rank_id", "6"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "6"}, {"device_ip", "192.168.0.24"}}}
                                    }
                                },
                                 {   {"rank_id", "7"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "7"}, {"device_ip", "192.168.0.26"}}}
                                    }
                                },                                                                                               
                            }
                        },
                        {
                            "server_list",
                            {
                                {
                                    {"server_id", "192.168.10.2"},
                                    {
                                        "para_plane_info",
                                        {{
                                                {"eth1", "192.168.210.2"},
                                            },
                                            {
                                                {"eth0", "192.168.200.2"},
                                            }
                                        }
                                    }

                                },
                            }
                        }
                }
            }
        }
    };

    char file_name_t[] = "./st_reduce_inter_sum_float_slice.json";
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

    s32 rank, errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    float* result_buff[DEV_NUM_8];
    float* sendbuf[DEV_NUM_8];
    float* recvbuf[DEV_NUM_8];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];

    HcclDataType datatype = HCCL_DATA_TYPE_FP32;

    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = 128;
    s32 ndev = DEV_NUM_8;

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(float), 0, count * sizeof(float));
         ret = hrtMalloc((void **)&recvbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count * sizeof(float), 0, count * sizeof(float));
        ret = hrtMalloc((void **)&result_buff[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(result_buff[i], count * sizeof(s32), 0, count * sizeof(float));
    }

    //sendbuf 赋值
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[j][i] = 1.0;
        }
    }

    //resultbuf 赋值

    for (u32 j = 0; j < count; j++)
    {
        result_buff[7][j] = 8.0;
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuf[i];
        para_info[i].op = op;
        para_info[i].root = 7;
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_reduce_task, (void*)&para_info[i]);
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
    for (s32 i = 0; i < count; i++)
    {
        s32 res = result_buff[7][i];
        s32 recv = recvbuf[7][i];

        if ( abs(res - recv) > 1e-6 )
        {
            HCCL_ERROR(" recvbuf[%f] result_buff[%f] \n", recv, res);
            errors ++;
            break;
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

    for (s32 i = 0; i < ndev; i++)
    {
        rt_ret = rtStreamDestroy(stream[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    for (s32 i = 0; i < ndev; i++)
    {
        hrtFree(sendbuf[i]);
        hrtFree(recvbuf[i]);
        hrtFree(result_buff[i]);
    }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}
#endif

#define HCCL_ALLGATHER_DATA_SIZE 10
#define HCC_ALLGATHER_SIZE_2M (1024*1024*2+3)
TEST_F(HcclInterTest, st_allgather_inter_char)
{
    // ranktable 的读取，直接使用进程
    nlohmann::json rank_table = rank_table_910_1server_2rank;
    char file_name_t[] = "./st_allgather_inter_char.json";
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

    s32 errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s8* result_buff[DEV_NUM_2];
    s8* sendbuf[DEV_NUM_2];
    s8* recvbuf[DEV_NUM_2];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_2];
    sal_thread_t tid[DEV_NUM_2];
    para_t para_info[DEV_NUM_2];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;

    s32 count = HCCL_ALLGATHER_DATA_SIZE;
    s32 ndev = DEV_NUM_2;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(s8), 0, count * sizeof(s8));

        ret = hrtMalloc((void **)&recvbuf[i], ndev * count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], ndev * count * sizeof(s8), 0, ndev  * count * sizeof(s8));

        result_buff[i] = (s8*)sal_malloc(ndev * count * sizeof(s8));
        sal_memset(result_buff[i], ndev  * count * sizeof(s8), 0, ndev * count * sizeof(s8));
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[j][i] = 1;
        }
    }

    //resultbuf 赋值
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < ndev * count; j++)
        {
            result_buff[i][j] = 1 ;
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuf[i];

        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_all_gather_task, (void*)&para_info[i]);

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


    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count * ndev; i++)
        {
            s8 res = result_buff[j][i];
            s8 recv = recvbuf[j][i];

            if (res != recv)
            {
                HCCL_ERROR("recvbuf[%d][%d]:%d \n", j, i, recv);
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
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        sal_free(result_buff[j]);
        rt_ret = rtStreamDestroy(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}

TEST_F(HcclInterTest, st_allgather_inter_char_common_pid)
{
    // ranktable 的读取，直接使用进程
    nlohmann::json rank_table = rank_table_910_1server_2rank;

    rtSetCommonPidMode(true);
    char file_name_t[] = "./ut_allgather_inter_char.json";
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

    s32 errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s8* result_buff[DEV_NUM_2];
    s8* sendbuf[DEV_NUM_2];
    s8* recvbuf[DEV_NUM_2];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_2];
    sal_thread_t tid[DEV_NUM_2];
    para_t para_info[DEV_NUM_2];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;

    s32 count = HCCL_ALLGATHER_DATA_SIZE;
    s32 ndev = DEV_NUM_2;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(s8), 0, count * sizeof(s8));

        ret = hrtMalloc((void **)&recvbuf[i], ndev * count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], ndev * count * sizeof(s8), 0, ndev  * count * sizeof(s8));

        result_buff[i] = (s8*)sal_malloc(ndev * count * sizeof(s8));
        sal_memset(result_buff[i], ndev  * count * sizeof(s8), 0, ndev * count * sizeof(s8));
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[j][i] = 1;
        }
    }

    //resultbuf 赋值
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < ndev * count; j++)
        {
            result_buff[i][j] = 1 ;
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuf[i];

        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_all_gather_task, (void*)&para_info[i]);

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


    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count * ndev; i++)
        {
            s8 res = result_buff[j][i];
            s8 recv = recvbuf[j][i];

            if (res != recv)
            {
                HCCL_ERROR("recvbuf[%d][%d]:%d \n", j, i, recv);
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
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        sal_free(result_buff[j]);
        rt_ret = rtStreamDestroy(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    rtSetCommonPidMode(false);
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}

TEST_F(HcclInterTest, st_allgather_inter_float_slice)
{
    // ranktable 的读取，直接使用进程
    nlohmann::json rank_table = rank_table_910_1server_2rank;
    char file_name_t[] = "./st_allgather_inter_float_slice.json";
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

    s32 errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s32* result_buff[DEV_NUM_2];
    s32* sendbuf[DEV_NUM_2];
    s32* recvbuf[DEV_NUM_2];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_2];
    sal_thread_t tid[DEV_NUM_2];
    para_t para_info[DEV_NUM_2];

    HcclDataType datatype = HCCL_DATA_TYPE_FP32;

    s32 count = HCC_ALLGATHER_SIZE_2M;
    s32 ndev = DEV_NUM_2;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(s32));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count* sizeof(s32), 0, count* sizeof(s32));

        ret = hrtMalloc((void **)&recvbuf[i], ndev * count * sizeof(s32));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], ndev * count* sizeof(s32), 0, ndev  * count* sizeof(s32));

        result_buff[i] = (s32*)sal_malloc(ndev * count * sizeof(s32));
        sal_memset(result_buff[i], ndev  * count * sizeof(s32), 0, ndev * count* sizeof(s32));
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[j][i] = 1.0;
        }
    }

    //resultbuf 赋值
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < ndev * count; j++)
        {
            result_buff[i][j] = 1.0 ;
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuf[i];

        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_all_gather_task, (void*)&para_info[i]);

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


    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count * ndev; i++)
        {
            s32 res = result_buff[j][i];
            s32 recv = recvbuf[j][i];

           if ( abs(res-recv) >1e-6 )
            {
                HCCL_ERROR("recvbuf[%d][%d]:%d \n", j, i, recv);
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
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        sal_free(result_buff[j]);
        rt_ret = rtStreamDestroy(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}
#define HCCL_BROADCAST_DATA_SIZE 10
#define HCCL_BROADCAST_DATA_SLICE 1024
#if 1
TEST_F(HcclInterTest, st_broadcast_inter_char_4pmesh_root0_1000)
{
    // ranktable 的读取，直接使用进程
    nlohmann::json rank_table = rank_table_910_1server_4rank;
    char file_name_t[] = "./st_broadcast_inter_char_4pmesh_root0_1000.json";
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

    s32 errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s8* sendbuf[DEV_NUM_4];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    s32 count = 1000;
    s32 ndev = DEV_NUM_4;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /** 初始化输入输出缓存 */
    DeviceMem mem;
    for (s32 i = 0; i < ndev; i++ )
    {
        mem = DeviceMem::alloc(count * sizeof(s8));
        sendbuf[i] = (s8 *)mem.ptr();
        for (u32 j = 0; j < count; j++)
        {
            sendbuf[i][j] = 0;
        }
    }

        for (u32 i = 0; i < count; i++)
        {
            sendbuf[0][i] = 1;
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];

        para_info[i].root = 0;
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_broadcast_task, (void*)&para_info[i]);

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

    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count; i++)
        {
            s8 recv = sendbuf[j][i];

            if (sendbuf[j][i] != 1)
            {
                HCCL_ERROR("recvbuf[%d][%d]:%d \n", j, i, sendbuf[j][i]);
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
        rt_ret = rtStreamDestroy(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}
#endif
#if 1
TEST_F(HcclInterTest, st_broadcast_inter_char_4pmesh_root0_60)
{
    // ranktable 的读取，直接使用进程
    nlohmann::json rank_table = rank_table_910_1server_4rank;
    char file_name_t[] = "./st_broadcast_inter_char_4pmesh_root0_60.json";
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

    s32 errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s8* sendbuf[DEV_NUM_4];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    s32 count = 60;
    s32 ndev = DEV_NUM_4;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /** 初始化输入输出缓存 */
    DeviceMem mem;
    for (s32 i = 0; i < ndev; i++ )
    {
        mem = DeviceMem::alloc(count * sizeof(s8));
        sendbuf[i] = (s8 *)mem.ptr();
        for (u32 j = 0; j < count; j++)
        {
            sendbuf[i][j] = 0;
        }
    }

        for (u32 i = 0; i < count; i++)
        {
            sendbuf[0][i] = 1;
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];

        para_info[i].root = 0;
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_broadcast_task, (void*)&para_info[i]);

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

    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count; i++)
        {
            s8 recv = sendbuf[j][i];

            if (sendbuf[j][i] != 1)
            {
                HCCL_ERROR("recvbuf[%d][%d]:%d \n", j, i, sendbuf[j][i]);
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
        rt_ret = rtStreamDestroy(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}

TEST_F(HcclInterTest, st_broadcast_inter_char_4pmesh_root_not0_40)
{
    nlohmann::json rank_table = rank_table_910_1server_4rank;
    
    char file_name_t[] = "./st_broadcast_inter_char_4pmesh_root_not0_40.json";
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
    s32 errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s8* sendbuf[DEV_NUM_4];
    s32 sync_value = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];
    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    s32 count = 40;
    s32 ndev = DEV_NUM_4;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem mem;
    for (s32 i = 0; i < ndev; i++ )
    {
        mem = DeviceMem::alloc(count * sizeof(s8));
        sendbuf[i] = (s8 *)mem.ptr();
        for (u32 j = 0; j < count; j++)
        {
            sendbuf[i][j] = 0;
        }
    }
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[1][i] = 1;
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].root = 1;
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_broadcast_task, (void*)&para_info[i]);
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
        for (s32 i = 0; i < count; i++)
        {
            s8 recv = sendbuf[j][i];
            if (sendbuf[j][i] != 1)
            {
                HCCL_ERROR("recvbuf[%d][%d]:%d \n", j, i, sendbuf[j][i]);
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
        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
     remove(file_name_t);
    EXPECT_EQ(errors, 0);
}
TEST_F(HcclInterTest, st_broadcast_inter_char_4pmesh_root_not0_515)
{
    nlohmann::json rank_table = rank_table_910_1server_4rank;
   
    char file_name_t[] = "./st_broadcast_inter_char_4pmesh_root_not0_515.json";
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
    s32 errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s8* sendbuf[DEV_NUM_4];
    s32 sync_value = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];
    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    s32 count = 515;
    s32 ndev = DEV_NUM_4;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem mem;
    for (s32 i = 0; i < ndev; i++ )
    {
        mem = DeviceMem::alloc(count * sizeof(s8));
        sendbuf[i] = (s8 *)mem.ptr();
        for (u32 j = 0; j < count; j++)
        {
            sendbuf[i][j] = 0;
        }
    }
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[1][i] = 1;
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].root = 1;
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_broadcast_task, (void*)&para_info[i]);
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
        for (s32 i = 0; i < count; i++)
        {
            s8 recv = sendbuf[j][i];
            if (sendbuf[j][i] != 1)
            {
                HCCL_ERROR("recvbuf[%d][%d]:%d \n", j, i, sendbuf[j][i]);
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
        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
      remove(file_name_t);
    EXPECT_EQ(errors, 0);
}
#endif
#if 0
TEST_F(HcclInterTest, st_broadcast_inter_char_4pring_root0_1000)
{

#if 0
    nlohmann::json rank_table = rank_table_910_1server_4rank;
    
    char file_name_t[] = "./st_broadcast_inter_char_4pring_root0_1000.json";
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
#endif
    s32 errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s8* sendbuf[DEV_NUM_4];
    s32 sync_value = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];
    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    s32 count = 1000;
    s32 ndev = DEV_NUM_4;
    set_board_id(0x0000);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(s8));
        sal_memset(sendbuf[i], count, 0, count);
    }
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[0][i] = 1;
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].root = 0;
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_broadcast_task, (void*)&para_info[i]);
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
        for (s32 i = 0; i < count; i++)
        {
            s8 recv = sendbuf[j][i];
            if (sendbuf[j][i] != 1)
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
        hrtFree(sendbuf[j]);
        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0);
     remove(file_name_t);
    EXPECT_EQ(errors, 0);
}
#endif

#if 0
TEST_F(HcclInterTest, st_broadcast_inter_char_4pring_root2_40)
{
    nlohmann::json rank_table = rank_table_910_1server_4rank;
    char file_name_t[] = "./st_broadcast_inter_char_4pring_root2_40.json";
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
    s32 errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s8* sendbuf[DEV_NUM_4];
    s32 sync_value = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];
    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    s32 count = 40;
    s32 ndev = DEV_NUM_4;
    set_board_id(0x0000);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(s8));
        sal_memset(sendbuf[i], count, 0, count);
    }
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[2][i] = 1;
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].root = 2;
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_broadcast_task, (void*)&para_info[i]);
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
        for (s32 i = 0; i < count; i++)
        {
            s8 recv = sendbuf[j][i];
            if (sendbuf[j][i] != 1)
            {
                HCCL_ERROR("recvbuf[%d][%d]:%d \n", j, i, sendbuf[j][i]);
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
        hrtFree(sendbuf[j]);
        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    set_board_id(0);
    EXPECT_EQ(errors, 0);
}
#endif

#if 1
TEST_F(HcclInterTest, st_broadcast_inter_char_8pring_root0_4096)
{
    nlohmann::json rank_table = rank_table_1server_8rank;
   
    char file_name_t[] = "./st_broadcast_inter_char_8pring_root0_4096.json";
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
    s32 errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s8* sendbuf[8];
    s32 sync_value = 0;
    rtStream_t stream[8];
    sal_thread_t tid[8];
    para_t para_info[8];
    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    s32 count = 4096;
    s32 ndev = 8;
    set_board_id(0x0000);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(s8));
        sal_memset(sendbuf[i], count, 0, count);
    }
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[0][i] = 1;
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].root = 0;
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }
    
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_broadcast_task, (void*)&para_info[i]);
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
        for (s32 i = 0; i < count; i++)
        {
            s8 recv = sendbuf[j][i];
            if (sendbuf[j][i] != 1)
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
        hrtFree(sendbuf[j]);
        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    set_board_id(0);
    EXPECT_EQ(errors, 0);
}
#endif

#if 0
TEST_F(HcclInterTest, st_broadcast_inter_char_4pring_root0_10)
{
    nlohmann::json rank_table = rank_table_910_1server_4rank;
   
    char file_name_t[] = "./st_broadcast_inter_char_4pring_root0_10.json";
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
    s32 errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s8* sendbuf[DEV_NUM_4];
    s32 sync_value = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];
    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    s32 count = 10;
    s32 ndev = DEV_NUM_4;
    set_board_id(0x0000);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(s8));
        sal_memset(sendbuf[i], count, 0, count);
    }
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[0][i] = 1;
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].root = 0;
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_broadcast_task, (void*)&para_info[i]);
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
        for (s32 i = 0; i < count; i++)
        {
            s8 recv = sendbuf[j][i];
            if (sendbuf[j][i] != 1)
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
        hrtFree(sendbuf[j]);
        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    set_board_id(0);
    EXPECT_EQ(errors, 0);
}
#endif

#if 0
TEST_F(HcclInterTest, st_broadcast_inter_char_4pring_root2)
{
    nlohmann::json rank_table = rank_table_910_1server_4rank;
   
    char file_name_t[] = "./st_broadcast_inter_char_4pring_root2.json";
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
    s32 errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s8* sendbuf[DEV_NUM_4];
    s32 sync_value = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];
    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    s32 count = 725;
    s32 ndev = DEV_NUM_4;
    set_board_id(0x0000);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(s8));
        sal_memset(sendbuf[i], count, 0, count);
    }
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[2][i] = 1;
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].root = 2;
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_broadcast_task, (void*)&para_info[i]);
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
        for (s32 i = 0; i < count; i++)
        {
            s8 recv = sendbuf[j][i];
            if (sendbuf[j][i] != 1)
            {
                HCCL_ERROR("recvbuf[%d][%d]:%d \n", j, i, sendbuf[j][i]);
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
        hrtFree(sendbuf[j]);
        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0);
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}
#endif

#if 1
TEST_F(HcclInterTest, st_broadcast_inter_float_slice)
{
    // ranktable 的读取，直接使用进程 
    nlohmann::json rank_table = rank_table_910_1server_4rank;
    char file_name_t[] = "./st_broadcast_inter_float_slice.json";
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

    s32 errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s32* sendbuf[DEV_NUM_4];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];

    HcclDataType datatype = HCCL_DATA_TYPE_FP32;

    s32 count = HCCL_BROADCAST_DATA_SLICE;
    s32 ndev = DEV_NUM_4;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(s32));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count* sizeof(s32), 0, count* sizeof(s32));

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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];

        para_info[i].root = 0;
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
        if (i == 0)
        {
            for (u32 i = 0; i < count; i++)
            {
                sendbuf[0][i] = 1.0;
            }
        }
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_broadcast_task, (void*)&para_info[i]);

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


    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count; i++)
        {
            s32 recv = sendbuf[j][i];

            if (abs(sendbuf[j][i] -1.0) >1e-6)
            {
                HCCL_ERROR("recvbuf[%d][%d]:%f \n", j, i, sendbuf[j][i]);
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
        hrtFree(sendbuf[j]);
        rt_ret = rtStreamDestroy(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}
#endif
#if 1
#define HCCL_ALLREDUCE_DATA_SIZE 1000
#define HCCL_ALLREDUCE_DATA_SLICE 1024*1024*2+10
TEST_F(HcclInterTest, st_allreduce_inter_char)
{
    nlohmann::json rank_table = rank_table_910_1server_4rank;
   

    char file_name_t[] = "./st_allreduce_inter_char.json";
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

    s32 rank, errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s8* result_buff[DEV_NUM_4];
    s8* sendbuf[DEV_NUM_4];
    s8* recvbuf[DEV_NUM_4];
    s8* inputbuf[DEV_NUM_4];
    s8* outputbuf[DEV_NUM_4];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;

    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = HCCL_ALLREDUCE_DATA_SIZE;
    s32 ndev = DEV_NUM_4;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(s8), 0, count * sizeof(s8));
        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count * sizeof(s8), 0, count * sizeof(s8));
        ret = hrtMalloc((void**)&result_buff[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(result_buff[i], count * sizeof(s8), 0, count * sizeof(s8));
        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    //sendbuf 赋值
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = 1;
        }
    }

    //resultbuf 赋值
   for (s32 i = 0; i < ndev; ++i)
 {
    for (u32 j = 0; j < count; j++)
     {
            result_buff[i][j] = ndev;
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = inputbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = outputbuf[i];
        para_info[i].op = op;

        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; i++)
    {
        tid[i] = sal_thread_create("thread", inter_all_reduce_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //获取stream的操作的同步信号量
    for (s32 i = 0; i < ndev; i++)
  {
     for (s32 j = 0; j < count; j++)
    {
            s8 res = result_buff[i][j];
            s8 recv = outputbuf[i][j];

            if (res != recv)
            {
                HCCL_ERROR(" rank :%d recvbuf[%d] :%d result_buff[%d]:%d \n", i, j, recv, j, res);
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
    for (s32 i = 0; i < ndev; i++)
   {
        hrtFree(sendbuf[i]);
        hrtFree(recvbuf[i]);
        hrtFree(result_buff[i]);
    rt_ret = rtStreamDestroy(stream[i]);

    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
   }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}

TEST_F(HcclInterTest, st_allreduce_inter_float_max)
{
    nlohmann::json rank_table = rank_table_910_1server_4rank;
  
    char file_name_t[] = "./st_allreduce_inter_float_slice.json";
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

    s32 rank, errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    float* result_buff[DEV_NUM_4];
    float* sendbuf[DEV_NUM_4];
    float* recvbuf[DEV_NUM_4];
    float* inputbuf[DEV_NUM_4];
    float* outputbuf[DEV_NUM_4];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];

    HcclDataType datatype = HCCL_DATA_TYPE_FP32;

    HcclReduceOp op = HCCL_REDUCE_MAX;
    s32 count = HCCL_ALLREDUCE_DATA_SLICE;
    s32 ndev = DEV_NUM_4;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(s32));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(s32), 0, count * sizeof(s32));
        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(s32));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count * sizeof(s32), 0, count * sizeof(s32));
        ret = hrtMalloc((void**)&result_buff[i], count * sizeof(s32));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(result_buff[i], count * sizeof(s32), 0, count * sizeof(s32));
        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];

        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(s32));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(s32), 0, count * sizeof(s32));
        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(s32));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count * sizeof(s32), 0, count * sizeof(s32));
        ret = hrtMalloc((void**)&result_buff[i], count * sizeof(s32));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(result_buff[i], count * sizeof(s32), 0, count * sizeof(s32));
        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    //sendbuf 赋值
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = 1.0;
        }
    }

    //resultbuf 赋值
    for (s32 i = 0; i < ndev; ++i)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = 1.0;
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = inputbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = outputbuf[i];
        para_info[i].op = op;

        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; i++)
    {
        tid[i] = sal_thread_create("thread", inter_all_reduce_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //获取stream的操作的同步信号量
    for (s32 i = 0; i < ndev; i++)
    {
        for (s32 j = 0; j < count; j++)
        {
            float res = result_buff[i][j];
            float recv = outputbuf[i][j];

            if (abs(res - recv) > 1e-6)
            {
                HCCL_ERROR(" recvbuf[%f] result_buff[%f] \n", recv, res);
                errors ++;
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

    for (s32 i = 0; i < ndev; i++)
    {
        rt_ret = rtStreamDestroy(stream[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    for (s32 i = 0; i < ndev; i++)
    {
        hrtFree(sendbuf[i]);
        hrtFree(recvbuf[i]);
        hrtFree(result_buff[i]);
    }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}
#endif
#if 1
#define HCCL_REDUCESCATTER_DATA_SIZE 12

TEST_F(HcclInterTest, st_reducescatter_inter_char)
{
    nlohmann::json rank_table = rank_table_910_1server_2rank;
    

    char file_name_t[] = "./st_reducescatter_inter_char.json";
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

    s32 rank, errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s8* result_buff[DEV_NUM_2];
    s8* sendbuf[DEV_NUM_2];
    s8* recvbuf[DEV_NUM_2];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_2];
    sal_thread_t tid[DEV_NUM_2];
    para_t para_info[DEV_NUM_2];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;

    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = HCCL_REDUCESCATTER_DATA_SIZE;
    s32 ndev = DEV_NUM_2;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], ndev * count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], ndev * count * sizeof(s8), 0, ndev * count * sizeof(s8));

        ret = hrtMalloc((void **)&recvbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count * sizeof(s8), 0, count * sizeof(s8));
        result_buff[i] = (s8*)sal_malloc(count * sizeof(s8));
        sal_memset(result_buff[i], count * sizeof(s8), 0, count * sizeof(s8));
    }

    //sendbuf 赋值
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < ndev * count; i++)
        {
            sendbuf[j][i] = i % 12;
        }
    }

    //resultbuf 赋值
    for (s32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = ndev * j;
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuf[i];
        para_info[i].op = op;

        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_reduce_scatter_task, (void*)&para_info[i]);
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

    for (s32 i = 0; i < ndev; i++)
    {
        for (s32 j = 0; j < count; j++)
        {
            s8 res = result_buff[i][j];
            s8 recv = recvbuf[i][j];

            if (res != recv)
            {
                HCCL_ERROR(" recvbuf[%d] result_buff[%d] \n", recv, res);
                errors ++;
                break;
            }
        }
    }

    if (errors)
    {
        errors = 0;
        HCCL_ERROR("%d errors. Test FAILED.\n", errors);
    }
    else
    {
        HCCL_INFO("Test PASSED.\n");
    }

    for (s32 i = 0; i < ndev; i++)
    {
        hrtFree(sendbuf[i]);
        hrtFree(recvbuf[i]);
        sal_free(result_buff[i]);
        rt_ret = rtStreamDestroy(stream[i]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}

#define HCCL_REDUCESCATTER_DATA_SLICE 1024*4+2

TEST_F(HcclInterTest, st_reducescatter_inter_float_slice)
{
    nlohmann::json rank_table = rank_table_910_1server_2rank;
  

    char file_name_t[] = "./st_reducescatter_inter_float_slice.json";
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

    s32 rank, errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s32* result_buff[DEV_NUM_2];
    s32* sendbuf[DEV_NUM_2];
    s32* recvbuf[DEV_NUM_2];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_2];
    sal_thread_t tid[DEV_NUM_2];
    para_t para_info[DEV_NUM_2];

    HcclDataType datatype = HCCL_DATA_TYPE_FP32;

    HcclReduceOp op = HCCL_REDUCE_MAX;
    s32 count = HCCL_REDUCESCATTER_DATA_SLICE;
    s32 ndev = DEV_NUM_2;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], ndev * count * sizeof(s32));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], ndev * count * sizeof(s32), 0, ndev * count * sizeof(s32));

        ret = hrtMalloc((void **)&recvbuf[i], count * sizeof(s32));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count * sizeof(s32), 0, count * sizeof(s32));
        result_buff[i] = (s32*)sal_malloc(count * sizeof(s32));
        sal_memset(result_buff[i], count * sizeof(s32), 0, count * sizeof(s32));
    }

    //sendbuf 赋值
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < ndev * count; i++)
        {
            sendbuf[j][i] = 1.0;
        }
    }

    //resultbuf 赋值
    for (s32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = 1.0;
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuf[i];
        para_info[i].op = op;

        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_reduce_scatter_task, (void*)&para_info[i]);
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

    for (s32 i = 0; i < ndev; i++)
    {
        for (s32 j = 0; j < count; j++)
        {
            s32 res = result_buff[i][j];
            s32 recv = recvbuf[i][j];

            if (abs(res - recv) > 1e-6)
            {
                HCCL_ERROR(" recvbuf[%f] result_buff[%f] \n", recv, res);
                errors ++;
                break;
            }
        }
    }

    if (errors)
    {
        HCCL_ERROR("%d errors. Test FAILED.\n", errors);
        errors = 0;
    }
    else
    {
        HCCL_INFO("Test PASSED.\n");
    }

    for (s32 i = 0; i < ndev; i++)
    {
        hrtFree(sendbuf[i]);
        hrtFree(recvbuf[i]);
        sal_free(result_buff[i]);
        rt_ret = rtStreamDestroy(stream[i]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}

#endif
