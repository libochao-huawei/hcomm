#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>

#define protected public
#include "comm_ring_pub.h"
#undef protected
#include "sal.h"
#include <vector>
#include "comm_factory_pub.h"
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub.h"
#include "dlra_function.h"
#include "dltdt_function.h"
#include "profiler_manager.h"
#include "network_manager_pub.h"
#include "p2p_mgmt_pub.h"
#include "externalinput.h"
#include "rank_consistentcy_checker.h"

using namespace std;
using namespace hccl;

class CommRingTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--CommRingTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--CommRingTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        MOCKER(hrtRaGetSingleSocketVnicIpInfo)
        .stubs()
        .with(any())
        .will(invoke(stub_hrtRaGetSingleSocketVnicIpInfo));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
};

typedef struct innerpara_struct_ring
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
    std::unique_ptr<NotifyPool> notifyPool;
    IntraExchanger *exchanger;
    std::vector<RankInfo> para_vector;
    std::shared_ptr<CommRing> comm_ring;
    DeviceMem inputMem = DeviceMem();
    DeviceMem outputMem = DeviceMem();
} innerpara_t_ring;

HcclDispatcher get_ring_dispatcher(s32 devid, std::shared_ptr<hccl::ProfilerManager> &profilerManager)
{
    HcclResult ret = HCCL_SUCCESS;
    ret = hrtSetDevice(devid);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    profilerManager.reset(new (std::nothrow) ProfilerManager(0, 0, 2));
    profilerManager->InitProfiler();
     // 创建dispatcher
    DevType chipType = DevType::DEV_TYPE_910;

    void *dispatcher = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, devid, &dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcher, nullptr);

    return dispatcher;
}

std::unique_ptr<NotifyPool> get_notifyPool(s32 rank)
{
    HcclResult ret = HCCL_SUCCESS;

    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    return std::move(notifyPool);
}

