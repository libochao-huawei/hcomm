/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <mpi.h>
#include <stdio.h>

#include <stdio.h>
#include <stdlib.h>
#include <runtime/rt.h>
#include <assert.h>
#include <securec.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string>
#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <algorithm>
#include "dlra_function.h"
#include "dlhal_function.h"
#include "mem_host_pub.h"
#include "mem_device_pub.h"
#include "sal.h"
#define private public
#define protected public
#include "hccl_communicator.h"
#include "hccl_impl.h"
#include "network_manager_pub.h"
#include "transport_shm_event_pub.h"
#include "op_base.h"
#include "hccl_ex.h"
#include "hccl_comm_pub.h"
#include "hcom_pub.h"
#undef protected
#undef private

#include <hccl/hccl.h>
#include <hccl/hccl_ex.h>
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub_gdr.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include "hccl/base.h"
#include "hccl/hcom.h"
#include <hccl/hccl_types.h>
#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"
#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include "externalinput_pub.h"
#include "rank_consistentcy_checker.h"
#include "remote_notify.h"
#include "socket.h"

#define private public
#define protected public
#undef protected
#undef private

using namespace std;
using namespace hccl;

class TransportShmEventTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "TransportShmEventTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "TransportShmEventTest TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
};

TEST_F(TransportShmEventTest, ut_TransportShmEvent_ExchangeIpcMesg)
{
    MachinePara machine_para;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(10000);
    std::string tag = "name";
    HcclIpAddress invaildIp;
    TransportShmEvent transport(nullptr, nullptr, machine_para, timeout, invaildIp, invaildIp, 18000, 18000, 1,
        CLIENT_ROLE_SOCKET, TransportResourceInfo(), nullptr);
    transport.GetLinkTag(tag);
    HcclIpAddress invalidIp;
    DeviceMem inputMem = DeviceMem::alloc(sizeof(s32));
    DeviceMem outputMem = DeviceMem::alloc(sizeof(s32));
    transport.machinePara_.machineType = MachineType::MACHINE_SERVER_TYPE;
    transport.machinePara_.serverId = "name";
    transport.machinePara_.localDeviceId = 1;
    transport.machinePara_.remoteDeviceId = 0;
    transport.machinePara_.localUserrank = 0;
    transport.machinePara_.localWorldRank = 0;
    transport.machinePara_.remoteUserrank = 1;
    transport.machinePara_.remoteWorldRank = 1;


    transport.machinePara_.deviceType = DevType::DEV_TYPE_310P3;
    transport.machinePara_.inputMem = inputMem;
    transport.machinePara_.outputMem = outputMem;
    transport.machinePara_.localIpAddr = invalidIp;

    transport.machinePara_.localUserrank = 1;
    transport.machinePara_.remoteUserrank = 0;

    MOCKER_CPP(&MemNameRepository::SetIpcMem, 
        HcclResult (MemNameRepository::*)(void *, u64, u8*, u32, u64 &, s32, s32, bool))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketBlockSend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = transport.InitMem();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = transport.ExchangeIpcMesg();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    TransportShmEvent transport1(nullptr, nullptr, machine_para, timeout, invaildIp, invaildIp, 18000, 18000, 0,
        SERVER_ROLE_SOCKET, TransportResourceInfo(), nullptr);

    transport1.machinePara_.machineType = MachineType::MACHINE_CLIENT_TYPE;
    transport1.machinePara_.serverId = "name";
    transport1.machinePara_.localDeviceId = 1;
    transport1.machinePara_.remoteDeviceId = 0;
    transport1.machinePara_.localUserrank = 0;
    transport1.machinePara_.localWorldRank = 0;
    transport1.machinePara_.remoteUserrank = 1;
    transport1.machinePara_.remoteWorldRank = 1;


    transport1.machinePara_.deviceType = DevType::DEV_TYPE_310P3;
    transport1.machinePara_.inputMem = inputMem;
    transport1.machinePara_.outputMem = outputMem;
    transport1.machinePara_.localIpAddr = invalidIp;

    transport1.machinePara_.localUserrank = 1;
    transport1.machinePara_.remoteUserrank = 0;

    MOCKER(hrtRaSocketBlockRecv)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = transport1.InitMem();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = transport1.ExchangeIpcMesg();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

#if 1
TEST_F(TransportShmEventTest, ut_TransportShmEvent_ExchangePidMesg)
{
    MachinePara machine_para;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(10000);
    HcclIpAddress invaildIp;
    TransportShmEvent transport(nullptr, nullptr, machine_para, timeout, invaildIp, invaildIp, 18000, 18000, 1,
        CLIENT_ROLE_SOCKET, TransportResourceInfo(), nullptr);
    HcclIpAddress invalidIp;
    DeviceMem inputMem = DeviceMem::alloc(sizeof(s32));
    DeviceMem outputMem = DeviceMem::alloc(sizeof(s32));
    transport.machinePara_.machineType = MachineType::MACHINE_SERVER_TYPE;
    transport.machinePara_.serverId = "name";
    transport.machinePara_.localDeviceId = 1;
    transport.machinePara_.remoteDeviceId = 0;
    transport.machinePara_.localUserrank = 0;
    transport.machinePara_.localWorldRank = 0;
    transport.machinePara_.remoteUserrank = 1;
    transport.machinePara_.remoteWorldRank = 1;


    transport.machinePara_.deviceType = DevType::DEV_TYPE_310P3;
    transport.machinePara_.inputMem = inputMem;
    transport.machinePara_.outputMem = outputMem;
    transport.machinePara_.localIpAddr = invalidIp;

    transport.machinePara_.localUserrank = 1;
    transport.machinePara_.remoteUserrank = 0;
    HcclResult ret;

    MOCKER_CPP(&MemNameRepository::SetIpcMem, 
        HcclResult (MemNameRepository::*)(void *, u64, u8*, u32, u64 &, s32, s32, bool))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketBlockSend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketBlockRecv)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtDrvDeviceGetBareTgid)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));


    MOCKER_CPP(&TransportShmEvent::CreateIpcSignal)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtDrvDeviceGetPhyIdByIndex)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&RemoteNotify::Init, HcclResult(RemoteNotify::*)(const std::vector<u8>&))
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&RemoteNotify::Open)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    ret = transport.ExchangePidMesg();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = transport.ExchangeSignalMesg();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    TransportShmEvent transport1(nullptr, nullptr, machine_para, timeout, invaildIp, invaildIp, 18000, 18000, 0,
        SERVER_ROLE_SOCKET, TransportResourceInfo(), nullptr);

    transport1.machinePara_.machineType = MachineType::MACHINE_CLIENT_TYPE;
    transport1.machinePara_.serverId = "name";
    transport1.machinePara_.localDeviceId = 1;
    transport1.machinePara_.remoteDeviceId = 0;
    transport1.machinePara_.localUserrank = 0;
    transport1.machinePara_.localWorldRank = 0;
    transport1.machinePara_.remoteUserrank = 1;
    transport1.machinePara_.remoteWorldRank = 1;


    transport1.machinePara_.deviceType = DevType::DEV_TYPE_310P3;
    transport1.machinePara_.inputMem = inputMem;
    transport1.machinePara_.outputMem = outputMem;
    transport1.machinePara_.localIpAddr = invalidIp;

    transport1.machinePara_.localUserrank = 1;
    transport1.machinePara_.remoteUserrank = 0;


    ret = transport1.ExchangePidMesg();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = transport1.ExchangeSignalMesg();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}
