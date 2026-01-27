
#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <cstdio>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "network_manager_pub.h"
#include "dltdt_function.h"
#include "dlra_function.h"

#include "sal.h"
#define private public
#define protected public
#include "network_manager_pub.h"
#include "comm_factory.h"
#undef protected
#undef private
#include "llt_hccl_stub_sal_pub.h"
#include "llt_hccl_stub_gdr.h"

#include "hccl_comm_pub.h"
#include "p2p_mgmt_pub.h"
#include "p2p_mgmt/p2p_mgmt.h"
#include "dlra_function.h"
#include "dltdt_function.h"

#include <iostream>
#include <fstream>
#include "profiler_manager.h"
#include "externalinput.h"

using namespace std;
using namespace hccl;

class CommFactoryTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        DlRaFunction::GetInstance().DlRaFunctionInit();
        DlTdtFunction::GetInstance().DlTdtFunctionInit();
        std::cout << "\033[36m--CommFactoryTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--CommFactoryTest TearDown--\033[0m" << std::endl;
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
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;

};
HcclDispatcher CommFactoryTest::dispatcherPtr = nullptr;
DispatcherPub *CommFactoryTest::dispatcher = nullptr;

typedef struct factorypara_struct
{
    std::string collectiveId;
    u32 userRank;
    u32 user_rank_size;
    TopoType topo_flag;
    DevType deviceType;
    std::vector<RankInfo> rank_vector;
    u64 comm_attribute;
    HcclDispatcher dispatcher;
    std::shared_ptr<CommFactory> comm_factory;
    std::vector<std::unique_ptr<CommBase> >comm_inner_ring;
    std::vector<std::unique_ptr<CommBase> >comm_combined;
    std::vector<std::unique_ptr<CommBase> >comm_inner_halving_doubling;
    std::vector<std::unique_ptr<CommBase> >comm_inner_nhr;
    std::vector<std::unique_ptr<CommBase> >comm_nhr_combined;
    std::vector<std::unique_ptr<CommBase> >comm_inner_nhr_v1;
    std::vector<std::unique_ptr<CommBase> >comm_nhr_v1_combined;
    std::vector<std::unique_ptr<CommBase> >comm_inner_nb;
    std::vector<std::unique_ptr<CommBase> >comm_nb_combined;
    std::vector<std::unique_ptr<CommBase> >comm_outer;
    std::vector<std::unique_ptr<CommBase> >comm_p2p;
} factorypara_t;

std::vector<std::unique_ptr<CommBase> > get_comm_inner()
{
    std::vector<std::unique_ptr<CommBase> > comm_inner(4);
    return comm_inner;
}

std::vector<std::unique_ptr<CommBase> > get_comm_outer()
{
    std::vector<std::unique_ptr<CommBase> > comm_outer(1);
    return comm_outer;
}

std::vector<std::unique_ptr<CommBase> > get_comm_p2p()
{
    std::vector<std::unique_ptr<CommBase> > comm_p2p(1);
    return comm_p2p;
}

HcclDispatcher get_dispatcher(s32 devid , std::shared_ptr<hccl::ProfilerManager> &profilerManager)
{
    HcclResult ret = HCCL_SUCCESS;
    profilerManager.reset(new (std::nothrow) ProfilerManager(0, 0, 2));
    profilerManager->InitProfiler();
     // 创建dispatcher
    void *dispatcher = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, devid, &dispatcher);
    EXPECT_NE(dispatcher, nullptr);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    return dispatcher;
}

void get_rank_vector(std::vector<RankInfo>& rank_vector)
{
    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.0.0.10";
    tmp_para_0.nicIp.push_back(HcclIpAddress("192.168.0.11"));
    tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    RankInfo tmp_para_1;

    tmp_para_1.userRank = 1;
    tmp_para_1.devicePhyId = 1;
    tmp_para_1.deviceType = DevType::DEV_TYPE_910;
    tmp_para_1.serverIdx = 0;
    tmp_para_1.serverId = "10.0.0.10";
    tmp_para_1.nicIp.push_back(HcclIpAddress("192.168.0.12"));
    tmp_para_1.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    RankInfo tmp_para_2;

    tmp_para_2.userRank = 2;
    tmp_para_2.devicePhyId = 2;
    tmp_para_2.deviceType = DevType::DEV_TYPE_910;
    tmp_para_2.serverIdx = 0;
    tmp_para_2.serverId = "10.0.0.10";
    tmp_para_2.nicIp.push_back(HcclIpAddress("192.168.0.13"));
    tmp_para_2.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    RankInfo tmp_para_3;

    tmp_para_3.userRank = 3;
    tmp_para_3.devicePhyId = 3;
    tmp_para_3.deviceType = DevType::DEV_TYPE_910;
    tmp_para_3.serverIdx = 0;
    tmp_para_3.serverId = "10.0.0.10";
    tmp_para_3.nicIp.push_back(HcclIpAddress("192.168.0.14"));
    tmp_para_3.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    RankInfo tmp_para_4;

    tmp_para_4.userRank = 4;
    tmp_para_4.devicePhyId = 0;
    tmp_para_4.deviceType = DevType::DEV_TYPE_910;
    tmp_para_4.serverIdx = 0;
    tmp_para_4.serverId = "10.0.0.10";
    tmp_para_4.nicIp.push_back(HcclIpAddress("192.168.0.15"));
    tmp_para_4.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    RankInfo tmp_para_5;

    tmp_para_5.userRank = 5;
    tmp_para_5.devicePhyId = 1;
    tmp_para_5.deviceType = DevType::DEV_TYPE_910;
    tmp_para_5.serverIdx = 0;
    tmp_para_5.serverId = "10.0.0.10";
    tmp_para_5.nicIp.push_back(HcclIpAddress("192.168.0.16"));
    tmp_para_5.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    RankInfo tmp_para_6;

    tmp_para_6.userRank = 6;
    tmp_para_6.devicePhyId = 2;
    tmp_para_6.deviceType = DevType::DEV_TYPE_910;
    tmp_para_6.serverIdx = 0;
    tmp_para_6.serverId = "10.0.0.10";
    tmp_para_6.nicIp.push_back(HcclIpAddress("192.168.0.17"));
    tmp_para_6.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    RankInfo tmp_para_7;

    tmp_para_7.userRank = 7;
    tmp_para_7.devicePhyId = 3;
    tmp_para_7.deviceType = DevType::DEV_TYPE_910;
    tmp_para_7.serverIdx = 0;
    tmp_para_7.serverId = "10.0.0.10";
    tmp_para_7.nicIp.push_back(HcclIpAddress("192.168.0.18"));
    tmp_para_7.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    rank_vector.push_back(tmp_para_0);
    rank_vector.push_back(tmp_para_1);
    rank_vector.push_back(tmp_para_2);
    rank_vector.push_back(tmp_para_3);
    rank_vector.push_back(tmp_para_4);
    rank_vector.push_back(tmp_para_5);
    rank_vector.push_back(tmp_para_6);
    rank_vector.push_back(tmp_para_7);

    return;
}

void get_rank_vector_4pring(std::vector<RankInfo>& rank_vector)
{
    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.0.0.10";
    tmp_para_0.nicIp.push_back(HcclIpAddress("192.168.0.11"));
    tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    RankInfo tmp_para_1;

    tmp_para_1.userRank = 1;
    tmp_para_1.devicePhyId = 1;
    tmp_para_1.deviceType = DevType::DEV_TYPE_910;
    tmp_para_1.serverIdx = 0;
    tmp_para_1.serverId = "10.0.0.10";
    tmp_para_1.nicIp.push_back(HcclIpAddress("192.168.0.12"));
    tmp_para_1.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    RankInfo tmp_para_2;

    tmp_para_2.userRank = 2;
    tmp_para_2.devicePhyId = 2;
    tmp_para_2.deviceType = DevType::DEV_TYPE_910;
    tmp_para_2.serverIdx = 0;
    tmp_para_2.serverId = "10.0.0.10";
    tmp_para_2.nicIp.push_back(HcclIpAddress("192.168.0.13"));
    tmp_para_2.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    RankInfo tmp_para_3;

    tmp_para_3.userRank = 3;
    tmp_para_3.devicePhyId = 3;
    tmp_para_3.deviceType = DevType::DEV_TYPE_910;
    tmp_para_3.serverIdx = 0;
    tmp_para_3.serverId = "10.0.0.10";
    tmp_para_3.nicIp.push_back(HcclIpAddress("192.168.0.14"));
    tmp_para_3.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    rank_vector.push_back(tmp_para_0);
    rank_vector.push_back(tmp_para_1);
    rank_vector.push_back(tmp_para_2);
    rank_vector.push_back(tmp_para_3);

    return;
}