void* comm_ring_task_handle(void* para)
{
    RankConsistentcyChecker::GetInstance().RecordProtocolType(ProtocolType::RESERVED);
    s32 ret = HCCL_SUCCESS;
    innerpara_t_ring* para_info = (innerpara_t_ring*)para;

    hrtSetDevice(para_info->devicePhyId);
	EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, para_info->devicePhyId, para_info->devicePhyId, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclNetDevCtx portCtx;
    ret = HcclNetOpenDev(&portCtx, NicType::VNIC_TYPE, para_info->devicePhyId, para_info->devicePhyId, HcclIpAddress(para_info->devicePhyId));
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    netDevCtxMap.insert(make_pair(HcclIpAddress(para_info->devicePhyId), portCtx));

    IntraExchanger exchanger{};
    ret = CreateIntraExchanger(para_info->collectiveId, portCtx,
        para_info->devicePhyId, para_info->devicePhyId, para_info->userRank, para_info->user_rank_size, 
        para_info->device_ids, para_info->user_ranks,
        true, exchanger);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    TopoType topoFlag = TopoType::TOPO_TYPE_8P_RING;
    para_info->comm_ring.reset(new CommRing(para_info->collectiveId,
                                para_info->userRank,
                                para_info->user_rank_size,
                                para_info->rank,
                                para_info->rank_size,
                                topoFlag,
                                para_info->dispatcher, para_info->notifyPool,
                                netDevCtxMap,
                                exchanger,
                                para_info->para_vector,
                                para_info->inputMem,
                                para_info->outputMem,
                                false,
                                nullptr, 0,
                                para_info->tag));
    ret = para_info->notifyPool->RegisterOp(para_info->tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = para_info->comm_ring->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::shared_ptr<Transport>& link_rank_2 = para_info->comm_ring->GetTransportByRank(para_info->rank);
    EXPECT_EQ(link_rank_2, nullptr);

    std::shared_ptr<Transport>& link_rank_3 = para_info->comm_ring->GetTransportByRank(para_info->rank_size + 1);
    EXPECT_EQ(link_rank_3, nullptr);
    ret = para_info->notifyPool->UnregisterOp(para_info->tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclNetCloseDev(portCtx);
    HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, para_info->devicePhyId, para_info->devicePhyId);
    return (NULL);
}

TEST_F(CommRingTest, destructor_D0)
{
    std::string rootInfo = "test_collective";
    IntraExchanger exchanger{};
    std::vector<RankInfo> para_vector(1);

    TopoType topoFlag = TopoType::TOPO_TYPE_8P_RING;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    CommRing* comm_ring = new CommRing(rootInfo, 0, 1, 0, 1, topoFlag, nullptr, nullptr, netDevCtxMap, exchanger,
        para_vector, DeviceMem(), DeviceMem(), true, nullptr, 0);

    delete comm_ring;
}

TEST_F(CommRingTest, ut_comminter_init_4_thread_sameip)
{
    s32 ret = HCCL_SUCCESS;
    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();

    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.21.78.208";
    tmp_para_0.nicIp.push_back(HcclIpAddress("10.21.78.208"));
    tmp_para_0.superDeviceId = 0;
    // tmp_para_0.inputMem = DeviceMem::alloc(1024);
    // tmp_para_0.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_1;

    tmp_para_1.userRank = 1;
    tmp_para_1.devicePhyId = 1;
    tmp_para_1.deviceType = DevType::DEV_TYPE_910;
    tmp_para_1.serverIdx = 0;
    tmp_para_1.serverId = "10.21.78.208";
    tmp_para_1.nicIp.push_back(HcclIpAddress("10.21.78.207"));
    tmp_para_0.superDeviceId = 1;
    // tmp_para_1.inputMem = DeviceMem::alloc(1024);
    // tmp_para_1.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_2;

    tmp_para_2.userRank = 2;
    tmp_para_2.devicePhyId = 2;
    tmp_para_2.deviceType = DevType::DEV_TYPE_910;
    tmp_para_2.serverIdx = 0;
    tmp_para_2.serverId = "10.21.78.208";
    tmp_para_2.nicIp.push_back(HcclIpAddress("10.21.78.206"));
    tmp_para_0.superDeviceId = 2;
    // tmp_para_2.inputMem = DeviceMem::alloc(1024);
    // tmp_para_2.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_3;

    tmp_para_3.userRank = 3;
    tmp_para_3.devicePhyId = 3;
    tmp_para_3.deviceType = DevType::DEV_TYPE_910;
    tmp_para_3.serverIdx = 0;
    tmp_para_3.serverId = "10.21.78.208";
    tmp_para_3.nicIp.push_back(HcclIpAddress("10.21.78.205"));
    tmp_para_0.superDeviceId = 3;
    // tmp_para_3.inputMem = DeviceMem::alloc(1024);
    // tmp_para_3.outputMem = DeviceMem::alloc(1024);

    std::vector<RankInfo> para_vector;
    para_vector.push_back(tmp_para_0);
    para_vector.push_back(tmp_para_1);
    para_vector.push_back(tmp_para_2);
    para_vector.push_back(tmp_para_3);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    DlTdtFunction::GetInstance().DlTdtFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);

    std::string collective_id_tmp = collectiveId;

    const s32 dev_num = 4;
    s32 dev_list[dev_num] = {0, 1, 2, 3};
    std::vector<s32> device_ids(dev_list, dev_list+dev_num);

    std::vector<u32> ip_list;
    for (int i = 0;i<dev_num;i++ )
    {
        u32 ipAddr = 0;
        (void)rt_get_dev_ip(0, i, &ipAddr);
        ip_list.push_back(ipAddr);
    }

    const u32 rank_list[dev_num] = {0, 1, 2, 3};
    std::vector<u32> user_ranks(rank_list, rank_list+dev_num);

    std::shared_ptr<CommRing> comm_ring = nullptr;

    sal_thread_t tid[4];
    innerpara_t_ring para_info[4];
    s32 ndev = 4;
    std::shared_ptr<ProfilerManager> profilerManager[4];
    for (s32 i = 0; i < ndev; i++)
    {
        para_info[i].collectiveId = collective_id_tmp;
        para_info[i].userRank = i;
        para_info[i].user_rank_size = ndev;
        para_info[i].rank = i;
        para_info[i].rank_size = ndev;
        para_info[i].devicePhyId = dev_list[i];
        para_info[i].dispatcher= get_ring_dispatcher(i, profilerManager[i]);
        para_info[i].notifyPool= get_notifyPool(i);
        para_info[i].device_ids.assign(device_ids.begin(), device_ids.end());
        para_info[i].device_ips.assign(ip_list.begin(), ip_list.end());
        para_info[i].user_ranks.assign(user_ranks.begin(), user_ranks.end());
        para_info[i].tag = "st_comminter_init_4_thread_sameip";
        para_info[i].para_vector = para_vector;
        para_info[i].inputMem = DeviceMem::alloc(1024);
        para_info[i].outputMem = DeviceMem::alloc(1024);
        para_info[i].comm_ring = comm_ring;
    }

    MOCKER(&P2PMgmtPub::WaitP2PEnabled)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    tid[0] = sal_thread_create("commRing rank0 thread", comm_ring_task_handle, (void*)&para_info[0]);

    tid[1] = sal_thread_create("commRing rank1 thread", comm_ring_task_handle, (void*)&para_info[1]);

    tid[2] = sal_thread_create("commRing rank2 thread", comm_ring_task_handle, (void*)&para_info[2]);

    tid[3] = sal_thread_create("commRing rank3 thread", comm_ring_task_handle, (void*)&para_info[3]);

    while (sal_thread_is_running(tid[1]) || sal_thread_is_running(tid[2])
           || sal_thread_is_running(tid[3]) || sal_thread_is_running(tid[0]))
    {
        SaluSleep(SAL_MILLISECOND_USEC * 10);;
    }

    for (s32 j = 0; j < ndev; j++)
    {
        NetworkManager::GetInstance(dev_list[j]).Destroy();
        (void)sal_thread_destroy(tid[j]);
    }

    for(s32 j = 0; j < ndev; j++) {
        para_info[j].comm_ring.reset();
        if (para_info[j].dispatcher != nullptr) {
            ret = HcclDispatcherDestroy(para_info[j].dispatcher);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            para_info[j].dispatcher = nullptr;
        }
    }

    GlobalMockObject::verify();
}

