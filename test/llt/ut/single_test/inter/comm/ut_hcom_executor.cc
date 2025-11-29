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
//#include <mpi.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <runtime/rt.h>
#include <iostream>
#include <fstream>

#include "hcom_pub.h"
#include "hccl/hcom.h"
#include "workflow_pub.h"
#include "hcom.h"
#include <hccl/hccl_comm.h>
#include <hccl/hccl_inner.h>
#include "llt_hccl_stub_pub.h"
#include "config.h"
#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"
#include "hccl/base.h"
#include <hccl/hccl_types.h>
#include "tbe_vector_reduce.h"
#include "tbe_crack_cleared.h"
#include "opexecounter_pub.h"

using namespace std;
using namespace hccl;

class HcomExecutorTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "HcomExecutorTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "HcomExecutorTest TearDown" << std::endl;
    }
    virtual void SetUp()
    {
        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
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

static HcclResult g_excutorStatus;
void setExecutorStatus(HcclResult status)
{
    HCCL_INFO("this is setExecutorStatus fuction");
    g_excutorStatus = status;
}


void getExecutorStatus(HcclResult &status)
{
    status = g_excutorStatus;
}

#define HCCL_COM_DATA_SIZE 1024
#if 1
TEST_F(HcomExecutorTest, ut_executor_broadcast)
{

    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_executor_broadcast.json";
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

    // 走1910 4pring
    char* rank_table_file = "./st_executor_broadcast.json";
    char* rank_ID = "0";

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomExecInitialize();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void* comm;
    std::shared_ptr<hccl::hcclComm> hcclComm;
    ret = HcomGetCommByGroup(NULL, hcclComm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    comm = hcclComm.get();
    ret = hccl::HcomExecutor::GetInstance().InitHcclComm(nullptr, comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, ACL_SUCCESS);
    sendbuf = (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendbuf, count * sizeof(s8) , 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcomBroadcast("testbcast", sendbuf, count, HCCL_DATA_TYPE_INT8, 0, NULL, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtSynchronizeStream(stream);
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

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    (void) SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    ret = HcclBroadcastInner(sendbuf, count, HCCL_DATA_TYPE_INT8, 0, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtSynchronizeStream(stream);
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

    ret = HcomExecFinalize();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sal_free(sendbuf);
    rt_ret = aclrtDestroyStream(stream);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);

}


TEST_F(HcomExecutorTest, ut_executor_allreduce)
{

    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_executor_allreduce.json";
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
    s32 errors=0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_executor_allreduce.json";
    char* rank_ID = "0";

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomExecInitialize();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void* comm;
    std::shared_ptr<hccl::hcclComm> hcclComm;
    ret = HcomGetCommByGroup(NULL, hcclComm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    comm = hcclComm.get();
    ret = hccl::HcomExecutor::GetInstance().InitHcclComm(nullptr, comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, ACL_SUCCESS);
        sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8) , 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8) , 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcomAllReduce("testallreduce", sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, NULL, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtSynchronizeStream(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            errors ++;
            break;
        }
    }

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    (void) SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    ret = HcclAllReduceInner(sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rt_ret = aclrtSynchronizeStream(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            errors ++;
            break;
        }
    }

    ret = HcomExecFinalize();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = aclrtDestroyStream(stream);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}
#endif
#if 1
TEST_F(HcomExecutorTest, ut_executor_equeue_allreduce_mix)
{

    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_executor_equeue_allreduce_mix.json";
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
    s32 errors=0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_executor_equeue_allreduce_mix.json";
    char* rank_ID = "0";

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomExecInitialize();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void* comm;
    std::shared_ptr<hccl::hcclComm> hcclComm;
    ret = HcomGetCommByGroup(NULL, hcclComm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    comm = hcclComm.get();
    ret = hccl::HcomExecutor::GetInstance().InitHcclComm(nullptr, comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, ACL_SUCCESS);
    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8) , 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcomAllReduce("testallreduce", sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, NULL, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtSynchronizeStream(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            errors ++;
            break;
        }
    }

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
    setExecutorStatus(HCCL_E_RESERVED);

    HCCL_INFO("executor start");
    HcomOperation opInfo;
    opInfo.hcclType = HCCL_TYPE_ALLREDUCE;
    opInfo.inputPtr = sendbuf;
    opInfo.outputPtr = recvbuf;
    opInfo.dataType = HCCL_DATA_TYPE_INT8;
    opInfo.count = count;
    opInfo.opType = HCCL_REDUCE_SUM;
    ret = HcomExecEnqueueOperation(opInfo, setExecutorStatus);
    if (ret != HCCL_SUCCESS) {
        HcomExecFinalize();
    }
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclResult excutorStatus = HCCL_E_RESERVED;
    const std::chrono::seconds TIMEOUT(60);
    const auto start = std::chrono::steady_clock::now();
    while (excutorStatus != HCCL_SUCCESS)
    {
        getExecutorStatus(excutorStatus);
        std::this_thread::sleep_for(std::chrono::milliseconds(SOCKET_SLEEP_MILLISECONDS));
        /* ?????????????timeout?????? */
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start);
        if (elapsed > TIMEOUT) {
            HcomExecFinalize();
            HCCL_ERROR("Wait timeout for getExecutor status timeout[%lld]", TIMEOUT);
            break;
        }
    }
    EXPECT_EQ(excutorStatus, HCCL_SUCCESS);
    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            errors ++;
            break;
        }
    }

    ret = HcomExecFinalize();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = aclrtDestroyStream(stream);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    setExecutorStatus(HCCL_E_RESERVED);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}
