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

#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "rt_external.h"

#include <assert.h>
#include <semaphore.h>
#include <signal.h>
#include <syscall.h>
#include <sys/prctl.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
// #include <cce/dnn.h>
#include <securec.h>

#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <driver/ascend_hal.h>
#include "rt_external.h"

#include "dlra_function.h"

#include "hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "hccl_comm_pub.h"
#include "externalinput_pub.h"
#include "externalinput.h"
#include "workflow_pub.h"
#include "rank_consistentcy_checker.h"
#define private public
#include "alltoallv_staged_pairwise.h"
#undef private
#include "sal.h"
#include "config.h"
#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"

#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include "param_check_pub.h"
#include "hcom_private.h"

#include <iostream>
#include <fstream>

using namespace std;
using namespace hccl;

class Inter_AlltoAllV_Staged_FFTS_Test : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "Inter_AlltoAllV_Staged_FFTS_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "Inter_AlltoAllV_Staged_FFTS_Test TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        DlTdtFunction::GetInstance().DlTdtFunctionInit();
        TsdOpen(1, 2);
    
        static s32 call_cnt = 0;
        string name = std::to_string(call_cnt++) + "_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name.c_str());
        std::cout << "A Inter_AlltoAllV_Staged_FFTS_Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        TsdClose(1);
        std::cout << "A Inter_AlltoAllV_Staged_FFTS_Test TearDown" << std::endl;
    }
};

#define GOUP_DEV_NUM 8