#endif

#if 1
TEST_F(TransportShmEventTest, ut_TransportShmEvent_SocketSend)
{
    MachinePara machine_para;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(10000);
    HcclIpAddress invaildIp;
    TransportShmEvent transport(nullptr, nullptr, machine_para, timeout, invaildIp, invaildIp, 18000, 18000, 1,
        CLIENT_ROLE_SOCKET, TransportResourceInfo(), nullptr);
    HcclIpAddress invalidIp;
    DeviceMem inputMem = DeviceMem::alloc(sizeof(s32));
    DeviceMem outputMem = DeviceMem::alloc(sizeof(s32));
    transport.machinePara_.machineType = MachineType::MACHINE_SERVER_TYPE;
    transport.machinePara_.serverId = "name";
    transport.machinePara_.localDeviceId = 1;
    transport.machinePara_.remoteDeviceId = 0;
    transport.machinePara_.localUserrank = 0;
    transport.machinePara_.localWorldRank = 0;
    transport.machinePara_.remoteUserrank = 1;
    transport.machinePara_.remoteWorldRank = 1;


    transport.machinePara_.deviceType = DevType::DEV_TYPE_310P3;
    transport.machinePara_.inputMem = inputMem;
    transport.machinePara_.outputMem = outputMem;
    transport.machinePara_.localIpAddr = invalidIp;

    transport.machinePara_.localUserrank = 1;
    transport.machinePara_.remoteUserrank = 0;
    HcclResult ret;

    MOCKER_CPP(&MemNameRepository::SetIpcMem, 
        HcclResult (MemNameRepository::*)(void *, u64, u8*, u32, u64 &, s32, s32, bool))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketBlockSend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketBlockRecv)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    std::string message = "test";
    ret = transport.SocketSend(message);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    TransportShmEvent transport1(nullptr, nullptr, machine_para, timeout, invaildIp, invaildIp, 18000, 18000, 0,
        SERVER_ROLE_SOCKET, TransportResourceInfo(), nullptr);

    transport1.machinePara_.machineType = MachineType::MACHINE_CLIENT_TYPE;
    transport1.machinePara_.serverId = "name";
    transport1.machinePara_.localDeviceId = 1;
    transport1.machinePara_.remoteDeviceId = 0;
    transport1.machinePara_.localUserrank = 0;
    transport1.machinePara_.localWorldRank = 0;
    transport1.machinePara_.remoteUserrank = 1;
    transport1.machinePara_.remoteWorldRank = 1;


    transport1.machinePara_.deviceType = DevType::DEV_TYPE_310P3;
    transport1.machinePara_.inputMem = inputMem;
    transport1.machinePara_.outputMem = outputMem;
    transport1.machinePara_.localIpAddr = invalidIp;

    transport1.machinePara_.localUserrank = 1;
    transport1.machinePara_.remoteUserrank = 0;


    ret = transport1.SocketRecv(message);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}
#endif

#if 1
TEST_F(TransportShmEventTest, ut_TransportShmEvent_PrepareConnect)
{
    MachinePara machine_para;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(10000);
    HcclIpAddress invaildIp;
    TransportShmEvent transport(nullptr, nullptr, machine_para, timeout, invaildIp, invaildIp, 18000, 18000, 1,
        CLIENT_ROLE_SOCKET, TransportResourceInfo(), nullptr);
    HcclIpAddress invalidIp;
    DeviceMem inputMem = DeviceMem::alloc(sizeof(s32));
    DeviceMem outputMem = DeviceMem::alloc(sizeof(s32));
    transport.machinePara_.machineType = MachineType::MACHINE_SERVER_TYPE;
    transport.machinePara_.serverId = "name";
    transport.machinePara_.localDeviceId = 1;
    transport.machinePara_.remoteDeviceId = 0;
    transport.machinePara_.localUserrank = 0;
    transport.machinePara_.localWorldRank = 0;
    transport.machinePara_.remoteUserrank = 1;
    transport.machinePara_.remoteWorldRank = 1;


    transport.machinePara_.deviceType = DevType::DEV_TYPE_310P3;
    transport.machinePara_.inputMem = inputMem;
    transport.machinePara_.outputMem = outputMem;
    transport.machinePara_.localIpAddr = invalidIp;

    transport.machinePara_.localUserrank = 1;
    transport.machinePara_.remoteUserrank = 0;
    HcclResult ret;

    MOCKER_CPP(&MemNameRepository::SetIpcMem, 
        HcclResult (MemNameRepository::*)(void *, u64, u8*, u32, u64 &, s32, s32, bool))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketBlockSend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketBlockRecv)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketWhiteListAdd)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketWhiteListDel)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketBatchConnect)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    
    MOCKER(hrtRaBlockGetSockets)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = transport.PrepareConnect();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = transport.GetConnection();
    EXPECT_EQ(ret, HCCL_E_TCP_TRANSFER);

    ret = transport.ConnectToServer();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = transport.InitSocketWhiteList();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = transport.AddSocketWhiteList();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    TransportShmEvent transport1(nullptr, nullptr, machine_para, timeout, invaildIp, invaildIp, 18000, 18000, 0,
        SERVER_ROLE_SOCKET, TransportResourceInfo(), nullptr);

    transport1.machinePara_.machineType = MachineType::MACHINE_CLIENT_TYPE;
    transport1.machinePara_.serverId = "name";
    transport1.machinePara_.localDeviceId = 1;
    transport1.machinePara_.remoteDeviceId = 0;
    transport1.machinePara_.localUserrank = 0;
    transport1.machinePara_.localWorldRank = 0;
    transport1.machinePara_.remoteUserrank = 1;
    transport1.machinePara_.remoteWorldRank = 1;


    transport1.machinePara_.deviceType = DevType::DEV_TYPE_310P3;
    transport1.machinePara_.inputMem = inputMem;
    transport1.machinePara_.outputMem = outputMem;
    transport1.machinePara_.localIpAddr = invalidIp;

    transport1.machinePara_.localUserrank = 1;
    transport1.machinePara_.remoteUserrank = 0;

    ret = transport1.PrepareConnect();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = transport1.GetConnection();
    EXPECT_EQ(ret, HCCL_E_TCP_TRANSFER);

    ret = transport1.ConnectToServer();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = transport1.InitSocketWhiteList();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = transport1.AddSocketWhiteList();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}
#endif


#if 1
TEST_F(TransportShmEventTest, ut_TransportShmEvent_Init)
{
    MachinePara machine_para;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(10000);
    HcclIpAddress invaildIp;
    int tmp = 45;
    rtContext_t ctx =  (rtContext_t)&tmp;
    TransportShmEvent transport(nullptr, nullptr, machine_para, timeout, invaildIp, invaildIp, 18000, 18000, 1,
        CLIENT_ROLE_SOCKET, TransportResourceInfo(), ctx);
    HcclIpAddress invalidIp;
    DeviceMem inputMem = DeviceMem::alloc(sizeof(s32));
    DeviceMem outputMem = DeviceMem::alloc(sizeof(s32));
    transport.machinePara_.machineType = MachineType::MACHINE_SERVER_TYPE;
    transport.machinePara_.serverId = "name";
    transport.machinePara_.localDeviceId = 1;
    transport.machinePara_.remoteDeviceId = 0;
    transport.machinePara_.localUserrank = 0;
    transport.machinePara_.localWorldRank = 0;
    transport.machinePara_.remoteUserrank = 1;
    transport.machinePara_.remoteWorldRank = 1;


    transport.machinePara_.deviceType = DevType::DEV_TYPE_310P3;
    transport.machinePara_.inputMem = inputMem;
    transport.machinePara_.outputMem = outputMem;
    transport.machinePara_.localIpAddr = invalidIp;
    transport.machinePara_.localUserrank = 1;
    transport.machinePara_.remoteUserrank = 0;

    IpSocket ipsocketInfo;
    ipsocketInfo.nicSocketHandle = (void*)0x00000001;
    ipsocketInfo.nicRdmaHandle = (void*)0x00000001;
    transport.machinePara_.deviceLogicId = 0;
    RaResourceInfo &raResourceInfo = NetworkManager::GetInstance(transport.machinePara_.deviceLogicId).raResourceInfo_;
    raResourceInfo.hostNetSocketMap.insert({invaildIp, ipsocketInfo});

    HcclResult ret;
    MOCKER_CPP(&MemNameRepository::SetIpcMem, 
        HcclResult (MemNameRepository::*)(void *, u64, u8*, u32, u64 &, s32, s32, bool))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportShmEvent::GetConnection)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketBlockSend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketBlockRecv)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketWhiteListAdd)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketWhiteListDel)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketBatchConnect)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    
    MOCKER(hrtRaBlockGetSockets)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtDrvDeviceGetBareTgid)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtCtxGetCurrent)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtCtxSetCurrent)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportShmEvent::CreateIpcSignal)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtDrvDeviceGetPhyIdByIndex)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtHalEschedAttachDevice)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&DlHalFunction::DlHalFunctionInit)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
   MOCKER_CPP(&Socket::PrepareConnect)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
   MOCKER_CPP(&TransportHeterog::CheckRecvMsgAndRequestBuffer)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&RemoteNotify::Init, HcclResult(RemoteNotify::*)(const std::vector<u8>&))
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&RemoteNotify::Open)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    ret = transport.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    raResourceInfo.hostNetSocketMap.erase(invaildIp);

    TransportShmEvent transport1(nullptr, nullptr, machine_para, timeout, invaildIp, invaildIp, 18000, 18000, 0,
        SERVER_ROLE_SOCKET, TransportResourceInfo(), ctx);

    transport1.machinePara_.machineType = MachineType::MACHINE_CLIENT_TYPE;
    transport1.machinePara_.serverId = "name";
    transport1.machinePara_.localDeviceId = 1;
    transport1.machinePara_.remoteDeviceId = 0;
    transport1.machinePara_.localUserrank = 0;
    transport1.machinePara_.localWorldRank = 0;
    transport1.machinePara_.remoteUserrank = 1;
    transport1.machinePara_.remoteWorldRank = 1;


    transport1.machinePara_.deviceType = DevType::DEV_TYPE_310P3;
    transport1.machinePara_.inputMem = inputMem;
    transport1.machinePara_.outputMem = outputMem;
    transport1.machinePara_.localIpAddr = invalidIp;

    transport1.machinePara_.localUserrank = 1;
    transport1.machinePara_.remoteUserrank = 0;

    transport1.machinePara_.deviceLogicId = 0;
    RaResourceInfo &raResourceInfo1 = NetworkManager::GetInstance(transport1.machinePara_.deviceLogicId).raResourceInfo_;
    raResourceInfo1.hostNetSocketMap.insert({invaildIp, ipsocketInfo});

    ret = transport1.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    raResourceInfo1.hostNetSocketMap.erase(invaildIp);

    GlobalMockObject::verify();
}
#endif

TEST_F(TransportShmEventTest, ut_TransportShmEvent_Imrecv)
{
    MachinePara machine_para;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(10000);
    HcclIpAddress invaildIp;
    TransportShmEvent transport(nullptr, nullptr, machine_para, timeout, invaildIp, invaildIp, 18000, 18000, 1,
        CLIENT_ROLE_SOCKET, TransportResourceInfo(), nullptr);
    HcclIpAddress invalidIp;
    DeviceMem inputMem = DeviceMem::alloc(sizeof(s32));
    DeviceMem outputMem = DeviceMem::alloc(sizeof(s32));
    transport.machinePara_.machineType = MachineType::MACHINE_SERVER_TYPE;
    transport.machinePara_.serverId = "name";
    transport.machinePara_.localDeviceId = 1;
    transport.machinePara_.remoteDeviceId = 0;
    transport.machinePara_.localUserrank = 0;
    transport.machinePara_.localWorldRank = 0;
    transport.machinePara_.remoteUserrank = 1;
    transport.machinePara_.remoteWorldRank = 1;


    transport.machinePara_.deviceType = DevType::DEV_TYPE_310P3;
    transport.machinePara_.inputMem = inputMem;
    transport.machinePara_.outputMem = outputMem;
    transport.machinePara_.localIpAddr = invalidIp;

    transport.machinePara_.localUserrank = 1;
    transport.machinePara_.remoteUserrank = 0;
    HcclResult ret;

    MemType memType = MemType::USER_INPUT_MEM;
    u64 offset = 4;
    u64 count = 1024;
    transport.deviceLogicId_ = HOST_DEVICE_ID;
    HcclEnvelopePcie pcie;
    pcie.count = 1024;
    pcie.dataType = 2;
    pcie.memType = USER_INPUT_MEM;
    pcie.offset = 4;
    pcie.updateEndFlag = true;
    pcie.tableId = 2;

    u32 recvData = 1;
    TransData transData;
    transData.count = 1;
    transData.dataType = HCCL_DATA_TYPE_INT8;
    transData.srcBuf = reinterpret_cast<u64>(&recvData);

    HcclMessageInfo msg;
    msg.envelope.pcieEnvelope = pcie;
    HcclRequestInfo *request;

    MOCKER_CPP(&TransportHeterog::GenerateRecvRequest)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportShmEvent::CheckShmMemRange)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtDrvMemCpy)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    

    MOCKER_CPP(&RemoteNotify::Post)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&LocklessRingMemoryAllocate<HcclMessageInfo>::Free)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = transport.Imrecv(transData, msg, request);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

