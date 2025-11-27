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
#include <stdio.h>

#include "hccl/base.h"
#include <hccl/hccl_types.h>
 
#define private public
#define protected public
#include "comm_base_pub.h"
#include "transport_roce.h"
#include "comm_star_pub.h"
#include "hccl_socket_manager.h"
#include "network_manager_pub.h"
#undef protected
#undef private
#include "sal.h"
#include <vector>
#include "transport_pub.h"
#include "comm_factory_pub.h"
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub.h"
#include "dlra_function.h"
#include "dltdt_function.h"
#include "profiler_manager.h"
#include "network_manager_pub.h"
 
using namespace std;
using namespace hccl;
 
 

typedef struct innerpara_struct_star
{
    std::string collectiveId;
    u32 userRank;
    u32 user_rank_size;
    u32 rank;
    u32 rank_size;
    u32 devicePhyId;
    std::vector<s32> device_ids;
    std::vector<u32> device_ips;
    std::vector<u32> user_ranks;
    std::string tag;
    HcclDispatcher dispatcher;
    std::vector<RankInfo> para_vector;
    std::shared_ptr<CommStar> comm_star;
    DeviceMem inputMem = DeviceMem();
    DeviceMem outputMem = DeviceMem();
} innerpara_t_star;
class CommStarTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        DlRaFunction::GetInstance().DlRaFunctionInit();
        DlTdtFunction::GetInstance().DlTdtFunctionInit();
        std::cout << "\033[36m--CommStarTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        for (auto& iter : g_netDevCtxMap) {
            HcclNetCloseDev(iter.second);
        }
        for (s32 i = 0; i < g_paraInfo.size(); i++) {
            HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE,
                g_paraInfo[i].para_vector[i].devicePhyId,
                g_paraInfo[i].para_vector[i].devicePhyId);
            g_paraInfo[i].comm_star = nullptr;
        }
        g_paraInfo.clear();
        std::cout << "\033[36m--CommStarTest TearDown--\033[0m" << std::endl;
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
        std::cout << "A Test TearDown" << std::endl;
    }
    static vector<innerpara_t_star> g_paraInfo;
    static std::map<HcclIpAddress, HcclNetDevCtx> g_netDevCtxMap;
};
vector<innerpara_t_star> CommStarTest::g_paraInfo;
std::map<HcclIpAddress, HcclNetDevCtx> CommStarTest::g_netDevCtxMap;
 
const HcclIpAddress hostIp("10.21.78.208");
HcclIpAddress devIp("10.21.78.207");
 
void *get_star_dispatcher(s32 devid, std::shared_ptr<hccl::ProfilerManager> &profilerManager)
{
    HcclResult ret = HCCL_SUCCESS;
    ret = hrtSetDevice(devid);
    EXPECT_EQ(ret, HCCL_SUCCESS);
     // 创建dispatcher
    DevType chipType = DevType::DEV_TYPE_910;
    void * dispatcher;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, devid, &dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcher, nullptr);

    return dispatcher;
}
 
void comm_star_task_handle()
{
    s32 ret = HCCL_SUCCESS;
 
    for (s32 i = 0; i < CommStarTest::g_paraInfo.size(); i++) {
        innerpara_t_star* para_info = &(CommStarTest::g_paraInfo[i]);
        hrtSetDevice(para_info->devicePhyId);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HcclNetDevCtx nicPortCtx;
        HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, para_info->devicePhyId, para_info->devicePhyId, false);
        HcclNetOpenDev(&nicPortCtx, NicType::DEVICE_NIC_TYPE, para_info->devicePhyId, para_info->devicePhyId,
            para_info->para_vector[i].nicIp[0]);
        CommStarTest::g_netDevCtxMap.insert(std::make_pair(para_info->para_vector[i].nicIp[0], nicPortCtx));
        IntraExchanger exchanger{};
        std::unique_ptr<NotifyPool> notifyPool(new NotifyPool());
        EXPECT_NE(notifyPool, nullptr);
 
        TopoType topoFlag = TopoType::TOPO_TYPE_ES_MESH;
        para_info->comm_star.reset(new CommStar(para_info->collectiveId,
                                    para_info->userRank,
                                    para_info->user_rank_size,
                                    para_info->rank,
                                    para_info->rank_size,
                                    topoFlag,
                                    para_info->dispatcher,
                                    notifyPool,
                                    CommStarTest::g_netDevCtxMap,
                                    exchanger,
                                    para_info->para_vector,
                                    DeviceMem(),
                                    DeviceMem(),
                                    false,
                                    nullptr, 0,
                                    para_info->tag,
                                    NICDeployment::NIC_DEPLOYMENT_DEVICE, 0, true));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        u32 devId = 0;
        para_info->comm_star->rankDevicePhyIdNicInfoMap_[hostIp.GetReadableAddress()][devId] = devIp;
        para_info->comm_star->ranksPort_.push_back(60008);
        para_info->comm_star->ranksPort_.push_back(60009);
    }
 
    return;
} 
 