TEST_F(CommRingTest, ut_comminter_init_5_thread)
{
    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    s32 ret = HCCL_SUCCESS;

    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.21.78.208";
    tmp_para_0.nicIp.push_back(HcclIpAddress("10.21.78.208"));
    // tmp_para_0.inputMem = DeviceMem::alloc(1024);
    // tmp_para_0.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_1;

    tmp_para_1.userRank = 1;
    tmp_para_1.devicePhyId = 1;
    tmp_para_1.deviceType = DevType::DEV_TYPE_910;
    tmp_para_1.serverIdx = 0;
    tmp_para_1.serverId = "10.21.78.208";
    tmp_para_1.nicIp.push_back(HcclIpAddress("10.21.78.207"));
    // tmp_para_1.inputMem = DeviceMem::alloc(1024);
    // tmp_para_1.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_2;

    tmp_para_2.userRank = 2;
    tmp_para_2.devicePhyId = 2;
    tmp_para_2.deviceType = DevType::DEV_TYPE_910;
    tmp_para_2.serverIdx = 0;
    tmp_para_2.serverId = "10.21.78.208";
    tmp_para_2.nicIp.push_back(HcclIpAddress("10.21.78.205"));
    // tmp_para_2.inputMem = DeviceMem::alloc(1024);
    // tmp_para_2.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_3;

    tmp_para_3.userRank = 3;
    tmp_para_3.devicePhyId = 3;
    tmp_para_3.deviceType = DevType::DEV_TYPE_910;
    tmp_para_3.serverIdx = 0;
    tmp_para_3.serverId = "10.21.78.208";
    tmp_para_3.nicIp.push_back(HcclIpAddress("10.21.78.206"));
    // tmp_para_3.inputMem = DeviceMem::alloc(1024);
    // tmp_para_3.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_4;

    tmp_para_4.userRank = 4;
    tmp_para_4.devicePhyId = 4;
    tmp_para_4.deviceType = DevType::DEV_TYPE_910;
    tmp_para_4.serverIdx = 0;
    tmp_para_4.serverId = "10.21.78.208";
    tmp_para_4.nicIp.push_back(HcclIpAddress("10.21.78.204"));
    // tmp_para_4.inputMem = DeviceMem::alloc(1024);
    // tmp_para_4.outputMem = DeviceMem::alloc(1024);

    std::vector<RankInfo> para_vector;
    para_vector.push_back(tmp_para_0);
    para_vector.push_back(tmp_para_1);
    para_vector.push_back(tmp_para_2);
    para_vector.push_back(tmp_para_3);
    para_vector.push_back(tmp_para_4);

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    DlTdtFunction::GetInstance().DlTdtFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string collective_id_tmp = collectiveId;

    const s32 dev_num = 5;
    s32 dev_list[dev_num] = {0, 1, 2, 3,4};
    std::vector<s32> device_ids(dev_list, dev_list+dev_num);

    std::vector<u32> ip_list;
    for (int i = 0;i<dev_num;i++ )
    {
        u32 ipAddr = 0;
        (void)rt_get_dev_ip(0, i, &ipAddr);
        ip_list.push_back(ipAddr);
    }

    const u32 rank_list[dev_num] = {0, 1, 2, 3, 4};
    std::vector<u32> user_ranks(rank_list, rank_list+dev_num);

    std::shared_ptr<CommRing> comm_ring = nullptr;

    sal_thread_t tid[5];
    innerpara_t_ring para_info[5];
    s32 ndev = 5;
    std::shared_ptr<ProfilerManager> profilerManager[5];
    for (s32 i = 0; i < ndev; i++)
    {
        para_info[i].collectiveId = collective_id_tmp;
        para_info[i].userRank = i;
        para_info[i].user_rank_size = ndev;
        para_info[i].rank = i;
        para_info[i].rank_size = ndev;
        para_info[i].devicePhyId = dev_list[i];
        para_info[i].dispatcher = get_ring_dispatcher(i, profilerManager[i]);
        para_info[i].notifyPool = get_notifyPool(i);
        para_info[i].device_ids.assign(device_ids.begin(), device_ids.end());
        para_info[i].device_ips.assign(ip_list.begin(), ip_list.end());
        para_info[i].user_ranks.assign(user_ranks.begin(), user_ranks.end());
        para_info[i].tag = "st_comminter_init_5_thread_diffip";
        para_info[i].para_vector = para_vector;
        para_info[i].comm_ring = comm_ring;
        para_info[i].inputMem = DeviceMem::alloc(1024);
        para_info[i].outputMem = DeviceMem::alloc(1024);
    }

    MOCKER(&P2PMgmtPub::WaitP2PEnabled)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    for(s32 j = 0; j < ndev; j++) {
        para_info[j].comm_ring.reset();
        if (para_info[j].dispatcher != nullptr) {
            ret = HcclDispatcherDestroy(para_info[j].dispatcher);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            para_info[j].dispatcher = nullptr;
        }
    }

    GlobalMockObject::verify();
}

