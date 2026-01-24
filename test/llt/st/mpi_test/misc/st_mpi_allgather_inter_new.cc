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

#include "sal.h"
#include "config.h"
#include "dlra_function.h"
#include "topoinfo_ranktableParser_pub.h"
#include "externalinput.h"
#include "param_check_pub.h"

#include "v80_rank_table.h"
#include "network_manager_pub.h"
#include "dltdt_function.h"
#include "hcom_private.h"

#include <iostream>
#include <fstream>

using namespace std;
using namespace hccl;

class MPI_AllGatherInter_Test : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_AllGatherInter_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_AllGatherInter_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        static s32  call_cnt = 0;
        DlTdtFunction::GetInstance().DlTdtFunctionInit();
        TsdOpen(1, 2);
        HCCL_INFO("hccl test TsdOpen");
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "ST_MPI_TEST");
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A MPI_AllGatherInter_Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        TsdClose(1);
        MPI_Barrier(MPI_COMM_WORLD);
        std::cout << "A MPI_AllGatherInter_Test TearDown" << std::endl;
    }
};

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

void* all_gather_8p_subgroup_task(void* parg)
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
    u32 devicePhyId;
    ret = hrtGetDevicePhyIdByIndex((u32)para_info->device_id, devicePhyId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
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
        HCCL_ERROR("dev[%d] task all_gather fails", para_info->device_id);
    }
    else
    {
        HCCL_INFO("wordGroupId[%d], hcclComm init success!",wordGroupId);
    }
    for(u32 i = 0;i < para_info->groupRanksNum;i++) {
        if(wordGroupId == para_info->pGroupRanks[i]) {
            subGroupFlag = true;
            groupid = i;
            HCCL_INFO("get groupid[%d]!",groupid);
        }
        groupRanks.push_back(para_info->pGroupRanks[i]);
    }

    //-----------------Set Workspace Resource Start------------------//
    u64 stream_list_size = 0;
    ret = hcom_info.pComm->GetWorkspaceSubStreamNum(stream_list_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("get stream_list_size[%d] success", stream_list_size);
    vector<HcclRtStream> streamList(stream_list_size);

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
    ret = hcom_info.pComm->GetWorkspaceMemSize(HCCL_KERNEL_OP_TYPE_ALLGATHER, para_info->count, para_info->datatype, rankSize, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource("tag_all_gather_8p_subgroup_1", memptr, memSize, streamList);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//

    if(subGroupFlag) { //是否为group内
        hcom_info.pComm->CreateGroup(para_info->groupName, groupid, hcom_info.params.userRank, groupRanks, pSubComm);

        HCCL_INFO("wordGroupId[%d], groupid[%d], CreateGroup success!",wordGroupId, groupid);

        ret =  pSubComm->AllGather("tag_all_gather_8p_subgroup_1",
                                   para_info->sendbuff,
                                   para_info->recvbuff,
                                   para_info->count,
                                   para_info->datatype,
                                   para_info->stream);
        if (ret != HCCL_SUCCESS)
        {
            HCCL_ERROR("rank[%d] task allgather fails", hcom_info.params.rank);
        }
        subGroupFlag = false;
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("wordGroupId[%d], groupid[%d], all_gather success!",wordGroupId, groupid);

        HCCL_INFO("wordGroupId[%d], groupid[%d], aclrtSynchronizeStream success!",wordGroupId, groupid);
    }

    rt_ret = aclrtSynchronizeStream(para_info->stream);

    if ( rt_ret != ACL_SUCCESS)
    {
        HCCL_ERROR("rank[%d] task allgather fails", hcom_info.params.rank);
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

    ret = hcom_info.pComm->DestroyGroup(para_info->groupName);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("wordGroupId[%d], groupid[%d], DestroyGroup success!",wordGroupId, groupid);
    return (NULL);
}




void* inter_all_gather_task(void* parg)
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
    rtModel_t model = (void*)1;

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
    ret = hcom_info.pComm->GetWorkspaceMemSize(HCCL_KERNEL_OP_TYPE_ALLGATHER, para_info->count, para_info->datatype, rankSize, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource("tag_inter_all_gather", memptr, memSize, streamList);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task all_gather fails", para_info->device_id);
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
    { sched_yield(); } // linux提供一个系统调用运行进程主动让出执行权

    __sync_synchronize();  // 插入内存屏障，对顺序性有要求，但是有没有使用lock指令的时候

    HCCL_DEBUG("all %d  ranks init ok ,then allgather", hcom_info.params.totalRanks);
    ret = hcom_info.pComm->AllGather("tag_inter_all_gather",
                                       para_info->sendbuff,
                                       para_info->recvbuff,
                                       para_info->count,
                                       para_info->datatype,
                                       para_info->stream);

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("rank[%d] task allgather fails", hcom_info.params.rank);
    }

    rt_ret = aclrtSynchronizeStream(para_info->stream);

    if ( rt_ret != ACL_SUCCESS)
    {
        HCCL_ERROR("rank[%d] task allgather fails", hcom_info.params.rank);
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


//MPI, 每个进程内作为一个服务器，每个服务器内一个rank

#define DEV_NUM_2 2
#define DEV_NUM_4 4
#define DEV_NUM_8 8

#define MPI_ALLGATHER_ALIGN_DATA_SIZE 128  //对齐的datacount

#if 0
TEST_F(MPI_AllGatherInter_Test, st_mpi_allgather_8ranks_2server_char_count1025)
{
    nlohmann::json g_rank_table_16p_V80 = rank_table_910_2server_8rank;
    char file_name_t[] = "./st_mpi_allgather_8ranks_2server_char_count1025.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << g_rank_table_16p_V80 << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    u32 groupRanks[GOUP_DEV_NUM] = {0,1,2,3,8,9,10,11};

    s32 nnode, rank, errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s8* result_buff[DEV_NUM_4];
    s8* sendbuf[DEV_NUM_4];
    s8* recvbuf[DEV_NUM_4];

    s32 sync_value = 0;
    s32 noderank = 0;
    aclrtStream stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];
    HcclDataType datatype = HCCL_DATA_TYPE_INT8;

    s32 count = 1025;
    s32 ndev = DEV_NUM_4;

    set_board_id(0);
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(s8), 0, count * sizeof(s8));
        ret = hrtMalloc((void **)&recvbuf[i], ndev * nnode * count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], ndev * nnode * count  * sizeof(s8), 0, ndev * nnode * count * sizeof(s8));
        result_buff[i] = (s8*)sal_malloc(ndev * nnode * count * sizeof(s8));
        sal_memset(result_buff[i], ndev * nnode * count * sizeof(s8), 0, ndev * nnode * count * sizeof(s8));
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
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < ndev * nnode * count; j++)
        {
            result_buff[i][j] =  1.0 ;
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
        //identify << i + ndev * noderank;
        identify << groupRanks[i+ ndev * noderank];
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
        para_info[i].groupRanksNum = GOUP_DEV_NUM;
        para_info[i].groupName = "group1" ;
        para_info[i].pGroupRanks = groupRanks;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", all_gather_8p_subgroup_task, (void*)&para_info[i]);
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
    bool subGroupFlag = false;

    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {

        u32 wordGroupId = std::stoi(para_info[j].identify);
        for(u32 i = 0;i < para_info->groupRanksNum;i++) {
            if(wordGroupId == para_info->pGroupRanks[i]) {
                subGroupFlag = true;
            }
        }
        if (subGroupFlag) {
            for (s32 i = 0; i < count * ndev * nnode; i++)
          {
                s8 res = result_buff[j][i];
                s8 recv = recvbuf[j][i];

                if (res != recv)
                {
                    HCCL_ERROR("worldgroupid[%d] noderank:%d recvbuf[%d][%d]:%f res[%d][%d]:%f\n",wordGroupId, noderank, j, i, recv,j,i,res);
                    errors++;
                    break;
                }
            }
        }
        subGroupFlag = false;
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    if (rank == 0)
    {
        if (errors)
        {
            HCCL_ERROR("%d errors. Test FAILED.\n", errors);
        }
        else
        {
            HCCL_INFO("Test PASSED.\n");
        }
    }

 /*   if (noderank == 0)
        for (s32 j = 0; j < ndev; j++) {
            for (s32 i = 0; i < count * ndev * nnode; i++)
            {
                printf("recvbuf[%d][%d] = %f \n", j, i, recvbuf[j][i]);
            }
        }
*/

    for (s32 j = 0; j < ndev; j++)
    {
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        sal_free(result_buff[j]);


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

#if 0
TEST_F(MPI_AllGatherInter_Test, st_mpi_allgather_8ranks_2server_float_count3)
{
    nlohmann::json g_rank_table_16p_V80 = rank_table_910_2server_8rank;
    char file_name_t[] = "./st_mpi_allgather_8ranks_2server_float.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << g_rank_table_16p_V80 << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    u32 groupRanks[GOUP_DEV_NUM] = {0,1,2,3,8,9,10,11};

    s32 nnode, rank, errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    float* result_buff[DEV_NUM_4];
    float* sendbuf[DEV_NUM_4];
    float* recvbuf[DEV_NUM_4];

    s32 sync_value = 0;
    s32 noderank = 0;
    aclrtStream stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];
    HcclDataType datatype = HCCL_DATA_TYPE_FP32;

    s32 count = 3;
    s32 ndev = DEV_NUM_4;

    set_board_id(0);
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
        sal_memset(sendbuf[i], count * sizeof(float), 0, count * sizeof(float));
        ret = hrtMalloc((void **)&recvbuf[i], ndev * nnode * count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], ndev * nnode * count  * sizeof(float), 0, ndev * nnode * count * sizeof(float));
        result_buff[i] = (float*)sal_malloc(ndev * nnode * count * sizeof(float));
        sal_memset(result_buff[i], ndev * nnode * count * sizeof(float), 0, ndev * nnode * count * sizeof(float));
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
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < ndev * nnode * count; j++)
        {
            result_buff[i][j] =  1.0 ;
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
        //identify << i + ndev * noderank;
        identify << groupRanks[i+ ndev * noderank];
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
        para_info[i].groupRanksNum = GOUP_DEV_NUM;
        para_info[i].groupName = "group1" ;
        para_info[i].pGroupRanks = groupRanks;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", all_gather_8p_subgroup_task, (void*)&para_info[i]);
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
    bool subGroupFlag = false;

    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {

        u32 wordGroupId = std::stoi(para_info[j].identify);
        for(u32 i = 0;i < para_info->groupRanksNum;i++) {
            if(wordGroupId == para_info->pGroupRanks[i]) {
                subGroupFlag = true;
            }
        }
        if (subGroupFlag) {
            for (s32 i = 0; i < count * ndev * nnode; i++)
          {
                float res = result_buff[j][i];
                float recv = recvbuf[j][i];

                if (abs(res - recv) > 1e-6)
                {
                    HCCL_ERROR("worldgroupid[%d] noderank:%d recvbuf[%d][%d]:%f res[%d][%d]:%f\n",wordGroupId, noderank, j, i, recv,j,i,res);
                    errors++;
                    break;
                }
            }
        }
        subGroupFlag = false;
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    if (rank == 0)
    {
        if (errors)
        {
            HCCL_ERROR("%d errors. Test FAILED.\n", errors);
        }
        else
        {
            HCCL_INFO("Test PASSED.\n");
        }
    }

 /*   if (noderank == 0)
        for (s32 j = 0; j < ndev; j++) {
            for (s32 i = 0; i < count * ndev * nnode; i++)
            {
                printf("recvbuf[%d][%d] = %f \n", j, i, recvbuf[j][i]);
            }
        }
*/

    for (s32 j = 0; j < ndev; j++)
    {
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        sal_free(result_buff[j]);


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

#if 0
TEST_F(MPI_AllGatherInter_Test, st_mpi_allgather_8ranks_2server_float_count1048571)  //  1024*1024-5
{
    nlohmann::json g_rank_table_16p_V80 = rank_table_910_2server_8rank;
    char file_name_t[] = "./st_mpi_allgather_8ranks_2server_float.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << g_rank_table_16p_V80 << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    u32 groupRanks[GOUP_DEV_NUM] = {0,1,2,3,8,9,10,11};

    s32 nnode, rank, errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    float* result_buff[DEV_NUM_4];
    float* sendbuf[DEV_NUM_4];
    float* recvbuf[DEV_NUM_4];

    s32 sync_value = 0;
    s32 noderank = 0;
    aclrtStream stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];
    HcclDataType datatype = HCCL_DATA_TYPE_FP32;

    s32 count = 1024*1024-5;
    s32 ndev = DEV_NUM_4;

    set_board_id(0);
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
        sal_memset(sendbuf[i], count * sizeof(float), 0, count * sizeof(float));
        ret = hrtMalloc((void **)&recvbuf[i], ndev * nnode * count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], ndev * nnode * count  * sizeof(float), 0, ndev * nnode * count * sizeof(float));
        result_buff[i] = (float*)sal_malloc(ndev * nnode * count * sizeof(float));
        sal_memset(result_buff[i], ndev * nnode * count * sizeof(float), 0, ndev * nnode * count * sizeof(float));
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
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < ndev * nnode * count; j++)
        {
            result_buff[i][j] =  1.0 ;
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
        //identify << i + ndev * noderank;
        identify << groupRanks[i+ ndev * noderank];
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
        para_info[i].groupRanksNum = GOUP_DEV_NUM;
        para_info[i].groupName = "group1" ;
        para_info[i].pGroupRanks = groupRanks;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", all_gather_8p_subgroup_task, (void*)&para_info[i]);
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
    bool subGroupFlag = false;

    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {

        u32 wordGroupId = std::stoi(para_info[j].identify);
        for(u32 i = 0;i < para_info->groupRanksNum;i++) {
            if(wordGroupId == para_info->pGroupRanks[i]) {
                subGroupFlag = true;
            }
        }
        if (subGroupFlag) {
            for (s32 i = 0; i < count * ndev * nnode; i++)
          {
                float res = result_buff[j][i];
                float recv = recvbuf[j][i];

                if (abs(res - recv) > 1e-6)
                {
                    HCCL_ERROR("worldgroupid[%d] noderank:%d recvbuf[%d][%d]:%f res[%d][%d]:%f\n",wordGroupId, noderank, j, i, recv,j,i,res);
                    errors++;
                    break;
                }
            }
        }
        subGroupFlag = false;
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    if (rank == 0)
    {
        if (errors)
        {
            HCCL_ERROR("%d errors. Test FAILED.\n", errors);
        }
        else
        {
            HCCL_INFO("Test PASSED.\n");
        }
    }

 /*   if (noderank == 0)
        for (s32 j = 0; j < ndev; j++) {
            for (s32 i = 0; i < count * ndev * nnode; i++)
            {
                printf("recvbuf[%d][%d] = %f \n", j, i, recvbuf[j][i]);
            }
        }
*/

    for (s32 j = 0; j < ndev; j++)
    {
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        sal_free(result_buff[j]);


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

#if 0
TEST_F(MPI_AllGatherInter_Test, st_mpi_all_gather_8p_ring_char_count1023)
{
    nlohmann::json rank_table = rank_table_910_2server_8rank;
    char file_name_t[] = "./st_mpi_all_gather_8p_ring_char_count1023.json";
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
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s8* result_buff[DEV_NUM_8];
    s8* sendbuf[DEV_NUM_8];
    s8* recvbuf[DEV_NUM_8];

    s32 sync_value = 0;
    s32 noderank = 0;
    aclrtStream stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];
    HcclDataType datatype = HCCL_DATA_TYPE_INT8;

    s32 count = 1023;
    s32 ndev = DEV_NUM_8;
    set_board_id(0x0000);
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(s8), 0, count * sizeof(s8));

        ret = hrtMalloc((void**)&recvbuf[i], ndev * nnode * count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], ndev * nnode * count * sizeof(s8), 0,  ndev * nnode * count * sizeof(s8));

        result_buff[i] = (s8*)sal_malloc(ndev * nnode * count * sizeof(s8));
        sal_memset(result_buff[i], ndev * nnode * count * sizeof(s8), 0, ndev * nnode * count * sizeof(s8));
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[j][i] = 1.0;
        }
    }

    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < ndev * nnode * count; j++)
        {
            result_buff[i][j] =  1.0;
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
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
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
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = aclrtSynchronizeStream(stream[j]);
       EXPECT_EQ(rt_ret, ACL_SUCCESS);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count * ndev * nnode; i++)
        {
            s8 res = result_buff[j][i];
            s8 recv = recvbuf[j][i];

            if (res != recv)
            {
                HCCL_ERROR("node:%d result[%d][%d]:%f sendbuf:%f\n", noderank, j, i, recvbuf[j][i] , sendbuf[j][i]);
                errors++;
                break;
            }
        }
    }

  /*
    if (noderank == 0)
        for (s32 j = 0; j < ndev; j++) {
            for (s32 i = 0; i < count * ndev * nnode; i++)
            {
                printf("recvbuf[%d][%d] = %f \n", j, i, recvbuf[j][i]);
            }
        }
*/
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
        sal_free(result_buff[j]);
        rt_ret = aclrtDestroyStream(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0x0000);
    remove(file_name_t);

    EXPECT_EQ(errors, 0);
}