typedef struct para_struct {
    HcclRootInfo rootInfo;
    std::string identify;
    s32 userRankSize;
    s32 device_id;
    s32 ranks_local;  //本服务器内的rank数

    char *file_name;
    void *sendbuff;
    void *sendCounts;
    void *sdispls;
    void *recvbuff;
    void *recvCounts;
    void *rdispls;
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

void *inter_alltoallv_staged_ffts_task(void *parg)
{
    s32 portNum = 7;
    MOCKER(hrtGetHccsPortNum)
        .stubs()
        .with(any(), outBound(portNum))
        .will(returnValue(HCCL_SUCCESS));
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

    HcclWorkflowMode mode = GetWorkflowMode();
    InitWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);

    //-----------------Set Workspace Resource Start------------------//
    u64 stream_list_size = 0;
    ret = hcom_info.pComm->GetWorkspaceSubStreamNum(stream_list_size);
    HCCL_INFO("==TMP== stream_list_size=%d", stream_list_size);
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

    }

    // 获取workspace内存大小
    string tag = "tag-alltoallv-staged-ffts";
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
    }  // linux提供一个系统调用运行进程主动让出执行权

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
    rt_ret = hcclStreamSynchronize(para_info->stream);

    if (rt_ret != RT_ERROR_NONE) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }
    //--------------Resource destroy----------------//
    for (s32 i = 0; i < stream_list_size; i++)
    {

        
        rt_ret = rtStreamDestroy(streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    ret = hcom_info.pComm->ClearOpResource(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hrtFree(memptr);

    return (nullptr);
}

#define DEV_NUM_2 2
#define DEV_NUM_4 4
#define DEV_NUM_8 8

#if 1
TEST_F(Inter_AlltoAllV_Staged_FFTS_Test, ut_inter_alltoallv_staged_4ranks_1server_float_ffts)
{
    HcclResult ret;
    std::string algo = "level0:pairwise;level1:pairwise";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();

    nlohmann::json rank_table = rank_table_910_1server_4rank;
    char file_name_t[] = "./st_inter_alltoallv_staged_4ranks_1server_float.json";
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
    float* result_buff[DEV_NUM_4];
    float* sendbuff[DEV_NUM_4];
    float* recvbuff[DEV_NUM_4];
    u64* sendCounts[DEV_NUM_4];
    u64* sdispls[DEV_NUM_4];
    u64* recvCounts[DEV_NUM_4];
    u64* rdispls[DEV_NUM_4];

    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];
    HcclDataType sendDataType = HCCL_DATA_TYPE_FP32;
    HcclDataType recvDataType = HCCL_DATA_TYPE_FP32;

    s32 ndev = DEV_NUM_4;
    set_board_id(0x0000);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    // 每个rank上 发两个float 数据是index*2.0f

    /** 初始化输入输出缓存 */
    u64 count = nnode * ndev;
    for (u32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuff[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        for (u32 j = 0; j < count; j++) {
            sendbuff[i][j] = j;
        }

        ret = hrtMalloc((void**)&recvbuff[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuff[i], count * sizeof(float), 0, count * sizeof(float));

        result_buff[i] = (float*)sal_malloc(count * sizeof(float));
        for (u32 j = 0; j < count; j++) {
            result_buff[i][j] = i + ndev * noderank;
        }

        sendCounts[i] = (u64*)sal_malloc(count * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            sendCounts[i][j] = 1;
        }

        HCCL_INFO("==TMP== sendCounts[%llu]", sendCounts[i][0]);

        sdispls[i] = (u64*)sal_malloc(count * sizeof(u64));
        for (u32 j = 0; j < count; j++) {
            sdispls[i][j] = j;
        }

        recvCounts[i] = (u64*)sal_malloc(count * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            recvCounts[i][j] = 1;
        }

        HCCL_INFO("==TMP== recvCounts[%llu]", recvCounts[i][0]);

        rdispls[i] = (u64*)sal_malloc(count * sizeof(u64));
        for (u32 j = 0; j < count; j++) {
            rdispls[i][j] = j;
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

        para_info[i].sendDataType = sendDataType;
        para_info[i].recvDataType = recvDataType;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_alltoallv_staged_ffts_task, (void*)&para_info[i]);
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
       rt_ret = hcclStreamSynchronize(stream[j]);
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
    set_board_id(0x0000);
    remove(file_name_t);
    EXPECT_EQ(errors, 0);

    algo = "level0:ring;level1:H-D_R";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(Inter_AlltoAllV_Staged_FFTS_Test, ut_inter_alltoallv_fullmesh_4ranks_1server_float_ffts)
{
    HcclResult ret;
    std::string algo = "level0:NA;level1:pairwise";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();

    nlohmann::json rank_table = rank_table_910_1server_4rank;
    char file_name_t[] = "./st_inter_alltoallv_staged_4ranks_1server_float.json";
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
    float* result_buff[DEV_NUM_4];
    float* sendbuff[DEV_NUM_4];
    float* recvbuff[DEV_NUM_4];
    u64* sendCounts[DEV_NUM_4];
    u64* sdispls[DEV_NUM_4];
    u64* recvCounts[DEV_NUM_4];
    u64* rdispls[DEV_NUM_4];

    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];
    HcclDataType sendDataType = HCCL_DATA_TYPE_FP32;
    HcclDataType recvDataType = HCCL_DATA_TYPE_FP32;

    s32 ndev = DEV_NUM_4;
    set_board_id(0x0000);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    // 每个rank上 发两个float 数据是index*2.0f

    /** 初始化输入输出缓存 */
    u64 count = nnode * ndev;
    for (u32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuff[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        for (u32 j = 0; j < count; j++) {
            sendbuff[i][j] = j;
        }

        ret = hrtMalloc((void**)&recvbuff[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuff[i], count * sizeof(float), 0, count * sizeof(float));

        result_buff[i] = (float*)sal_malloc(count * sizeof(float));
        for (u32 j = 0; j < count; j++) {
            result_buff[i][j] = i + ndev * noderank;
        }

        sendCounts[i] = (u64*)sal_malloc(count * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            sendCounts[i][j] = 1;
        }

        HCCL_INFO("==TMP== sendCounts[%llu]", sendCounts[i][0]);

        sdispls[i] = (u64*)sal_malloc(count * sizeof(u64));
        for (u32 j = 0; j < count; j++) {
            sdispls[i][j] = j;
        }

        recvCounts[i] = (u64*)sal_malloc(count * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            recvCounts[i][j] = 1;
        }

        HCCL_INFO("==TMP== recvCounts[%llu]", recvCounts[i][0]);

        rdispls[i] = (u64*)sal_malloc(count * sizeof(u64));
        for (u32 j = 0; j < count; j++) {
            rdispls[i][j] = j;
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

        para_info[i].sendDataType = sendDataType;
        para_info[i].recvDataType = recvDataType;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_alltoallv_staged_ffts_task, (void*)&para_info[i]);
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
       rt_ret = hcclStreamSynchronize(stream[j]);
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
    set_board_id(0x0000);
    remove(file_name_t);
    EXPECT_EQ(errors, 0);

    algo = "level0:ring;level1:H-D_R";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif


