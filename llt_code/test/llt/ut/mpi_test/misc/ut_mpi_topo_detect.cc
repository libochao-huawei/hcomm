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
#include "rt_external.h"
#include <assert.h>
#include <securec.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netdb.h>

#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "dlra_function.h"

#define private public
#define protected public
#include "topoinfo_detect.h"
#undef protected
#undef private

#include <hccl/hccl.h>
#include "hccl_comm_pub.h"
#include "llt_hccl_stub_pub.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include "hccl/base.h"
#include <hccl/hccl_types.h>
#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"
#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include "externalinput_pub.h"
#include "externalinput.h"
#include "hcom_private.h"
#include "hccl_communicator.h"

using namespace std;
using namespace hccl;
class MPI_TOPO_DETECT_Test : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        GlobalMockObject::verify();
        std::cout << "MPI_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        GlobalMockObject::verify();
        MPI_Barrier(MPI_COMM_WORLD);
        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        ra_set_test_type(0, "ST_MPI_TOPODETECT_TEST");
        ResetInitState();
        std::cout << "A TestCase SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        MPI_Barrier(MPI_COMM_WORLD);
        std::cout << "A TestCase TearDown" << std::endl;
    }
};

TEST_F(MPI_TOPO_DETECT_Test, ut_hcclGetRootInfo_single_server_2p_success_normal_nic_spec)
{
    HcclResult ret;
    s32 nnode, rank = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    hrtSetDevice(rank);

    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
    setenv("HCCL_IF_IP", "127.0.0.1", 1);
    setenv("HCCL_BUFFSIZE", "200", 1);

    HcclRootInfo id;
    if (rank == 0) {
        NetworkManager::GetInstance(0).hostNicInitRef_.Clear();
        ret = HcclGetRootInfo(&id);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
    MPI_Bcast(&id, sizeof(id), MPI_BYTE, 0, MPI_COMM_WORLD);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(nnode, &id, rank, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_IF_IP");
    unsetenv("HCCL_BUFFSIZE");
}

TEST_F(MPI_TOPO_DETECT_Test, ut_hcclGetRootInfo_single_server_2p_success_security)
{
    nlohmann::json whitelist =
    {
        {
            "host_ip",
            {
                "127.0.0.1",
            }
        }
    };

    HcclResult ret;
    s32 nnode, rank = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    hrtSetDevice(rank);
    char *buffer;
    //也可以将buffer作为输出参数
    if((buffer = getcwd(NULL, 0)) != NULL)
    {
        std::string dirPath = buffer;
        std::string fileName = "/ut_hccl_host_allowlist";
        std::string realPath = dirPath.c_str() + fileName;
        setenv("HCCL_WHITELIST_FILE", realPath.c_str(), 1);
        ofstream outfile(realPath);
        outfile << std::setw(4) << whitelist << std::endl;
        outfile.close();

        HcclRootInfo id;
        if (rank == 0) {
            NetworkManager::GetInstance(0).hostNicInitRef_.Clear();
            ret = HcclGetRootInfo(&id);
            EXPECT_EQ(ret, HCCL_SUCCESS);
        }

        MPI_Bcast(&id, sizeof(id), MPI_BYTE, 0, MPI_COMM_WORLD);

        HcclComm newcomm;
        ret = HcclCommInitRootInfo(nnode, &id, rank, &newcomm);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        MPI_Barrier(MPI_COMM_WORLD);

        ret = HcclCommDestroy(newcomm);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        unsetenv("HCCL_WHITELIST_FILE");
        remove(realPath.c_str());
    }
    free(buffer);
}

TEST_F(MPI_TOPO_DETECT_Test, ut_hcclGetRootInfo_single_server_2p_success_normal)
{
    HcclResult ret;
    s32 nnode, rank = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    hrtSetDevice(rank);

    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
    setenv("HCCL_IF_IP", "127.0.0.1", 1);
    setenv("HCCL_BUFFSIZE", "200", 1);
    HcclRootInfo id;
    if (rank == 0) {
        NetworkManager::GetInstance(0).hostNicInitRef_.Clear();
        ret = HcclGetRootInfo(&id);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    MPI_Bcast(&id, sizeof(id), MPI_BYTE, 0, MPI_COMM_WORLD);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(nnode, &id, rank, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_IF_IP");
    unsetenv("HCCL_BUFFSIZE");
}

TEST_F(MPI_TOPO_DETECT_Test, ut_hcclGetRootInfo_single_server_2p_success_normal_1)
{
    HcclResult ret;
    s32 nnode, rank = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    hrtSetDevice(rank);
    // 初始化环境变量，just for st，防止用例间影响
    ResetInitState();
    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
    // 网卡信息在ra_get_ifaddrs接口已初始化（eth0,docker,lo）
    // 匹配eth0，eth1名称的网卡
    setenv("HCCL_SOCKET_IFNAME", "=eth0,eth1", 1);

    HcclRootInfo id;
    if (rank == 0) {
        NetworkManager::GetInstance(0).hostNicInitRef_.Clear();
        ret = HcclGetRootInfo(&id);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    MPI_Bcast(&id, sizeof(id), MPI_BYTE, 0, MPI_COMM_WORLD);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(nnode, &id, rank, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_SOCKET_IFNAME");
}

TEST_F(MPI_TOPO_DETECT_Test, ut_hcclGetRootInfo_invaild_env_01)
{
    setenv("HCCL_WHITELIST_DISABLE", "2", 1);
    HcclRootInfo id;
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_E_PARA);

    unsetenv("HCCL_WHITELIST_DISABLE");
}
TEST_F(MPI_TOPO_DETECT_Test, ut_hcclMasterInfo_single_server_2p_success_normal)
{
    HcclResult ret;
    s32 nnode, rank = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    hrtSetDevice(rank);

    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
    setenv("HCCL_IF_IP", "127.0.0.1", 1);
    setenv("HCCL_BUFFSIZE", "200", 1);
    string masterIp = "127.0.0.1";
    string masterPort = "6000";
    string masterID = "0";
    string rankIp = "127.0.0.1";
    string rankSize = "2";

    NetworkManager::GetInstance(0).hostNicInitRef_.Clear();
    ret = HcomInitByMasterInfo(masterIp.c_str(), masterPort.c_str(), masterID.c_str(), rankSize.c_str(), rankIp.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);


    MPI_Barrier(MPI_COMM_WORLD);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ResetInitState();
    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_IF_IP");
    unsetenv("HCCL_BUFFSIZE");
    
}

// 超时用例影响线上执行耗时已下掉