#endif

#if 0
TEST_F(MPI_AllGatherInter_Test, st_mpi_all_gather_8p_ring_float32_count3)
{
    nlohmann::json rank_table = rank_table_910_2server_8rank;
    char file_name_t[] = "./st_mpi_all_gather_8p_ring_float32.json";
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

    s32 count = 3;
    s32 ndev = DEV_NUM_8;
    set_board_id(0x0000);
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(float), 0, count * sizeof(float));

        ret = hrtMalloc((void**)&recvbuf[i], ndev * nnode * count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], ndev * nnode * count * sizeof(float), 0,  ndev * nnode * count * sizeof(float));

        result_buff[i] = (float*)sal_malloc(ndev * nnode * count * sizeof(float));
        sal_memset(result_buff[i], ndev * nnode * count * sizeof(float), 0, ndev * nnode * count * sizeof(float));
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[j][i] = 1.0;
        }
    }

    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < ndev * nnode * count; j++)
        {
            result_buff[i][j] =  1.0;
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
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
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
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = aclrtSynchronizeStream(stream[j]);
       EXPECT_EQ(rt_ret, ACL_SUCCESS);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count * ndev * nnode; i++)
        {
            float res = result_buff[j][i];
            float recv = recvbuf[j][i];

            if (abs(res - recv) > 1e-6)
            {
                HCCL_ERROR("node:%d result[%d][%d]:%f sendbuf:%f\n", noderank, j, i, recvbuf[j][i] , sendbuf[j][i]);
                errors++;
                break;
            }
        }
    }

  /*
    if (noderank == 0)
        for (s32 j = 0; j < ndev; j++) {
            for (s32 i = 0; i < count * ndev * nnode; i++)
            {
                printf("recvbuf[%d][%d] = %f \n", j, i, recvbuf[j][i]);
            }
        }
*/
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
        sal_free(result_buff[j]);
        rt_ret = aclrtDestroyStream(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0x0000);
    remove(file_name_t);

    EXPECT_EQ(errors, 0);
}
#endif

