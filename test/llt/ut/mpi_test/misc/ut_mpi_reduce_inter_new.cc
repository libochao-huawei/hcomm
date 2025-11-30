/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

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


#include "hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "hccl_comm_pub.h"

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>
#include "hccl/hcom.h"
#include "stream_pub.h"
#include "mem_host_pub.h"
#include "mem_device_pub.h"
#include "hccl_comm_pub.h"
#include "sal.h"
#include "hccl_impl.h"
#include "hccl_alg.h"
#include "config.h"
#include "dlra_function.h"
#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"
#include "dltdt_function.h"
#include "param_check_pub.h"
#include "hcom_private.h"
#include <iostream>
#include <fstream>

using namespace std;
using namespace hccl;

class MPI_ReduceInter_Test : public testing::Test
{
protected:
    static void SetUpTestCase()
    {

        std::cout << "MPI_ReduceInter_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_ReduceInter_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {

        DlTdtFunction::GetInstance().DlTdtFunctionInit();
        TsdOpen(1, 2);
        MPI_Barrier(MPI_COMM_WORLD);
        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "ST_MPI_TEST");
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A MPI_ReduceInter_Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        TsdClose(1);
        MPI_Barrier(MPI_COMM_WORLD);
        std::cout << "A MPI_ReduceInter_Test TearDown" << std::endl;
    }
};
#define MPI_REDUCE_DATA_SIZE_10 10

#define MPI_REDUCE_DATA_SIZE_CHUNK   (1024*1024+5)
#define MPI_REDUCE_DATA_NUM          (1024*256)
#define MPI_REDUCE_DATA_NUM_1M       (1024*256+3)
#define MPI_REDUCE_DATA_NUM_2M       (1024*256*2 + 5)


#define REDUCE_NUM 4
#define REDUCE_SIZE 10

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
    s32 root;
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

void stub_SetHDCModeInfo(std::map<std::string, std::map<u32, HcclIpAddress>> &rankDevicePhyIdNicInfoMap,
    std::vector<u32> &ranksPort, bool isSetHDCModeInfo, bool isUseRankPort)
{
    return;
}

void* reduce_8p_task(void* parg)
{
    MOCKER_CPP(&HcclAlg::SetHDCModeInfo)
    .stubs()
    .with(any())
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
    ret = HcomLoadRanktableFile(ranktable_file.c_str(), rankTableM, realFilePath);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, para_info->identify, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    sal_memcpy(hcom_info.params.id.internal, sizeof(HcclRootInfo), &para_info->rootInfo, sizeof(HcclRootInfo));

    hcom_info.pComm.reset(new(std::nothrow) hccl::hcclComm());
    char* charModel = new char;
    rtModel_t model = (void*)charModel;

    hcom_info.pComm->init(hcom_info.params, hcom_info.rankTable);
    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task reduce falis", para_info->device_id);
    }
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
        rt_ret = aclrtCreateStreamWithConfig(&streamList[i], 0, ACL_STREAM_PERSISTENT);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        //从流bind到model
        rt_ret = rtModelBindStream(model, streamList[i], RT_MODEL_WAIT_ACTIVE_STREAM);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    u32 rankSize = 0;
    ret = hcom_info.pComm->GetRankSize(rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 memSize = 0;
    ret = hcom_info.pComm->GetWorkspaceMemSize("tag_reduce_8p", para_info->count, para_info->datatype, rankSize, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource("tag_reduce_8p", memptr, memSize, streamList);
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

    ret =  hcom_info.pComm->Reduce("tag_reduce_8p",
                                   para_info->sendbuff,
                                   para_info->recvbuff,
                                   para_info->count,
                                   para_info->datatype,
                                   para_info->op,
                                   para_info->root,
                                   para_info->stream);

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task HcclReduce falis", para_info->device_id);
    }

    rt_ret = aclrtSynchronizeStream(para_info->stream);

    //--------------Resource destroy----------------//
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtModelUnbindStream(model, streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);

        rt_ret = aclrtDestroyStream(streamList[i]);
        EXPECT_EQ(rt_ret, ACL_SUCCESS);
    }
    hrtFree(memptr);
    delete charModel;
    charModel = nullptr;
    return (NULL);
}

void* reduce_8p_subgroup_task(void* parg)
{
    HcclResult ret = HCCL_SUCCESS;
    bool subGroupFlag = false;
    para_t* para_info = (para_t*)parg;
    s32 rank_num_tmp;
    std::vector<u32> groupRanks;
    std::shared_ptr<hccl::hcclComm> pSubComm;
    HcomInfo hcom_info;
    hcom_info.params.deviceType = DevType::DEV_TYPE_910;
    std::string ranktable_file = para_info->file_name;
    std::string rankTableM;
    std::string realFilePath;

    rtError_t rt_ret = RT_ERROR_NONE;
    HCCL_INFO("device_id=[%d]", para_info->device_id);
    hrtSetDevice(para_info->device_id);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomLoadRanktableFile(ranktable_file.c_str(), rankTableM, realFilePath);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, para_info->identify, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sal_memcpy(hcom_info.params.id.internal, sizeof(HcclRootInfo), &para_info->rootInfo, sizeof(HcclRootInfo));

    hcom_info.pComm.reset(new(std::nothrow) hccl::hcclComm());
    rtModel_t model = (void*)1;

    bool swapped;
    rank_num_tmp = *(para_info->sync_addr) - 1;
    u32 wordGroupId = std::stoi(para_info->identify);
    s32 groupid = -1;
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
    __sync_synchronize();  // 插入内存屏障，对顺序性有要求，但是有没有使用lock指令的时候

    ret =  hcom_info.pComm->init(hcom_info.params, hcom_info.rankTable);

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task all_gather falis", para_info->device_id);
    }
    HCCL_INFO("wordGroupId[%d], hcclComm init success!",wordGroupId);
    
    for(u32 i = 0;i < para_info->groupRanksNum;i++) {
        if(wordGroupId == para_info->pGroupRanks[i]) {
            subGroupFlag = true;
            groupid = i;
            HCCL_INFO("get groupid[%d]!",groupid);
        }
        groupRanks.push_back(para_info->pGroupRanks[i]);
    }
    
    if(subGroupFlag) { //是否为group内

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
            rt_ret = aclrtCreateStreamWithConfig(&streamList[i], 0, ACL_STREAM_PERSISTENT);
            EXPECT_EQ(rt_ret, RT_ERROR_NONE);
            //从流bind到model
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

        ret = hcom_info.pComm->SetWorkspaceResource("tag_reduce_8p_subgroup_1", memptr, memSize, streamList);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        //-----------------Set Workspace Resource End------------------//

        hcom_info.pComm->CreateGroup(para_info->groupName, groupid, hcom_info.params.userRank, groupRanks, pSubComm);

        HCCL_INFO("wordGroupId[%d], groupid[%d], CreateGroup success!",wordGroupId, groupid);

        ret =  pSubComm->Reduce("tag_reduce_8p_subgroup_1",
                                   para_info->sendbuff,
                                   para_info->recvbuff,
                                   para_info->count,
                                   para_info->datatype,
                                   para_info->op,
                                   para_info->root,
                                   para_info->stream);
        subGroupFlag = false;
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("wordGroupId[%d], groupid[%d], all_gather success!",wordGroupId, groupid);
        //--------------Resource destroy----------------//
        rt_ret = aclrtSynchronizeStream(para_info->stream);

        if ( rt_ret != ACL_SUCCESS)
        {
            HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
        }
        for (s32 i = 0; i < stream_list_size; i++)
        {
            rt_ret = rtModelUnbindStream(model, streamList[i]);
            EXPECT_EQ(rt_ret, RT_ERROR_NONE);

            rt_ret = aclrtDestroyStream(streamList[i]);
            EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        }
        
        hrtFree(memptr);

        HCCL_INFO("wordGroupId[%d], groupid[%d], aclrtSynchronizeStream success!",wordGroupId, groupid);
    }
    
    ret = hcom_info.pComm->DestroyGroup(para_info->groupName);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("wordGroupId[%d], groupid[%d], DestroyGroup success!",wordGroupId, groupid);
    return (NULL);
}