void* comm_factory_task_handle_with_rank_table(void* para)
{
    HcclResult ret = HCCL_SUCCESS;

    factorypara_t* para_info = (factorypara_t*)para;

    ret = hrtSetDevice(para_info->userRank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;

    std::shared_ptr<TopoInfoExtractor> topoInfoExt;
    topoInfoExt.reset(new TopoInfoExtractor(para_info->collectiveId, para_info->userRank,
        para_info->user_rank_size, TopoType::TOPO_TYPE_COMMON,
        para_info->deviceType, para_info->rank_vector));
    para_info->comm_factory.reset(new CommFactory(para_info->collectiveId,
                                para_info->userRank,
                                para_info->user_rank_size,
                                para_info->dispatcher, nullptr, netDevCtxMap, topoInfoExt,
                                false,
                                para_info->topo_flag,
                                para_info->deviceType,
                                para_info->rank_vector));

    ret = para_info->comm_factory->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    para_info->comm_factory->GetSubRootUserRank(para_info->userRank, 0);

    return (NULL);
}

void* comm_factory_task_handle_ring(void* para)
{
    HcclResult ret = HCCL_SUCCESS;

    factorypara_t* para_info = (factorypara_t*)para;

    ret = hrtSetDevice(para_info->userRank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;

    std::shared_ptr<TopoInfoExtractor> topoInfoExt;
    topoInfoExt.reset(new TopoInfoExtractor(para_info->collectiveId, para_info->userRank,
        para_info->user_rank_size, TopoType::TOPO_TYPE_COMMON,
        para_info->deviceType, para_info->rank_vector));

    para_info->comm_factory.reset(new CommFactory(para_info->collectiveId,
                                para_info->userRank,
                                para_info->user_rank_size,
                                para_info->dispatcher, nullptr, netDevCtxMap, topoInfoExt,
                                false,
                                para_info->topo_flag,
                                para_info->deviceType,
                                para_info->rank_vector));

    ret = para_info->comm_factory->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    s32 mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(mem_size);
    DeviceMem outputMem = DeviceMem::alloc(mem_size);
    const string strTag = "test_tag";

    CommParaInfo commParaInfo;
    commParaInfo = CommParaInfo(COMM_LEVEL1, CommType::COMM_TAG_RING_INNER);
    ret = para_info->comm_factory->CreateCommPlane(strTag, inputMem, outputMem, commParaInfo, para_info->comm_inner_ring);

    return (NULL);
}

void* comm_factory_task_handle_combined(void* para)
{
    HcclResult ret = HCCL_SUCCESS;

    factorypara_t* para_info = (factorypara_t*)para;

    ret = hrtSetDevice(para_info->userRank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    std::shared_ptr<TopoInfoExtractor> topoInfoExt;
    topoInfoExt.reset(new TopoInfoExtractor(para_info->collectiveId, para_info->userRank,
        para_info->user_rank_size, TopoType::TOPO_TYPE_COMMON,
        para_info->deviceType, para_info->rank_vector));
    para_info->comm_factory.reset(new CommFactory(para_info->collectiveId,
                                para_info->userRank,
                                para_info->user_rank_size,
                                para_info->dispatcher, nullptr, netDevCtxMap, topoInfoExt,
                                false,
                                para_info->topo_flag,
                                para_info->deviceType,
                                para_info->rank_vector));

    ret = para_info->comm_factory->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    s32 mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(mem_size);
    DeviceMem outputMem = DeviceMem::alloc(mem_size);
    const string strTag = "test_tag";

    CommParaInfo commParaInfo;
    commParaInfo = CommParaInfo(COMM_COMBINE, CommType::COMM_TAG_RING_COMBINED);
    ret = para_info->comm_factory->CreateCommPlane(strTag, inputMem, outputMem, commParaInfo, para_info->comm_inner_ring);

    return (NULL);
}

void* comm_factory_task_handle_H_D(void* para)
{
    HcclResult ret = HCCL_SUCCESS;

    factorypara_t* para_info = (factorypara_t*)para;

    ret = hrtSetDevice(para_info->userRank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    std::shared_ptr<TopoInfoExtractor> topoInfoExt;
    topoInfoExt.reset(new TopoInfoExtractor(para_info->collectiveId, para_info->userRank,
        para_info->user_rank_size, TopoType::TOPO_TYPE_COMMON,
        para_info->deviceType, para_info->rank_vector));
    para_info->comm_factory.reset(new CommFactory(para_info->collectiveId,
                                para_info->userRank,
                                para_info->user_rank_size,
                                para_info->dispatcher, nullptr, netDevCtxMap, topoInfoExt,
                                false,
                                para_info->topo_flag,
                                para_info->deviceType,
                                para_info->rank_vector));

    ret = para_info->comm_factory->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    s32 mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(mem_size);
    DeviceMem outputMem = DeviceMem::alloc(mem_size);
    const string strTag = "test_tag";

    CommParaInfo commParaInfo;
    commParaInfo = CommParaInfo(COMM_LEVEL1, CommType::COMM_TAG_HALVING_DOUBLING);
    ret = para_info->comm_factory->CreateCommPlane(strTag, inputMem, outputMem, commParaInfo, para_info->comm_inner_ring);

    if (para_info->deviceType == DevType::DEV_TYPE_910B) {
        std::string tag = "hccl_allreduce";



    }

    return (NULL);
}

void* comm_factory_task_handle_P2P(void* para)
{
    HcclResult ret = HCCL_SUCCESS;

    factorypara_t* para_info = (factorypara_t*)para;

    ret = hrtSetDevice(para_info->userRank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    std::shared_ptr<TopoInfoExtractor> topoInfoExt;
    topoInfoExt.reset(new TopoInfoExtractor(para_info->collectiveId, para_info->userRank,
        para_info->user_rank_size, TopoType::TOPO_TYPE_COMMON,
        para_info->deviceType, para_info->rank_vector));
    para_info->comm_factory.reset(new CommFactory(para_info->collectiveId,
                                para_info->userRank,
                                para_info->user_rank_size,
                                para_info->dispatcher, nullptr, netDevCtxMap, topoInfoExt,
                                false,
                                para_info->topo_flag,
                                para_info->deviceType,
                                para_info->rank_vector));

    ret = para_info->comm_factory->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    s32 mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(mem_size);
    DeviceMem outputMem = DeviceMem::alloc(mem_size);
    const string strTag = "test_tag";

    CommParaInfo commParaInfo;
    commParaInfo = CommParaInfo(COMM_COMBINE, CommType::COMM_TAG_P2P);
    ret = para_info->comm_factory->CreateCommPlane(strTag, inputMem, outputMem, commParaInfo, para_info->comm_inner_ring);

    return (NULL);
}


TEST_F(CommFactoryTest, destructor_D0)
{
    std::string rootInfo = "test_collective";

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    std::vector<RankInfo> rank_vector = std::vector<RankInfo>(0);
    std::shared_ptr<TopoInfoExtractor> topoInfoExt;
    topoInfoExt.reset(new TopoInfoExtractor(rootInfo, 0, 1, TopoType::TOPO_TYPE_COMMON,
        DevType::DEV_TYPE_910, rank_vector));
    CommFactory* comm_factory = new CommFactory(rootInfo, 0, 1, dispatcher, nullptr, netDevCtxMap, topoInfoExt, true);

    delete comm_factory;
}

TEST_F(CommFactoryTest, init)
{
    s32 ret = HCCL_SUCCESS;

    u32 userRank = 0;
    u32 user_rank_size = 1;

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string collective_id_tmp = collectiveId;

    DevType chipType = DevType::DEV_TYPE_910;

    std::vector<RankInfo> rank_vector;
    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.0.0.10";
    tmp_para_0.nicIp.push_back(HcclIpAddress("192.168.0.11"));
    tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    rank_vector.push_back(tmp_para_0);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    std::shared_ptr<TopoInfoExtractor> topoInfoExt;
    topoInfoExt.reset(new TopoInfoExtractor(collective_id_tmp, userRank, user_rank_size, TopoType::TOPO_TYPE_COMMON,
        DevType::DEV_TYPE_910, rank_vector));
    CommFactory* comm_factory = new CommFactory(collective_id_tmp, userRank, user_rank_size, dispatcher, nullptr,
        netDevCtxMap, topoInfoExt, true, TopoType::TOPO_TYPE_COMMON, DevType::DEV_TYPE_910, rank_vector);

    ret = comm_factory->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete comm_factory;
}

TEST_F(CommFactoryTest, init_8ranks)
{
    s32 ret = HCCL_SUCCESS;

    u32 userRank = 0;
    u32 user_rank_size = 4;

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string collective_id_tmp = collectiveId;
    DevType chipType = DevType::DEV_TYPE_910;

    std::vector<RankInfo> rank_vector;
    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.0.0.10";
    tmp_para_0.nicIp.push_back(HcclIpAddress("192.168.0.11"));
    tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    RankInfo tmp_para_1;

    tmp_para_1.userRank = 1;
    tmp_para_1.devicePhyId = 1;
    tmp_para_1.deviceType = DevType::DEV_TYPE_910;
    tmp_para_1.serverIdx = 0;
    tmp_para_1.serverId = "10.0.0.10";
    tmp_para_1.nicIp.push_back(HcclIpAddress("192.168.0.12"));
    tmp_para_1.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    RankInfo tmp_para_2;

    tmp_para_2.userRank = 2;
    tmp_para_2.devicePhyId = 0;
    tmp_para_2.deviceType = DevType::DEV_TYPE_910;
    tmp_para_2.serverIdx = 1;
    tmp_para_2.serverId = "10.0.0.11";
    tmp_para_2.nicIp.push_back(HcclIpAddress("192.168.0.13"));
    tmp_para_2.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    RankInfo tmp_para_3;

    tmp_para_3.userRank = 3;
    tmp_para_3.devicePhyId = 1;
    tmp_para_3.deviceType = DevType::DEV_TYPE_910;
    tmp_para_3.serverIdx = 1;
    tmp_para_3.serverId = "10.0.0.11";
    tmp_para_3.nicIp.push_back(HcclIpAddress("192.168.0.14"));
    tmp_para_3.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    rank_vector.push_back(tmp_para_0);
    rank_vector.push_back(tmp_para_1);
    rank_vector.push_back(tmp_para_2);
    rank_vector.push_back(tmp_para_3);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    std::shared_ptr<TopoInfoExtractor> topoInfoExt;
    topoInfoExt.reset(new TopoInfoExtractor(collective_id_tmp, userRank, user_rank_size, TopoType::TOPO_TYPE_COMMON,
        DevType::DEV_TYPE_910, rank_vector));
    CommFactory* comm_factory = new CommFactory(collective_id_tmp, userRank, user_rank_size, dispatcher,
        nullptr, netDevCtxMap, topoInfoExt, true, TopoType::TOPO_TYPE_COMMON, DevType::DEV_TYPE_910, rank_vector);

    ret = comm_factory->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete comm_factory;
}

TEST_F(CommFactoryTest, create_comm)
{
    s32 ret = HCCL_SUCCESS;

    u32 userRank = 0;
    u32 user_rank_size = 1;

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string collective_id_tmp = collectiveId;

    std::vector<RankInfo> rank_vector;
    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.0.0.10";
    tmp_para_0.nicIp.push_back(HcclIpAddress("192.168.0.11"));
    tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    rank_vector.push_back(tmp_para_0);

    ret = HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, 0, 0, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclNetDevCtx portCtx;
    ret = HcclNetOpenDev(&portCtx, NicType::DEVICE_NIC_TYPE, tmp_para_0.devicePhyId, tmp_para_0.devicePhyId, tmp_para_0.nicIp[0]);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    netDevCtxMap.insert(make_pair(tmp_para_0.nicIp[0], portCtx));

    std::shared_ptr<TopoInfoExtractor> topoInfoExt;
    topoInfoExt.reset(new TopoInfoExtractor(collective_id_tmp, userRank, user_rank_size, TopoType::TOPO_TYPE_1P_MESH,
        DevType::DEV_TYPE_910, rank_vector));

    CommFactory* comm_factory = new CommFactory(collective_id_tmp, userRank, user_rank_size, dispatcher, nullptr,
        netDevCtxMap, topoInfoExt, true, TopoType::TOPO_TYPE_1P_MESH, DevType::DEV_TYPE_910, rank_vector);

    ret = comm_factory->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    s32 mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(mem_size);
    DeviceMem outputMem = DeviceMem::alloc(mem_size);
    const string strTag = "test_tag";

    CommParaInfo commParaInfo;
    std::vector<std::unique_ptr<CommBase> > commVec;

    commParaInfo = CommParaInfo(COMM_LEVEL1, CommType::COMM_TAG_RING_INNER);
    ret = comm_factory->CreateCommPlane(strTag, inputMem, outputMem, commParaInfo, commVec);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    commParaInfo = CommParaInfo(COMM_COMBINE, CommType::COMM_TAG_RING_COMBINED);
    ret = comm_factory->CreateCommPlane(strTag, inputMem, outputMem, commParaInfo, commVec);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    commParaInfo = CommParaInfo(COMM_COMBINE, CommType::COMM_TAG_WHOLE_NHR);
    ret = comm_factory->CreateCommPlane(strTag, inputMem, outputMem, commParaInfo, commVec);
    EXPECT_EQ(ret, HCCL_E_PARA);

    commParaInfo = CommParaInfo(COMM_COMBINE, CommType::COMM_TAG_WHOLE_NHR_V1);
    ret = comm_factory->CreateCommPlane(strTag, inputMem, outputMem, commParaInfo, commVec);
    EXPECT_EQ(ret, HCCL_E_PARA);

    commParaInfo = CommParaInfo(COMM_LEVEL1, CommType::COMM_TAG_HALVING_DOUBLING);
    ret = comm_factory->CreateCommPlane(strTag, inputMem, outputMem, commParaInfo, commVec);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    commParaInfo = CommParaInfo(COMM_LEVEL1, CommType::COMM_TAG_NONUNIFORM_HIERARCHICAL_RING);
    ret = comm_factory->CreateCommPlane(strTag, inputMem, outputMem, commParaInfo, commVec);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    commParaInfo = CommParaInfo(COMM_LEVEL1, CommType::COMM_TAG_NONUNIFORM_HIERARCHICAL_RING_V1);
    ret = comm_factory->CreateCommPlane(strTag, inputMem, outputMem, commParaInfo, commVec);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    commParaInfo = CommParaInfo(COMM_COMBINE, CommType::COMM_TAG_WHOLE_NB);
    ret = comm_factory->CreateCommPlane(strTag, inputMem, outputMem, commParaInfo, commVec);
    EXPECT_EQ(ret, HCCL_E_PARA);

    commParaInfo = CommParaInfo(COMM_LEVEL1, CommType::COMM_TAG_NONUNIFORM_BRUCK);
    ret = comm_factory->CreateCommPlane(strTag, inputMem, outputMem, commParaInfo, commVec);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    commParaInfo = CommParaInfo(COMM_LEVEL0, CommType::COMM_TAG_MESH);
    ret = comm_factory->CreateCommPlane(strTag, inputMem, outputMem, commParaInfo, commVec);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER_CPP(&CommBase::IsSupportMC2)
    .stubs()  
    .with(any())
    .will(returnValue(2));
    
    DeviceMem expMem = DeviceMem::alloc(mem_size);
    commParaInfo = CommParaInfo(COMM_COMBINE_ORDER, CommType::COMM_TAG_MESH_COMBINED);
    std::vector<std::vector<RankInfo> > commPlaneVec;
    commPlaneVec.push_back(rank_vector);
    ret = comm_factory->CreateCommMesh(strTag, inputMem, outputMem, commParaInfo, commPlaneVec, false, commVec, expMem);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    setenv("HCCL_INTRA_PCIE_ENABLE", "1", 1);
    commParaInfo = CommParaInfo(COMM_COMBINE_ORDER, CommType::COMM_TAG_MESH_COMBINED);
    ret = comm_factory->CreateCommPlane(strTag, inputMem, outputMem, commParaInfo, commVec);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    unsetenv("HCCL_INTRA_PCIE_ENABLE");

    setenv("HCCL_INTRA_PCIE_ENABLE", "0", 1);
    commParaInfo = CommParaInfo(COMM_COMBINE_ORDER, CommType::COMM_TAG_MESH_COMBINED);
    ret = comm_factory->CreateCommPlane(strTag, inputMem, outputMem, commParaInfo, commVec);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    unsetenv("HCCL_INTRA_PCIE_ENABLE");

    HcclNetCloseDev(portCtx);
    HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, tmp_para_0.devicePhyId, tmp_para_0.devicePhyId);
    delete comm_factory;
}

TEST_F(CommFactoryTest, create_comm_inner_err_type)
{
    s32 ret = HCCL_SUCCESS;


    u32 userRank = 0;
    u32 user_rank_size = 1;

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string collective_id_tmp = collectiveId;

    std::vector<RankInfo> rank_vector;
    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.0.0.10";
    tmp_para_0.nicIp.push_back(HcclIpAddress("192.168.0.11"));
    tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    rank_vector.push_back(tmp_para_0);

    ret = HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, 0, 0, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclNetDevCtx portCtx;
    ret = HcclNetOpenDev(&portCtx, NicType::DEVICE_NIC_TYPE, rank_vector[0].devicePhyId, rank_vector[0].devicePhyId, rank_vector[0].nicIp[0]);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    netDevCtxMap.insert(make_pair(rank_vector[0].nicIp[0], portCtx));

    std::shared_ptr<TopoInfoExtractor> topoInfoExt;
    topoInfoExt.reset(new TopoInfoExtractor(collective_id_tmp, userRank, user_rank_size, TopoType::TOPO_TYPE_1P_MESH,
        DevType::DEV_TYPE_910, rank_vector));

    CommFactory* comm_factory = new CommFactory(collective_id_tmp, userRank, user_rank_size, dispatcher, nullptr,
        netDevCtxMap, topoInfoExt, true, TopoType::TOPO_TYPE_1P_MESH, DevType::DEV_TYPE_910, rank_vector);

    ret = comm_factory->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    s32 mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(mem_size);
    DeviceMem outputMem = DeviceMem::alloc(mem_size);
    const string strTag = "test_tag";

    CommParaInfo commParaInfo;
    std::vector<std::unique_ptr<CommBase> > commVec;
    commParaInfo = CommParaInfo(COMM_COMBINE, CommType::COMM_TAG_MAX);
    ret = comm_factory->CreateCommPlane(strTag, inputMem, outputMem, commParaInfo, commVec);
    EXPECT_NE(ret, HCCL_SUCCESS);

    HcclNetCloseDev(portCtx);
    HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, rank_vector[0].devicePhyId, rank_vector[0].devicePhyId);

    delete comm_factory;
}

TEST_F(CommFactoryTest, ut_init_with_err_input)
{
    s32 ret = HCCL_SUCCESS;

    u32 userRank = 0;
    u32 user_rank_size = 8;

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string collective_id_tmp = collectiveId;
    // set device
    s32 devicePhyId = 0;
    ret = hrtSetDevice(devicePhyId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::vector<RankInfo> rank_vector;
    get_rank_vector(rank_vector);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;

    std::shared_ptr<TopoInfoExtractor> topoInfoExt;
    topoInfoExt.reset(new TopoInfoExtractor(collective_id_tmp, userRank, user_rank_size, TopoType::TOPO_TYPE_RESERVED,
        DevType::DEV_TYPE_910, rank_vector));

    std::shared_ptr<CommFactory> comm_factory_0 = nullptr;
    comm_factory_0.reset(new CommFactory(collective_id_tmp, userRank, user_rank_size, dispatcher, nullptr,
        netDevCtxMap, topoInfoExt, true, TopoType::TOPO_TYPE_RESERVED, DevType::DEV_TYPE_910, rank_vector));

    ret = comm_factory_0->Init();
    EXPECT_NE(ret, HCCL_SUCCESS);

    std::shared_ptr<CommFactory> comm_factory_1 = nullptr;

    std::shared_ptr<TopoInfoExtractor> topoInfoExt1;
    topoInfoExt1.reset(new TopoInfoExtractor(collective_id_tmp, userRank, user_rank_size, TopoType::TOPO_TYPE_2P_MESH,
        DevType::DEV_TYPE_910, rank_vector));

    comm_factory_1.reset(new CommFactory(collective_id_tmp, userRank, user_rank_size, dispatcher, nullptr,
        netDevCtxMap, topoInfoExt1, true, TopoType::TOPO_TYPE_2P_MESH, DevType::DEV_TYPE_910, rank_vector));

    ret = topoInfoExt1->Init();
    EXPECT_NE(ret, HCCL_SUCCESS);

    std::shared_ptr<CommFactory> comm_factory_2 = nullptr;

    std::shared_ptr<TopoInfoExtractor> topoInfoExt2;
    topoInfoExt2.reset(new TopoInfoExtractor(collective_id_tmp, userRank, user_rank_size, TopoType::TOPO_TYPE_4P_RING,
        DevType::DEV_TYPE_910, rank_vector));

    comm_factory_2.reset(new CommFactory(collective_id_tmp, userRank, user_rank_size, dispatcher, nullptr,
        netDevCtxMap, topoInfoExt2, true, TopoType::TOPO_TYPE_4P_RING, DevType::DEV_TYPE_910, rank_vector));

    ret = comm_factory_2->Init();
    EXPECT_NE(ret, HCCL_SUCCESS);

    std::vector<RankInfo> rank_vector_tmp(0);
    std::shared_ptr<CommFactory> comm_factory_3 = nullptr;

    std::shared_ptr<TopoInfoExtractor> topoInfoExt3;
    topoInfoExt3.reset(new TopoInfoExtractor(collective_id_tmp, userRank, user_rank_size, TopoType::TOPO_TYPE_4P_RING,
        DevType::DEV_TYPE_910, rank_vector_tmp));

    comm_factory_3.reset(new CommFactory(collective_id_tmp, userRank, user_rank_size, dispatcher, nullptr,
        netDevCtxMap, topoInfoExt3, true, TopoType::TOPO_TYPE_4P_RING, DevType::DEV_TYPE_910, rank_vector_tmp));

    ret = comm_factory_3->Init();
    EXPECT_NE(ret, HCCL_SUCCESS);
}

TEST_F(CommFactoryTest, ut_init_with_err_topo)
{
    s32 ret = HCCL_SUCCESS;

    u32 userRank = 0;
    u32 user_rank_size = 8;

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string collective_id_tmp = collectiveId;
    // set device
    s32 devicePhyId = 0;
    ret = hrtSetDevice(devicePhyId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::vector<RankInfo> rank_vector;
    get_rank_vector(rank_vector);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;

    std::shared_ptr<TopoInfoExtractor> topoInfoExt;
    topoInfoExt.reset(new TopoInfoExtractor(collective_id_tmp, userRank, user_rank_size, TopoType::TOPO_TYPE_RESERVED,
        DevType::DEV_TYPE_910, rank_vector));

    std::shared_ptr<CommFactory> comm_factory = nullptr;
    comm_factory.reset(new CommFactory(collective_id_tmp, userRank, user_rank_size, dispatcher, nullptr,
        netDevCtxMap, topoInfoExt, true, TopoType::TOPO_TYPE_RESERVED, DevType::DEV_TYPE_910, rank_vector));

    ret = comm_factory->Init();
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(CommFactoryTest, ut_init_with_err_rank_size)
{
    s32 ret = HCCL_SUCCESS;

    u32 userRank = 0;
    u32 user_rank_size = 2;

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string collective_id_tmp = collectiveId;
    // set device
    s32 devicePhyId = 0;
    ret = hrtSetDevice(devicePhyId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::vector<RankInfo> rank_vector;
    get_rank_vector(rank_vector);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;

    std::shared_ptr<TopoInfoExtractor> topoInfoExt;
    topoInfoExt.reset(new TopoInfoExtractor(collective_id_tmp, userRank, user_rank_size, TopoType::TOPO_TYPE_8P_RING,
        DevType::DEV_TYPE_910, rank_vector));

    std::shared_ptr<CommFactory> comm_factory = nullptr;
    comm_factory.reset(new CommFactory(collective_id_tmp, userRank, user_rank_size, dispatcher, nullptr,
        netDevCtxMap, topoInfoExt, true, TopoType::TOPO_TYPE_8P_RING, DevType::DEV_TYPE_910, rank_vector));

    ret = comm_factory->Init();
    EXPECT_NE(ret, HCCL_SUCCESS);
}

TEST_F(CommFactoryTest, ut_init_with_rank_table_8ranks_4P_MESH)
{
    s32 ret = HCCL_SUCCESS;

    HcclRootInfo rootInfo;

    ret = hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);

    std::string collective_id_tmp = collectiveId;

    std::vector<std::shared_ptr<CommFactory>> comm_factory_vec(8);

    sal_thread_t tid[8];
    factorypara_t para_info[8];
    s32 ndev = 8;

    std::vector<RankInfo> rank_vector;
    get_rank_vector(rank_vector);
    std::shared_ptr<ProfilerManager> profilerManager[8];
    for (s32 i = 0; i < ndev; i++)
    {
        para_info[i].collectiveId = collective_id_tmp;
        para_info[i].userRank = i;
        para_info[i].user_rank_size = ndev;
        para_info[i].topo_flag = TopoType::TOPO_TYPE_4P_MESH;
        para_info[i].deviceType = DevType::DEV_TYPE_910;
        para_info[i].rank_vector = rank_vector;
        para_info[i].comm_attribute = 6;
        para_info[i].dispatcher= get_dispatcher(i, profilerManager[i]);
        para_info[i].comm_factory = comm_factory_vec[i];
        para_info[i].comm_inner_ring = get_comm_inner();
        para_info[i].comm_combined = get_comm_outer();
        para_info[i].comm_inner_halving_doubling = get_comm_inner();
        para_info[i].comm_outer = get_comm_outer();
    }

    tid[0] = sal_thread_create("commfactory offline rank0 thread", comm_factory_task_handle_with_rank_table, (void*)&para_info[0]);

    tid[1] = sal_thread_create("commfactory offline rank1 thread", comm_factory_task_handle_with_rank_table, (void*)&para_info[1]);

    tid[2] = sal_thread_create("commfactory offline rank2 thread", comm_factory_task_handle_with_rank_table, (void*)&para_info[2]);

    tid[3] = sal_thread_create("commfactory offline rank3 thread", comm_factory_task_handle_with_rank_table, (void*)&para_info[3]);

    tid[4] = sal_thread_create("commfactory offline rank4 thread", comm_factory_task_handle_with_rank_table, (void*)&para_info[4]);

    tid[5] = sal_thread_create("commfactory offline rank5 thread", comm_factory_task_handle_with_rank_table, (void*)&para_info[5]);

    tid[6] = sal_thread_create("commfactory offline rank6 thread", comm_factory_task_handle_with_rank_table, (void*)&para_info[6]);

    tid[7] = sal_thread_create("commfactory offline rank7 thread", comm_factory_task_handle_with_rank_table, (void*)&para_info[7]);

    while (sal_thread_is_running(tid[0]) || sal_thread_is_running(tid[1])
           || sal_thread_is_running(tid[2]) || sal_thread_is_running(tid[3])
           || sal_thread_is_running(tid[4]) || sal_thread_is_running(tid[5])
           || sal_thread_is_running(tid[6]) || sal_thread_is_running(tid[7]))
    {
        SaluSleep(SAL_MILLISECOND_USEC * 10);;
    }

    for (s32 j = 0; j < ndev; j++)
    {
        (void)sal_thread_destroy(tid[j]);
        if (para_info[j].dispatcher != nullptr) {
            ret = HcclDispatcherDestroy(para_info[j].dispatcher);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            para_info[j].dispatcher = nullptr;
        }
    }
}

TEST_F(CommFactoryTest, ut_init_with_rank_table_4ranks)
{
    s32 ret = HCCL_SUCCESS;

    HcclRootInfo rootInfo;

    ret = hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);

    std::string collective_id_tmp = collectiveId;

    std::vector<std::shared_ptr<CommFactory>> comm_factory_vec(4);

    sal_thread_t tid[4];
    factorypara_t para_info[4];
    s32 ndev = 4;

    std::vector<RankInfo> rank_vector;
    get_rank_vector_4pring(rank_vector);
    std::shared_ptr<ProfilerManager> profilerManager[4];
    for (s32 i = 0; i < ndev; i++)
    {
        para_info[i].collectiveId = collective_id_tmp;
        para_info[i].userRank = i;
        para_info[i].user_rank_size = ndev;
        para_info[i].topo_flag = TopoType::TOPO_TYPE_4P_MESH;
        para_info[i].deviceType = DevType::DEV_TYPE_910;
        para_info[i].rank_vector = rank_vector;
        para_info[i].comm_attribute = 6;
        para_info[i].dispatcher= get_dispatcher(i, profilerManager[i]);
        para_info[i].comm_factory = comm_factory_vec[i];
        para_info[i].comm_inner_ring = get_comm_inner();
        para_info[i].comm_combined = get_comm_outer();
        para_info[i].comm_inner_halving_doubling = get_comm_inner();
        para_info[i].comm_outer = get_comm_outer();
    }

    tid[0] = sal_thread_create("commfactory offline rank0 thread", comm_factory_task_handle_ring, (void*)&para_info[0]);

    tid[1] = sal_thread_create("commfactory offline rank1 thread", comm_factory_task_handle_ring, (void*)&para_info[1]);

    tid[2] = sal_thread_create("commfactory offline rank2 thread", comm_factory_task_handle_ring, (void*)&para_info[2]);

    tid[3] = sal_thread_create("commfactory offline rank3 thread", comm_factory_task_handle_ring, (void*)&para_info[3]);

    while (sal_thread_is_running(tid[0]) || sal_thread_is_running(tid[1])
           || sal_thread_is_running(tid[2]) || sal_thread_is_running(tid[3]))
    {
        SaluSleep(SAL_MILLISECOND_USEC * 10);;
    }

    for (s32 j = 0; j < ndev; j++)
    {
        (void)sal_thread_destroy(tid[j]);
        if (para_info[j].dispatcher != nullptr) {
            ret = HcclDispatcherDestroy(para_info[j].dispatcher);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            para_info[j].dispatcher = nullptr;
        }
    }
}

TEST_F(CommFactoryTest, ut_init_with_rank_table_combined)
{
    s32 ret = HCCL_SUCCESS;

    HcclRootInfo rootInfo;

    ret = hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);

    std::string collective_id_tmp = collectiveId;

    std::vector<std::shared_ptr<CommFactory>> comm_factory_vec(1);

    sal_thread_t tid[1];
    factorypara_t para_info[1];
    s32 ndev = 1;

    std::vector<RankInfo> rank_vector;
    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.0.0.10";
    tmp_para_0.nicIp.push_back(HcclIpAddress("192.168.0.11"));
    tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    rank_vector.push_back(tmp_para_0);
    std::shared_ptr<ProfilerManager> profilerManager[1];
    for (s32 i = 0; i < ndev; i++)
    {
        para_info[i].collectiveId = collective_id_tmp;
        para_info[i].userRank = i;
        para_info[i].user_rank_size = ndev;
        para_info[i].topo_flag = TopoType::TOPO_TYPE_COMMON;
        para_info[i].deviceType = DevType::DEV_TYPE_910;
        para_info[i].rank_vector = rank_vector;
        para_info[i].comm_attribute = 6;
        para_info[i].dispatcher= get_dispatcher(i, profilerManager[i]);
        para_info[i].comm_factory = comm_factory_vec[i];
        para_info[i].comm_inner_ring = get_comm_inner();
        para_info[i].comm_combined = get_comm_outer();
        para_info[i].comm_inner_halving_doubling = get_comm_inner();
        para_info[i].comm_outer = get_comm_outer();
    }

    tid[0] = sal_thread_create("commfactory offline rank0 thread", comm_factory_task_handle_combined, (void*)&para_info[0]);

    while (sal_thread_is_running(tid[0]))
    {
        SaluSleep(SAL_MILLISECOND_USEC * 10);;
    }

    for (s32 j = 0; j < ndev; j++)
    {
        (void)sal_thread_destroy(tid[j]);
        if (para_info[j].dispatcher != nullptr) {
            ret = HcclDispatcherDestroy(para_info[j].dispatcher);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            para_info[j].dispatcher = nullptr;
        }
    }
}

TEST_F(CommFactoryTest, ut_init_with_rank_table_H_D)
{
    s32 ret = HCCL_SUCCESS;

    HcclRootInfo rootInfo;

    ret = hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);

    std::string collective_id_tmp = collectiveId;

    std::vector<std::shared_ptr<CommFactory>> comm_factory_vec(1);

    sal_thread_t tid[1];
    factorypara_t para_info[1];
    s32 ndev = 1;

    std::vector<RankInfo> rank_vector;
    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.0.0.10";
    tmp_para_0.nicIp.push_back(HcclIpAddress("192.168.0.11"));
    tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    rank_vector.push_back(tmp_para_0);
    std::shared_ptr<ProfilerManager> profilerManager[1];
    for (s32 i = 0; i < ndev; i++)
    {
        para_info[i].collectiveId = collective_id_tmp;
        para_info[i].userRank = i;
        para_info[i].user_rank_size = ndev;
        para_info[i].topo_flag = TopoType::TOPO_TYPE_1P_MESH;
        para_info[i].deviceType = DevType::DEV_TYPE_910;
        para_info[i].rank_vector = rank_vector;
        para_info[i].comm_attribute = 6;
        para_info[i].dispatcher= get_dispatcher(i, profilerManager[i]);
        para_info[i].comm_factory = comm_factory_vec[i];
        para_info[i].comm_inner_ring = get_comm_inner();
        para_info[i].comm_combined = get_comm_outer();
        para_info[i].comm_inner_halving_doubling = get_comm_inner();
        para_info[i].comm_outer = get_comm_outer();
    }

    tid[0] = sal_thread_create("commfactory offline rank0 thread", comm_factory_task_handle_H_D, (void*)&para_info[0]);

    while (sal_thread_is_running(tid[0]))
    {
        SaluSleep(SAL_MILLISECOND_USEC * 10);;
    }

    for (s32 j = 0; j < ndev; j++)
    {
        (void)sal_thread_destroy(tid[j]);
        if (para_info[j].dispatcher != nullptr) {
            ret = HcclDispatcherDestroy(para_info[j].dispatcher);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            para_info[j].dispatcher = nullptr;
        }
    }
}

TEST_F(CommFactoryTest, ut_init_with_rank_table_P2P)
{
    s32 ret = HCCL_SUCCESS;

    HcclRootInfo rootInfo;

    ret = hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);

    std::string collective_id_tmp = collectiveId;

    std::vector<std::shared_ptr<CommFactory>> comm_factory_vec(1);

    sal_thread_t tid[1];
    factorypara_t para_info[1];
    s32 ndev = 1;

    std::vector<RankInfo> rank_vector;
    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.0.0.10";
    tmp_para_0.nicIp.push_back(HcclIpAddress("192.168.0.11"));
    tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    rank_vector.push_back(tmp_para_0);
    std::shared_ptr<ProfilerManager> profilerManager[1];
    for (s32 i = 0; i < ndev; i++)
    {
        para_info[i].collectiveId = collective_id_tmp;
        para_info[i].userRank = i;
        para_info[i].user_rank_size = ndev;
        para_info[i].topo_flag = TopoType::TOPO_TYPE_1P_MESH;
        para_info[i].deviceType = DevType::DEV_TYPE_910;
        para_info[i].rank_vector = rank_vector;
        para_info[i].comm_attribute = 6;
        para_info[i].dispatcher= get_dispatcher(i, profilerManager[i]);
        para_info[i].comm_factory = comm_factory_vec[i];
        para_info[i].comm_inner_ring = get_comm_inner();
        para_info[i].comm_combined = get_comm_outer();
        para_info[i].comm_inner_halving_doubling = get_comm_inner();
        para_info[i].comm_p2p = get_comm_p2p();
    }

    tid[0] = sal_thread_create("commfactory offline rank0 thread", comm_factory_task_handle_P2P, (void*)&para_info[0]);

    while (sal_thread_is_running(tid[0]))
    {
        SaluSleep(SAL_MILLISECOND_USEC * 10);;
    }

    for (s32 j = 0; j < ndev; j++)
    {
        (void)sal_thread_destroy(tid[j]);
        if (para_info[j].dispatcher != nullptr) {
            ret = HcclDispatcherDestroy(para_info[j].dispatcher);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            para_info[j].dispatcher = nullptr;
        }
    }
}

TEST_F(CommFactoryTest, ut_init_V71_with_rank_table_H_D)
{
    s32 ret = HCCL_SUCCESS;

    HcclRootInfo rootInfo;

    ret = hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);

    std::string collective_id_tmp = collectiveId;

    std::vector<std::shared_ptr<CommFactory>> comm_factory_vec(2);

    sal_thread_t tid[2];
    factorypara_t para_info[2];
    s32 ndev = 2;

    std::vector<RankInfo> rank_vector;
    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910B;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.0.0.10";
    tmp_para_0.nicIp.push_back(HcclIpAddress("192.168.0.11"));
    tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    rank_vector.push_back(tmp_para_0);

    RankInfo tmp_para_1;
    tmp_para_1.userRank = 1;
    tmp_para_1.devicePhyId = 8;
    tmp_para_1.deviceType = DevType::DEV_TYPE_910B;
    tmp_para_1.serverIdx = 0;
    tmp_para_1.serverId = "10.0.0.10";
    tmp_para_1.nicIp.push_back(HcclIpAddress("192.168.0.21"));
    tmp_para_1.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    rank_vector.push_back(tmp_para_1);
    std::shared_ptr<ProfilerManager> profilerManager[2];
    for (s32 i = 0; i < ndev; i++)
    {
        para_info[i].collectiveId = collective_id_tmp;
        para_info[i].userRank = i;
        para_info[i].user_rank_size = ndev;
        para_info[i].topo_flag = TopoType::TOPO_TYPE_NP_SINGLE_RING;
        para_info[i].deviceType = DevType::DEV_TYPE_910B;
        para_info[i].rank_vector = rank_vector;
        para_info[i].comm_attribute = 6;
        para_info[i].dispatcher= get_dispatcher(i, profilerManager[i]);
        para_info[i].comm_factory = comm_factory_vec[i];
        para_info[i].comm_inner_ring = get_comm_inner();
        para_info[i].comm_combined = get_comm_outer();
        para_info[i].comm_inner_halving_doubling = get_comm_inner();
        para_info[i].comm_outer = get_comm_outer();
    }

    tid[0] = sal_thread_create("commfactory offline rank0 thread", comm_factory_task_handle_H_D, (void*)&para_info[0]);

    tid[1] = sal_thread_create("commfactory offline rank1 thread", comm_factory_task_handle_H_D, (void*)&para_info[1]);

    while (sal_thread_is_running(tid[0]) || sal_thread_is_running(tid[1]))
    {
        SaluSleep(SAL_MILLISECOND_USEC * 10);;
    }

    for (s32 j = 0; j < ndev; j++)
    {
        (void)sal_thread_destroy(tid[j]);
        if (para_info[j].dispatcher != nullptr) {
            ret = HcclDispatcherDestroy(para_info[j].dispatcher);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            para_info[j].dispatcher = nullptr;
        }
    }
}

TEST_F(CommFactoryTest, ut_init_V71_with_rank_table_nb)
{
    s32 ret = HCCL_SUCCESS;

    u32 userRank = 0;
    u32 user_rank_size = 2;

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string collective_id_tmp = collectiveId;
    // set device
    s32 devicePhyId = 0;
    ret = hrtSetDevice(devicePhyId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::vector<RankInfo> rank_vector;
    get_rank_vector(rank_vector);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;

    std::shared_ptr<TopoInfoExtractor> topoInfoExt;
    topoInfoExt.reset(new TopoInfoExtractor(collective_id_tmp, userRank, user_rank_size, TopoType::TOPO_TYPE_8P_RING,
        DevType::DEV_TYPE_910, rank_vector));

    std::shared_ptr<CommFactory> comm_factory = nullptr;
    comm_factory.reset(new CommFactory(collective_id_tmp, userRank, user_rank_size, dispatcher, nullptr,
        netDevCtxMap, topoInfoExt, true, TopoType::TOPO_TYPE_8P_RING, DevType::DEV_TYPE_910, rank_vector));

    ret = comm_factory->Init();
    EXPECT_NE(ret, HCCL_SUCCESS);
}

void get_rank_vector_4(std::vector<RankInfo>& rank_vector, u32 rank_size)
{
    std::string baseIp = "192.168.0.";
    u8 offset = 11;

    RankInfo tmp_para;
    for(int i = 0; i < rank_size; i++) {
        std::string ipStr = baseIp + std::to_string(offset + i);
        tmp_para.userRank = static_cast<u32>(i);
        tmp_para.devicePhyId = static_cast<u32>(i);
        tmp_para.deviceType = DevType::DEV_TYPE_910;
        tmp_para.serverIdx = 0;
        tmp_para.serverId = "10.0.0.10";
        tmp_para.nicIp.push_back(HcclIpAddress(ipStr));
        tmp_para.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

        rank_vector.push_back(tmp_para);
    }

    return;
}

HcclResult stub_CommFactoryTest_GetRaResourceInfo(NetworkManager* that, RaResourceInfo &raResourceInfo)
{
    static bool initialized = false;
    static RaResourceInfo fake_raResourceInfo;
    static int fake_handle = 1;
    if (!initialized) {
        IpSocket tmpIpSocket;
        fake_raResourceInfo.vnicSocketHandle = &fake_handle;
        tmpIpSocket.nicSocketHandle = &fake_handle;
        HcclIpAddress ipAddr("192.168.0.18");
        for (int i = 0; i < 8; i++) {
            fake_raResourceInfo.nicSocketMap[ipAddr] = tmpIpSocket;
        }
    }
    raResourceInfo = fake_raResourceInfo;
    return HCCL_SUCCESS;
}

s32 stub_CommFactoryTest_hrtRaSocketNonBlockSendHB(const FdHandle fdHandle, const void *data, u64 size, u64 *sent_size)
{
    *sent_size = size;
    return 0;
}

s32 stub_CommFactoryTest_hrtRaSocketNonBlockRecvHB(const FdHandle fdHandle, void *data, u64 size, u64 *recvSize)
{
    static u32 count = 0;
    if (count++ % 5 != 0) {
        *recvSize = size;
        count = 0;
    }
    return 0;
}

s32 stub_CommFactoryTest_hrtRaGetSockets(u32 role, struct SocketInfoT conn[], u32 num, u32 *connectedNum)
{
    static std::vector<int> fdHandle;
    for (int i = 0; i < num; i++) {
        fdHandle.push_back(0);
        conn[i].fdHandle = 0;
        conn[i].status = CONNECT_OK;
    }
    *connectedNum = num;
    return 0;
}

HcclResult stub_CommFactoryTest_GetIsSupSockBatchCloseImmed(u32 phyId, bool& isSupportBatchClose)
{
    isSupportBatchClose = true;
    return HCCL_SUCCESS;
}

HcclResult stub_CommFactoryTest_GetNicHandleInfo(std::map<HcclIpAddress, IpSocket> &socketMap,
    const HcclIpAddress &ip, SocketHandle &nicSocketHandle)
{
    nicSocketHandle = (void*)0x00000001;
    return HCCL_SUCCESS;
}

TEST_F(CommFactoryTest, ut_create_comm_suppod)
{
    s32 ret = HCCL_SUCCESS;

    DlTdtFunction::GetInstance().DlTdtFunctionInit();
    DlRaFunction::GetInstance().DlRaFunctionInit();

    u32 userRank = 0;
    u32 user_rank_size = 4;

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string collective_id_tmp = collectiveId;

    MOCKER_CPP(&CommBase::IsSupportInterHccs)
    .stubs()
    .with(any())
    .will(returnValue(true));

    MOCKER_CPP(&CommBase::GetSuperNodeIntraRankIPInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&CommBase::CreateDestLink)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(GetIsSupSockBatchCloseImmed)
    .stubs()
    .will(invoke(stub_CommFactoryTest_GetIsSupSockBatchCloseImmed));

    MOCKER(hrtRaSocketWhiteListAdd)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketWhiteListDel)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketBatchConnect)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaGetSockets)
    .stubs()
    .will(invoke(stub_CommFactoryTest_hrtRaGetSockets));

    MOCKER(hrtRaSocketBatchClose)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketNonBlockSend)
    .stubs()
    .will(invoke(stub_CommFactoryTest_hrtRaSocketNonBlockSendHB));

    MOCKER(hrtRaSocketNonBlockRecv)
    .stubs()
    .will(invoke(stub_CommFactoryTest_hrtRaSocketNonBlockRecvHB));

    std::vector<RankInfo> rank_vector;
    RankInfo tmp_para_0;
    tmp_para_0.userRank = 0;
    tmp_para_0.worldRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910_93;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.0.0.10";
    tmp_para_0.nicIp.push_back(HcclIpAddress("192.168.0.18"));
    tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    ret = HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, tmp_para_0.devicePhyId, tmp_para_0.devicePhyId, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclNetDevCtx portCtx;
    ret = HcclNetOpenDev(&portCtx, NicType::DEVICE_NIC_TYPE, tmp_para_0.devicePhyId, tmp_para_0.devicePhyId, HcclIpAddress(tmp_para_0.devicePhyId));
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    netDevCtxMap.insert(make_pair(HcclIpAddress(tmp_para_0.devicePhyId), portCtx));

    RankInfo tmp_para_1;
    tmp_para_1.userRank = 1;
    tmp_para_1.worldRank = 1;
    tmp_para_1.devicePhyId = 1;
    tmp_para_1.deviceType = DevType::DEV_TYPE_910_93;
    tmp_para_1.serverIdx = 0;
    tmp_para_1.serverId = "10.0.0.10";
    tmp_para_1.nicIp.push_back(HcclIpAddress("192.168.0.19"));
    tmp_para_1.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    RankInfo tmp_para_2;
    tmp_para_2.userRank = 2;
    tmp_para_2.worldRank = 2;
    tmp_para_2.devicePhyId = 0;
    tmp_para_2.deviceType = DevType::DEV_TYPE_910_93;
    tmp_para_2.serverIdx = 1;
    tmp_para_2.serverId = "10.0.0.20";
    tmp_para_2.nicIp.push_back(HcclIpAddress("192.168.0.20"));
    tmp_para_2.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    RankInfo tmp_para_3;
    tmp_para_3.userRank = 3;
    tmp_para_3.worldRank = 3;
    tmp_para_3.devicePhyId = 1;
    tmp_para_3.deviceType = DevType::DEV_TYPE_910_93;
    tmp_para_3.serverIdx = 1;
    tmp_para_3.serverId = "10.0.0.20";
    tmp_para_3.nicIp.push_back(HcclIpAddress("192.168.0.21"));
    tmp_para_3.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    rank_vector.push_back(tmp_para_0);
    rank_vector.push_back(tmp_para_1);
    rank_vector.push_back(tmp_para_2);
    rank_vector.push_back(tmp_para_3);

    std::shared_ptr<TopoInfoExtractor> topoInfoExt;
    topoInfoExt.reset(new TopoInfoExtractor(collective_id_tmp, userRank, user_rank_size, TopoType::TOPO_TYPE_NP_DOUBLE_RING,
        DevType::DEV_TYPE_910_93, rank_vector));

    CommFactory* comm_factory = new CommFactory(collective_id_tmp, userRank, user_rank_size, dispatcher, nullptr,
        netDevCtxMap, topoInfoExt, true,
        TopoType::TOPO_TYPE_NP_DOUBLE_RING, DevType::DEV_TYPE_910_93, rank_vector,
        NICDeployment::NIC_DEPLOYMENT_DEVICE, false, nullptr, 0, 0, false, true);

    ret = comm_factory->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    s32 mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(mem_size);
    DeviceMem outputMem = DeviceMem::alloc(mem_size);
    const string strTag = collective_id_tmp;

    CommParaInfo commParaInfo;
    std::vector<std::unique_ptr<CommBase> > commVec;

    commParaInfo = CommParaInfo(COMM_LEVEL1, CommType::COMM_TAG_RING_INNER);
    ret = comm_factory->CreateCommPlane(strTag, inputMem, outputMem, commParaInfo, commVec);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclNetCloseDev(portCtx);
    HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, tmp_para_0.devicePhyId, tmp_para_0.devicePhyId);

    delete comm_factory;
}
TEST_F(CommFactoryTest, ut_create_commmesh_combined_1server_16p)
{
    s32 ret = HCCL_SUCCESS;

    u32 userRank = 0;
    u32 user_rank_size = 5;

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string collective_id_tmp = collectiveId;

    std::vector<RankInfo> rank_vector;
    for(u32 i = 0; i < 5; i++) {
        RankInfo tmp_para_0;
        tmp_para_0.userRank = i;
        tmp_para_0.devicePhyId = i;
        tmp_para_0.serverIdx = 1;
        tmp_para_0.deviceType = DevType::DEV_TYPE_910B;
        tmp_para_0.serverId = "10.0.0.10";
        tmp_para_0.nicIp.push_back(HcclIpAddress("192.168.0.18"));
        tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
        rank_vector.push_back(tmp_para_0);
    }

    ret = HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, 0, 0, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclNetDevCtx portCtx;
    ret = HcclNetOpenDev(&portCtx, NicType::DEVICE_NIC_TYPE, rank_vector[0].devicePhyId, rank_vector[0].devicePhyId, rank_vector[0].nicIp[0]);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    netDevCtxMap.insert(make_pair(rank_vector[0].nicIp[0], portCtx));

    std::shared_ptr<TopoInfoExtractor> topoInfoExt;
    topoInfoExt.reset(new TopoInfoExtractor(collective_id_tmp, userRank, user_rank_size, TopoType::TOPO_TYPE_COMMON,
        DevType::DEV_TYPE_910B, rank_vector));

    topoInfoExt->isDiffAggregation_ = true;

    CommFactory* comm_factory = new CommFactory(collective_id_tmp, userRank, user_rank_size, dispatcher, nullptr,
        netDevCtxMap, topoInfoExt, false,
        TopoType::TOPO_TYPE_COMMON, DevType::DEV_TYPE_910B, rank_vector);

    ret = comm_factory->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    s32 mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(mem_size);
    DeviceMem outputMem = DeviceMem::alloc(mem_size);
    const string strTag = "test_tag";

    std::set<u32> targetRanks = {1,2,3,4};

    CommParaInfo commParaInfo;
    std::vector<std::unique_ptr<CommBase> > commVec;

    HcclNetCloseDev(portCtx);
    HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, 0, 0);
    delete comm_factory;

}