#if 0
TEST_F(MPI_AllGatherInter_Test, st_mpi_all_gather_8p_ring_float32_count1048581)  //  1024*1024+5
{
    nlohmann::json rank_table = rank_table_910_2server_8rank;
    char file_name_t[] = "./st_mpi_all_gather_8p_ring_float32.json";
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

    s32 count = 1024*1024+5;
    s32 ndev = DEV_NUM_8;
    set_board_id(0x0000);
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(float), 0, count * sizeof(float));

        ret = hrtMalloc((void**)&recvbuf[i], ndev * nnode * count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], ndev * nnode * count * sizeof(float), 0,  ndev * nnode * count * sizeof(float));

        result_buff[i] = (float*)sal_malloc(ndev * nnode * count * sizeof(float));
        sal_memset(result_buff[i], ndev * nnode * count * sizeof(float), 0, ndev * nnode * count * sizeof(float));
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[j][i] = 1.0;
        }
    }

    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < ndev * nnode * count; j++)
        {
            result_buff[i][j] =  1.0;
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
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
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
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = aclrtSynchronizeStream(stream[j]);
       EXPECT_EQ(rt_ret, ACL_SUCCESS);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count * ndev * nnode; i++)
        {
            float res = result_buff[j][i];
            float recv = recvbuf[j][i];

            if (abs(res - recv) > 1e-6)
            {
                HCCL_ERROR("node:%d result[%d][%d]:%f sendbuf:%f\n", noderank, j, i, recvbuf[j][i] , sendbuf[j][i]);
                errors++;
                break;
            }
        }
    }

 /*
    if (noderank == 0)
        for (s32 j = 0; j < ndev; j++) {
            for (s32 i = 0; i < count * ndev * nnode; i++)
            {
                printf("recvbuf[%d][%d] = %f \n", j, i, recvbuf[j][i]);
            }
        }
 */
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
        sal_free(result_buff[j]);
        rt_ret = aclrtDestroyStream(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0x0000);
    remove(file_name_t);

    EXPECT_EQ(errors, 0);
}
#endif

