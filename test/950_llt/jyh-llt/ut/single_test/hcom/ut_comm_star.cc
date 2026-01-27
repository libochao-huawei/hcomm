#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
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
#include "profiler_manager.h"
#include "dlra_function.h"
#include "dltdt_function.h"

using namespace std;
using namespace hccl;


class CommStarTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--CommStarTest SetUP--\033[0m" << std::endl;
        DlRaFunction::GetInstance().DlRaFunctionInit();
        DlTdtFunction::GetInstance().DlTdtFunctionInit();
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--CommStarTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
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

const HcclIpAddress hostIp("10.21.78.208");
HcclIpAddress devIp("10.21.78.207");

void comm_star_task_handle(vector<innerpara_t_star> &paraInfo, std::map<HcclIpAddress, HcclNetDevCtx> &netDevCtxMap)
{
    std::string rootInfo = "test_collective";
    std::vector<RankInfo> para_vector;
    HcclNetDevCtx nicPortCtx[2];
    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.21.78.208";
    tmp_para_0.nicIp.push_back(HcclIpAddress("10.21.78.207"));
    tmp_para_0.hostIp = HcclIpAddress("10.21.78.208");

    HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, 0, 0, false);
    HcclNetOpenDev(&nicPortCtx[0], NicType::DEVICE_NIC_TYPE, 0, 0, tmp_para_0.nicIp[0]);
    netDevCtxMap.insert(std::make_pair(tmp_para_0.nicIp[0], nicPortCtx[0]));

    RankInfo tmp_para_1;

    tmp_para_1.userRank = 1;
    tmp_para_1.devicePhyId = -1;
    tmp_para_1.deviceType = DevType::DEV_TYPE_910;
    tmp_para_1.serverIdx = 0;
    tmp_para_1.serverId = "10.21.78.208";
    tmp_para_1.nicIp.push_back(HcclIpAddress("10.21.78.204"));
    tmp_para_1.hostIp = HcclIpAddress("10.21.78.208");

    HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, 1, 1, false);
    HcclNetOpenDev(&nicPortCtx[1], NicType::DEVICE_NIC_TYPE, 0, 0, tmp_para_1.nicIp[0]);
    netDevCtxMap.insert(std::make_pair(tmp_para_1.nicIp[0], nicPortCtx[1]));

    para_vector.push_back(tmp_para_0);
    para_vector.push_back(tmp_para_1);

    s32 devNum = 2;
    paraInfo.resize(devNum);
    s32 devList[devNum] = {0, -1};
    for (s32 i = 0; i < devNum; i++) {
        paraInfo[i].collectiveId = rootInfo;
        paraInfo[i].userRank = i;
        paraInfo[i].user_rank_size = devNum;
        paraInfo[i].rank = i;
        paraInfo[i].rank_size = devNum;
        paraInfo[i].devicePhyId = devList[i];     
        paraInfo[i].dispatcher = nullptr;
        paraInfo[i].tag = "ut_commstar_init";
        paraInfo[i].para_vector = para_vector;
        paraInfo[i].inputMem = DeviceMem::alloc(1024);
        paraInfo[i].outputMem = DeviceMem::alloc(1024);        
    }

    s32 ret = HCCL_SUCCESS;
    for (s32 i = 0; i < paraInfo.size(); i++) {
        innerpara_t_star* para_info = &(paraInfo[i]);
        IntraExchanger exchanger{};

        TopoType topoFlag = TopoType::TOPO_TYPE_ES_MESH;
        para_info->comm_star.reset(new CommStar(para_info->collectiveId,
                                    para_info->userRank,
                                    para_info->user_rank_size,
                                    para_info->rank,
                                    para_info->rank_size,
                                    topoFlag,
                                    para_info->dispatcher,
                                    nullptr,
                                    netDevCtxMap,
                                    exchanger,
                                    para_info->para_vector,
                                    DeviceMem(),
                                    DeviceMem(),
                                    false,
                                    nullptr, 0,
                                    para_info->tag,
                                    NICDeployment::NIC_DEPLOYMENT_DEVICE, 0, true));
        u32 devId = 0;
        para_info->comm_star->rankDevicePhyIdNicInfoMap_[hostIp.GetReadableAddress()][devId] = devIp;
        para_info->comm_star->ranksPort_.push_back(60008);
        para_info->comm_star->ranksPort_.push_back(60009);
    }

    return;
}

TEST_F(CommStarTest, ut_MakeClientInfo_test)
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

    vector<innerpara_t_star> paraInfo;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    comm_star_task_handle(paraInfo, netDevCtxMap);
    for (s32 i = 0; i < paraInfo.size(); i++) {
        paraInfo[i].comm_star->isHostUseDevNic_ = true;
        paraInfo[i].comm_star->isSetHDCModeInfo_ = true;
        paraInfo[i].comm_star->rankDevicePhyIdNicInfoMap_["10.21.78.208"][0] = HcclIpAddress("10.21.78.207");
        ret = paraInfo[i].comm_star->MakeClientInfo(i, dstRankInfos[i], isInterRdma, isInterHccs);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        paraInfo[i].comm_star->dstInterClientMap_.clear();
    }
    for (auto& iter : netDevCtxMap) {
        HcclNetCloseDev(iter.second);
    }
    for (s32 i = 0; i < paraInfo.size(); i++) {
        HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE,
            paraInfo[i].para_vector[i].devicePhyId,
            paraInfo[i].para_vector[i].devicePhyId);
    }
    paraInfo.clear();
}

