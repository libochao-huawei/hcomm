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
#include <fstream>
#include <sstream>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>

#include "hccl/base.h"
#include <hccl/hccl_types.h>

#include "sal.h"

#define private public
#define protected public
#include "transport_ibverbs_pub.h"
#undef protected
#undef private

#include "llt_hccl_stub_pub.h"
#include "dlra_function.h"
#include "profiler_manager.h"
#include "externalinput.h"
#include "transport_manager.h"
#include "env_config.h"

using namespace std;
using namespace hccl;


class LinkIbvExpTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--LinkIbvExpTest SetUP--\033[0m" << std::endl;
        DlRaFunction::GetInstance().DlRaFunctionInit();
    }
    static void TearDownTestCase()
    {

        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--LinkIbvExpTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        setenv("HCCL_DFS_CONFIG", "connection_fault_detection_time:0", 1);
        InitEnvParam();
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;
};
HcclDispatcher LinkIbvExpTest::dispatcherPtr = nullptr;
DispatcherPub *LinkIbvExpTest::dispatcher = nullptr;

TEST_F(LinkIbvExpTest, ut_link_base_test)
{
    s32 ret;
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    s32 mem_size = 256;
    DeviceMem mem = DeviceMem::alloc(mem_size);

    MachinePara machinePara;

    std::chrono::milliseconds timeout;
    const std::string tag;

    std::shared_ptr<Transport> link_base(new Transport(new (std::nothrow) TransportBase(
        dispatcher, nullptr, machinePara, timeout)));

    link_base->Init();
    link_base->TxAsync(UserMemType::INPUT_MEM, 0, mem.ptr(), mem_size, stream);
    link_base->RxAsync(UserMemType::INPUT_MEM, 0, mem.ptr(), mem_size, stream);
    link_base->TxAck(stream);
    link_base->RxAck(stream);
    link_base->TxPrepare(stream);
    link_base->RxPrepare(stream);
    link_base->TxData(UserMemType::INPUT_MEM, 0, mem.ptr(), mem_size, stream);
    link_base->RxData(UserMemType::INPUT_MEM, 0, mem.ptr(), mem_size, stream);
    link_base->TxDone(stream);
    link_base->RxDone(stream);
    link_base->PostReady(stream);
    link_base->PostReady(stream);
    struct Transport::Buffer remoteBuf(mem.ptr(), mem_size);
    struct Transport::Buffer localBuf(mem.ptr(), mem_size);
    link_base->WriteAsync(remoteBuf, localBuf, stream);
    link_base->PostFin(stream);
    link_base->WaitFin(stream);
}
 
TEST_F(LinkIbvExpTest, ut_transport_for_batchsendrecv_muti)
{
    s32 ret;
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    s32 mem_size = 256;
    DeviceMem mem = DeviceMem::alloc(mem_size);

    MachinePara machinePara;
    machinePara.localDeviceId = 0;
    machinePara.isAicpuModeEn = true;
    std::chrono::milliseconds timeout;
    const std::string tag;
     std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::shared_ptr<TransportIbverbs> linktmp = nullptr;
    linktmp.reset(new TransportIbverbs(dispatcher, notifyPool, machinePara, timeout));
    linktmp->Init();
    linktmp->TxPrepare(stream);
    linktmp->RxPrepare(stream);
    linktmp->TxData(UserMemType::INPUT_MEM, 0, mem.ptr(), mem_size, stream);
    linktmp->RxData(UserMemType::INPUT_MEM, 0, mem.ptr(), mem_size, stream);
    linktmp->TxDone(stream);
    linktmp->RxDone(stream);
    linktmp->Fence();
    ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}



TEST_F(LinkIbvExpTest, ut_function_for_onedoorbell_ibv)
{
    s32 ret;
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(0);

    MachinePara machinePara;
    machinePara.localDeviceId = 0;
    machinePara.isAicpuModeEn = true;
    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("test", 
        nullptr, remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machinePara.sockets.push_back(newSocket);
    machinePara.inputMem = DeviceMem::alloc(1);
    machinePara.outputMem = DeviceMem::alloc(1);

    std::chrono::milliseconds timeout;
    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    TransportIbverbs transportIbverbs(dispatcher, notifyPool, machinePara, timeout);
    std::vector<WqeInfo> wqeInfoVec;

    MOCKER_CPP(&TransportIbverbs::GetNicHandle, HcclResult(TransportIbverbs::*)())
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportIbverbs::InitQpConnect)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(transportIbverbs, &TransportIbverbs::TxWqeList)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    std::shared_ptr<TransportIbverbs> linktmp = nullptr;
    linktmp.reset(new TransportIbverbs(dispatcher, notifyPool, machinePara, timeout));
    ret = linktmp->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    linktmp->EnableUseOneDoorbell();
    linktmp->workFlowMode_ = HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE;
    ret = linktmp->TxWithReduce(UserMemType::OUTPUT_MEM, 0, inputMem.ptr(), 1,
                                     HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, stream);
    GlobalMockObject::verify();
}