#endif

#if 1
TEST_F(HcomExecutorTest, ut_executor_equeue_allreduce)
{

    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_executor_equeue_allreduce.json";
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
    s32 errors=0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_executor_equeue_allreduce.json";
    char* rank_ID = "0";

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomExecInitialize();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void* comm;
    std::shared_ptr<hccl::hcclComm> hcclComm;
    ret = HcomGetCommByGroup(NULL, hcclComm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    comm = hcclComm.get();
    ret = hccl::HcomExecutor::GetInstance().InitHcclComm(nullptr, comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, ACL_SUCCESS);
    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8) , 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
    setExecutorStatus(HCCL_E_RESERVED);

    HCCL_INFO("executor start");
    HcomOperation opInfo;
    opInfo.hcclType = HCCL_TYPE_ALLREDUCE;
    opInfo.inputPtr = sendbuf;
    opInfo.outputPtr = recvbuf;
    opInfo.dataType = HCCL_DATA_TYPE_INT8;
    opInfo.count = count;
    opInfo.opType = HCCL_REDUCE_SUM;
    ret = HcomExecEnqueueOperation(opInfo, setExecutorStatus);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("HcomExecEnqueueOperation error");
        HcomExecFinalize();
    }
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclResult excutorStatus = HCCL_E_RESERVED;
    const std::chrono::seconds TIMEOUT(60);
    const auto start = std::chrono::steady_clock::now();
    while (excutorStatus != HCCL_SUCCESS)
    {
        getExecutorStatus(excutorStatus);
        // HCCL_INFO("getExecutorStatus start excutorStatus[%d]",excutorStatus);
        std::this_thread::sleep_for(std::chrono::milliseconds(SOCKET_SLEEP_MILLISECONDS));
        /* ?????????????timeout?????? */
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start);
        if (elapsed > TIMEOUT) {
            HCCL_ERROR("Wait timeout for getExecutor status timeout[%lld]", TIMEOUT);
            HcomExecFinalize();
            break;
        }
    }
    HCCL_INFO("HcomExcutor excutorStatus[%d]",excutorStatus);
    EXPECT_EQ(excutorStatus, HCCL_SUCCESS);
    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            errors ++;
            break;
        }
    }

    HCCL_INFO("HcomExecFinalize start");
    ret = HcomExecFinalize();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = aclrtDestroyStream(stream);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    setExecutorStatus(HCCL_E_RESERVED);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}
#endif

