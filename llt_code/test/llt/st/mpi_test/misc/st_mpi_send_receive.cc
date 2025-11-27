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

#include <driver/ascend_hal.h>
#include "rt_external.h"

#include "adapter_hal.h"
#include "adapter_rts.h"

#include "hcom_pub.h"
#include "llt_hccl_stub_pub.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "sal.h"
#include "hccl_comm_pub.h"
#include "config.h"
#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"
#include "externalinput.h"
#include "dlra_function.h"
#include "param_check_pub.h"
#include "hccl/hcom.h"
#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include "dlhal_function.h"
#include "hcom.h"
#include "stream_pub.h"
#include <iostream>
#include <fstream>
#include "remote_notify.h"
#include "rank_consistentcy_checker.h"

using namespace std;
using namespace hccl;
class MPI_Send_Receive_Test : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        MPI_Barrier(MPI_COMM_WORLD);

        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "ST_MPI_TEST");
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A TestCase SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        std::cout << "A TestCase TearDown" << std::endl;
    }
};



#define P2P_DATA_SIZE_LIGHT 20
#define P2P_DATA_SIZE_HEAVY 1200000
#define P2P_DATA_SIZE_S_HEAVY 3000000   /* 超大数据，约10M */

typedef struct p2p_para_struct
{
    HcclRootInfo rootInfo;
    std::string identify;
    s32 device_id;
    char* file_name;
    bool sender_flag;
    s32 peer_rank;
    void* buffer;
    s32 count;
    HcclDataType datatype;
    rtStream_t stream;
    volatile s32* sync_addr;
    const char* tag;
    char* groupName;
    u32 groupRanksNum;
    u32 *pGroupRanks;
    s32 ranks_local;
} p2p_para_t;