#if 0
TEST_F(MPI_AllGatherInter_Test, st_mpi_allgather_8ranks_2server_int32_count1025)
{
    nlohmann::json g_rank_table_16p_V80 = rank_table_910_2server_8rank;
    char file_name_t[] = "./st_mpi_allgather_8ranks_2server_int32_count1025.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << g_rank_table_16p_V80 << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    u32 groupRanks[GOUP_DEV_NUM] = {0,1,2,3,8,9,10,11};

    s32 nnode, rank, errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    int32_t* result_buff[DEV_NUM_4];
    int32_t* sendbuf[DEV_NUM_4];
    int32_t* recvbuf[DEV_NUM_4];

    s32 sync_value = 0;
    s32 noderank = 0;
    aclrtStream stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];
    HcclDataType datatype = HCCL_DATA_TYPE_INT32;

    s32 count = 1025;
    s32 ndev = DEV_NUM_4;

    set_board_id(0);
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(int32_t));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(int32_t), 0, count * sizeof(int32_t));
        ret = hrtMalloc((void **)&recvbuf[i], ndev * nnode * count * sizeof(int32_t));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], ndev * nnode * count  * sizeof(int32_t), 0, ndev * nnode * count * sizeof(int32_t));
        result_buff[i] = (int32_t*)sal_malloc(ndev * nnode * count * sizeof(int32_t));
        sal_memset(result_buff[i], ndev * nnode * count * sizeof(int32_t), 0, ndev * nnode * count * sizeof(int32_t));
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
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < ndev * nnode * count; j++)
        {
            result_buff[i][j] =  1.0 ;
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
        //identify << i + ndev * noderank;
        identify << groupRanks[i+ ndev * noderank];
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
        para_info[i].groupRanksNum = GOUP_DEV_NUM;
        para_info[i].groupName = "group1" ;
        para_info[i].pGroupRanks = groupRanks;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", all_gather_8p_subgroup_task, (void*)&para_info[i]);
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
    bool subGroupFlag = false;

    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {

        u32 wordGroupId = std::stoi(para_info[j].identify);
        for(u32 i = 0;i < para_info->groupRanksNum;i++) {
            if(wordGroupId == para_info->pGroupRanks[i]) {
                subGroupFlag = true;
            }
        }
        if (subGroupFlag) {
            for (s32 i = 0; i < count * ndev * nnode; i++)
          {
                int32_t res = result_buff[j][i];
                int32_t recv = recvbuf[j][i];

                if (res != recv)
                {
                    HCCL_ERROR("worldgroupid[%d] noderank:%d recvbuf[%d][%d]:%f res[%d][%d]:%f\n",wordGroupId, noderank, j, i, recv,j,i,res);
                    errors++;
                    break;
                }
            }
        }
        subGroupFlag = false;
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    if (rank == 0)
    {
        if (errors)
        {
            HCCL_ERROR("%d errors. Test FAILED.\n", errors);
        }
        else
        {
            HCCL_INFO("Test PASSED.\n");
        }
    }

 /*   if (noderank == 0)
        for (s32 j = 0; j < ndev; j++) {
            for (s32 i = 0; i < count * ndev * nnode; i++)
            {
                printf("recvbuf[%d][%d] = %f \n", j, i, recvbuf[j][i]);
            }
        }
*/

    for (s32 j = 0; j < ndev; j++)
    {
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        sal_free(result_buff[j]);


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
#if 0
TEST_F(MPI_AllGatherInter_Test, st_mpi_allgather_2ranks_2server_float_count1048571)  //  1024*1024-5
{
    nlohmann::json g_rank_table_4p_V80 =
    {
        {"status", "completed"},
        {"version", "1.0"},
        {"server_count", "2"},
        {
            "server_list",
            {
                {
                    {"server_id", "10.0.0.10"},
                    {"host_nic_ip", "192.168.0.12:0,192.168.1.12:199"},
                    {
                        "device",
                        {
                            {   {"rank_id", "0"},
                                {"device_id", "0"},
                                {"device_ip", "192.168.0.12,192.168.0.35"}

                            },
                            {   {"rank_id", "1"},
                                {"device_id", "4"},
                                {"device_ip", "192.168.4.12,192.168.4.35"}

                            },
                        }
                    },
                }, 
                {
                    {"server_id", "10.0.0.11"},
                    {"host_nic_ip", "192.168.0.12:0,192.168.1.12:199"},
                    {
                        "device",
                        {
                            {   {"rank_id", "2"},
                                {"device_id", "0"},
                                {"device_ip", "192.168.8.12,192.168.8.35"}

                            },
                            {   {"rank_id", "3"},
                                {"device_id", "4"},
                                {"device_ip", "192.168.12.12,192.168.12.35"}

                            },
                        }
                    },
                }
            }
        }
    };
    char file_name_t[] = "./st_mpi_allgather_2ranks_2server_float.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << g_rank_table_4p_V80 << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    //u32 groupRanks[GOUP_DEV_NUM] = {0,1,2,3,8,9,10,11};
    
    ResetInitState();
    s32 nPID = SalGetPid();
    string strPID = std::to_string(nPID);
    string config_file_name = "./st_mpi_allgather_2ranks_2server_float" + strPID;
    ResetConfigFilePathName(config_file_name);
    remove(config_file_name.c_str());
    ofstream configfile(config_file_name);
    bool bRes = configfile.is_open();
    if(bRes)
    {
        HCCL_INFO("open %s success", config_file_name.c_str());
    }
    else
    {
        HCCL_ERROR("open %s failed", config_file_name.c_str());
    }
    EXPECT_EQ(bRes, true);
    configfile << "check_rank_switch=0" << endl;
    configfile.close();
    InitConfigFileParam(config_file_name);

    s32 nnode, rank, errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    float* result_buff[DEV_NUM_2];
    float* sendbuf[DEV_NUM_2];
    float* recvbuf[DEV_NUM_2];

    s32 sync_value = 0;
    s32 noderank = 0;
    aclrtStream stream[DEV_NUM_2];
    sal_thread_t tid[DEV_NUM_2];
    para_t para_info[DEV_NUM_2];
    HcclDataType datatype = HCCL_DATA_TYPE_FP32;

    s32 count = 1024*1024-5;
    s32 ndev = DEV_NUM_2;

    set_board_id(0);
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
        sal_memset(sendbuf[i], count * sizeof(float), 0, count * sizeof(float));
        ret = hrtMalloc((void **)&recvbuf[i], ndev * nnode * count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], ndev * nnode * count  * sizeof(float), 0, ndev * nnode * count * sizeof(float));
        result_buff[i] = (float*)sal_malloc(ndev * nnode * count * sizeof(float));
        sal_memset(result_buff[i], ndev * nnode * count * sizeof(float), 0, ndev * nnode * count * sizeof(float));
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
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < ndev * nnode * count; j++)
        {
            result_buff[i][j] =  1.0 ;
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
        //identify << i + ndev * noderank;
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
        if(i == 0 || i == 2) {
            para_info[i].device_id = 0;
        } else {
            para_info[i].device_id = 4;
        }
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

    s32 resRemove = remove(config_file_name.c_str());
    EXPECT_EQ(resRemove, 0);
    configfile.open(config_file_name);
    bRes = configfile.is_open();
    if(bRes)
    {
        HCCL_INFO("open %s success", config_file_name.c_str());
    }
    else
    {
        HCCL_ERROR("open %s failed", config_file_name.c_str());
    }
    EXPECT_EQ(bRes, true);
    configfile << "check_rank_switch=1" << endl;
    configfile.close();
    InitConfigFileParam(config_file_name);
    resRemove = remove(config_file_name.c_str());
    EXPECT_EQ(resRemove, 0);
    ResetInitState();

    //获取stream的操作的同步信号量
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = aclrtSynchronizeStream(stream[j]);
       EXPECT_EQ(rt_ret, ACL_SUCCESS);
    }

    MPI_Barrier(MPI_COMM_WORLD);


    /*check result*/

    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count * ndev * nnode; i++)
        {
            float res = result_buff[j][i];
            float recv = recvbuf[j][i];

            if (abs(res - recv) > 1e-6)
            {
                HCCL_ERROR("node:%d result[%d][%d]:%f sendbuf:%f\n", noderank, j, i, recvbuf[j][i] , sendbuf[j][i]);
                errors++;
                break;
            }
        }
    }

    // for (s32 j = 0; j < ndev; j++)
    // {

    //     u32 wordGroupId = std::stoi(para_info[j].identify);
    //     for(u32 i = 0;i < para_info->groupRanksNum;i++) {
    //         if(wordGroupId == para_info->pGroupRanks[i]) {
    //             subGroupFlag = true;
    //         }
    //     }
    //     if (subGroupFlag) {
    //         for (s32 i = 0; i < count * ndev * nnode; i++)
    //       {
    //             float res = result_buff[j][i];
    //             float recv = recvbuf[j][i];

    //             if (abs(res - recv) > 1e-6)
    //             {
    //                 HCCL_ERROR("worldgroupid[%d] noderank:%d recvbuf[%d][%d]:%f res[%d][%d]:%f\n",wordGroupId, noderank, j, i, recv,j,i,res);
    //                 errors++;
    //                 break;
    //             }
    //         }
    //     }
    //     subGroupFlag = false;
    // }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    if (rank == 0)
    {
        if (errors)
        {
            HCCL_ERROR("%d errors. Test FAILED.\n", errors);
        }
        else
        {
            HCCL_INFO("Test PASSED.\n");
        }
    }

 /*   if (noderank == 0)
        for (s32 j = 0; j < ndev; j++) {
            for (s32 i = 0; i < count * ndev * nnode; i++)
            {
                printf("recvbuf[%d][%d] = %f \n", j, i, recvbuf[j][i]);
            }
        }
*/

    for (s32 j = 0; j < ndev; j++)
    {
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        sal_free(result_buff[j]);
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

#if 0
TEST_F(MPI_AllGatherInter_Test, st_mpi_all_gather_8p_ring_float32_count1048581_1nic)  //  1024*1024+5
{
    nlohmann::json rank_table = rank_table_910_2server_8rank;
    char file_name_t[] = "./st_mpi_all_gather_8p_ring_float32.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << rank_table_910_2server_8rank_1nic << std::endl;
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

    s32 count = 1024*1024+5;
    s32 ndev = DEV_NUM_8;
    set_board_id(0x0000);
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(float), 0, count * sizeof(float));

        ret = hrtMalloc((void**)&recvbuf[i], ndev * nnode * count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], ndev * nnode * count * sizeof(float), 0,  ndev * nnode * count * sizeof(float));

        result_buff[i] = (float*)sal_malloc(ndev * nnode * count * sizeof(float));
        sal_memset(result_buff[i], ndev * nnode * count * sizeof(float), 0, ndev * nnode * count * sizeof(float));
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[j][i] = 1.0;
        }
    }

    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < ndev * nnode * count; j++)
        {
            result_buff[i][j] =  1.0;
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
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
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
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = aclrtSynchronizeStream(stream[j]);
       EXPECT_EQ(rt_ret, ACL_SUCCESS);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count * ndev * nnode; i++)
        {
            float res = result_buff[j][i];
            float recv = recvbuf[j][i];

            if (abs(res - recv) > 1e-6)
            {
                HCCL_ERROR("node:%d result[%d][%d]:%f sendbuf:%f\n", noderank, j, i, recvbuf[j][i] , sendbuf[j][i]);
                errors++;
                break;
            }
        }
    }

 /*
    if (noderank == 0)
        for (s32 j = 0; j < ndev; j++) {
            for (s32 i = 0; i < count * ndev * nnode; i++)
            {
                printf("recvbuf[%d][%d] = %f \n", j, i, recvbuf[j][i]);
            }
        }
 */
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
        sal_free(result_buff[j]);
        rt_ret = aclrtDestroyStream(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0x0000);
    remove(file_name_t);

    EXPECT_EQ(errors, 0);
}
#endif