TEST_F(CommStarTest, ut_MakeServerInfo_test)
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

    vector<innerpara_t_star> paraInfo;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    comm_star_task_handle(paraInfo, netDevCtxMap);
    for (s32 i = 0; i < paraInfo.size(); i++) {
        paraInfo[i].comm_star->isHostUseDevNic_ = true;
        paraInfo[i].comm_star->isSetHDCModeInfo_ = true;
        paraInfo[i].comm_star->rankDevicePhyIdNicInfoMap_["10.21.78.208"][0] = HcclIpAddress("10.21.78.207");
        ret = paraInfo[i].comm_star->MakeServerInfo(i, dstRankInfos[i], isInterRdma, isInterHccs);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        paraInfo[i].comm_star->dstInterServerMap_.clear();
    }
    for (auto& iter : netDevCtxMap) {
        HcclNetCloseDev(iter.second);
    }
    for (s32 i = 0; i < paraInfo.size(); i++) {
        HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE,
            paraInfo[i].para_vector[i].devicePhyId,
            paraInfo[i].para_vector[i].devicePhyId);
    }
    paraInfo.clear();
}

TEST_F(CommStarTest, ut_CreateInterLinks_test)
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

    vector<innerpara_t_star> paraInfo;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    comm_star_task_handle(paraInfo, netDevCtxMap);

    HcclRankLinkInfo rankLinks;
    for (s32 i = 0; i < paraInfo.size(); i++) {
        paraInfo[i].comm_star->netDevCtxMap_ = netDevCtxMap;
        paraInfo[i].comm_star->dstInterServerMap_[0] = rankLinks;
        paraInfo[i].comm_star->dstInterClientMap_[0] = rankLinks;
        ret = paraInfo[i].comm_star->CreateInterLinks();
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    for (auto& iter : netDevCtxMap) {
        HcclNetCloseDev(iter.second);
    }
    for (s32 i = 0; i < paraInfo.size(); i++) {
        HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE,
            paraInfo[i].para_vector[i].devicePhyId,
            paraInfo[i].para_vector[i].devicePhyId);
    }
    paraInfo.clear();
    GlobalMockObject::verify();
}

TEST_F(CommStarTest, ut_GetDevIP_test)
{
    s32 ret = HCCL_SUCCESS;

    HcclIpAddress hostIp("10.21.78.208");
    u32 devicePhyId = 0;
    HcclIpAddress ip;

    vector<innerpara_t_star> paraInfo;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    comm_star_task_handle(paraInfo, netDevCtxMap);
    for (s32 i = 0; i < paraInfo.size(); i++) {
        paraInfo[i].comm_star->rankDevicePhyIdNicInfoMap_["10.21.78.208"][0] = HcclIpAddress("10.21.78.207");
        ret = paraInfo[i].comm_star->GetDevIP(hostIp, devicePhyId, ip);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
    for (auto& iter : netDevCtxMap) {
        HcclNetCloseDev(iter.second);
    }
    for (s32 i = 0; i < paraInfo.size(); i++) {
        HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE,
            paraInfo[i].para_vector[i].devicePhyId,
            paraInfo[i].para_vector[i].devicePhyId);
    }
    paraInfo.clear();
}

TEST_F(CommStarTest, ut_SetMachinePara_test)
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

    vector<innerpara_t_star> paraInfo;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    comm_star_task_handle(paraInfo, netDevCtxMap);
    for (s32 i = 0; i < paraInfo.size(); i++) {
        u32 dstRank = i;
        ret = paraInfo[i].comm_star->SetMachinePara(machineType, serverId, (i + 1) % paraInfo.size(), sockets, machinePara);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
    for (auto& iter : netDevCtxMap) {
        HcclNetCloseDev(iter.second);
    }
    for (s32 i = 0; i < paraInfo.size(); i++) {
        HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE,
            paraInfo[i].para_vector[i].devicePhyId,
            paraInfo[i].para_vector[i].devicePhyId);
    }
    paraInfo.clear();
    GlobalMockObject::verify();
}


TEST_F(CommStarTest, ut_SetTransportParam_test)
{
    s32 ret = HCCL_SUCCESS;

    HcclIpAddress localIp("10.21.78.207");
    HcclIpAddress remoteIp("10.21.78.208");
    TransportPara para;
    MachinePara machinePara;
    machinePara.localIpAddr = localIp;
    machinePara.remoteIpAddr = remoteIp;

    vector<innerpara_t_star> paraInfo;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    comm_star_task_handle(paraInfo, netDevCtxMap);
    for (s32 i = 0; i < paraInfo.size(); i++) {
        paraInfo[i].comm_star->SetTransportParam(para, machinePara);
    }
    for (auto& iter : netDevCtxMap) {
        HcclNetCloseDev(iter.second);
    }
    for (s32 i = 0; i < paraInfo.size(); i++) {
        HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE,
            paraInfo[i].para_vector[i].devicePhyId,
            paraInfo[i].para_vector[i].devicePhyId);
    }
    paraInfo.clear();
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