void* inter_send_receive_task(void* parg)
{
    HcclResult ret = HCCL_SUCCESS;
    p2p_para_t* para_info = (p2p_para_t*)parg;

    hrtSetDevice(para_info->device_id);

    HcomInfo hcom_info;
    std::string ranktable_file = para_info->file_name;
    std::string rankTableM;
    std::string realFilePath;

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomLoadRanktableFile(ranktable_file.c_str(), rankTableM, realFilePath);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, para_info->identify, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rtError_t rt_ret;
    sal_memcpy(hcom_info.params.id.internal, sizeof(HcclRootInfo), &para_info->rootInfo, sizeof(HcclRootInfo));

    hcom_info.pComm.reset(new(std::nothrow) hccl::hcclComm());
    char* charModel = new char;
    rtModel_t model = (void*)charModel;

    CommConfig commConfig("hccl_world_group"); 
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);
    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task send_receive falis", para_info->device_id);
    }

    bool swapped;
    s32 rank_num_tmp = *(para_info->sync_addr) - 1;

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
    
    //-----------------Set Workspace Resource Start------------------//
    u64 stream_list_size = 0;
    ret = hcom_info.pComm->GetWorkspaceSubStreamNum(stream_list_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("get stream_list_size[%d] success", stream_list_size);
    vector<HcclRtStream> streamList(stream_list_size);

    //???stream
    for (s32 i = 0; i < stream_list_size; i++)
    { 
        rt_ret = aclrtCreateStreamWithConfig(&streamList[i], 0, ACL_STREAM_PERSISTENT);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        //??bind?model

    }

    u32 rankSize = 0;
    ret = hcom_info.pComm->GetRankSize(rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void *memptr = nullptr;
    if (para_info->sender_flag)
    {
        u64 memSize = 0;
        ret = hcom_info.pComm->GetWorkspaceMemSize("HcomSend", para_info->count, para_info->datatype, rankSize, memSize);
        EXPECT_EQ(ret, HCCL_SUCCESS);
            
        ret = hrtMalloc(&memptr, memSize);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = hcom_info.pComm->SetWorkspaceResource(para_info->tag, memptr, memSize, streamList);
        EXPECT_EQ(ret, HCCL_SUCCESS);    
        ret = hcom_info.pComm->send(
            para_info->tag,
            para_info->buffer,
            para_info->count,
            para_info->datatype,
            para_info->peer_rank,
            para_info->stream);
        HCCL_INFO("rank[%s] device[%d] send to rank[%d]", para_info->identify.c_str(), para_info->device_id, para_info->peer_rank);
        if (ret != HCCL_SUCCESS)
        {
            HCCL_ERROR("dev[%d] task send fails", para_info->device_id);
        }
    }
    else
    {
        u64 memSize = 0;
        ret = hcom_info.pComm->GetWorkspaceMemSize("HcomReceive", para_info->count, para_info->datatype, rankSize, memSize);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = hrtMalloc(&memptr, memSize);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = hcom_info.pComm->SetWorkspaceResource(para_info->tag, memptr, memSize, streamList);
        EXPECT_EQ(ret, HCCL_SUCCESS);    

        ret = hcom_info.pComm->receive(
            para_info->tag,
            para_info->buffer,
            para_info->count,
            para_info->datatype,
            para_info->peer_rank,
            para_info->stream);
        HCCL_INFO("rank[%s] device[%d] recv from rank[%d]", para_info->identify.c_str(), para_info->device_id, para_info->peer_rank);
        if (ret != HCCL_SUCCESS)
        {
            HCCL_ERROR("dev[%d] task receive fails", para_info->device_id);
        }
    }


    rt_ret = aclrtSynchronizeStream(para_info->stream);
    //--------------Resource destroy----------------//
    for (s32 i = 0; i < stream_list_size; i++)
    {

            
        rt_ret = aclrtDestroyStream(streamList[i]);
        EXPECT_EQ(rt_ret, ACL_SUCCESS);
    }
    hrtFree(memptr);
    delete charModel;
    charModel = nullptr;
    return (NULL);
}

#if 1
TEST_F(MPI_Send_Receive_Test, ut_mpi_heterog_hccl_signal)
{
    s32 ret = 0;
    s32 nnode, noderank = 0;

    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode ?mpi??????????
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);

    MOCKER(hrtNotifyRecord)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtNotifyWaitWithTimeOut)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtNotifyDestroy)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtSetIpcNotifyPid)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    DlHalFunction::GetInstance().DlHalFunctionInit();

    //??dispatcher, ????
    void *dispatcher = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcher, nullptr);

    s32 devId = noderank == 0 ? 0 : -1;

    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(devId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string tag = "signal_test";
    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    if (noderank == 0) {
        HcclResult ret;
        std::shared_ptr<LocalIpcNotify> localNotify = nullptr;
        std::shared_ptr<RemoteNotify> remoteNotify = nullptr;
        HCCL_INFO("alloc notify start");
        RemoteRankInfo info(-1, 0, 1);
        ret = notifyPool->Alloc(tag, info, localNotify);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        EXPECT_NE(localNotify, nullptr);

        HCCL_INFO("notify open ipc start");
        std::vector<u8> data(NOTIFY_INFO_LENGTH, 0);
        ret = localNotify->Serialize(data);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        remoteNotify.reset(new (std::nothrow) RemoteNotify());
        EXPECT_NE(remoteNotify, nullptr);

        ret = remoteNotify->Init(data);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        ret = remoteNotify->Open();
        EXPECT_EQ(ret, HCCL_SUCCESS);

        aclrtStream stream;
        rtError_t rt_ret = aclrtCreateStream(&stream);
        EXPECT_EQ(rt_ret, ACL_SUCCESS);
        Stream streamObj(stream);

        HCCL_INFO("notify record start");
        ret = remoteNotify->Post(streamObj, dispatcher);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("notify wait start");
        ret = localNotify->Wait(streamObj, dispatcher);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("notify ipc close");
        ret = remoteNotify->Close();
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("notify Destroy");
        ret = localNotify->Destroy();
        EXPECT_EQ(ret, HCCL_SUCCESS);

        aclrtDestroyStream(stream);
        localNotify = nullptr;
        remoteNotify = nullptr;

    } else if (noderank == 1) {

        HcclResult ret;
        std::shared_ptr<LocalIpcNotify> localNotify = nullptr;
        std::shared_ptr<RemoteNotify> remoteNotify = nullptr;
        HCCL_INFO("alloc esched event start");
        RemoteRankInfo info(0, 0, 1);
        ret = notifyPool->Alloc(tag, info, localNotify);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        EXPECT_NE(localNotify, nullptr);

        HCCL_INFO("esched event open ipc start");
        std::vector<u8> data(NOTIFY_INFO_LENGTH, 0);
        ret = localNotify->Serialize(data);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        remoteNotify.reset(new (std::nothrow) RemoteNotify());

        ret = remoteNotify->Init(data);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        ret = remoteNotify->Open();
        EXPECT_EQ(ret, HCCL_SUCCESS);

        aclrtStream stream;
        rtError_t rt_ret = aclrtCreateStream(&stream);
        EXPECT_EQ(rt_ret, ACL_SUCCESS);
        Stream streamObj(stream);

        HCCL_INFO("esched event record start");
        ret = remoteNotify->Post(streamObj, dispatcher);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("esched event wait start");
        ret = localNotify->Wait(streamObj, dispatcher);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("esched event ipc close");
        ret = remoteNotify->Close();
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("esched event ipc Destroy");
        ret = localNotify->Destroy();
        EXPECT_EQ(ret, HCCL_SUCCESS);
        aclrtDestroyStream(stream);
        localNotify = nullptr;
        remoteNotify = nullptr;

    }

    ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclDispatcherDestroy(dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    MPI_Barrier(MPI_COMM_WORLD);
}

#endif