#if 0
TEST_F(MPI_AllGatherInter_Test, st_mpi_all_gather_8p_ring_float32_count1048581_2nic)  //  1024*1024+5
{
    nlohmann::json rank_table = rank_table_910_2server_8rank;
    char file_name_t[] = "./st_mpi_all_gather_8p_ring_float32.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << rank_table_910_2server_8rank_2nic << std::endl;
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

    s32 count = 1024*1024+5;
    s32 ndev = DEV_NUM_8;
    set_board_id(0x0000);
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(float), 0, count * sizeof(float));

        ret = hrtMalloc((void**)&recvbuf[i], ndev * nnode * count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], ndev * nnode * count * sizeof(float), 0,  ndev * nnode * count * sizeof(float));

        result_buff[i] = (float*)sal_malloc(ndev * nnode * count * sizeof(float));
        sal_memset(result_buff[i], ndev * nnode * count * sizeof(float), 0, ndev * nnode * count * sizeof(float));
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[j][i] = 1.0;
        }
    }

    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < ndev * nnode * count; j++)
        {
            result_buff[i][j] =  1.0;
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
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
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
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = aclrtSynchronizeStream(stream[j]);
       EXPECT_EQ(rt_ret, ACL_SUCCESS);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count * ndev * nnode; i++)
        {
            float res = result_buff[j][i];
            float recv = recvbuf[j][i];

            if (abs(res - recv) > 1e-6)
            {
                HCCL_ERROR("node:%d result[%d][%d]:%f sendbuf:%f\n", noderank, j, i, recvbuf[j][i] , sendbuf[j][i]);
                errors++;
                break;
            }
        }
    }

 /*
    if (noderank == 0)
        for (s32 j = 0; j < ndev; j++) {
            for (s32 i = 0; i < count * ndev * nnode; i++)
            {
                printf("recvbuf[%d][%d] = %f \n", j, i, recvbuf[j][i]);
            }
        }
 */
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
        sal_free(result_buff[j]);
        rt_ret = aclrtDestroyStream(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0x0000);
    remove(file_name_t);

    EXPECT_EQ(errors, 0);
}
#endif