TEST_F(CommRingTest, ut_comminter_get_sockets_per_link)
{
    // 补充覆盖率
    MOCKER(GetWorkflowMode)
    .stubs()
    .with(any())
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    setenv("HCCL_RDMA_QP_PORT_CONFIG_PATH", "/tmp/", 1);
    setenv("HCCL_RDMA_QPS_PER_CONNECTION", "5", 1);
    InitEnvVarParam();

    HcclDispatcher dispatcher;
    std::string rootInfo = "test_collective";
    IntraExchanger exchanger{};
    RankInfo rankInfo;
    rankInfo.deviceType = DevType::DEV_TYPE_910B;
    std::vector<RankInfo> para_vector(1, rankInfo);

    TopoType topoFlag = TopoType::TOPO_TYPE_8P_RING;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    u32 mem[1024] {0};
    void* transportResourceInfoAddr = mem;
    CommRing comm_ring(rootInfo, 0, 1, 0, 1, topoFlag, dispatcher, nullptr, netDevCtxMap, exchanger, para_vector, DeviceMem(), DeviceMem(), true, transportResourceInfoAddr, 1024);
    comm_ring.GetSocketsPerLink();

    unsetenv("HCCL_RDMA_QP_PORT_CONFIG_PATH");
    unsetenv("HCCL_RDMA_QPS_PER_CONNECTION");
    ResetInitState();

    setenv("HCCL_RDMA_QPS_PER_CONNECTION", "5", 1);
    InitEnvVarParam();
    comm_ring.GetSocketsPerLink();
    unsetenv("HCCL_RDMA_QPS_PER_CONNECTION");
    ResetInitState();
}