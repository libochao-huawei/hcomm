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

#include "comm_base_pub.h"

#include <vector>
#include "comm_factory_pub.h"
#include "profiler_manager.h"
#include "env_config.h"

using namespace std;
using namespace hccl;

class MPI_CommInner_Test : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_CommInner_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_CommInner_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "ST_MPI_TEST");
        ra_set_work_mode(1);
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        setenv("HCCL_DFS_CONFIG", "connection_fault_detection_time:0", 1);
        InitEnvParam();
        std::cout << "MPI_CommInner_Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        ra_set_work_mode(0);
        std::cout << "MPI_CommInner_Test TearDown" << std::endl;
    }
};

TEST_F(MPI_CommInner_Test, ut_mpi_comm_init_sameip)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    HcclRootInfo commId;

    ret = hcclComm::GetUniqueId(&commId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hrtSetDevice(rank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Bcast(&commId, sizeof(commId), MPI_BYTE, 0, MPI_COMM_WORLD);

    RankInfo tmp_para_0;
    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverId = "10.21.78.208";
    tmp_para_0.nicIp.push_back(HcclIpAddress("10.21.78.208"));

    RankInfo tmp_para_1;
    tmp_para_1.userRank = 1;
    tmp_para_1.devicePhyId = 1;
    tmp_para_1.deviceType = DevType::DEV_TYPE_910;
    tmp_para_1.serverId = "10.21.78.208";
    tmp_para_1.nicIp.push_back(HcclIpAddress("10.21.78.208"));

    std::vector<RankInfo> para_vector;
    para_vector.push_back(tmp_para_0);
    para_vector.push_back(tmp_para_1);

    // 创建dispatcher
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    ret = HcclDispatcherDestroy(dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);//mpi通信域所有进程同步
}

TEST_F(MPI_CommInner_Test, ut_mpi_comm_init_diffip)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    HcclRootInfo commId;

    ret = hcclComm::GetUniqueId(&commId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hrtSetDevice(rank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Bcast(&commId, sizeof(commId), MPI_BYTE, 0, MPI_COMM_WORLD);

    RankInfo tmp_para_0;
    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverId = "10.21.78.208";
    tmp_para_0.nicIp.push_back(HcclIpAddress("10.21.78.208"));

    RankInfo tmp_para_1;
    tmp_para_1.userRank = 1;
    tmp_para_1.devicePhyId = 1;
    tmp_para_1.deviceType = DevType::DEV_TYPE_910;
    tmp_para_1.serverId = "10.21.78.209";
    tmp_para_1.nicIp.push_back(HcclIpAddress("10.21.78.209"));

    std::vector<RankInfo> para_vector;
    para_vector.push_back(tmp_para_0);
    para_vector.push_back(tmp_para_1);

    // 创建dispatcher
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    ret = HcclDispatcherDestroy(dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Barrier(MPI_COMM_WORLD);//mpi通信域所有进程同步
}