TEST_F(CommStarTest, ut_star_init_testdestructor_D0)
{
    s32 ret = 0;
    std::string rootInfo = "test_collective";
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);
    std::vector<RankInfo> para_vector;
 
    RankInfo tmp_para_0;
 
    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.21.78.208";
    tmp_para_0.nicIp.push_back(HcclIpAddress("10.21.78.207"));
    tmp_para_0.hostIp = HcclIpAddress("10.21.78.208");
 
    RankInfo tmp_para_1;
 
    tmp_para_1.userRank = 1;
    tmp_para_1.devicePhyId = -1;
    tmp_para_1.deviceType = DevType::DEV_TYPE_910;
    tmp_para_1.serverIdx = 0;
    tmp_para_1.serverId = "10.21.78.208";
    tmp_para_1.nicIp.push_back(HcclIpAddress("10.21.78.204"));
    tmp_para_1.hostIp = HcclIpAddress("10.21.78.208");
 
    para_vector.push_back(tmp_para_0);
    para_vector.push_back(tmp_para_1);

    s32 devNum = 2;
    g_paraInfo.resize(devNum);
    s32 devList[devNum] = {0, -1};
    for (s32 i = 0; i < devNum; i++) {
        g_paraInfo[i].collectiveId = rootInfo;
        g_paraInfo[i].userRank = i;
        g_paraInfo[i].user_rank_size = devNum;
        g_paraInfo[i].rank = i;
        g_paraInfo[i].rank_size = devNum;
        g_paraInfo[i].devicePhyId = devList[i];     
        g_paraInfo[i].dispatcher = dispatcher;
        g_paraInfo[i].tag = "ut_commstar_init";
        g_paraInfo[i].para_vector = para_vector;
        g_paraInfo[i].inputMem = DeviceMem::alloc(1024);
        g_paraInfo[i].outputMem = DeviceMem::alloc(1024);        
    }
 
    comm_star_task_handle();

    ret = HcclDispatcherDestroy(dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
 
TEST_F(CommStarTest, ut_star_MakeClientInfo_test)
{
    s32 ret = HCCL_SUCCESS;
 
    bool isInterRdma = true;
    bool isInterHccs = false;
 
    RankInfo dstRankInfo1;
    dstRankInfo1.userRank = 1;
    dstRankInfo1.nicIp.push_back(HcclIpAddress("10.21.78.208"));
    dstRankInfo1.devicePhyId = HOST_DEVICE_ID;
 
    RankInfo dstRankInfo2;
    dstRankInfo2.userRank = 0;
    dstRankInfo2.nicIp.push_back(HcclIpAddress("10.21.78.208"));
    dstRankInfo2.devicePhyId = 0;
 
    std::vector<RankInfo> dstRankInfos;
    dstRankInfos.push_back(dstRankInfo1);
    dstRankInfos.push_back(dstRankInfo2);
 
    for (s32 i = 0; i < g_paraInfo.size(); i++) {
        g_paraInfo[i].comm_star->isHostUseDevNic_ = true;
        g_paraInfo[i].comm_star->isSetHDCModeInfo_ = true;
        g_paraInfo[i].comm_star->rankDevicePhyIdNicInfoMap_["10.21.78.208"][0] = HcclIpAddress("10.21.78.207");
        ret = g_paraInfo[i].comm_star->MakeClientInfo(i, dstRankInfos[i], isInterRdma, isInterHccs);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        g_paraInfo[i].comm_star->dstInterClientMap_.clear();
    }
 
// MakeServerInfo test
    for (s32 i = 0; i < g_paraInfo.size(); i++) {
        g_paraInfo[i].comm_star->isHostUseDevNic_ = true;
        g_paraInfo[i].comm_star->isSetHDCModeInfo_ = true;
        g_paraInfo[i].comm_star->rankDevicePhyIdNicInfoMap_["10.21.78.208"][0] = HcclIpAddress("10.21.78.207");
        ret = g_paraInfo[i].comm_star->MakeServerInfo(i, dstRankInfos[i], isInterRdma, isInterHccs);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        g_paraInfo[i].comm_star->dstInterServerMap_.clear();
    }
}
 
TEST_F(CommStarTest, ut_star_CreateInterLinks_test)
{
    s32 ret = HCCL_SUCCESS;
 
    MOCKER_CPP(&CommBase::CreateInterThread)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&HcclSocketManager::CreateSockets)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    u32 deviceLogicId = 0;
    MOCKER(hrtGetDeviceIndexByPhyId)
    .stubs()
    .with(any(), outBound(deviceLogicId))
    .will(returnValue(HCCL_SUCCESS));

    HcclNetDevCtx nicPortCtx[2];
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;

    HcclRankLinkInfo rankLinks;
    for (s32 i = 0; i < g_paraInfo.size(); i++) {
        g_paraInfo[i].comm_star->dstInterServerMap_[0] = rankLinks;
        g_paraInfo[i].comm_star->dstInterClientMap_[0] = rankLinks;
        ret = g_paraInfo[i].comm_star->CreateInterLinks();
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
    GlobalMockObject::verify();
 
    HcclIpAddress hostIp("10.21.78.208");
    u32 devicePhyId = 0;
    HcclIpAddress ip;
 
    for (s32 i = 0; i < g_paraInfo.size(); i++) {
        g_paraInfo[i].comm_star->rankDevicePhyIdNicInfoMap_["10.21.78.208"][0] = HcclIpAddress("10.21.78.207");
        ret = g_paraInfo[i].comm_star->GetDevIP(hostIp, devicePhyId, ip);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
}
 
TEST_F(CommStarTest, ut_star_SetMachinePara_test)
{
    s32 ret = HCCL_SUCCESS;
 
    MOCKER_CPP(&CommStar::GetDevIP)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDeviceIndexByPhyId)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MachineType machineType = MachineType::MACHINE_SERVER_TYPE;
    string serverId = "10.21.78.208";
 
    std::shared_ptr<HcclSocket> hcclSocket;
    std::vector<std::shared_ptr<HcclSocket> > sockets;
    sockets.push_back(hcclSocket);
    MachinePara machinePara;
 
    for (s32 i = 0; i < g_paraInfo.size(); i++) {
        u32 dstRank = i;
        ret = g_paraInfo[i].comm_star->SetMachinePara(machineType, serverId, (i + 1) % g_paraInfo.size(), sockets, machinePara);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
    GlobalMockObject::verify();

    HcclIpAddress localIp("10.21.78.207");
    HcclIpAddress remoteIp("10.21.78.208");
    TransportPara para;
    machinePara.localIpAddr = localIp;
    machinePara.remoteIpAddr = remoteIp;

    for (s32 i = 0; i < g_paraInfo.size(); i++) {
        g_paraInfo[i].comm_star->SetTransportParam(para, machinePara);
    }
    GlobalMockObject::verify();  
}

TEST_F(CommStarTest, ut_transport_stop_test)
{
    s32 ret = HCCL_SUCCESS;
    DispatcherPub *dispatcher;
    const std::unique_ptr<NotifyPool> notifyPool;
    std::chrono::milliseconds timeout;
    MachinePara machinePara;
    TransportBase *pimpl_ = new (std::nothrow) TransportBase(dispatcher, notifyPool, machinePara, timeout);
    hccl::Transport transport(pimpl_);
    ret = transport.Stop();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
 
TEST_F(CommStarTest, ut_transport_Resume_test)
{
    s32 ret = HCCL_SUCCESS;
    DispatcherPub *dispatcher;
    const std::unique_ptr<NotifyPool> notifyPool;
    std::chrono::milliseconds timeout;
    MachinePara machinePara;
    TransportBase *pimpl_ = new (std::nothrow) TransportBase(dispatcher, notifyPool, machinePara, timeout);
    hccl::Transport transport(pimpl_);
    ret = transport.Resume();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}