#if 1
TEST_F(HcomExecutorTest, ut_executor_equeue_broardcast)
{

    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_executor_equeue_broardcast.json";
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
    s8* sendbuf;
    s32 rank = 0;
    s32 errors=0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_executor_equeue_broardcast.json";
    char* rank_ID = "0";

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomExecInitialize();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void* comm;
    std::shared_ptr<hccl::hcclComm> hcclComm;
    ret = HcomGetCommByGroup(NULL, hcclComm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    comm = hcclComm.get();
    ret = hccl::HcomExecutor::GetInstance().InitHcclComm(nullptr, comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sendbuf= (s8*)sal_malloc(count * sizeof(s8) + 512);
     sal_memset(sendbuf, count * sizeof(s8) + 512 , 0, count * sizeof(s8) + 512 );

    for (int j = 0; j < count; j++)
    {
        sendbuf[j + 128] = 2;
    }
    setExecutorStatus(HCCL_E_RESERVED);

    HCCL_INFO("executor start");
    HcomOperation opInfo;
    opInfo.hcclType = HCCL_TYPE_BROADCAST;
    opInfo.inputPtr = sendbuf + 128;
    opInfo.dataType = HCCL_DATA_TYPE_INT8;
    opInfo.count = count;
    opInfo.opType = HCCL_REDUCE_SUM;
    opInfo.root = 0;
    ret = HcomExecEnqueueOperation(opInfo, setExecutorStatus);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("HcomExecEnqueueOperation error");
        HcomExecFinalize();
    }
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclResult excutorStatus = HCCL_E_RESERVED;
    const std::chrono::seconds TIMEOUT(60);
    const auto start = std::chrono::steady_clock::now();
    while (excutorStatus != HCCL_SUCCESS)
    {
        getExecutorStatus(excutorStatus);
        // HCCL_INFO("getExecutorStatus start excutorStatus[%d]",excutorStatus);
        std::this_thread::sleep_for(std::chrono::milliseconds(SOCKET_SLEEP_MILLISECONDS));
        /* ?????????????timeout?????? */
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start);
        if (elapsed > TIMEOUT) {
            HCCL_ERROR("Wait timeout for getExecutor status timeout[%lld]", TIMEOUT);
            HcomExecFinalize();
            break;
        }
    }
    HCCL_INFO("HcomExcutor excutorStatus[%d]",excutorStatus);
    EXPECT_EQ(excutorStatus, HCCL_SUCCESS);
    for (int j = 0; j < count; j++)
    {
        if (sendbuf[j + 128] != 2)
        {
            HCCL_ERROR("\n rank:%d sendbuf[%d]:%f", rank, j+128, sendbuf[j + 128] );
            errors ++;
            break;
        }
    }

    HCCL_INFO("HcomExecFinalize start");
    ret = HcomExecFinalize();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    sal_free(sendbuf);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    setExecutorStatus(HCCL_E_RESERVED);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}
#endif


#if 1
TEST_F(HcomExecutorTest, ut_executor_equeue_allgather)
{

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_executor_equeue_allgather.json";
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
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors=0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_executor_equeue_allgather.json";
    char* rank_ID = "0";

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomExecInitialize();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void* comm;
    std::shared_ptr<hccl::hcclComm> hcclComm;
    ret = HcomGetCommByGroup(NULL, hcclComm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    comm = hcclComm.get();
    ret = hccl::HcomExecutor::GetInstance().InitHcclComm(nullptr, comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sendbuf= (s8*)sal_malloc(count * sizeof(s8) + 512);
     sal_memset(sendbuf, count * sizeof(s8) + 512 , 0, count * sizeof(s8) + 512 );
    recvbuf= (s8*)sal_malloc(count * sizeof(s8) + 512);
     sal_memset(recvbuf, count * sizeof(s8) + 512 , 0, count * sizeof(s8) + 512 );

    for (int j = 0; j < count; j++)
    {
        sendbuf[j + 128] = 2;
    }
    setExecutorStatus(HCCL_E_RESERVED);

    HCCL_INFO("executor start");
    HcomOperation opInfo;
    opInfo.hcclType = HCCL_TYPE_ALLGATHER;
    opInfo.inputPtr = sendbuf + 128;
    opInfo.outputPtr = recvbuf + 128;
    opInfo.dataType = HCCL_DATA_TYPE_INT8;
    opInfo.count = count;
    opInfo.opType = HCCL_REDUCE_SUM;
    ret = HcomExecEnqueueOperation(opInfo, setExecutorStatus);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("HcomExecEnqueueOperation error");
        HcomExecFinalize();
    }
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclResult excutorStatus = HCCL_E_RESERVED;
    const std::chrono::seconds TIMEOUT(60);
    const auto start = std::chrono::steady_clock::now();
    while (excutorStatus != HCCL_SUCCESS)
    {
        getExecutorStatus(excutorStatus);
        // HCCL_INFO("getExecutorStatus start excutorStatus[%d]",excutorStatus);
        std::this_thread::sleep_for(std::chrono::milliseconds(SOCKET_SLEEP_MILLISECONDS));
        /* ?????????????timeout?????? */
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start);
        if (elapsed > TIMEOUT) {
            HCCL_ERROR("Wait timeout for getExecutor status timeout[%lld]", TIMEOUT);
            HcomExecFinalize();
            return;
        }
    }
    HCCL_INFO("HcomExcutor excutorStatus[%d]",excutorStatus);
    EXPECT_EQ(excutorStatus, HCCL_SUCCESS);
    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j + 128] != 2)
        {
            errors ++;
            break;
        }
    }

    HCCL_INFO("HcomExecFinalize start");
    ret = HcomExecFinalize();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    sal_free(sendbuf);
    sal_free(recvbuf);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    setExecutorStatus(HCCL_E_RESERVED);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}