#if 0
TEST_F(MPI_AllGatherInter_Test, st_mpi_all_gather_8p_ring_float32_count1048581_4nic)  //  1024*1024+5
{
    nlohmann::json rank_table = rank_table_910_2server_8rank;
    char file_name_t[] = "./st_mpi_all_gather_8p_ring_float32.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << rank_table_910_2server_8rank_4nic << std::endl;
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

    s32 count = 1024*1024+5;
    s32 ndev = DEV_NUM_8;
    set_board_id(0x0000);
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(float), 0, count * sizeof(float));

        ret = hrtMalloc((void**)&recvbuf[i], ndev * nnode * count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], ndev * nnode * count * sizeof(float), 0,  ndev * nnode * count * sizeof(float));

        result_buff[i] = (float*)sal_malloc(ndev * nnode * count * sizeof(float));
        sal_memset(result_buff[i], ndev * nnode * count * sizeof(float), 0, ndev * nnode * count * sizeof(float));
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[j][i] = 1.0;
        }
    }

    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < ndev * nnode * count; j++)
        {
            result_buff[i][j] =  1.0;
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
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
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
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = aclrtSynchronizeStream(stream[j]);
       EXPECT_EQ(rt_ret, ACL_SUCCESS);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count * ndev * nnode; i++)
        {
            float res = result_buff[j][i];
            float recv = recvbuf[j][i];

            if (abs(res - recv) > 1e-6)
            {
                HCCL_ERROR("node:%d result[%d][%d]:%f sendbuf:%f\n", noderank, j, i, recvbuf[j][i] , sendbuf[j][i]);
                errors++;
                break;
            }
        }
    }

 /*
    if (noderank == 0)
        for (s32 j = 0; j < ndev; j++) {
            for (s32 i = 0; i < count * ndev * nnode; i++)
            {
                printf("recvbuf[%d][%d] = %f \n", j, i, recvbuf[j][i]);
            }
        }
 */
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
        sal_free(result_buff[j]);
        rt_ret = aclrtDestroyStream(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0x0000);
    remove(file_name_t);

    EXPECT_EQ(errors, 0);
}
#endif