TEST_F(LinkIbvExpTest, ut_transport_ibv_multi_qp_configpath)
{
    HcclResult ret = HCCL_SUCCESS;
    setenv("HCCL_RDMA_QP_PORT_CONFIG_PATH", "/tmp/", 1);
    ret = InitEnvVarParam();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string filePath = GetExternalInputQpSrcPortConfigPath();
    std::string fileStr = filePath + "/MultiQpSrcPort.cfg";
    HCCL_ERROR("==TMP== fileStr [%s]", fileStr.c_str());

    const int FILE_AUTHORITY = 0600;
    int fd = open(fileStr.c_str(), O_WRONLY | O_CREAT | O_TRUNC, FILE_AUTHORITY);
    if (fd < 0) {
        HCCL_ERROR("Fail to open the file: %s.", fileStr.c_str(), HCCL_E_PARA);
    }
    if (close(fd) != 0) {
        HCCL_ERROR("Fail to close the file: %s.", fileStr.c_str(), HCCL_E_PARA);
    }
    std::ofstream fileStream(fileStr.c_str(), std::ios::out | std::ios::binary);
    if (fileStream.is_open()) {
        fileStream << "0.0.0.0" << "," << "0.0.0.0" << "=" << "61000" << "," << "61001" << "," << "61002" << std::endl;
        fileStream << "192.2.100.2" << "," << "0.0.0.0" << "=" << "61000" << "," << "61001" << "," << "61002" << std::endl;
        fileStream << "0.0.0.0" << "," << "192.2.100.1" << "=" << "61000" << "," << "61001" << "," << "61002" << std::endl;
        fileStream << "192.2.100.2" << "," << "192.2.100.1" << "=" << "61100" << "," << "61101" << "," << "61102" << std::endl;
        fileStream.close();
    } else {
        HCCL_ERROR("[Initialize][GraphOptimizer]file %s open failed!", fileStr.c_str());
    }

    CCLBufferManager cclBufferManager;
    std::unique_ptr<HcclSocketManager> socketManager;
    HcclDispatcher dispatcher0;
    std::unique_ptr<NotifyPool> notifyPool0;
    std::vector<RankInfo> rankInfoList;
    RankId userRank;
    std::string identifier;
    s32 deviceLogicId;
    NICDeployment nicDeployment;
    bool isHaveCpuRank;
    void *transportResourceInfoAddr;
    size_t transportResourceInfoSize;
    bool isUseRankPort = false;
    bool isUsedRdmaOuter = true;
    std::vector<u32> nicRanksPort;
    std::vector<u32> vnicRanksPort;
    bool useSuperPodMode;
    std::vector<HcclIpAddress> devIpAddr;
    HcclIpAddress hostIp;
    HcclIpAddress localVnicIp;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    TransportManager transManager(cclBufferManager,
        socketManager,
        dispatcher0,
        notifyPool0,
        rankInfoList,
        userRank,
        identifier,
        deviceLogicId,
        nicDeployment,
        isHaveCpuRank,
        transportResourceInfoAddr,
        transportResourceInfoSize,
        isUseRankPort,
        isUsedRdmaOuter,
        nicRanksPort,
        vnicRanksPort,
        useSuperPodMode,
        devIpAddr,
        hostIp,
        localVnicIp,
        netDevCtxMap);
    ret = transManager.LoadMultiQpSrcPortFromFile();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    MachinePara machinePara;
    machinePara.localDeviceId = 0;
    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("test", 
        nullptr, remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machinePara.sockets.push_back(newSocket);
    machinePara.localIpAddr = HcclIpAddress("1.2.3.4");
    machinePara.remoteIpAddr = HcclIpAddress("2.2.3.4");
    ret = transManager.GetConfigSrcPorts(machinePara);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(0);

    std::chrono::milliseconds timeout;
    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);

    std::vector<WqeInfo> wqeInfoVec;

    TransportIbverbs transportIbverbs(dispatcher, notifyPool, machinePara, timeout);

    MOCKER_CPP(&TransportIbverbs::CreateOneQp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = transportIbverbs.CreateMultiQp(OPBASE_QP_MODE_EXT, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_RDMA_QP_PORT_CONFIG_PATH");
    ResetInitState();
}

TEST_F(LinkIbvExpTest, ut_transport_ibv_multi_qp_configpath_patch1)
{
    HcclResult ret = HCCL_SUCCESS;
    setenv("HCCL_RDMA_QP_PORT_CONFIG_PATH", "/tmp/", 1);
    ret = InitEnvVarParam();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string filePath = GetExternalInputQpSrcPortConfigPath();
    std::string fileStr = filePath + "/MultiQpSrcPort.cfg";
    HCCL_ERROR("==TMP== fileStr [%s]", fileStr.c_str());

    const int FILE_AUTHORITY = 0600;
    int fd = open(fileStr.c_str(), O_WRONLY | O_CREAT | O_TRUNC, FILE_AUTHORITY);
    if (fd < 0) {
        HCCL_ERROR("Fail to open the file: %s.", fileStr.c_str(), HCCL_E_PARA);
    }
    if (close(fd) != 0) {
        HCCL_ERROR("Fail to close the file: %s.", fileStr.c_str(), HCCL_E_PARA);
    }
    std::ofstream fileStream(fileStr.c_str(), std::ios::out | std::ios::binary);
    if (fileStream.is_open()) {
        fileStream << "192.2.100.2" << "," << "0.0.0.0" << "=" << "61000" << "," << "61001" << "," << "61002" << std::endl;
        fileStream << "0.0.0.0" << "," << "192.2.100.1" << "=" << "61000" << "," << "61001" << "," << "61002" << std::endl;
        fileStream << "192.2.100.2" << "," << "192.2.100.1" << "=" << "61100" << "," << "61101" << "," << "61102" << std::endl;
        fileStream.close();
    } else {
        HCCL_ERROR("[Initialize][GraphOptimizer]file %s open failed!", fileStr.c_str());
    }

    CCLBufferManager cclBufferManager;
    std::unique_ptr<HcclSocketManager> socketManager;
    HcclDispatcher dispatcher0;
    std::unique_ptr<NotifyPool> notifyPool0;
    std::vector<RankInfo> rankInfoList;
    RankId userRank;
    std::string identifier;
    s32 deviceLogicId;
    NICDeployment nicDeployment;
    bool isHaveCpuRank;
    void *transportResourceInfoAddr;
    size_t transportResourceInfoSize;
    bool isUseRankPort = false;
    bool isUsedRdmaOuter = true;
    std::vector<u32> nicRanksPort;
    std::vector<u32> vnicRanksPort;
    bool useSuperPodMode;
    std::vector<HcclIpAddress> devIpAddr;
    HcclIpAddress hostIp;
    HcclIpAddress localVnicIp;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    TransportManager transManager(cclBufferManager,
        socketManager,
        dispatcher0,
        notifyPool0,
        rankInfoList,
        userRank,
        identifier,
        deviceLogicId,
        nicDeployment,
        isHaveCpuRank,
        transportResourceInfoAddr,
        transportResourceInfoSize,
        isUseRankPort,
        isUsedRdmaOuter,
        nicRanksPort,
        vnicRanksPort,
        useSuperPodMode,
        devIpAddr,
        hostIp,
        localVnicIp,
        netDevCtxMap);
    ret = transManager.LoadMultiQpSrcPortFromFile();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    MachinePara machinePara;
    machinePara.localDeviceId = 0;
    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("test", 
        nullptr, remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machinePara.sockets.push_back(newSocket);
    machinePara.localIpAddr = HcclIpAddress("1.2.3.4");
    machinePara.remoteIpAddr = HcclIpAddress("2.2.3.4");
    ret = transManager.GetConfigSrcPorts(machinePara);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_RDMA_QP_PORT_CONFIG_PATH");
    ResetInitState();
}

TEST_F(LinkIbvExpTest, ut_transport_ibv_multi_qp_configpath_patch2)
{
    HcclResult ret = HCCL_SUCCESS;
    setenv("HCCL_RDMA_QP_PORT_CONFIG_PATH", "/tmp/", 1);
    ret = InitEnvVarParam();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string filePath = GetExternalInputQpSrcPortConfigPath();
    std::string fileStr = filePath + "/MultiQpSrcPort.cfg";
    HCCL_ERROR("==TMP== fileStr [%s]", fileStr.c_str());

    const int FILE_AUTHORITY = 0600;
    int fd = open(fileStr.c_str(), O_WRONLY | O_CREAT | O_TRUNC, FILE_AUTHORITY);
    if (fd < 0) {
        HCCL_ERROR("Fail to open the file: %s.", fileStr.c_str(), HCCL_E_PARA);
    }
    if (close(fd) != 0) {
        HCCL_ERROR("Fail to close the file: %s.", fileStr.c_str(), HCCL_E_PARA);
    }
    std::ofstream fileStream(fileStr.c_str(), std::ios::out | std::ios::binary);
    if (fileStream.is_open()) {
        fileStream << "fe80::1034:56ff:fe78:9abc"<< ","<< "fe80::6847:9eff:feec:9b6e"<< "="<< "61000"<< ","<< "61001"<< ","<< "61002" << std::endl;
        fileStream.close();
    } else {
        HCCL_ERROR("[Initialize][GraphOptimizer]file %s open failed!", fileStr.c_str());
    }

    CCLBufferManager cclBufferManager;
    std::unique_ptr<HcclSocketManager> socketManager;
    HcclDispatcher dispatcher0;
    std::unique_ptr<NotifyPool> notifyPool0;
    std::vector<RankInfo> rankInfoList;
    RankId userRank;
    std::string identifier;
    s32 deviceLogicId;
    NICDeployment nicDeployment;
    bool isHaveCpuRank;
    void *transportResourceInfoAddr;
    size_t transportResourceInfoSize;
    bool isUseRankPort = false;
    bool isUsedRdmaOuter = true;
    std::vector<u32> nicRanksPort;
    std::vector<u32> vnicRanksPort;
    bool useSuperPodMode;
    std::vector<HcclIpAddress> devIpAddr;
    HcclIpAddress hostIp;
    HcclIpAddress localVnicIp;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    TransportManager transManager(cclBufferManager,
        socketManager,
        dispatcher0,
        notifyPool0,
        rankInfoList,
        userRank,
        identifier,
        deviceLogicId,
        nicDeployment,
        isHaveCpuRank,
        transportResourceInfoAddr,
        transportResourceInfoSize,
        isUseRankPort,
        isUsedRdmaOuter,
        nicRanksPort,
        vnicRanksPort,
        useSuperPodMode,
        devIpAddr,
        hostIp,
        localVnicIp,
        netDevCtxMap);
    ret = transManager.LoadMultiQpSrcPortFromFile();
    EXPECT_EQ(ret, HCCL_E_PARA);

    unsetenv("HCCL_RDMA_QP_PORT_CONFIG_PATH");
    ResetInitState();
}

TEST_F(LinkIbvExpTest, ut_transport_ibv_multi_qp_configpath_patch3)
{
    HcclResult ret = HCCL_SUCCESS;
    setenv("HCCL_RDMA_QP_PORT_CONFIG_PATH", "/tmp/", 1);
    ret = InitEnvVarParam();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string filePath = GetExternalInputQpSrcPortConfigPath();
    std::string fileStr = filePath + "/MultiQpSrcPort.cfg";
    HCCL_ERROR("==TMP== fileStr [%s]", fileStr.c_str());

    const int FILE_AUTHORITY = 0600;
    int fd = open(fileStr.c_str(), O_WRONLY | O_CREAT | O_TRUNC, FILE_AUTHORITY);
    if (fd < 0) {
        HCCL_ERROR("Fail to open the file: %s.", fileStr.c_str(), HCCL_E_PARA);
    }
    if (close(fd) != 0) {
        HCCL_ERROR("Fail to close the file: %s.", fileStr.c_str(), HCCL_E_PARA);
    }
    std::ofstream fileStream(fileStr.c_str(), std::ios::out | std::ios::binary);
    if (fileStream.is_open()) {
        fileStream << "192.2.100.2" << "," << "1.0.0.0.0" << "=" << "61000" << "," << "61001" << "," << "61002" << std::endl;
        fileStream.close();
    } else {
        HCCL_ERROR("[Initialize][GraphOptimizer]file %s open failed!", fileStr.c_str());
    }

    CCLBufferManager cclBufferManager;
    std::unique_ptr<HcclSocketManager> socketManager;
    HcclDispatcher dispatcher0;
    std::unique_ptr<NotifyPool> notifyPool0;
    std::vector<RankInfo> rankInfoList;
    RankId userRank;
    std::string identifier;
    s32 deviceLogicId;
    NICDeployment nicDeployment;
    bool isHaveCpuRank;
    void *transportResourceInfoAddr;
    size_t transportResourceInfoSize;
    bool isUseRankPort = false;
    bool isUsedRdmaOuter = true;
    std::vector<u32> nicRanksPort;
    std::vector<u32> vnicRanksPort;
    bool useSuperPodMode;
    std::vector<HcclIpAddress> devIpAddr;
    HcclIpAddress hostIp;
    HcclIpAddress localVnicIp;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    TransportManager transManager(cclBufferManager,
        socketManager,
        dispatcher0,
        notifyPool0,
        rankInfoList,
        userRank,
        identifier,
        deviceLogicId,
        nicDeployment,
        isHaveCpuRank,
        transportResourceInfoAddr,
        transportResourceInfoSize,
        isUseRankPort,
        isUsedRdmaOuter,
        nicRanksPort,
        vnicRanksPort,
        useSuperPodMode,
        devIpAddr,
        hostIp,
        localVnicIp,
        netDevCtxMap);
    ret = transManager.LoadMultiQpSrcPortFromFile();
    EXPECT_EQ(ret, HCCL_E_PARA);

    unsetenv("HCCL_RDMA_QP_PORT_CONFIG_PATH");
    ResetInitState();
}

TEST_F(LinkIbvExpTest, ut_transport_ibv_multi_qp_configpath_patch4)
{
    HcclResult ret = HCCL_SUCCESS;
    setenv("HCCL_RDMA_QP_PORT_CONFIG_PATH", "/tmp/", 1);
    ret = InitEnvVarParam();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string filePath = GetExternalInputQpSrcPortConfigPath();
    std::string fileStr = filePath + "/MultiQpSrcPort.cfg";
    HCCL_ERROR("==TMP== fileStr [%s]", fileStr.c_str());

    const int FILE_AUTHORITY = 0600;
    int fd = open(fileStr.c_str(), O_WRONLY | O_CREAT | O_TRUNC, FILE_AUTHORITY);
    if (fd < 0) {
        HCCL_ERROR("Fail to open the file: %s.", fileStr.c_str(), HCCL_E_PARA);
    }
    if (close(fd) != 0) {
        HCCL_ERROR("Fail to close the file: %s.", fileStr.c_str(), HCCL_E_PARA);
    }
    std::ofstream fileStream(fileStr.c_str(), std::ios::out | std::ios::binary);
    if (fileStream.is_open()) {
        fileStream << "192.2.100.2" << "," << "1.0.0.0.0" << "==" << "61000" << "," << "61001" << "," << "61002" << std::endl;
        fileStream.close();
    } else {
        HCCL_ERROR("[Initialize][GraphOptimizer]file %s open failed!", fileStr.c_str());
    }

    CCLBufferManager cclBufferManager;
    std::unique_ptr<HcclSocketManager> socketManager;
    HcclDispatcher dispatcher0;
    std::unique_ptr<NotifyPool> notifyPool0;
    std::vector<RankInfo> rankInfoList;
    RankId userRank;
    std::string identifier;
    s32 deviceLogicId;
    NICDeployment nicDeployment;
    bool isHaveCpuRank;
    void *transportResourceInfoAddr;
    size_t transportResourceInfoSize;
    bool isUseRankPort = false;
    bool isUsedRdmaOuter = true;
    std::vector<u32> nicRanksPort;
    std::vector<u32> vnicRanksPort;
    bool useSuperPodMode;
    std::vector<HcclIpAddress> devIpAddr;
    HcclIpAddress hostIp;
    HcclIpAddress localVnicIp;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    TransportManager transManager(cclBufferManager,
        socketManager,
        dispatcher0,
        notifyPool0,
        rankInfoList,
        userRank,
        identifier,
        deviceLogicId,
        nicDeployment,
        isHaveCpuRank,
        transportResourceInfoAddr,
        transportResourceInfoSize,
        isUseRankPort,
        isUsedRdmaOuter,
        nicRanksPort,
        vnicRanksPort,
        useSuperPodMode,
        devIpAddr,
        hostIp,
        localVnicIp,
        netDevCtxMap);
    ret = transManager.LoadMultiQpSrcPortFromFile();
    EXPECT_EQ(ret, HCCL_E_PARA);

    unsetenv("HCCL_RDMA_QP_PORT_CONFIG_PATH");
    ResetInitState();
}

TEST_F(LinkIbvExpTest, ut_transport_ibv_multi_qp_configpath_patch5)
{
    HcclResult ret = HCCL_SUCCESS;
    setenv("HCCL_RDMA_QP_PORT_CONFIG_PATH", "/tmp/", 1);
    ret = InitEnvVarParam();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string filePath = GetExternalInputQpSrcPortConfigPath();
    std::string fileStr = filePath + "/MultiQpSrcPort.cfg";
    HCCL_ERROR("==TMP== fileStr [%s]", fileStr.c_str());

    const int FILE_AUTHORITY = 0600;
    int fd = open(fileStr.c_str(), O_WRONLY | O_CREAT | O_TRUNC, FILE_AUTHORITY);
    if (fd < 0) {
        HCCL_ERROR("Fail to open the file: %s.", fileStr.c_str(), HCCL_E_PARA);
    }
    if (close(fd) != 0) {
        HCCL_ERROR("Fail to close the file: %s.", fileStr.c_str(), HCCL_E_PARA);
    }
    std::ofstream fileStream(fileStr.c_str(), std::ios::out | std::ios::binary);
    if (fileStream.is_open()) {
        fileStream << "192.2.100.2" << "," << "1.0.0.0" << "=" << "61000" << "," << "61001" << "," << "61002" << std::endl;
        fileStream << "#192.2.100.2" << "," << "1.0.0.0.0" << "=" << "61000" << "," << "61001" << "," << "61002" << std::endl;
        fileStream << "192.2.100.2" << "," << "1.0.0.0" << "=" << "61000" << "," << "61001" << "," << "61002" << std::endl;
        fileStream.close();
    } else {
        HCCL_ERROR("[Initialize][GraphOptimizer]file %s open failed!", fileStr.c_str());
    }

    CCLBufferManager cclBufferManager;
    std::unique_ptr<HcclSocketManager> socketManager;
    HcclDispatcher dispatcher0;
    std::unique_ptr<NotifyPool> notifyPool0;
    std::vector<RankInfo> rankInfoList;
    RankId userRank;
    std::string identifier;
    s32 deviceLogicId;
    NICDeployment nicDeployment;
    bool isHaveCpuRank;
    void *transportResourceInfoAddr;
    size_t transportResourceInfoSize;
    bool isUseRankPort = false;
    bool isUsedRdmaOuter = true;
    std::vector<u32> nicRanksPort;
    std::vector<u32> vnicRanksPort;
    bool useSuperPodMode;
    std::vector<HcclIpAddress> devIpAddr;
    HcclIpAddress hostIp;
    HcclIpAddress localVnicIp;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    TransportManager transManager(cclBufferManager,
        socketManager,
        dispatcher0,
        notifyPool0,
        rankInfoList,
        userRank,
        identifier,
        deviceLogicId,
        nicDeployment,
        isHaveCpuRank,
        transportResourceInfoAddr,
        transportResourceInfoSize,
        isUseRankPort,
        isUsedRdmaOuter,
        nicRanksPort,
        vnicRanksPort,
        useSuperPodMode,
        devIpAddr,
        hostIp,
        localVnicIp,
        netDevCtxMap);
    ret = transManager.LoadMultiQpSrcPortFromFile();
    EXPECT_EQ(ret, HCCL_E_PARA);

    unsetenv("HCCL_RDMA_QP_PORT_CONFIG_PATH");
    ResetInitState();
}

TEST_F(LinkIbvExpTest, ut_transport_ibv_multi_qp_configpath_patch6)
{
    HcclResult ret = HCCL_SUCCESS;
    setenv("HCCL_RDMA_QP_PORT_CONFIG_PATH", "/tmp/", 1);
    ret = InitEnvVarParam();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string filePath = GetExternalInputQpSrcPortConfigPath();
    std::string fileStr = filePath + "/MultiQpSrcPort.cfg";
    HCCL_ERROR("==TMP== fileStr [%s]", fileStr.c_str());

    const int FILE_AUTHORITY = 0600;
    int fd = open(fileStr.c_str(), O_WRONLY | O_CREAT | O_TRUNC, FILE_AUTHORITY);
    if (fd < 0) {
        HCCL_ERROR("Fail to open the file: %s.", fileStr.c_str(), HCCL_E_PARA);
    }
    if (close(fd) != 0) {
        HCCL_ERROR("Fail to close the file: %s.", fileStr.c_str(), HCCL_E_PARA);
    }
    std::ofstream fileStream(fileStr.c_str(), std::ios::out | std::ios::binary);
    if (fileStream.is_open()) {
        fileStream << "192.2.100.2" << "," << "1.0.0.0" << "=" << "5a000" << "," << "61001" << "," << "61002" << std::endl;
        fileStream.close();
    } else {
        HCCL_ERROR("[Initialize][GraphOptimizer]file %s open failed!", fileStr.c_str());
    }

    CCLBufferManager cclBufferManager;
    std::unique_ptr<HcclSocketManager> socketManager;
    HcclDispatcher dispatcher0;
    std::unique_ptr<NotifyPool> notifyPool0;
    std::vector<RankInfo> rankInfoList;
    RankId userRank;
    std::string identifier;
    s32 deviceLogicId;
    NICDeployment nicDeployment;
    bool isHaveCpuRank;
    void *transportResourceInfoAddr;
    size_t transportResourceInfoSize;
    bool isUseRankPort = false;
    bool isUsedRdmaOuter = true;
    std::vector<u32> nicRanksPort;
    std::vector<u32> vnicRanksPort;
    bool useSuperPodMode;
    std::vector<HcclIpAddress> devIpAddr;
    HcclIpAddress hostIp;
    HcclIpAddress localVnicIp;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    TransportManager transManager(cclBufferManager,
        socketManager,
        dispatcher0,
        notifyPool0,
        rankInfoList,
        userRank,
        identifier,
        deviceLogicId,
        nicDeployment,
        isHaveCpuRank,
        transportResourceInfoAddr,
        transportResourceInfoSize,
        isUseRankPort,
        isUsedRdmaOuter,
        nicRanksPort,
        vnicRanksPort,
        useSuperPodMode,
        devIpAddr,
        hostIp,
        localVnicIp,
        netDevCtxMap);
    ret = transManager.LoadMultiQpSrcPortFromFile();
    EXPECT_EQ(ret, HCCL_E_PARA);

    unsetenv("HCCL_RDMA_QP_PORT_CONFIG_PATH");
    ResetInitState();
}

TEST_F(LinkIbvExpTest, ut_transport_ibv_multi_qp_configpath_patch7)
{
    HcclResult ret = HCCL_SUCCESS;
    setenv("HCCL_RDMA_QP_PORT_CONFIG_PATH", "0", 1);
    ret = InitEnvVarParam();
    EXPECT_EQ(ret, HCCL_E_PARA);
    std::string filePath = GetExternalInputQpSrcPortConfigPath();
    std::string fileStr = filePath + "/MultiQpSrcPort.cfg";
    HCCL_ERROR("==TMP== fileStr [%s]", fileStr.c_str());

    const int FILE_AUTHORITY = 0600;
    int fd = open(fileStr.c_str(), O_WRONLY | O_CREAT | O_TRUNC, FILE_AUTHORITY);
    if (fd < 0) {
        HCCL_ERROR("Fail to open the file: %s.", fileStr.c_str(), HCCL_E_PARA);
    }
    if (close(fd) != 0) {
        HCCL_ERROR("Fail to close the file: %s.", fileStr.c_str(), HCCL_E_PARA);
    }
    std::ofstream fileStream(fileStr.c_str(), std::ios::out | std::ios::binary);
    if (fileStream.is_open()) {
        fileStream << "192.2.100.2" << "," << "1.0.0.0" << "=" << "5a000" << "," << "61001" << "," << "61002" << std::endl;
        fileStream.close();
    } else {
        HCCL_ERROR("[Initialize][GraphOptimizer]file %s open failed!", fileStr.c_str());
    }

    CCLBufferManager cclBufferManager;
    std::unique_ptr<HcclSocketManager> socketManager;
    HcclDispatcher dispatcher0;
    std::unique_ptr<NotifyPool> notifyPool0;
    std::vector<RankInfo> rankInfoList;
    RankId userRank;
    std::string identifier;
    s32 deviceLogicId;
    NICDeployment nicDeployment;
    bool isHaveCpuRank;
    void *transportResourceInfoAddr;
    size_t transportResourceInfoSize;
    bool isUseRankPort = false;
    bool isUsedRdmaOuter = true;
    std::vector<u32> nicRanksPort;
    std::vector<u32> vnicRanksPort;
    bool useSuperPodMode;
    std::vector<HcclIpAddress> devIpAddr;
    HcclIpAddress hostIp;
    HcclIpAddress localVnicIp;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    TransportManager transManager(cclBufferManager,
        socketManager,
        dispatcher0,
        notifyPool0,
        rankInfoList,
        userRank,
        identifier,
        deviceLogicId,
        nicDeployment,
        isHaveCpuRank,
        transportResourceInfoAddr,
        transportResourceInfoSize,
        isUseRankPort,
        isUsedRdmaOuter,
        nicRanksPort,
        vnicRanksPort,
        useSuperPodMode,
        devIpAddr,
        hostIp,
        localVnicIp,
        netDevCtxMap);
    ret = transManager.LoadMultiQpSrcPortFromFile();
    EXPECT_EQ(ret, HCCL_E_PARA);

    unsetenv("HCCL_RDMA_QP_PORT_CONFIG_PATH");
    ResetInitState();
}

TEST_F(LinkIbvExpTest, ut_transport_ipc_memory_error)
{
    s32 ret = 0;
    CCLBufferManager cclBufferManager;
    std::unique_ptr<HcclSocketManager> socketManager(new (std::nothrow) HcclSocketManager(NICDeployment::NIC_DEPLOYMENT_DEVICE, 0, 0, 0));
    HcclDispatcher dispatcher0;
    std::unique_ptr<NotifyPool> notifyPool0;

    RankInfo tmp_para;
    tmp_para.userRank = 0;
    tmp_para.devicePhyId = 0;
    tmp_para.serverIdx = 0;
    tmp_para.serverId = "10.21.78.208";
    tmp_para.nicIp.push_back(HcclIpAddress("10.21.78.208"));
    std::vector<RankInfo> rankInfoList;
    rankInfoList.push_back(tmp_para);
    RankId userRank = 0;
    std::string identifier;
    s32 deviceLogicId;
    NICDeployment nicDeployment;
    bool isHaveCpuRank;
    void *transportResourceInfoAddr;
    size_t transportResourceInfoSize;
    bool isUseRankPort = false;
    bool isUsedRdmaOuter = true;
    std::vector<u32> nicRanksPort;
    std::vector<u32> vnicRanksPort;
    bool useSuperPodMode;
    std::vector<HcclIpAddress> devIpAddr;
    HcclIpAddress hostIp;
    HcclIpAddress localVnicIp;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    TransportManager transManager(cclBufferManager,
        socketManager,
        dispatcher0,
        notifyPool0,
        rankInfoList,
        userRank,
        identifier,
        deviceLogicId,
        nicDeployment,
        isHaveCpuRank,
        transportResourceInfoAddr,
        transportResourceInfoSize,
        isUseRankPort,
        isUsedRdmaOuter,
        nicRanksPort,
        vnicRanksPort,
        useSuperPodMode,
        devIpAddr,
        hostIp,
        localVnicIp,
        netDevCtxMap);
    
    MOCKER_CPP(&TransportManager::TransportInit)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_MEMORY));
    HcclIpAddress remoteIp{"1.1.1.1"};
    ErrContextPub error_context;
    error_context.work_stream_id = 1234567890;
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("test", 
        nullptr, remoteIp, 1, HcclSocketRole::SOCKET_ROLE_SERVER));
    std::vector<std::shared_ptr<HcclSocket> > sockets;
    sockets.push_back(newSocket);
    std::shared_ptr<Transport> link = nullptr;
    transManager.SetStopFlag(true);
    ret = transManager.CreateLink("tag", error_context, MachineType::MACHINE_SERVER_TYPE, "10.21.78.208", 0, true,
        LinkMode::LINK_SIMPLEX_MODE, true, "createLink", sockets, DeviceMem::alloc(1024), DeviceMem::alloc(1024), 
        false, link, false, 1, false);
    EXPECT_EQ(ret, HCCL_E_TCP_CONNECT);
    GlobalMockObject::verify();
}
struct StubQpInfo {
    u32 qpn = 0;
};
HcclResult stub_hrtRaQpCreate(RdmaHandle rdmaHandle, int flag, int qpMode, QpHandle &qpHandle)
{
    static u32 qpn = 0;
    StubQpInfo *info = new StubQpInfo();
    info->qpn = qpn++;
    qpHandle = (void*)info;
    return HCCL_SUCCESS;
}