#endif

#if 1
TEST_F(HcomExecutorTest, ut_executor_equeue_alltoallv)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_executor_equeue_alltoallv.json";
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
    s32 deviceId = 0;
    char *identify = "0";
    s32 rankSize = 1;
    s32 rank = atoi(identify);
    u64 count = 2;
    HCCL_INFO("alltoall_int32 : identify[%d], count[%llu]", rank, count);
    u32 devLogicId = 0xFFFF;
    HcclResult ret = hrtGetDeviceIndexByPhyId(deviceId, devLogicId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hrtSetDevice(devLogicId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char *rankTableFile = "./st_executor_equeue_alltoallv.json";
    ret = HcomInitByFile(rankTableFile, identify);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomExecInitialize();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void* comm;
    std::shared_ptr<hccl::hcclComm> hcclComm;
    ret = HcomGetCommByGroup(NULL, hcclComm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    comm = hcclComm.get();
    ret = hccl::HcomExecutor::GetInstance().InitHcclComm(nullptr, comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    const int COUNT_PER_RANK = count;
    u64 memSize = COUNT_PER_RANK * rankSize * sizeof(s32);
    HostMem hostSendMem = HostMem::alloc(memSize);
    memset_s(hostSendMem.ptr(), memSize, 0, COUNT_PER_RANK * rankSize);
    for (u32 i = 0; i < COUNT_PER_RANK * rankSize; i++) {
        *((s32 *)hostSendMem.ptr() + i) = rank + 1;
    }

    // 构造入参
    vector<u64> sendCounts(rankSize, COUNT_PER_RANK);
    vector<u64> recvCounts(rankSize, COUNT_PER_RANK);
    vector<u64> sdispls(rankSize, 0);
    vector<u64> rdispls(rankSize, 0);
    for (int i = 0; i < rankSize; i++) {
        sdispls[i] = COUNT_PER_RANK * i;
        rdispls[i] = COUNT_PER_RANK * i;
        HCCL_INFO("num[%d] displs[%d]", i, COUNT_PER_RANK * i);
    }
    DeviceMem devSendCounts = DeviceMem::alloc(rankSize * sizeof(u64));
    ret = hrtMemSyncCopy(devSendCounts.ptr(), rankSize * sizeof(u64), sendCounts.data(), rankSize * sizeof(u64), HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem devSdispls = DeviceMem::alloc(rankSize * sizeof(u64));
    ret = hrtMemSyncCopy(devSdispls.ptr(), rankSize * sizeof(u64), sdispls.data(), rankSize * sizeof(u64), HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem devRecvCounts = DeviceMem::alloc(rankSize * sizeof(u64));
    ret = hrtMemSyncCopy(devRecvCounts.ptr(), rankSize * sizeof(u64), recvCounts.data(), rankSize * sizeof(u64), HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem devRdispls = DeviceMem::alloc(rankSize * sizeof(u64));
    ret = hrtMemSyncCopy(devRdispls.ptr(), rankSize * sizeof(u64), rdispls.data(), rankSize * sizeof(u64), HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    DeviceMem sendMem = DeviceMem::alloc(memSize);
    ret = hrtMemSyncCopy(sendMem.ptr(), memSize, hostSendMem.ptr(), memSize, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem recvMem = DeviceMem::alloc(memSize);

    HcomAllToAllVParams opInfo;
    opInfo.group = HCCL_WORLD_GROUP;
    opInfo.sendbuf = sendMem.ptr();
    opInfo.sendcounts = devSendCounts.ptr();
    opInfo.sdispls = devSdispls.ptr();
    opInfo.sendtype = HCCL_DATA_TYPE_INT32;
    opInfo.recvbuf = recvMem.ptr();
    opInfo.recvcounts = devRecvCounts.ptr();
    opInfo.rdispls = devRdispls.ptr();
    opInfo.recvtype = HCCL_DATA_TYPE_INT32;

    setExecutorStatus(HCCL_E_RESERVED);

    ret = HcomExecEnqueueAllToAllV(opInfo, setExecutorStatus);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HCCL_INFO("HcomExecFinalize start");
    ret = HcomExecFinalize();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    setExecutorStatus(HCCL_E_RESERVED);
    remove(file_name_t);
    (void)aclrtResetDevice(0);
}
#endif

#if 1
TEST_F(HcomExecutorTest, ut_executor_equeue_alltoallvc)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_executor_equeue_alltoallvc.json";
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
    s32 deviceId = 0;
    char *identify = "0";
    s32 rankSize = 1;
    s32 rank = atoi(identify);
    u64 count = 2;
    HCCL_INFO("alltoallvc_int32 : identify[%d], count[%llu]", rank, count);
    u32 devLogicId = 0xFFFF;
    HcclResult ret = hrtGetDeviceIndexByPhyId(deviceId, devLogicId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hrtSetDevice(devLogicId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char *rankTableFile = "./st_executor_equeue_alltoallvc.json";
    ret = HcomInitByFile(rankTableFile, identify);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomExecInitialize();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void* comm;
    std::shared_ptr<hccl::hcclComm> hcclComm;
    ret = HcomGetCommByGroup(NULL, hcclComm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    comm = hcclComm.get();
    ret = hccl::HcomExecutor::GetInstance().InitHcclComm(nullptr, comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    const int COUNT_PER_RANK = count;
    u64 memSize = COUNT_PER_RANK * rankSize * sizeof(s32);
    HostMem hostSendMem = HostMem::alloc(memSize);
    memset_s(hostSendMem.ptr(), memSize, 0, COUNT_PER_RANK * rankSize);
    for (u32 i = 0; i < COUNT_PER_RANK * rankSize; i++) {
        *((s32 *)hostSendMem.ptr() + i) = rank + 1;
    }

    // 构造入参
    vector<u64> sendCounts(rankSize, COUNT_PER_RANK);
    vector<u64> recvCounts(rankSize, COUNT_PER_RANK);
    vector<u64> sendCountMatrix(rankSize * rankSize, COUNT_PER_RANK);

    DeviceMem devSendCountMatrix = DeviceMem::alloc(rankSize * rankSize * sizeof(u64));
    ret = hrtMemSyncCopy(devSendCountMatrix.ptr(), rankSize * rankSize * sizeof(u64),
        sendCountMatrix.data(), rankSize * rankSize * sizeof(u64), HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    DeviceMem sendMem = DeviceMem::alloc(memSize);
    ret = hrtMemSyncCopy(sendMem.ptr(), memSize, hostSendMem.ptr(), memSize, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem recvMem = DeviceMem::alloc(memSize);

    HcomAllToAllVCParams opInfo;
    opInfo.group = nullptr;
    opInfo.sendbuf = sendMem.ptr();
    opInfo.sendcountmatrix = devSendCountMatrix.ptr();
    opInfo.sendtype = HCCL_DATA_TYPE_INT32;
    opInfo.recvbuf = recvMem.ptr();
    opInfo.recvtype = HCCL_DATA_TYPE_INT32;

    setExecutorStatus(HCCL_E_RESERVED);

    ret = HcomExecEnqueueAllToAllVC(opInfo, setExecutorStatus);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HCCL_INFO("HcomExecFinalize start");
    ret = HcomExecFinalize();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    setExecutorStatus(HCCL_E_RESERVED);
    remove(file_name_t);
    (void)aclrtResetDevice(0);
}
#endif

TEST_F(HcomExecutorTest, ut_executor_equeue_gather_alltoallv)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_executor_equeue_gather_alltoallv.json";
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
    s32 deviceId = 0;
    char *identify = "0";
    s32 rankSize = 1;
    s32 rank = atoi(identify);
    u64 count = 200;
    HCCL_INFO("alltoall_int32 : identify[%d], count[%llu]", rank, count);
    u32 devLogicId = 0xFFFF;
    HcclResult ret = hrtGetDeviceIndexByPhyId(deviceId, devLogicId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hrtSetDevice(devLogicId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char *rankTableFile = "./st_executor_equeue_gather_alltoallv.json";
    ret = HcomInitByFile(rankTableFile, identify);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomExecInitialize();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void* comm;
    std::shared_ptr<hccl::hcclComm> hcclComm;
    ret = HcomGetCommByGroup(NULL, hcclComm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    comm = hcclComm.get();
    ret = hccl::HcomExecutor::GetInstance().InitHcclComm(nullptr, comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u32 countsNum = 200;
    vector<u64> addrInfo;
    vector<u64> addrInfoCountPerRank(rankSize, countsNum);

    const int COUNT_PER_RANK = count;
    u64 memSize = COUNT_PER_RANK * rankSize * sizeof(s32) * countsNum;
    HostMem hostSendMem = HostMem::alloc(memSize);
    memset_s(hostSendMem.ptr(), memSize, 0, COUNT_PER_RANK * rankSize);
    for (u32 i = 0; i < COUNT_PER_RANK * rankSize * countsNum; i++) {
        *((s32 *)hostSendMem.ptr() + i) = rank + 1;
        if (i % COUNT_PER_RANK == 0) {
            addrInfo.push_back((uintptr_t)((s32 *)hostSendMem.ptr() + i));
            addrInfo.push_back(COUNT_PER_RANK * sizeof(s32));
        }
    }

    // 构造入参
    DeviceMem devAddrInfo = DeviceMem::alloc(addrInfo.size() * sizeof(u64));
    ret = hrtMemSyncCopy(devAddrInfo.ptr(), addrInfo.size() * sizeof(u64), addrInfo.data(), addrInfo.size() * sizeof(u64), HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem devCountPerRank = DeviceMem::alloc(addrInfoCountPerRank.size() * sizeof(u64));
    ret = hrtMemSyncCopy(devCountPerRank.ptr(), addrInfoCountPerRank.size() * sizeof(u64), addrInfoCountPerRank.data(),
                         addrInfoCountPerRank.size() * sizeof(u64), HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    vector<u64> recvCounts(rankSize, COUNT_PER_RANK);
    vector<u64> rdispls(rankSize, 0);
    for (int i = 0; i < rankSize; i++) {
        rdispls[i] = COUNT_PER_RANK * i;
        HCCL_INFO("num[%d] displs[%d]", i, COUNT_PER_RANK * i);
    }
    DeviceMem devRecvCounts = DeviceMem::alloc(rankSize * sizeof(u64));
    ret = hrtMemSyncCopy(devRecvCounts.ptr(), rankSize * sizeof(u64), recvCounts.data(), rankSize * sizeof(u64), HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem devRdispls = DeviceMem::alloc(rankSize * sizeof(u64));
    ret = hrtMemSyncCopy(devRdispls.ptr(), rankSize * sizeof(u64), rdispls.data(), rankSize * sizeof(u64), HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem recvMem = DeviceMem::alloc(memSize);
    DeviceMem gatherMem = DeviceMem::alloc(memSize);
    HCCL_ERROR("memsize[%llu]", memSize);

    HcomGatherAllToAllVParams opInfo;
    opInfo.addrInfo = devAddrInfo.ptr();
    opInfo.addrInfoCountPerRank = devCountPerRank.ptr();
    opInfo.recvbuf = recvMem.ptr();
    opInfo.recvcounts = devRecvCounts.ptr();
    opInfo.rdispls = devRdispls.ptr();
    opInfo.recvtype = HCCL_DATA_TYPE_INT32;
    opInfo.gatheredbuf = gatherMem.ptr();
    opInfo.group = HCCL_WORLD_GROUP;
    opInfo.addrLength = -1;

    setExecutorStatus(HCCL_E_RESERVED);

    ret = HcomExecEnqueueGatherAllToAllV(opInfo, setExecutorStatus);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclResult exeRet = HCCL_E_RESERVED;
    const std::chrono::seconds TIMEOUT(1);
    const auto start = std::chrono::steady_clock::now();
    while (exeRet != HCCL_SUCCESS)
    {
        getExecutorStatus(exeRet);
        // HCCL_INFO("getExecutorStatus start excutorStatus[%d]",excutorStatus);
        std::this_thread::sleep_for(std::chrono::milliseconds(SOCKET_SLEEP_MILLISECONDS));

        const auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start);
        if (elapsed > TIMEOUT) {
            HCCL_ERROR("Wait timeout for getExecutor status timeout[%lld]", TIMEOUT);
            HcomExecFinalize();
            HcomDestroy();
            return;
        }
    }

    HCCL_INFO("HcomExecFinalize start");
    ret = HcomExecFinalize();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    setExecutorStatus(HCCL_E_RESERVED);
    remove(file_name_t);
    (void)aclrtResetDevice(0);
}

#if 1
TEST_F(HcomExecutorTest, ut_executor_reduce)
{

    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_executor_reduce.json";
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
    s32 errors=0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_executor_reduce.json";
    char* rank_ID = "0";

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomExecInitialize();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void* comm;
    std::shared_ptr<hccl::hcclComm> hcclComm;
    ret = HcomGetCommByGroup(NULL, hcclComm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    comm = hcclComm.get();
    ret = hccl::HcomExecutor::GetInstance().InitHcclComm(nullptr, comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, ACL_SUCCESS);
    sendbuf = (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendbuf, count * sizeof(s8) , 0, count * sizeof(s8));
    recvbuf = (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(recvbuf, count * sizeof(s8) , 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcomReduce("testreduce", sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, 0, NULL, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtSynchronizeStream(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            HCCL_ERROR("\n rank:%d recvbuf[%d]:%f", rank, j, recvbuf[j] );
            errors ++;
            break;
        }
    }

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    (void) SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    ret = HcclReduceInner(sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, 0, comm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rt_ret = aclrtSynchronizeStream(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            HCCL_ERROR("\n rank:%d recvbuf[%d]:%f", rank, j, recvbuf[j] );
            errors ++;
            break;
        }
    }

    ret = HcomExecFinalize();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = aclrtDestroyStream(stream);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}
#endif

TEST_F(HcomExecutorTest, ut_executor_allreduce_on_created_comm)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_executor_equeue_allreduce.json";
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
    s32 errors=0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_executor_equeue_allreduce.json";
    char* rank_ID = "0";

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomExecInitialize();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclRootInfo id;

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclCommConfig config;
    HcclCommConfigInit(&config);
    strcpy_s(config.hcclCommName, COMM_NAME_MAX_LENGTH, "comm1");

    HcclComm newcomm;
    ret = HcclCommInitRootInfoConfig(1, &id, 0, &config, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::shared_ptr<hccl::hcclComm> hcclComm;
    ret = HcomGetCommByGroup("comm1", hcclComm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(newcomm, hcclComm.get());

    void* comm;
    comm = hcclComm.get();
    ret = hccl::HcomExecutor::GetInstance().InitHcclComm(nullptr, comm);

    rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, ACL_SUCCESS);
    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8) , 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
    setExecutorStatus(HCCL_E_RESERVED);

    HCCL_INFO("executor start");
    HcomOperation opInfo;
    opInfo.hcclType = HCCL_TYPE_ALLREDUCE;
    opInfo.inputPtr = sendbuf;
    opInfo.outputPtr = recvbuf;
    opInfo.dataType = HCCL_DATA_TYPE_INT8;
    opInfo.count = count;
    opInfo.opType = HCCL_REDUCE_SUM;
    ret = HcomExecEnqueueOperation(opInfo, setExecutorStatus);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("HcomExecEnqueueOperation error");
        HcomExecFinalize();
    }
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclResult excutorStatus = HCCL_E_RESERVED;
    const std::chrono::seconds TIMEOUT(60);
    const auto start = std::chrono::steady_clock::now();
    while (excutorStatus != HCCL_SUCCESS)
    {
        getExecutorStatus(excutorStatus);
        // HCCL_INFO("getExecutorStatus start excutorStatus[%d]",excutorStatus);
        std::this_thread::sleep_for(std::chrono::milliseconds(SOCKET_SLEEP_MILLISECONDS));
        /* ?????????????timeout?????? */
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start);
        if (elapsed > TIMEOUT) {
            HCCL_ERROR("Wait timeout for getExecutor status timeout[%lld]", TIMEOUT);
            HcomExecFinalize();
            break;
        }
    }
    HCCL_INFO("HcomExcutor excutorStatus[%d]",excutorStatus);
    EXPECT_EQ(excutorStatus, HCCL_SUCCESS);
    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            errors ++;
            break;
        }
    }

    HCCL_INFO("HcomExecFinalize start");
    ret = HcomExecFinalize();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = aclrtDestroyStream(stream);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    setExecutorStatus(HCCL_E_RESERVED);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}