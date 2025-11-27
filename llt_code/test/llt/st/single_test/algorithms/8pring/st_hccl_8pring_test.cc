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

#include "hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub.h"
#include "llt_hccl_stub_pub.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "hccl_comm_pub.h"
#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include "network_manager_pub.h"

#include "sal.h"
#include "config.h"
#include "dlra_function.h"
#include "topoinfo_ranktableParser_pub.h"

#include "v80_rank_table.h"

#include <iostream>
#include <fstream>
#include "network_manager_pub.h"
#include "opexecounter_pub.h"
#include "param_check_pub.h"
#include "externalinput.h"
#include "transport_base_pub.h"
#include "hcom_private.h"
#include "rank_consistentcy_checker.h"
#include "tbe_vector_reduce.h"
#include "coll_comm_executor.h"

using namespace std;
using namespace hccl;

typedef struct para_struct
{
    HcclRootInfo rootInfo;
    std::string identify;
    s32 comm_num;
    s32 device_id;
    s32 ranks_local; //闁哄牜鍓氬﹢鍥礉閳ヨ櫕鐝ら柛鎰噽濞堟唵ank闁轰緤鎷�

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


void* all_reduce_8pring_task(void* parg)
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
    HcclWorkflowMode mode = GetWorkflowMode();
    HCCL_ERROR("workFlowMode is %d", mode);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();

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
    
    SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB);

    bool swapped;
    //-----------------Get Workspace Resource Start------------------//
    u64 stream_list_size = 0;
    ret = hcom_info.pComm->GetWorkspaceSubStreamNum(stream_list_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("get stream_list_size[%d] success", stream_list_size);
    vector<HcclRtStream> streamList(stream_list_size);

    rtError_t rt_ret;
    //閻㈢喐鍨氭禒宸梩ream
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = aclrtCreateStreamWithConfig(&streamList[i], 0, ACL_STREAM_PERSISTENT);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        //娴犲孩绁ind閸掔櫩odel

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

    ret = hcom_info.pComm->SetWorkspaceResource("tag_all_reduce_8pring_task", memptr, memSize, streamList);
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
    { sched_yield(); } // linux闁圭粯鍔掔欢鍨▔閳ь剚绋夐鍡涘厙缂備胶鍠曢惃鐔兼偨閵娿劎绠ラ悶娑樼焷缁绘绮欑€ｂ晛鐦滈柛鏂诲姀椤斺偓闁告垹鍎ゆ晶鐣屾偘鐏炵偓缍€

    __sync_synchronize();  // 闁圭粯甯掗崣鍡涘礃閸涱厾鎽犻悘鐐茬箻濞堜即鏁嶇仦绛嬪殸濡炪倕鎼花顓㈠箑瑜庡﹢浣烘啺娴ｅ湱婀撮柨娑樺缁查箖寮伴娑欑畳婵炲备鍓濆﹢浣规媴鐠恒劍鏆弆ock闁圭ǹ娲ｉ幎銈夋儍閸曨剚顦ч柛濠忔嫹

    ret =  hcom_info.pComm->AllReduce("tag_all_reduce_8pring_task",
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
    rt_ret = aclrtSynchronizeStream(para_info->stream);

    if ( rt_ret != RT_ERROR_NONE)
    {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }
    for (s32 i = 0; i < stream_list_size; i++)
    {

    }
    for (int i = 0; i < stream_list_size; i++)
    {
        rt_ret = aclrtDestroyStream(streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    hrtFree(memptr);
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    return (NULL);
}

void* all_reduce_8pring_task_ffts(void* parg)
{
    HcclResult ret = HCCL_SUCCESS;
    para_t* para_info = (para_t*)parg;
    s32 rank_num_tmp;
    u32 port = 16666;

    HcomInfo hcom_info;
    std::string ranktable_file = para_info->file_name;
    std::string rankTableM;
    std::string realFilePath;

    hrtSetDevice(para_info->device_id);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();

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
    hcom_info.params.deviceType = DevType::DEV_TYPE_910B;

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
    
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = aclrtCreateStreamWithConfig(&streamList[i], 0, ACL_STREAM_PERSISTENT);
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

    ret = hcom_info.pComm->SetWorkspaceResource("tag_all_reduce_8pring_task_ffts_0", memptr, memSize, streamList);
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
    { sched_yield(); } 

    __sync_synchronize();  

    ret =  hcom_info.pComm->AllReduceOutPlace("tag_all_reduce_8pring_task_ffts_0",
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
    rt_ret = aclrtSynchronizeStream(para_info->stream);

    if ( rt_ret != RT_ERROR_NONE)
    {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }
    for (s32 i = 0; i < stream_list_size; i++)
    {

    }
    for (int i = 0; i < stream_list_size; i++)
    {
        rt_ret = aclrtDestroyStream(streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    hrtFree(memptr);

    ret = NetworkManager::GetInstance(para_info->device_id).DeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = NetworkManager::GetInstance(para_info->device_id).StopVnic();

    EXPECT_EQ(ret, HCCL_SUCCESS);
    return (NULL);
}

class Hccl8pRingTest : public testing::TestWithParam<bool>
{
protected:
    // static void SetUpTestCase()
    // {
    //     std::cout << "Hccl8pRingTest SetUP" << std::endl;
    // }
    // static void TearDownTestCase()
    // {
    //     std::cout << "Hccl8pRingTest TearDown" << std::endl;
    // }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        static s32  call_cnt = 0;
        // DlTdtFunction::GetInstance().DlTdtFunctionInit();
        TsdOpen(1,2);
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        TsdClose(1);
        std::cout << "A Test TearDown" << std::endl;
    }
};
  
#define DEV_NUM_8 8
INSTANTIATE_TEST_CASE_P(FFTSSwitch, Hccl8pRingTest, testing::Bool());
#if 1
TEST_P(Hccl8pRingTest, st_allreduce_inter_char_1ring)
{

    nlohmann::json rank_table = rank_table_1server_8rank;
 

    char file_name_t[] = "./st_allreduce_inter_char_1ring.json";
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
    s8* inputbuf[DEV_NUM_8];
    s8* outputbuf[DEV_NUM_8];

    s32 sync_value = 0;

    aclrtStream stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;

    HcclReduceOp op = HCCL_REDUCE_SUM;
//    s32 count =1125;
    s32 count =10;
    s32 ndev = DEV_NUM_8;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /** 闁告帗绻傞～鎰板礌閺嶎剛缈婚柛蹇嬪劥缁额參宕欓搹鍦閻庢冻鎷� */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count, 0, count);

        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count, 0, count);

        result_buff[i] = (s8*)sal_malloc(count * sizeof(s8));
        sal_memset(result_buff[i], count, 0, count);
        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    //sendbuf 閻犙冾儏閳ь剨鎷�
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = 1;
        }
    }

    //resultbuf 閻犙冾儏閳ь剨鎷�
    for (s32 i = 0; i < ndev; ++i)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = ndev;
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

    bool fftsSwitch = 0;
    // 闁告帗绋戠紓鎾承掕箛搴ㄥ殝Dev闁汇劌鍨穕lreduce濞寸姾顕ф慨鐔虹棯鐠恒劉鏌�
    for (s32 i = 0; i < ndev; i++)
    {
        if (fftsSwitch) {
            SetFftsSwitch(true);
            InitEnvVarParam();
            tid[i] = sal_thread_create("thread0", all_reduce_8pring_task_ffts, (void*)&para_info[i]);
        } else {
            tid[i] = sal_thread_create("thread", all_reduce_8pring_task, (void*)&para_info[i]);
        }
        
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //闁兼儳鍢茶ぐ鍣憈ream闁汇劌瀚幖閿嬫媴濠婂懏鐣遍柛姘湰椤掔偞绌遍垾鍐插▏闂佽鎷�


    //闁兼儳鍢茶ぐ鍣憈ream闁汇劌瀚幖閿嬫媴濠婂懏鐣遍柛姘湰椤掔偞绌遍垾鍐插▏闂佽鎷�
    if (!fftsSwitch) {
        for (s32 i = 0; i < ndev; i++) {
            for (s32 j = 0; j < count; j++) {
                s8 res = result_buff[i][j];
                s8 recv = outputbuf[i][j];

                if (res != recv) {
                    HCCL_ERROR(" rank[%d] recvbuf[%d] recvbuf[%d] \n", i, j, res);
                }
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
        sal_free(result_buff[i]);
        rt_ret = aclrtDestroyStream(stream[i]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}
#endif
#if 0
TEST_F(Hccl8pRingTest, st_allreduce_inter_char_2ring)
{

    nlohmann::json rank_table = rank_table_1server_8rank;
 

    char file_name_t[] = "./st_allreduce_inter_char_2ring.json";
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
    s8* inputbuf[DEV_NUM_8];
    s8* outputbuf[DEV_NUM_8];

    s32 sync_value = 0;

    aclrtStream stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;

    HcclReduceOp op = HCCL_REDUCE_SUM;
 //   s32 count =2048;
    s32 count =10;
    s32 ndev = DEV_NUM_8;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /** 闁告帗绻傞～鎰板礌閺嶎剛缈婚柛蹇嬪劥缁额參宕欓搹鍦閻庢冻鎷� */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);

        sal_memset(sendbuf[i], count, 0, count);

        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count, 0, count);

        result_buff[i] = (s8*)sal_malloc(count * sizeof(s8));
        sal_memset(result_buff[i], count, 0, count);
        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    //sendbuf 閻犙冾儏閳ь剨鎷�
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = 1;
        }
    }

    //resultbuf 閻犙冾儏閳ь剨鎷�
    for (s32 i = 0; i < ndev; ++i)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = ndev;
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

    // 闁告帗绋戠紓鎾承掕箛搴ㄥ殝Dev闁汇劌鍨穕lreduce濞寸姾顕ф慨鐔虹棯鐠恒劉鏌�
    for (s32 i = 0; i < ndev; i++)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8pring_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //闁兼儳鍢茶ぐ鍣憈ream闁汇劌瀚幖閿嬫媴濠婂懏鐣遍柛姘湰椤掔偞绌遍垾鍐插▏闂佽鎷�


    //闁兼儳鍢茶ぐ鍣憈ream闁汇劌瀚幖閿嬫媴濠婂懏鐣遍柛姘湰椤掔偞绌遍垾鍐插▏闂佽鎷�
    for (s32 i = 0; i < ndev; i++)
    {
        for (s32 j = 0; j < count; j++)
        {
            s8 res = result_buff[i][j];
            s8 recv = outputbuf[i][j];

            if (res != recv)
            {
                HCCL_ERROR(" rank[%d] recvbuf[%d] recvbuf[%d] \n",i, j, res);
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
        sal_free(result_buff[i]);
        rt_ret = aclrtDestroyStream(stream[i]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}

TEST_F(Hccl8pRingTest, st_allreduce_inter_char_4rings)
{
    nlohmann::json rank_table = rank_table_1server_8rank;

    char file_name_t[] = "./st_allreduce_inter_char_4rings.json";
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
    s8* inputbuf[DEV_NUM_8];
    s8* outputbuf[DEV_NUM_8];

    s32 sync_value = 0;

    aclrtStream stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;

    HcclReduceOp op = HCCL_REDUCE_SUM;
  //  s32 count =4833;
    s32 count =10;
    s32 ndev = DEV_NUM_8;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /** 闁告帗绻傞～鎰板礌閺嶎剛缈婚柛蹇嬪劥缁额參宕欓搹鍦閻庢冻鎷� */
    for (s32 i = 0; i < ndev; i++ )
    {
         ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);

        sal_memset(sendbuf[i], count, 0, count);

        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count, 0, count);

        result_buff[i] = (s8*)sal_malloc(count * sizeof(s8));
        sal_memset(result_buff[i], count, 0, count);
        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    //sendbuf 閻犙冾儏閳ь剨鎷�
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = 1;
        }
    }

    //resultbuf 閻犙冾儏閳ь剨鎷�
    for (s32 i = 0; i < ndev; ++i)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = ndev;
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

    // 闁告帗绋戠紓鎾承掕箛搴ㄥ殝Dev闁汇劌鍨穕lreduce濞寸姾顕ф慨鐔虹棯鐠恒劉鏌�
    for (s32 i = 0; i < ndev; i++)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8pring_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //闁兼儳鍢茶ぐ鍣憈ream闁汇劌瀚幖閿嬫媴濠婂懏鐣遍柛姘湰椤掔偞绌遍垾鍐插▏闂佽鎷�


    //闁兼儳鍢茶ぐ鍣憈ream闁汇劌瀚幖閿嬫媴濠婂懏鐣遍柛姘湰椤掔偞绌遍垾鍐插▏闂佽鎷�
    for (s32 i = 0; i < ndev; i++)
    {
        for (s32 j = 0; j < count; j++)
        {
            s8 res = result_buff[i][j];
            s8 recv = outputbuf[i][j];

            if (res != recv)
            {
                HCCL_ERROR(" rank[%d] recvbuf[%d] recvbuf[%d] \n",i, j, res);
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
        sal_free(result_buff[i]);
        rt_ret = aclrtDestroyStream(stream[i]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}

TEST_F(Hccl8pRingTest, st_allreduce_inter_char_slices)
{

    nlohmann::json rank_table = rank_table_1server_8rank;

    char file_name_t[] = "./st_allreduce_inter_char_slices.json";
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
    s8* inputbuf[DEV_NUM_8];
    s8* outputbuf[DEV_NUM_8];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;

    HcclReduceOp op = HCCL_REDUCE_SUM;
//    s32 count =1024*1024*10+8;
    s32 count =10;
    s32 ndev = DEV_NUM_8;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /** 闁告帗绻傞～鎰板礌閺嶎剛缈婚柛蹇嬪劥缁额參宕欓搹鍦閻庢冻鎷� */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);

        sal_memset(sendbuf[i], count, 0, count);

        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);

        sal_memset(recvbuf[i], count, 0, count);
        result_buff[i] = (s8*)sal_malloc(count * sizeof(s8));
        sal_memset(result_buff[i], count, 0, count);
        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    //sendbuf 閻犙冾儏閳ь剨鎷�
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = 1;
        }
    }

    //resultbuf 閻犙冾儏閳ь剨鎷�
    for (s32 i = 0; i < ndev; ++i)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = ndev;
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

    // 闁告帗绋戠紓鎾承掕箛搴ㄥ殝Dev闁汇劌鍨穕lreduce濞寸姾顕ф慨鐔虹棯鐠恒劉鏌�
    for (s32 i = 0; i < ndev; i++)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8pring_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //闁兼儳鍢茶ぐ鍣憈ream闁汇劌瀚幖閿嬫媴濠婂懏鐣遍柛姘湰椤掔偞绌遍垾鍐插▏闂佽鎷�


    //闁兼儳鍢茶ぐ鍣憈ream闁汇劌瀚幖閿嬫媴濠婂懏鐣遍柛姘湰椤掔偞绌遍垾鍐插▏闂佽鎷�
    for (s32 i = 0; i < ndev; i++)
    {
        for (s32 j = 0; j < count; j++)
        {
            s8 res = result_buff[i][j];
            s8 recv = outputbuf[i][j];

            if (res != recv)
            {
                HCCL_ERROR(" rank[%d] recvbuf[%d] recvbuf[%d] \n",i, j, res);
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
        sal_free(result_buff[i]);
        rt_ret = aclrtDestroyStream(stream[i]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}
#endif
TEST_F(Hccl8pRingTest, st_allreduce_inter_float_128bytes)
{

    nlohmann::json rank_table = rank_table_1server_8rank;
    

    char file_name_t[] = "./st_allreduce_inter_float_128bytes.json";
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
    float* inputbuf[DEV_NUM_8];
    float* outputbuf[DEV_NUM_8];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];

    HcclDataType datatype = HCCL_DATA_TYPE_FP32;

    HcclReduceOp op = HCCL_REDUCE_MAX;
  //  s32 count =335;
      s32 count =10;
    s32 ndev = DEV_NUM_8;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /** 闁告帗绻傞～鎰板礌閺嶎剛缈婚柛蹇嬪劥缁额參宕欓搹鍦閻庢冻鎷� */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], (count * sizeof(float)));
        EXPECT_EQ(ret, HCCL_SUCCESS);

        sal_memset(sendbuf[i], count, 0, count);

        ret = hrtMalloc((void**)&recvbuf[i], (count * sizeof(float)));
        EXPECT_EQ(ret, HCCL_SUCCESS);

        sal_memset(recvbuf[i], count, 0, count);
        result_buff[i] = (float*)sal_malloc(count * sizeof(float));
        sal_memset(result_buff[i], count, 0, count);
        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    //sendbuf 閻犙冾儏閳ь剨鎷�
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = i*1.0;
        }
    }

    //resultbuf 閻犙冾儏閳ь剨鎷�
    for (s32 i = 0; i < ndev; ++i)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = j*1.0;
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

    // 闁告帗绋戠紓鎾承掕箛搴ㄥ殝Dev闁汇劌鍨穕lreduce濞寸姾顕ф慨鐔虹棯鐠恒劉鏌�
    for (s32 i = 0; i < ndev; i++)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8pring_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //闁兼儳鍢茶ぐ鍣憈ream闁汇劌瀚幖閿嬫媴濠婂懏鐣遍柛姘湰椤掔偞绌遍垾鍐插▏闂佽鎷�


    //闁兼儳鍢茶ぐ鍣憈ream闁汇劌瀚幖閿嬫媴濠婂懏鐣遍柛姘湰椤掔偞绌遍垾鍐插▏闂佽鎷�
    for (s32 i = 0; i < ndev; i++)
    {
        for (s32 j = 0; j < count; j++)
        {
            float res = result_buff[i][j];
            float recv = outputbuf[i][j];

              if (abs(res - recv) > 1e-6)
            {
                HCCL_ERROR("rank:%d result[%d]:%f recv[%d]:%f \n", i, j, res ,j,recv );
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

    for (s32 i = 0; i < ndev; i++)
    {
        hrtFree(sendbuf[i]);
        hrtFree(recvbuf[i]);
        sal_free(result_buff[i]);
        rt_ret = aclrtDestroyStream(stream[i]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}
#if 1
TEST_F(Hccl8pRingTest, st_allreduce_inter_float_4096bytes)
{
    nlohmann::json rank_table = rank_table_1server_8rank;
   

    char file_name_t[] = "./st_allreduce_inter_float_4096bytes.json";
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
    float* inputbuf[DEV_NUM_8];
    float* outputbuf[DEV_NUM_8];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];

    HcclDataType datatype = HCCL_DATA_TYPE_FP32;

    HcclReduceOp op = HCCL_REDUCE_MAX;
  //  s32 count =1024;
      s32 count =10;
    s32 ndev = DEV_NUM_8;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /** 闁告帗绻傞～鎰板礌閺嶎剛缈婚柛蹇嬪劥缁额參宕欓搹鍦閻庢冻鎷� */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], (count * sizeof(float)));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count, 0, count);
        ret = hrtMalloc((void**)&recvbuf[i], (count * sizeof(float)));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count, 0, count);
        result_buff[i] = (float*)sal_malloc(count * sizeof(float));
        sal_memset(result_buff[i], count, 0, count);
        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    //sendbuf 閻犙冾儏閳ь剨鎷�
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = i*1.0;
        }
    }

    //resultbuf 閻犙冾儏閳ь剨鎷�
    for (s32 i = 0; i < ndev; ++i)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = j*1.0;
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

    // 闁告帗绋戠紓鎾承掕箛搴ㄥ殝Dev闁汇劌鍨穕lreduce濞寸姾顕ф慨鐔虹棯鐠恒劉鏌�
    for (s32 i = 0; i < ndev; i++)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8pring_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //闁兼儳鍢茶ぐ鍣憈ream闁汇劌瀚幖閿嬫媴濠婂懏鐣遍柛姘湰椤掔偞绌遍垾鍐插▏闂佽鎷�


    //闁兼儳鍢茶ぐ鍣憈ream闁汇劌瀚幖閿嬫媴濠婂懏鐣遍柛姘湰椤掔偞绌遍垾鍐插▏闂佽鎷�
    for (s32 i = 0; i < ndev; i++)
    {
        for (s32 j = 0; j < count; j++)
        {
            float res = result_buff[i][j];
            float recv = outputbuf[i][j];

              if (abs(res - recv) > 1e-6)
            {
                HCCL_ERROR("rank:%d result[%d]:%f recv[%d]:%f \n", i, j, res ,j,recv );
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

    for (s32 i = 0; i < ndev; i++)
    {
        hrtFree(sendbuf[i]);
        hrtFree(recvbuf[i]);
        sal_free(result_buff[i]);
        rt_ret = aclrtDestroyStream(stream[i]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}
#endif

TEST_F(Hccl8pRingTest, st_allreduce_8P_common_ring_float)
{
    nlohmann::json rank_table = rank_table_1server_8rank;
   

    char file_name_t[] = "./st_allreduce_8P_common_ring_float.json";
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
    float* inputbuf[DEV_NUM_8];
    float* outputbuf[DEV_NUM_8];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];

    HcclDataType datatype = HCCL_DATA_TYPE_FP32;

    HcclReduceOp op = HCCL_REDUCE_MAX;
  //  s32 count =1024;
      s32 count =10;
    s32 ndev = DEV_NUM_8;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /** 闁告帗绻傞～鎰板礌閺嶎剛缈婚柛蹇嬪劥缁额參宕欓搹鍦閻庢冻鎷� */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], (count * sizeof(float)));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count, 0, count);
        ret = hrtMalloc((void**)&recvbuf[i], (count * sizeof(float)));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count, 0, count);
        result_buff[i] = (float*)sal_malloc(count * sizeof(float));
        sal_memset(result_buff[i], count, 0, count);
        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    //sendbuf 閻犙冾儏閳ь剨鎷�
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = i * 1.0;
        }
    }

    //resultbuf 閻犙冾儏閳ь剨鎷�
    for (s32 i = 0; i < ndev; ++i)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = j*1.0;
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

    // 闁告帗绋戠紓鎾承掕箛搴ㄥ殝Dev闁汇劌鍨穕lreduce濞寸姾顕ф慨鐔虹棯鐠恒劉鏌�
    for (s32 i = 0; i < ndev; i++)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8pring_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //闁兼儳鍢茶ぐ鍣憈ream闁汇劌瀚幖閿嬫媴濠婂懏鐣遍柛姘湰椤掔偞绌遍垾鍐插▏闂佽鎷�


    //闁兼儳鍢茶ぐ鍣憈ream闁汇劌瀚幖閿嬫媴濠婂懏鐣遍柛姘湰椤掔偞绌遍垾鍐插▏闂佽鎷�
    for (s32 i = 0; i < ndev; i++)
    {
        for (s32 j = 0; j < count; j++)
        {
            float res = result_buff[i][j];
            float recv = outputbuf[i][j];

            if (abs(res - recv) > 1e-6)
            {
                HCCL_ERROR("rank:%d result[%d]:%f recv[%d]:%f \n", i, j, res , j, recv );
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

    for (s32 i = 0; i < ndev; i++)
    {
        hrtFree(sendbuf[i]);
        hrtFree(recvbuf[i]);
        sal_free(result_buff[i]);
        rt_ret = aclrtDestroyStream(stream[i]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}
#if 1
TEST_F(Hccl8pRingTest, st_allreduce_intra_8P_cloud)
{
    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"group_count", "1"},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"instance_count", "4"},
                    {"device_count", "8"},
                    {
                        "instance_list",
                        {
                            {   {"pod_name", "tf-0"},
                                {"server_id", "10.0.0.10"},
                                {
                                    "devices",
                                    {
                                        {   {"device_id", "0"},
                                            {"device_ip", "192.168.0.10"}
                                        },
                                        {   {"device_id", "1"},
                                            {"device_ip", "192.168.0.11"}
                                        }
                                    }
                                }
                            },
                            {   {"pod_name", "tf-1"},
                                {"server_id", "10.0.0.10"},
                                {
                                    "devices",
                                    {
                                        {   {"device_id", "2"},
                                            {"device_ip", "192.168.0.12"}
                                        },
                                        {   {"device_id", "3"},
                                            {"device_ip", "192.168.0.13"}
                                        }
                                    }
                                }
                            },
                            {   {"pod_name", "tf-2"},
                                {"server_id", "10.0.0.10"},
                                {
                                    "devices",
                                    {
                                        {   {"device_id", "4"},
                                            {"device_ip", "192.168.0.14"}
                                        },
                                        {   {"device_id", "5"},
                                            {"device_ip", "192.168.0.15"}
                                        }
                                    }
                                }
                            },
                            {   {"pod_name", "tf-3"},
                                {"server_id", "10.0.0.10"},
                                {
                                    "devices",
                                    {
                                        {   {"device_id", "6"},
                                            {"device_ip", "192.168.0.16"}
                                        },
                                        {   {"device_id", "7"},
                                            {"device_ip", "192.168.0.17"}
                                        }
                                    }
                                }
                            },
                        }
                    },
                }
            }
        },
    };

    char file_name_t[] = "./st_allreduce_intra_8P_cloud.json";
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
    float* inputbuf[DEV_NUM_8];
    float* outputbuf[DEV_NUM_8];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];

    HcclDataType datatype = HCCL_DATA_TYPE_FP32;

    HcclReduceOp op = HCCL_REDUCE_MAX;
    s32 count =1111;
    s32 ndev = DEV_NUM_8;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("test allreduce");
    /** 闁告帗绻傞～鎰板礌閺嶎剛缈婚柛蹇嬪劥缁额參宕欓搹鍦閻庢冻鎷� */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], (count * sizeof(float)));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count, 0, count);
        ret = hrtMalloc((void**)&recvbuf[i], (count * sizeof(float)));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count, 0, count);
        result_buff[i] = (float*)sal_malloc(count * sizeof(float));
        sal_memset(result_buff[i], count, 0, count);
        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    //sendbuf 閻犙冾儏閳ь剨鎷�
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = i*1.0;
        }
    }

    //resultbuf 閻犙冾儏閳ь剨鎷�
    for (s32 i = 0; i < ndev; ++i)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = j*1.0;
        }
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = aclrtCreateStream(&stream[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    para_info[0].identify = "tf-0";
    para_info[1].identify = "tf-0";
    para_info[2].identify = "tf-1";
    para_info[3].identify = "tf-1";
    para_info[4].identify = "tf-2";
    para_info[5].identify = "tf-2";
    para_info[6].identify = "tf-3";
    para_info[7].identify = "tf-3";

    for (s32 i = 0; i < ndev; i++)
    {
        sal_memcpy(&para_info[i].rootInfo, sizeof(HcclRootInfo), &rootInfo, sizeof(HcclRootInfo));
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

    // 闁告帗绋戠紓鎾承掕箛搴ㄥ殝Dev闁汇劌鍨穕lreduce濞寸姾顕ф慨鐔虹棯鐠恒劉鏌�
    for (s32 i = 0; i < ndev; i++)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8pring_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //闁兼儳鍢茶ぐ鍣憈ream闁汇劌瀚幖閿嬫媴濠婂懏鐣遍柛姘湰椤掔偞绌遍垾鍐插▏闂佽鎷�


    //闁兼儳鍢茶ぐ鍣憈ream闁汇劌瀚幖閿嬫媴濠婂懏鐣遍柛姘湰椤掔偞绌遍垾鍐插▏闂佽鎷�
    for (s32 i = 0; i < ndev; i++)
    {
        for (s32 j = 0; j < count; j++)
        {
            float res = result_buff[i][j];
            float recv = outputbuf[i][j];

              if (abs(res - recv) > 1e-6)
            {
                HCCL_ERROR("rank:%d result[%d]:%f recv[%d]:%f \n", i, j, res ,j,recv );
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

    for (s32 i = 0; i < ndev; i++)
    {
        hrtFree(sendbuf[i]);
        hrtFree(recvbuf[i]);
        sal_free(result_buff[i]);
        rt_ret = aclrtDestroyStream(stream[i]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}
#endif

TEST_F(Hccl8pRingTest, st_allreduce_inter_float_3Mcounts)
{
    nlohmann::json rank_table = rank_table_1server_8rank;
   

    char file_name_t[] = "./st_allreduce_inter_float_3Mcounts.json";
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
    float* inputbuf[DEV_NUM_8];
    float* outputbuf[DEV_NUM_8];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];

    HcclDataType datatype = HCCL_DATA_TYPE_FP32;

    HcclReduceOp op = HCCL_REDUCE_MAX;
//    s32 count = 1024 * 1024 * 3;
    s32 count =10;
    s32 ndev = DEV_NUM_8;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    /** 闁告帗绻傞～鎰板礌閺嶎剛缈婚柛蹇嬪劥缁额參宕欓搹鍦閻庢冻鎷� */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], (count * sizeof(float)));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count, 0, count);
        ret = hrtMalloc((void**)&recvbuf[i], (count * sizeof(float)));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count, 0, count);
        result_buff[i] = (float*)sal_malloc(count * sizeof(float));
        sal_memset(result_buff[i], count, 0, count);
        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    //sendbuf 閻犙冾儏閳ь剨鎷�
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = i*1.0;
        }
    }

    //resultbuf 閻犙冾儏閳ь剨鎷�
    for (s32 i = 0; i < ndev; ++i)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = j * 1.0;
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

    // 闁告帗绋戠紓鎾承掕箛搴ㄥ殝Dev闁汇劌鍨穕lreduce濞寸姾顕ф慨鐔虹棯鐠恒劉鏌�
    for (s32 i = 0; i < ndev; i++)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8pring_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //闁兼儳鍢茶ぐ鍣憈ream闁汇劌瀚幖閿嬫媴濠婂懏鐣遍柛姘湰椤掔偞绌遍垾鍐插▏闂佽鎷�


    //闁兼儳鍢茶ぐ鍣憈ream闁汇劌瀚幖閿嬫媴濠婂懏鐣遍柛姘湰椤掔偞绌遍垾鍐插▏闂佽鎷�
    for (s32 i = 0; i < ndev; i++)
    {
        for (s32 j = 0; j < count; j++)
        {
            float res = result_buff[i][j];
            float recv = outputbuf[i][j];

              if (abs(res - recv) > 1e-6)
            {
                HCCL_ERROR("rank:%d result[%d]:%f recv[%d]:%f \n", i, j, res ,j,recv );
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

    for (s32 i = 0; i < ndev; i++)
    {
        hrtFree(sendbuf[i]);
        hrtFree(recvbuf[i]);
        sal_free(result_buff[i]);
        rt_ret = aclrtDestroyStream(stream[i]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}


TEST_F(Hccl8pRingTest, st_allreduce_inter_char_padding)
{

    nlohmann::json rank_table = rank_table_1server_8rank;
   

    char file_name_t[] = "./st_allreduce_inter_char_padding.json";
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
    s8* inputbuf[DEV_NUM_8];
    s8* outputbuf[DEV_NUM_8];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;

    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = 8832;
    s32 ndev = DEV_NUM_8;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /** 闁跨喐鏋婚幏宄邦潗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐕傛嫹 */
    int FOT = 512;
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count, 0, count);

        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count, 0, count);

        result_buff[i] = (s8*)sal_malloc(count * sizeof(s8));
        sal_memset(result_buff[i], count, 0, count);
        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    //sendbuf 闁跨喐鏋婚幏宄扳偓锟�
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = 1;
        }
    }

    //resultbuf 闁跨喐鏋婚幏宄扳偓锟�
    for (s32 i = 0; i < ndev; ++i)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = ndev;
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

    // 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚瑰В蹇涙晸閺傘倖瀚笵ev闁跨喐鏋婚幏绌塴lreduce闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔侯仾缁涜瀚�
    for (s32 i = 0; i < ndev; i++)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8pring_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //闁跨喐鏋婚幏宄板絿stream闁跨喍鑼庣拠褎瀚归柨鐔告灮閹风兘鏁撻弬銈嗗閸氬矂鏁撻弬銈嗗闁跨喕鍓奸悮瀛樺闁跨喐鏋婚幏锟�


    //闁跨喐鏋婚幏宄板絿stream闁跨喍鑼庣拠褎瀚归柨鐔告灮閹风兘鏁撻弬銈嗗閸氬矂鏁撻弬銈嗗闁跨喕鍓奸悮瀛樺闁跨喐鏋婚幏锟�
    for (s32 i = 0; i < ndev; i++)
    {
        for (s32 j = 0; j < count; j++)
        {
            s8 res = result_buff[i][j];
            s8 recv = outputbuf[i][j];

            if (res != recv)
            {
                HCCL_ERROR(" rank[%d] data[%d] recvbuf[%d] result_buff[%d] \n",i, j, recv, res);
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
        sal_free(result_buff[i]);
        rt_ret = aclrtDestroyStream(stream[i]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}


void* reduce_scatter_8pring_task(void* parg)
{
    HcclResult ret = HCCL_SUCCESS;
    para_t* para_info = (para_t*)parg;
    s32 rank_num_tmp;
    u32 port = 16666;

    HcomInfo hcom_info;
    std::string ranktable_file = para_info->file_name;
    std::string rankTableM;
    std::string realFilePath;

    hrtSetDevice(para_info->device_id);

    HcclWorkflowMode mode = GetWorkflowMode();
    HCCL_ERROR("workFlowMode is %d", mode);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();

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
    
    SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB);

    bool swapped;
    //-----------------Get Workspace Resource Start------------------//
    u64 stream_list_size = 0;
    ret = hcom_info.pComm->GetWorkspaceSubStreamNum(stream_list_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("get stream_list_size[%d] success", stream_list_size);
    vector<HcclRtStream> streamList(stream_list_size);

    rtError_t rt_ret;
    //閻㈢喐鍨氭禒宸梩ream
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = aclrtCreateStreamWithConfig(&streamList[i], 0, ACL_STREAM_PERSISTENT);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        //娴犲孩绁ind閸掔櫩odel

    }

    u32 rankSize = 0;
    ret = hcom_info.pComm->GetRankSize(rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 memSize = 0;
    ret = hcom_info.pComm->GetWorkspaceMemSize("HcomReduceScatter", para_info->count, para_info->datatype, rankSize, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource("tag_reduce_scatter_8pring_task", memptr, memSize, streamList);
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
    { sched_yield(); } // linux闁圭粯鍔掔欢鍨▔閳ь剚绋夐鍡涘厙缂備胶鍠曢惃鐔兼偨閵娿劎绠ラ悶娑樼焷缁绘绮欑€ｂ晛鐦滈柛鏂诲姀椤斺偓闁告垹鍎ゆ晶鐣屾偘鐏炵偓缍€

    __sync_synchronize();  // 闁圭粯甯掗崣鍡涘礃閸涱厾鎽犻悘鐐茬箻濞堜即鏁嶇仦绛嬪殸濡炪倕鎼花顓㈠箑瑜庡﹢浣烘啺娴ｅ湱婀撮柨娑樺缁查箖寮伴娑欑畳婵炲备鍓濆﹢浣规媴鐠恒劍鏆弆ock闁圭ǹ娲ｉ幎銈夋儍閸曨剚顦ч柛濠忔嫹

    ret =  hcom_info.pComm->ReduceScatter("tag_reduce_scatter_8pring_task",
                               para_info->sendbuff,
                               para_info->recvbuff,
                               para_info->count,
                               para_info->datatype,
                               para_info->op,
                               para_info->stream);

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task HcomReduceScatter falis", para_info->device_id);
    }

    rt_ret = RT_ERROR_NONE;
    rt_ret = aclrtSynchronizeStream(para_info->stream);

    if ( rt_ret != RT_ERROR_NONE)
    {
        HCCL_ERROR("rank[%d] task reducescatter falis", hcom_info.params.rank);
    }
    for (s32 i = 0; i < stream_list_size; i++)
    {

    }
    for (int i = 0; i < stream_list_size; i++)
    {
        rt_ret = aclrtDestroyStream(streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    hrtFree(memptr);

    ret = NetworkManager::GetInstance(para_info->device_id).DeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = NetworkManager::GetInstance(para_info->device_id).StopVnic();

    EXPECT_EQ(ret, HCCL_SUCCESS);
    return nullptr;
}

TEST_F(Hccl8pRingTest, st_reducescatter_inter_char_4rings)
{
    nlohmann::json rank_table = rank_table_1server_8rank;

    char file_name_t[] = "./st_reducescatter_inter_char_4rings.json";
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
    s8* inputbuf[DEV_NUM_8];
    s8* outputbuf[DEV_NUM_8];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;

    HcclReduceOp op = HCCL_REDUCE_SUM;
  //  s32 count =4833;
    s32 count =10;
    s32 ndev = DEV_NUM_8;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /** 闁告帗绻傞～鎰板礌閺嶎剛缈婚柛蹇嬪劥缁额參宕欓搹鍦閻庢冻鎷� */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(s8) * ndev);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        sal_memset(sendbuf[i], count * ndev, 0, count * ndev);

        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count, 0, count);

        result_buff[i] = (s8*)sal_malloc(count * sizeof(s8));
        sal_memset(result_buff[i], count, 0, count);
        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    //sendbuf 閻犙冾儏閳ь剨鎷�
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count * ndev; i++)
        {
            inputbuf[j][i] = 1;
        }
    }

    //resultbuf 閻犙冾儏閳ь剨鎷�
    for (s32 i = 0; i < ndev; ++i)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = ndev;
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

    // 闁告帗绋戠紓鎾承掕箛搴ㄥ殝Dev闁汇劌鍨穕lreduce濞寸姾顕ф慨鐔虹棯鐠恒劉鏌�
    for (s32 i = 0; i < ndev; i++)
    {
        tid[i] = sal_thread_create("thread", reduce_scatter_8pring_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //闁兼儳鍢茶ぐ鍣憈ream闁汇劌瀚幖閿嬫媴濠婂懏鐣遍柛姘湰椤掔偞绌遍垾鍐插▏闂佽鎷�


    //闁兼儳鍢茶ぐ鍣憈ream闁汇劌瀚幖閿嬫媴濠婂懏鐣遍柛姘湰椤掔偞绌遍垾鍐插▏闂佽鎷�
    for (s32 i = 0; i < ndev; i++)
    {
        for (s32 j = 0; j < count; j++)
        {
            s8 res = result_buff[i][j];
            s8 recv = outputbuf[i][j];

            if (res != recv)
            {
                HCCL_ERROR(" rank[%d] recvbuf[%d] recvbuf[%d] \n",i, j, res);
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
        sal_free(result_buff[i]);
        rt_ret = aclrtDestroyStream(stream[i]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}