HcclResult stub_hrtRaAiQpCreate(u32 phy_id, RdmaHandle rdmaHandle, struct qp_ext_attrs *attrs,
    struct ai_qp_info *info1, QpHandle &qpHandle)
{
    static u32 qpn = 0;
    StubQpInfo *info = new StubQpInfo();
    info->qpn = qpn++;
    qpHandle = (void*)info;
    return HCCL_SUCCESS;
}

HcclResult stub_hrtRaQpCreateWithAttrs(RdmaHandle rdmaHandle, struct qp_ext_attrs *attrs, QpHandle &qpHandle)
{
    static u32 qpn = 0;
    StubQpInfo *info = new StubQpInfo();
    info->qpn = qpn++;
    qpHandle = (void*)info;
    return HCCL_SUCCESS;
}

HcclResult stub_hrtRaQpDestroy(QpHandle handle)
{
    delete (StubQpInfo*)handle;
    return HCCL_SUCCESS;
}

HcclResult stub_hrtRaGetQpAttr(QpHandle qpHandle, struct qp_attr *attr)
{
    static u32 qpn = 0;
    attr->qpn = ((StubQpInfo*)qpHandle)->qpn;
    return HCCL_SUCCESS;
}

HcclResult stub_CreateNotifyBuffer(TransportIbverbs *, std::shared_ptr<LocalIpcNotify> &localNotify, MemType notifyType,
    u8*& exchangeDataPtr, u64& exchangeDataBlankSize, NotifyLoadType notifyLoadType)
{
    exchangeDataPtr += sizeof(MemMsg);
    exchangeDataBlankSize -= sizeof(MemMsg);
    return HCCL_SUCCESS;
}

HcclResult stub_hrtRaGetInterfaceVersionX(unsigned int phyId, unsigned int interfaceOpcode,
                                         unsigned int* interfaceVersion)
{
    *interfaceVersion = 1;
    return HCCL_SUCCESS;
}

const u32 CQE_NUM = 130;
u32 g_index = 0;
struct cqe_err_info g_infolist[CQE_NUM] = {0};
const u32 CQE_ARRAY_SIZE = 128;
HcclResult stub_hrtRaGetCqeErrInfoList(RdmaHandle rdmaHandle, struct cqe_err_info *infolist, u32 *num)
{
    u32 cqeNum = 0;
    for (int idx = 0; idx < CQE_ARRAY_SIZE && idx < *num && g_index <= CQE_NUM; idx++){
        infolist[idx].qpn = g_infolist[g_index].qpn;
        infolist[idx].status = g_infolist[g_index].status;
        g_index++;
        cqeNum++;
    }
    *num = *num <= CQE_ARRAY_SIZE ? cqeNum : CQE_ARRAY_SIZE;
    return HCCL_SUCCESS;
}

TEST_F(LinkIbvExpTest, ut_error_cqe_test)
{
    s32 ret ;
    SetTcpMode(false);

    std::string port_name = "mlx5_0";

    s32 device_id = 0;
    DevType chipType = DevType::DEV_TYPE_910;
    u32 ifnumVersion = 3;
    MOCKER(hrtRaGetInterfaceVersion)
    .stubs()
    .with(any(), any(), outBoundP(&ifnumVersion))
    .will(returnValue(0));

    /*创建link*/
    MachinePara machine_para;

    machine_para.localDeviceId = 0;
    machine_para.deviceLogicId = 0;
    machine_para.nicDeploy == NICDeployment::NIC_DEPLOYMENT_DEVICE;
    machine_para.localIpAddr = HcclIpAddress("192.168.0.23");
    machine_para.inputMem = DeviceMem::alloc(1);
    machine_para.outputMem = DeviceMem::alloc(1);

    HcclIpAddress remoteIp("192.168.0.24");
    HcclIpAddress localIp("192.168.0.23");
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("test", 
        nullptr, remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machine_para.sockets.push_back(newSocket);
    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const std::string &))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(std::string &))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HrtRaQpCreate)
    .stubs()
    .will(invoke(stub_hrtRaQpCreate));

    MOCKER(hrtRaAiQpCreate)
    .stubs()
    .will(invoke(stub_hrtRaAiQpCreate));

    MOCKER(hrtRaQpCreateWithAttrs)
    .stubs()
    .will(invoke(stub_hrtRaQpCreateWithAttrs));

    MOCKER(hrtRaSetQpAttrQos)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSetQpAttrTimeOut)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSetQpAttrRetryCnt)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaGetQpAttr)
    .stubs()
    .will(invoke(stub_hrtRaGetQpAttr));

    MOCKER(HrtRaQpDestroy)
    .stubs()
    .will(invoke(stub_hrtRaQpDestroy));

    MOCKER(hrtRaSocketNonBlockListenStart)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportIbverbs::CreateNotifyBuffer)
    .stubs()
    .will(invoke(stub_CreateNotifyBuffer));

    std::chrono::milliseconds timeout = std::chrono::milliseconds(0);
    TransportIbverbs ibv(nullptr, nullptr, machine_para, timeout);
    MOCKER_CPP_VIRTUAL(ibv, &TransportIbverbs::ParseReceivedExchangeData)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportIbverbs::ConnectSingleQp)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    std::shared_ptr<Transport> link = nullptr;
    std::shared_ptr<Transport> link1 = nullptr;
    std::shared_ptr<Transport> link2 = nullptr;

    HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, 0, 0, false);
    HcclNetDevCtx nicPortCtx;
    HcclNetOpenDev(&nicPortCtx, NicType::DEVICE_NIC_TYPE, 0, 0, localIp);

    std::shared_ptr<HcclSocketManager> socketManager = nullptr;
    socketManager.reset(new (std::nothrow) HcclSocketManager(NICDeployment::NIC_DEPLOYMENT_DEVICE, 0, 0, 0));
    u32 port  = 16666;
    ret = socketManager->ServerInit(nicPortCtx, port);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    const std::string tag = "tag";
    TransportPara para = {};
    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    link.reset(new Transport(TransportType::TRANS_TYPE_IBV_EXP, para, dispatcher, notifyPool, machine_para));
    link1.reset(new Transport(TransportType::TRANS_TYPE_IBV_EXP, para, dispatcher, notifyPool, machine_para));
    link2.reset(new Transport(TransportType::TRANS_TYPE_IBV_EXP, para, dispatcher, notifyPool, machine_para));

    ret = link->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = link1->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = link2->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    for (u32 i = 0; i < CQE_NUM; i++) {
        if (i % 2 == 0) {
            g_infolist[i].qpn = ((StubQpInfo*)((reinterpret_cast<TransportIbverbs*>(link->pimpl_))->combineQpHandles_[0].qpHandle))->qpn;
            g_infolist[i].status = 12;
        } else {
            g_infolist[i].qpn = ((StubQpInfo*)((reinterpret_cast<TransportIbverbs*>(link2->pimpl_))->combineQpHandles_[0].qpHandle))->qpn;
            g_infolist[i].status = 12;
        }
    }

    MOCKER(hrtRaGetCqeErrInfoList)
    .stubs()
    .will(invoke(stub_hrtRaGetCqeErrInfoList));
    g_index = 0;

    std::vector<std::pair<Transport*, CqeInfo>> infos;
    u32 cqeNum = 1;
    ret = Transport::GetTransportErrorCqe(nicPortCtx, infos, cqeNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(infos.size(), 1);
    EXPECT_EQ(link.get(), infos[0].first);
    EXPECT_EQ(12, infos[0].second.status);

    infos.clear();
    g_index = 0;
    cqeNum = CQE_NUM;
    ret = Transport::GetTransportErrorCqe(nicPortCtx, infos, cqeNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(infos.size(), CQE_NUM);

    for (u32 i = 0; i < CQE_NUM; i++) {
        if (i % 2 == 0) {
            EXPECT_EQ(link.get(), infos[i].first);
            EXPECT_EQ(12, infos[i].second.status);
        } else {
            EXPECT_EQ(link2.get(), infos[i].first);
            EXPECT_EQ(12, infos[i].second.status);
        }
    }

    MOCKER(hrtRaGetCqeErrInfo)
    .stubs()
    .with(any(), outBoundP(&g_infolist[0], sizeof(struct cqe_err_info)), any())
    .will(returnValue(HCCL_SUCCESS));

    TransportIbverbs::g_isSupCqeErrInfoListConfig = false;
    infos.clear();
    g_index = 0;
    cqeNum = 1;
    ret = Transport::GetTransportErrorCqe(nicPortCtx, infos, cqeNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    infos.clear();
    g_index = 0;
    cqeNum = 2;
    ret = Transport::GetTransportErrorCqe(nicPortCtx, infos, cqeNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    socketManager->ServerDeInit(nicPortCtx, 0);
    HcclNetCloseDev(nicPortCtx);
    HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, 0, 0);
    link = nullptr;
    link1 = nullptr;
    link2 = nullptr;
    GlobalMockObject::verify();
}

TEST_F(LinkIbvExpTest, ut_aiv_get_RMA_queue)
{
    MOCKER_CPP(&TransportIbverbs::CreateOneQp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .will(invoke(stub_hrtRaGetInterfaceVersionX));

    HcclResult ret = HCCL_SUCCESS;
    MachinePara machinePara;
    machinePara.localDeviceId = 0;
    machinePara.qpMode = QPMode::NORMAL;
    machinePara.sl = HCCL_RDMA_SL_DEFAULT;

    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(0);

    std::chrono::milliseconds timeout = std::chrono::milliseconds(0);
    TransportIbverbs ibv(dispatcher, notifyPool, machinePara, timeout);
    ret = ibv.CreateMultiQp(0, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::vector<HcclAiRMAQueueInfo> aiRMAQueueInfo;
    ret = ibv.GetAiRMAQueueInfo(aiRMAQueueInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    std::shared_ptr<Transport> link_base(new Transport(new (std::nothrow) TransportBase(
        dispatcher, notifyPool, machinePara, timeout)));
    ret = link_base->GetAiRMAQueueInfo(aiRMAQueueInfo);
    EXPECT_EQ(ret, HCCL_E_PARA);

    GlobalMockObject::verify();
}