#if 0
TEST_F(MPI_AllGatherInter_Test, st_mpi_allgather_2ranks_2server_float_count1048571_id_nosame)  //  1024*1024-5
{
    nlohmann::json g_rank_table_4p_V80 =
    {
        {"status", "completed"},
        {"version", "1.0"},
        {"server_count", "2"},
        {
            "server_list",
            {
                {
                    {"server_id", "10.0.0.10"},
                    {"host_nic_ip", "192.168.0.12:0"},
                    {
                        "device",
                        {
                            {   {"rank_id", "0"},
                                {"device_id", "0"},
                                {"device_ip", "192.168.0.12"}

                            },
                            {   {"rank_id", "1"},
                                {"device_id", "4"},
                                {"device_ip", "192.168.4.12"}

                            },
                        }
                    },
                }, 
                {
                    {"server_id", "10.0.0.11"},
                    {"host_nic_ip", "192.168.0.12:0"},
                    {
                        "device",
                        {
                            {   {"rank_id", "2"},
                                {"device_id", "2"},
                                {"device_ip", "192.168.8.12"}

                            },
                            {   {"rank_id", "3"},
                                {"device_id", "5"},
                                {"device_ip", "192.168.12.12"}

                            },
                        }
                    },
                }
            }
        }
    };
    char file_name_t[] = "./st_mpi_allgather_2ranks_2server_float_id_nosame.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << g_rank_table_4p_V80 << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();
    set_board_id(0X1E);
    //u32 groupRanks[GOUP_DEV_NUM] = {0,1,2,3,8,9,10,11};
    
    

    s32 nnode, rank, errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    float* result_buff[DEV_NUM_2];
    float* sendbuf[DEV_NUM_2];
    float* recvbuf[DEV_NUM_2];

    s32 sync_value = 0;
    s32 noderank = 0;
    aclrtStream stream[DEV_NUM_2];
    sal_thread_t tid[DEV_NUM_2];
    para_t para_info[DEV_NUM_2];
    HcclDataType datatype = HCCL_DATA_TYPE_FP32;

    s32 count = 1024*1024-5;
    s32 ndev = DEV_NUM_2;

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
        sal_memset(sendbuf[i], count * sizeof(float), 0, count * sizeof(float));
        ret = hrtMalloc((void **)&recvbuf[i], ndev * nnode * count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], ndev * nnode * count  * sizeof(float), 0, ndev * nnode * count * sizeof(float));
        result_buff[i] = (float*)sal_malloc(ndev * nnode * count * sizeof(float));
        sal_memset(result_buff[i], ndev * nnode * count * sizeof(float), 0, ndev * nnode * count * sizeof(float));
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
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < ndev * nnode * count; j++)
        {
            result_buff[i][j] =  1.0 ;
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
        //identify << i + ndev * noderank;
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
        if(i == 0 && noderank == 0) {
            para_info[i].device_id = 0 ;
            SetDevicePlaneId(0, 0);
        } else if (i == 1 && noderank == 0) {
            para_info[i].device_id = 4 ;
            SetDevicePlaneId(4, 1);
        }else if (i == 0 && noderank == 1) {
            para_info[i].device_id = 2 ;
            SetDevicePlaneId(2, 0);
        }else if (i == 1 && noderank == 1) {
            para_info[i].device_id = 5 ;
            SetDevicePlaneId(5, 1);
        }
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
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = aclrtSynchronizeStream(stream[j]);
       EXPECT_EQ(rt_ret, ACL_SUCCESS);
    }

    MPI_Barrier(MPI_COMM_WORLD);


    /*check result*/

    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count * ndev * nnode; i++)
        {
            float res = result_buff[j][i];
            float recv = recvbuf[j][i];

            if (abs(res - recv) > 1e-6)
            {
                HCCL_ERROR("node:%d result[%d][%d]:%f sendbuf:%f\n", noderank, j, i, recvbuf[j][i] , sendbuf[j][i]);
                errors++;
                break;
            }
        }
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    if (rank == 0)
    {
        if (errors)
        {
            HCCL_ERROR("%d errors. Test FAILED.\n", errors);
        }
        else
        {
            HCCL_INFO("Test PASSED.\n");
        }
    }

 /*   if (noderank == 0)
        for (s32 j = 0; j < ndev; j++) {
            for (s32 i = 0; i < count * ndev * nnode; i++)
            {
                printf("recvbuf[%d][%d] = %f \n", j, i, recvbuf[j][i]);
            }
        }
*/

    for (s32 j = 0; j < ndev; j++)
    {
        aclrtFree(sendbuf[j]);
        aclrtFree(recvbuf[j]);
        sal_free(result_buff[j]);
    }

    for (s32 j = 0; j < ndev; j++)
    {
        rt_ret = aclrtDestroyStream(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0);
    ClearDevicePlaneId();
    remove(file_name_t);

    EXPECT_EQ(errors, 0);
}
#endif