#if 0
TEST_F(CommFactoryTest, ut_create_comm_partial_mesh_combined_1server_16p_diff_mesh)
{
    s32 ret = HCCL_SUCCESS;

    u32 userRank = 0;
    u32 user_rank_size = 5;

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string collective_id_tmp = collectiveId;

    std::vector<RankInfo> rank_vector;
    for(u32 i = 0; i < 5; i++) {
        RankInfo tmp_para_0;
        tmp_para_0.userRank = i;
        tmp_para_0.devicePhyId = i;
        tmp_para_0.serverIdx = 1;
        tmp_para_0.deviceType = DevType::DEV_TYPE_910B;
        tmp_para_0.serverId = "10.0.0.10";
        tmp_para_0.nicIp.push_back(HcclIpAddress("192.168.0.18"));
        tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
        rank_vector.push_back(tmp_para_0);
    }

    rank_vector[1].devicePhyId = 8;
    rank_vector[2].devicePhyId = 10;
    rank_vector[3].devicePhyId = 11;
    rank_vector[4].devicePhyId = 12;
    ret = HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, 0, 0, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclNetDevCtx portCtx;
    ret = HcclNetOpenDev(&portCtx, NicType::DEVICE_NIC_TYPE, rank_vector[0].devicePhyId, rank_vector[0].devicePhyId, rank_vector[0].nicIp[0]);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    netDevCtxMap.insert(make_pair(rank_vector[0].nicIp[0], portCtx));

    std::shared_ptr<TopoInfoExtractor> topoInfoExt;
    topoInfoExt.reset(new TopoInfoExtractor(collective_id_tmp, userRank, user_rank_size, TopoType::TOPO_TYPE_COMMON,
        DevType::DEV_TYPE_910B, rank_vector));

    CommFactory* comm_factory = new CommFactory(collective_id_tmp, userRank, user_rank_size, dispatcher, nullptr,
        netDevCtxMap, topoInfoExt, false,
        TopoType::TOPO_TYPE_COMMON, DevType::DEV_TYPE_910B, rank_vector);

    ret = comm_factory->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    comm_factory->isDiffAggregation_ = true;
    comm_factory->serverNum_ = 1;
    s32 mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(mem_size);
    DeviceMem outputMem = DeviceMem::alloc(mem_size);
    const string strTag = "test_tag";

    TransportMemType inputMemType = PARAM_INPUT;
    TransportMemType outputMemType = PARAM_OUTPUT;
    std::vector<SingleSubCommTransport> commTransport;
    commTransport.clear();
    std::set<u32> commTargetUserRankSet = {1, 2, 3, 4};
    CommParaInfo commParaInfo = CommParaInfo(COMM_COMBINE, CommType::COMM_TAG_PARTIAL_MESH_COMBINED, INVALID_VALUE_RANKID,
    INVALID_VALUE_RANKID, false, false, commTargetUserRankSet);
    ret = comm_factory->CalcCommPlaneInfo(strTag, commParaInfo, commTransport, inputMemType, outputMemType);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    HcclNetCloseDev(portCtx);
    HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, 0, 0);
    delete comm_factory;
}

