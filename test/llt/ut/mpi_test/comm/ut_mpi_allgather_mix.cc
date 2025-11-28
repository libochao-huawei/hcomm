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
#include "llt_hccl_stub_sal_pub.h"

#define private public
#define protected public

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

#include "hccl/base.h"
#include <hccl/hccl_types.h>
#include "hcom_pub.h"
#include "hccl/hcom.h"
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
#include <iostream>
#include <fstream>
#include "opexecounter_pub.h"
#include "hccl_communicator.h"
#include "op_base.h"
#include "env_config.h"
#include "heartbeat.h"
#include "topoinfo_exchange_agent.h"

#include "param_check_pub.h"
#undef private

using namespace std;
using namespace hccl;

class MPI_AllGatherMix_Test : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_AllGatherMix_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_AllGatherMix_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        static s32  call_cnt = 0;
        DlTdtFunction::GetInstance().DlTdtFunctionInit();
        TsdOpen(1, 2);
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "ST_MPI_MIX_TEST");
        ra_set_work_mode(0);
        setenv("HCCL_DFS_CONFIG", "connection_fault_detection_time:0", 1);
        InitEnvParam();
        MOCKER_CPP(&Heartbeat::Init)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&Heartbeat::RegisterRanks)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&Heartbeat::UnRegisterRanks)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
        std::cout << "A MPI_MIX_Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        TsdClose(1);
        MPI_Barrier(MPI_COMM_WORLD);
        GlobalMockObject::verify();
        std::cout << "A MPI_MIX_Test TearDown" << std::endl;
    }
};

#define HCCL_COM_DATA_SIZE 1024
#define DEV_NUM_8 8

HcclResult FakeHrtGetDeviceType(DevType &devType)
{
    s32 rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == 0) {
        devType = DevType::DEV_TYPE_910B;
    } else {
        devType = DevType::DEV_TYPE_910_93;
    }

    return HCCL_SUCCESS;
}

TEST_F(MPI_AllGatherMix_Test, ut_mpi_HcclAllGatherOutPlace_mix_2p)
{
    setenv("HCCL_OP_EXPANSION_MODE", "HOST", 1);
    ResetInitState();
    InitExternalInput();

    hccl::HcclIpAddress ips[2] = {};
    ips[0] = hccl::HcclIpAddress("127.0.0.1");
    ips[1] = hccl::HcclIpAddress("127.0.0.2");

    SetRtCallbackModleStub(0);
    HcclResult ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s32 nnode, rank = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    ret = hrtSetDevice(rank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ResetInitState();

    MOCKER(hrtGetDeviceType)
    .stubs()
    .will(invoke(FakeHrtGetDeviceType));
    MOCKER_CPP(&HcclCommunicator::IsAtomicInit)
    .stubs()
    .will(returnValue(true));
	MOCKER(hrtRaGetSingleSocketVnicIpInfo)
	.stubs()
	.with(any())
	.will(invoke(stub_hrtRaGetSingleSocketVnicIpInfo));
	MOCKER(GetExternalInputHcclControlIfIp)
	.stubs()
	.with(any())
	.will(returnValue(ips[rank]));
    MOCKER_CPP(&HcclCommunicator::InitNic)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtCtxSetCurrent)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    s32 portNum = -1;
    MOCKER(hrtGetHccsPortNum)
    .stubs()
    .with(any(), outBound(portNum))
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::RegisterToHeartBeat, HcclResult(HcclCommunicator::*)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::ExecOp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcclRootInfo id;
    if (rank == 0) {
        ret = HcclGetRootInfo(&id);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    MPI_Bcast(&id, sizeof(id), MPI_BYTE, 0, MPI_COMM_WORLD);

    HcclComm newcomm;
    MOCKER_CPP(&HcclSocketManager::ServerInit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    ret = HcclCommInitRootInfo(nnode, &id, rank, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    aclrtStream stream;
    rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    s32 count = 1024;
    s8* sendBuf = (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendBuf, count * sizeof(s8), 0, count * sizeof(s8));
    s8* recvBuf = (s8*)sal_malloc(2 * count * sizeof(s8));
    sal_memset(recvBuf, 2 * count * sizeof(s8), 0, 2 * count * sizeof(s8));

    ret = HcclAllGatherInner(sendBuf, recvBuf, count, HCCL_DATA_TYPE_INT8, newcomm, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtSynchronizeStream(stream);
    EXPECT_EQ(rt_ret, ACL_SUCCESS);

    sal_free(sendBuf);
    sal_free(recvBuf);
    rt_ret = aclrtDestroyStream(stream);

    MPI_Barrier(MPI_COMM_WORLD);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    ParseHcclWhitelistSwitch();
    ParseHcclIfIp();
    SetRtCallbackModleStub(1);
    GlobalMockObject::verify();

    unsetenv("HCCL_OP_EXPANSION_MODE");
    ResetInitState();
    InitExternalInput();
}