#if 1
TEST_F(TransportShmEventTest, ut_TransportShmEvent_CheckShmMemRange)
{
    MachinePara machine_para;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(10000);
    HcclIpAddress invaildIp;
    TransportShmEvent transport(nullptr, nullptr, machine_para, timeout, invaildIp, invaildIp, 18000, 18000, 1,
        CLIENT_ROLE_SOCKET, TransportResourceInfo(), nullptr);
    HcclIpAddress invalidIp;
    DeviceMem inputMem = DeviceMem::alloc(sizeof(s32));
    DeviceMem outputMem = DeviceMem::alloc(sizeof(s32));
    transport.machinePara_.machineType = MachineType::MACHINE_SERVER_TYPE;
    transport.machinePara_.serverId = "name";
    transport.machinePara_.localDeviceId = 1;
    transport.machinePara_.remoteDeviceId = 0;
    transport.machinePara_.localUserrank = 0;
    transport.machinePara_.localWorldRank = 0;
    transport.machinePara_.remoteUserrank = 1;
    transport.machinePara_.remoteWorldRank = 1;


    transport.machinePara_.deviceType = DevType::DEV_TYPE_310P3;
    transport.machinePara_.inputMem = inputMem;
    transport.machinePara_.outputMem = outputMem;
    transport.machinePara_.localIpAddr = invalidIp;

    transport.machinePara_.localUserrank = 1;
    transport.machinePara_.remoteUserrank = 0;
    HcclResult ret;

    MOCKER_CPP(&MemNameRepository::SetIpcMem, 
        HcclResult (MemNameRepository::*)(void *, u64, u8*, u32, u64 &, s32, s32, bool))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketBlockSend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketBlockRecv)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MemType memType = MemType::USER_INPUT_MEM;
    u64 offset = 4;
    u64 count = 1024;

    ret = transport.CheckShmMemRange(memType, offset, count, 1);
    EXPECT_EQ(ret, HCCL_E_MEMORY);

    ret = transport.CheckShmMemRange(memType, offset, count, HCCL_DATA_TYPE_RESERVED + 1);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
    GlobalMockObject::verify();
}
#endif