void* inter_reduce_task(void* parg)
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
    hcom_info.params.totalRanks = hcom_info.rankTable.rankNum;

    hcom_info.pComm.reset(new(std::nothrow) hccl::hcclComm());
    rtModel_t model = (void*)1;

    hrtSetDevice(para_info->device_id);

    CommConfig commConfig("hccl_world_group"); 
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);

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
        rt_ret = aclrtCreateStreamWithConfig(&streamList[i], 0, ACL_STREAM_PERSISTENT);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        //从流bind到model
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

    ret = hcom_info.pComm->SetWorkspaceResource("tag_inter_reduce", memptr, memSize, streamList);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//

    bool swapped;

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task reduce falis", para_info->device_id);
    }

    rank_num_tmp = *(para_info->sync_addr) - 1;

    do
    {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1);
    }
    while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local)
    { sched_yield(); } // linux提供一个系统调用运行进程主动让出执行权

    __sync_synchronize();  // 插入内存屏障，对顺序性有要求，但是有没有使用lock指令的时候

    HCCL_DEBUG("all %d  ranks init ok ,then reduce", hcom_info.params.totalRanks);
    ret = hcom_info.pComm->Reduce("tag_inter_reduce", para_info->sendbuff,
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

    rt_ret = aclrtSynchronizeStream(para_info->stream);

    if ( rt_ret != ACL_SUCCESS)
    {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }
    //--------------Resource destroy----------------//
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtModelUnbindStream(model, streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);

        rt_ret = aclrtDestroyStream(streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    
    hrtFree(memptr);
    return (NULL);
}


#define MPI_REDUCE_DATA_SIZE 10
#define DEV_NUM_1 1
#define DEV_NUM_2 2
#define DEV_NUM_4 4
#define DEV_NUM_8 8
#define GOUP_DEV_NUM 8


#if 1
TEST_F(MPI_ReduceInter_Test, ut_mpi_reduce_16ranks_2server_float_sum_1024count_root8)
{
    nlohmann::json g_rank_table_16p = rank_table_910_2server_8rank;
       char file_name_t[] = "./st_mpi_reduce_inter_sum_16plus.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << g_rank_table_16p << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    s32 nnode, rank, errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    float* result_buff[DEV_NUM_8];
    float* sendbuf[DEV_NUM_8];
    float* recvbuf[DEV_NUM_8];

    s32 sync_value = 0;
    s32 noderank = 0;
    aclrtStream stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];
    HcclDataType datatype = HCCL_DATA_TYPE_FP32;

    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = 1024;
    s32 ndev = DEV_NUM_8;

    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i],  count * sizeof(float) , 0,  count * sizeof(float));
        ret = hrtMalloc((void **)&recvbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count * sizeof(float), 0, count * sizeof(float));
        ret = hrtMalloc((void **)&result_buff[i], (count * sizeof(float)));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(result_buff[i], (count * sizeof(float)), 0, (count * sizeof(float)));
    }

    //sendbuf 赋值
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[j][i] = 2.0;
        }
    }

    //resultbuf 赋值
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = 32.0 ;
        }
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = aclrtCreateStream(&stream[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        sal_memcpy(&para_info[i].rootInfo, sizeof(HcclRootInfo), &rootInfo, sizeof(HcclRootInfo));
        std::ostringstream identify("");
        identify << i + ndev * noderank;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuf[i];
        para_info[i].op = op;
        para_info[i].root = 8;
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    // 创建每个Dev的reduce scatter任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", reduce_8p_task, (void*)&para_info[i]);
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
       rt_ret = aclrtSynchronizeStream(stream[j]);
       EXPECT_EQ(rt_ret, ACL_SUCCESS);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    /*check result*/
   if(noderank == 1) {
        for (s32 i = 0; i < count; i++)
        {   
			 float res = result_buff[0][i];
            float recv = recvbuf[0][i];
			HCCL_DEBUG("node1 recvbuf[0][%d]:%f result_buff:%f \n",i, recvbuf[0][i], result_buff[0][i]);
            if (abs(recv - res) > 1e-6)
            {
               
               errors++;
               break;
            }
        }
    }
 /*     if(noderank == 1) {
	for(s32 j = 0; j < ndev; j++) {  
        for (s32 i = 0; i < count; i++)
        {  
			float res = result_buff[j][i];
            float recv = recvbuf[j][i];
			if (res==16.0) {
            HCCL_ERROR("node1  sendbuf[%d][%d]:%f recvbuf[%d][%d]:%f\n",j,i, sendbuf[j][i], j,i, recv);}
        }
    }
   } */
    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

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
        hrtFree(result_buff[j]);

    }
    for (s32 j = 0; j < ndev; j++)
    {
        rt_ret = aclrtDestroyStream(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0);
    remove(file_name_t);

    EXPECT_EQ(errors, 0);
}

TEST_F(MPI_ReduceInter_Test, ut_mpi_reduce_16ranks_2server_float_sum_1024count_root0)
{
    nlohmann::json g_rank_table_16p = rank_table_910_2server_8rank;
       char file_name_t[] = "./st_mpi_reduce_inter_sum_16plus.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << g_rank_table_16p << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    s32 nnode, rank, errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    float* result_buff[DEV_NUM_8];
    float* sendbuf[DEV_NUM_8];
    float* recvbuf[DEV_NUM_8];

    s32 sync_value = 0;
    s32 noderank = 0;
    aclrtStream stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];
    HcclDataType datatype = HCCL_DATA_TYPE_FP32;

    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = 1024;
    s32 ndev = DEV_NUM_8;

    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i],  count * sizeof(float) , 0,  count * sizeof(float));
        ret = hrtMalloc((void **)&recvbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count * sizeof(float), 0, count * sizeof(float));
        ret = hrtMalloc((void **)&result_buff[i], (count * sizeof(float)));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(result_buff[i], (count * sizeof(float)), 0, (count * sizeof(float)));
    }

    //sendbuf 赋值
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[j][i] = 2.0;
        }
    }

    //resultbuf 赋值
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = 32.0 ;
        }
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = aclrtCreateStream(&stream[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        sal_memcpy(&para_info[i].rootInfo, sizeof(HcclRootInfo), &rootInfo, sizeof(HcclRootInfo));
        std::ostringstream identify("");
        identify << i + ndev * noderank;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
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

    // 创建每个Dev的reduce scatter任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", reduce_8p_task, (void*)&para_info[i]);
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
       rt_ret = aclrtSynchronizeStream(stream[j]);
       EXPECT_EQ(rt_ret, ACL_SUCCESS);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    /*check result*/
   if(noderank == 0) {
        for (s32 i = 0; i < count; i++)
        {   
			 float res = result_buff[0][i];
            float recv = recvbuf[0][i];
			HCCL_DEBUG("recvbuf[0][%d]:%f result_buff:%f \n",i, recvbuf[0][i], result_buff[0][i]);
            if (abs(recv - res) > 1e-6)
            {
               
               errors++;
               break;
            }
        }
    }
 /*     if(noderank == 1) {
	for(s32 j = 0; j < ndev; j++) {  
        for (s32 i = 0; i < count; i++)
        {  
			float res = result_buff[j][i];
            float recv = recvbuf[j][i];
			if (res==16.0) {
            HCCL_ERROR("node1  sendbuf[%d][%d]:%f recvbuf[%d][%d]:%f\n",j,i, sendbuf[j][i], j,i, recv);}
        }
    }
   } */
    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

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
        hrtFree(result_buff[j]);

    }
    for (s32 j = 0; j < ndev; j++)
    {
        rt_ret = aclrtDestroyStream(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0);
    remove(file_name_t);

    EXPECT_EQ(errors, 0);
}
#endif