TEST_F(CommFactoryTest, ut_calc_comm_plane_info)
{
    s32 ret = HCCL_SUCCESS;

    u32 userRank = 0;
    u32 user_rank_size = 1;

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string collective_id_tmp = collectiveId;

    std::vector<RankInfo> rank_vector;
    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.0.0.10";
    tmp_para_0.nicIp.push_back(HcclIpAddress("192.168.0.11"));
    tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    rank_vector.push_back(tmp_para_0);

    ret = HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, 0, 0, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclNetDevCtx portCtx;
    ret = HcclNetOpenDev(&portCtx, NicType::DEVICE_NIC_TYPE, tmp_para_0.devicePhyId, tmp_para_0.devicePhyId, tmp_para_0.nicIp[0]);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    netDevCtxMap.insert(make_pair(tmp_para_0.nicIp[0], portCtx));

    std::shared_ptr<TopoInfoExtractor> topoInfoExt;
    topoInfoExt.reset(new TopoInfoExtractor(collective_id_tmp, userRank, user_rank_size, TopoType::TOPO_TYPE_1P_MESH,
        DevType::DEV_TYPE_910, rank_vector));

    CommFactory* comm_factory = new CommFactory(collective_id_tmp, userRank, user_rank_size, dispatcher, nullptr,
        netDevCtxMap, topoInfoExt, true, TopoType::TOPO_TYPE_1P_MESH, DevType::DEV_TYPE_910, rank_vector);

    ret = comm_factory->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    TransportMemType inputMemType = PARAM_INPUT;
    TransportMemType outputMemType = PARAM_OUTPUT;
    const string strTag = "test_tag";

    CommParaInfo commParaInfo;
    std::vector<SingleSubCommTransport> commTransport;

    commParaInfo = CommParaInfo(COMM_LEVEL1, CommType::COMM_TAG_RING_INNER);
    ret = comm_factory->CalcCommPlaneInfo(strTag, commParaInfo, commTransport, inputMemType, outputMemType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    commTransport.clear();
    commParaInfo = CommParaInfo(COMM_COMBINE, CommType::COMM_TAG_RING_COMBINED);
    ret = comm_factory->CalcCommPlaneInfo(strTag, commParaInfo, commTransport, inputMemType, outputMemType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    commTransport.clear();
    commParaInfo = CommParaInfo(COMM_COMBINE, CommType::COMM_TAG_WHOLE_NHR);
    ret = comm_factory->CalcCommPlaneInfo(strTag, commParaInfo, commTransport, inputMemType, outputMemType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    commTransport.clear();
    commParaInfo = CommParaInfo(COMM_COMBINE, CommType::COMM_TAG_WHOLE_NHR_V1);
    ret = comm_factory->CalcCommPlaneInfo(strTag, commParaInfo, commTransport, inputMemType, outputMemType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    commTransport.clear();
    commParaInfo = CommParaInfo(COMM_LEVEL1, CommType::COMM_TAG_HALVING_DOUBLING);
    ret = comm_factory->CalcCommPlaneInfo(strTag, commParaInfo, commTransport, inputMemType, outputMemType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    commTransport.clear();
    commParaInfo = CommParaInfo(COMM_LEVEL1, CommType::COMM_TAG_NONUNIFORM_HIERARCHICAL_RING);
    ret = comm_factory->CalcCommPlaneInfo(strTag, commParaInfo, commTransport, inputMemType, outputMemType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    commTransport.clear();
    commParaInfo = CommParaInfo(COMM_LEVEL1, CommType::COMM_TAG_NONUNIFORM_HIERARCHICAL_RING_V1);
    ret = comm_factory->CalcCommPlaneInfo(strTag, commParaInfo, commTransport, inputMemType, outputMemType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    commTransport.clear();
    commParaInfo = CommParaInfo(COMM_COMBINE, CommType::COMM_TAG_WHOLE_NB);
    ret = comm_factory->CalcCommPlaneInfo(strTag, commParaInfo, commTransport, inputMemType, outputMemType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    commTransport.clear();
    commParaInfo = CommParaInfo(COMM_LEVEL1, CommType::COMM_TAG_NONUNIFORM_BRUCK);
    ret = comm_factory->CalcCommPlaneInfo(strTag, commParaInfo, commTransport, inputMemType, outputMemType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    commTransport.clear();
    commParaInfo = CommParaInfo(COMM_LEVEL0, CommType::COMM_TAG_MESH);
    ret = comm_factory->CalcCommPlaneInfo(strTag, commParaInfo, commTransport, inputMemType, outputMemType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    commTransport.clear();
    commParaInfo = CommParaInfo(COMM_COMBINE_ORDER, CommType::COMM_TAG_MESH_COMBINED);
    ret = comm_factory->CalcCommPlaneInfo(strTag, commParaInfo, commTransport, inputMemType, outputMemType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    commTransport.clear();
    std::set<u32> commTargetUserRankSet = {0};
    commParaInfo = CommParaInfo(COMM_COMBINE, CommType::COMM_TAG_PARTIAL_MESH_COMBINED, INVALID_VALUE_RANKID,
    INVALID_VALUE_RANKID, false, false, commTargetUserRankSet);
    ret = comm_factory->CalcCommPlaneInfo(strTag, commParaInfo, commTransport, inputMemType, outputMemType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclNetCloseDev(portCtx);
    HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, tmp_para_0.devicePhyId, tmp_para_0.devicePhyId);

    delete comm_factory;
}

#endif