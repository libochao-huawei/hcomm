#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>

#define protected public
#include "comm_mesh_pub.h"
#undef protected
#include "sal.h"
#include <vector>
#include "comm_factory_pub.h"
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub.h"
#include "dlra_function.h"
#include "profiler_manager.h"
#include "network_manager_pub.h"
#include "hccl_network_pub.h"
#include "p2p_mgmt_pub.h"
#include "rank_consistentcy_checker.h"

using namespace std;
using namespace hccl;

class CommMeshTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--CommMeshTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--CommMeshTest TearDown--\033[0m" << std::endl;
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
    	MOCKER(hrtRaGetSingleSocketVnicIpInfo)
        .stubs()
        .with(any())
        .will(invoke(stub_hrtRaGetSingleSocketVnicIpInfo));
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
        GlobalMockObject::verify();
    }
    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;

};
HcclDispatcher CommMeshTest::dispatcherPtr = nullptr;
DispatcherPub *CommMeshTest::dispatcher = nullptr;

typedef struct innerpara_struct_mesh
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
    DeviceMem inputMem = DeviceMem();
    DeviceMem outputMem = DeviceMem();
    std::shared_ptr<CommMesh> comm_mesh;
} innerpara_t_mesh;

HcclDispatcher get_mesh_dispatcher(s32 devid, std::shared_ptr<hccl::ProfilerManager> &profilerManager)
{
    HcclResult ret = HCCL_SUCCESS;
    ret = hrtSetDevice(devid);
    EXPECT_EQ(ret, HCCL_SUCCESS);
     // 创建dispatcher
    DevType chipType = DevType::DEV_TYPE_910;
    profilerManager.reset(new (std::nothrow) ProfilerManager(0, 0, 2));
    profilerManager->InitProfiler();
        void *dispatcher = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, devid, &dispatcher);
    EXPECT_NE(dispatcher, nullptr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    return dispatcher;
}

void* comm_mesh_task_handle(void* para)
{
    RankConsistentcyChecker::GetInstance().RecordProtocolType(ProtocolType::RESERVED);
    s32 ret = HCCL_SUCCESS;
    innerpara_t_mesh* para_info = (innerpara_t_mesh*)para;
    hrtSetDevice(para_info->devicePhyId);


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
    para_info->comm_mesh.reset(new CommMesh(para_info->collectiveId,
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

    ret = para_info->comm_mesh->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = para_info->notifyPool->UnregisterOp(para_info->tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclNetCloseDev(portCtx);
    HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, para_info->devicePhyId, para_info->devicePhyId);
    return (NULL);
}

TEST_F(CommMeshTest, destructor_D0)
{
    std::string rootInfo = "test_collective";
    IntraExchanger exchanger{};
    std::vector<RankInfo> para_vector(1);

    TopoType topoFlag = TopoType::TOPO_TYPE_8P_RING;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    CommMesh* comm_mesh = new CommMesh(rootInfo, 0, 1, 0, 1, topoFlag, nullptr, nullptr, netDevCtxMap, exchanger, para_vector, DeviceMem(), DeviceMem(), false, nullptr, 0, "");

    delete comm_mesh;
}

TEST_F(CommMeshTest, init)
{
    s32 ret = HCCL_SUCCESS;

    u32 userRank = 1;
    u32 user_rank_size = 5;

    RankInfo tmp_para;



    tmp_para.userRank = userRank;

    s32 device_id = 0;
    ret = hrtGetDevice(&device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    tmp_para.devicePhyId = device_id;

    tmp_para.serverIdx = 0;
    tmp_para.serverId = "10.21.78.208";
    tmp_para.nicIp.push_back(HcclIpAddress("10.21.78.208"));
    // tmp_para.inputMem = DeviceMem::alloc(1024);
    // tmp_para.outputMem = DeviceMem::alloc(1024);

    std::vector<RankInfo> para_vector;
    para_vector.push_back(tmp_para);

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string collective_id_tmp = collectiveId;
    IntraExchanger exchanger{};

    TopoType topoFlag = TopoType::TOPO_TYPE_8P_RING;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    CommMesh* comm_mesh = new CommMesh(collective_id_tmp, userRank, user_rank_size, 0, 1, topoFlag, dispatcher, nullptr, netDevCtxMap, exchanger, para_vector, DeviceMem(), DeviceMem(), false, nullptr, 0, "");

    ret = comm_mesh->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete comm_mesh;
}

extern std::unique_ptr<NotifyPool> get_notifyPool(s32 rank);
TEST_F(CommMeshTest, ut_comminter_init_4_thread)
{
    s32 ret = HCCL_SUCCESS;

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

    RankInfo tmp_para;

    tmp_para.userRank = 1;
    tmp_para.devicePhyId = 1;
    tmp_para.deviceType = DevType::DEV_TYPE_910;
    tmp_para.serverIdx = 0;
    tmp_para.serverId = "10.21.78.208";
    tmp_para.nicIp.push_back(HcclIpAddress("10.21.78.209"));
    tmp_para_0.superDeviceId = 1;
    // tmp_para.inputMem = DeviceMem::alloc(1024);
    // tmp_para.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_2;

    tmp_para_2.userRank = 2;
    tmp_para_2.devicePhyId = 2;
    tmp_para_2.deviceType = DevType::DEV_TYPE_910;
    tmp_para_2.serverIdx = 0;
    tmp_para_2.serverId = "10.21.78.208";
    tmp_para_2.nicIp.push_back(HcclIpAddress("10.21.78.218"));
    tmp_para_0.superDeviceId = 2;
    // tmp_para_2.inputMem = DeviceMem::alloc(1024);
    // tmp_para_2.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_3;

    tmp_para_3.userRank = 3;
    tmp_para_3.devicePhyId = 3;
    tmp_para_3.deviceType = DevType::DEV_TYPE_910;
    tmp_para_3.serverIdx = 0;
    tmp_para_3.serverId = "10.21.78.208";
    tmp_para_3.nicIp.push_back(HcclIpAddress("10.21.78.218"));
    tmp_para_0.superDeviceId = 3;
    // tmp_para_3.inputMem = DeviceMem::alloc(1024);
    // tmp_para_3.outputMem = DeviceMem::alloc(1024);

    std::vector<RankInfo> para_vector;
    para_vector.push_back(tmp_para_0);
    para_vector.push_back(tmp_para);
    para_vector.push_back(tmp_para_2);
    para_vector.push_back(tmp_para_3);

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
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

    std::shared_ptr<CommMesh> comm_mesh = nullptr;

    sal_thread_t tid[4];
    innerpara_t_mesh para_info[4];
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
        para_info[i].dispatcher = get_mesh_dispatcher(i, profilerManager[i]);
        para_info[i].notifyPool = get_notifyPool(i);
        para_info[i].device_ids.assign(device_ids.begin(), device_ids.end());
        para_info[i].device_ips.assign(ip_list.begin(), ip_list.end());
        para_info[i].user_ranks.assign(user_ranks.begin(), user_ranks.end());
        para_info[i].tag = "ut_comminter_init_4_thread";
        para_info[i].para_vector = para_vector;
        para_info[i].inputMem = DeviceMem::alloc(1024);
        para_info[i].outputMem = DeviceMem::alloc(1024);
        para_info[i].comm_mesh = comm_mesh;
    }

    MOCKER(&P2PMgmtPub::WaitP2PEnabled)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    tid[0] = sal_thread_create("commMesh rank0 thread", comm_mesh_task_handle, (void*)&para_info[0]);

    tid[1] = sal_thread_create("commMesh rank1 thread", comm_mesh_task_handle, (void*)&para_info[1]);

    tid[2] = sal_thread_create("commMesh rank2 thread", comm_mesh_task_handle, (void*)&para_info[2]);

    tid[3] = sal_thread_create("commMesh rank3 thread", comm_mesh_task_handle, (void*)&para_info[3]);

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
        para_info[j].comm_mesh.reset();
        if (para_info[j].dispatcher != nullptr) {
            ret = HcclDispatcherDestroy(para_info[j].dispatcher);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            para_info[j].dispatcher = nullptr;
        }
    }
    GlobalMockObject::verify();
}

TEST_F(CommMeshTest, ut_comminter_init_5_thread)
{
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

    RankInfo tmp_para;

    tmp_para.userRank = 1;
    tmp_para.devicePhyId = 1;
    tmp_para.deviceType = DevType::DEV_TYPE_910;
    tmp_para.serverIdx = 0;
    tmp_para.serverId = "10.21.78.208";
    tmp_para.nicIp.push_back(HcclIpAddress("10.21.78.207"));
    // tmp_para.inputMem = DeviceMem::alloc(1024);
    // tmp_para.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_2;

    tmp_para_2.userRank = 2;
    tmp_para_2.devicePhyId = 2;
    tmp_para_2.deviceType = DevType::DEV_TYPE_910;
    tmp_para_2.serverIdx = 0;
    tmp_para_2.serverId = "10.21.78.208";
    tmp_para_2.nicIp.push_back(HcclIpAddress("10.21.78.206"));
    // tmp_para_2.inputMem = DeviceMem::alloc(1024);
    // tmp_para_2.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_3;

    tmp_para_3.userRank = 3;
    tmp_para_3.deviceType = DevType::DEV_TYPE_910;
    tmp_para_3.devicePhyId = 3;
    tmp_para_3.serverIdx = 0;
    tmp_para_3.serverId = "10.21.78.208";
    tmp_para_3.nicIp.push_back(HcclIpAddress("10.21.78.205"));
    // tmp_para_3.inputMem = DeviceMem::alloc(1024);
    // tmp_para_3.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_4;

    tmp_para_4.userRank = 4;
    tmp_para_4.deviceType = DevType::DEV_TYPE_910;
    tmp_para_4.devicePhyId = 4;
    tmp_para_4.serverIdx = 0;
    tmp_para_4.serverId = "10.21.78.208";
    tmp_para_4.nicIp.push_back(HcclIpAddress("10.21.78.204"));
    // tmp_para_4.inputMem = DeviceMem::alloc(1024);
    // tmp_para_4.outputMem = DeviceMem::alloc(1024);

    std::vector<RankInfo> para_vector;
    para_vector.push_back(tmp_para_0);
    para_vector.push_back(tmp_para);
    para_vector.push_back(tmp_para_2);
    para_vector.push_back(tmp_para_3);
    para_vector.push_back(tmp_para_4);

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);

    std::string collective_id_tmp = collectiveId;

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    const s32 dev_num = 5;
    s32 dev_list[dev_num] = {0, 1, 2, 3, 4};
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

    std::shared_ptr<CommMesh> comm_mesh = nullptr;

    sal_thread_t tid[5];
    innerpara_t_mesh para_info[5];
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
        para_info[i].dispatcher = get_mesh_dispatcher(i, profilerManager[i]);
        para_info[i].notifyPool = get_notifyPool(i);
        para_info[i].device_ids.assign(device_ids.begin(), device_ids.end());
        para_info[i].device_ips.assign(ip_list.begin(), ip_list.end());
        para_info[i].user_ranks.assign(user_ranks.begin(), user_ranks.end());
        para_info[i].tag = "ut_comminter_init_5_thread";
        para_info[i].para_vector = para_vector;
        para_info[i].inputMem = DeviceMem::alloc(1024);
        para_info[i].outputMem = DeviceMem::alloc(1024);
        para_info[i].comm_mesh = comm_mesh;
    }

    MOCKER(&P2PMgmtPub::WaitP2PEnabled)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    tid[0] = sal_thread_create("commMesh rank0 thread", comm_mesh_task_handle, (void*)&para_info[0]);

    tid[1] = sal_thread_create("commMesh rank1 thread", comm_mesh_task_handle, (void*)&para_info[1]);

    tid[2] = sal_thread_create("commMesh rank2 thread", comm_mesh_task_handle, (void*)&para_info[2]);

    tid[3] = sal_thread_create("commMesh rank3 thread", comm_mesh_task_handle, (void*)&para_info[3]);

    tid[4] = sal_thread_create("commMesh rank4 thread", comm_mesh_task_handle, (void*)&para_info[4]);

    while (sal_thread_is_running(tid[1]) || sal_thread_is_running(tid[2])
           || sal_thread_is_running(tid[3]) || sal_thread_is_running(tid[0])
           || sal_thread_is_running(tid[4]))
    {
        SaluSleep(SAL_MILLISECOND_USEC * 10);;
    }

    for (s32 j = 0; j < ndev; j++)
    {
        NetworkManager::GetInstance(dev_list[j]).Destroy();
        (void)sal_thread_destroy(tid[j]);
    }

    for(s32 j = 0; j < ndev; j++) {
        para_info[j].comm_mesh.reset();
        if (para_info[j].dispatcher != nullptr) {
            ret = HcclDispatcherDestroy(para_info[j].dispatcher);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            para_info[j].dispatcher = nullptr;
        }
    }
    GlobalMockObject::verify();
}

HcclResult StupIsSupportAicpuNormalQP(const u32& devicePhyId, bool &isSupportNormalQP)
{
    isSupportNormalQP = devicePhyId >= 0 ? true: false;
    return HCCL_SUCCESS;
}

TEST_F(CommMeshTest, ut_set_machinePara)
{
    s32 ret = HCCL_SUCCESS;

    u32 userRank = 1;
    u32 user_rank_size = 5;

    RankInfo tmp_para;

    tmp_para.userRank = userRank;
    DeviceMem inputMem = DeviceMem::alloc(128*3);
    DeviceMem outputMem = DeviceMem::alloc(128*3);

    s32 device_id = 0;
    ret = hrtGetDevice(&device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    tmp_para.devicePhyId = device_id;

    tmp_para.serverId = "10.21.78.208";
    tmp_para.nicIp.push_back(HcclIpAddress("10.21.78.208"));

    std::vector<RankInfo> para_vector;
    para_vector.push_back(tmp_para);

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string collective_id_tmp = collectiveId;

    IntraExchanger exchanger{};

    TopoType topoFlag = TopoType::TOPO_TYPE_8P_RING;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    CommMesh* comm_mesh = new CommMesh(collective_id_tmp, userRank, user_rank_size, 0, 1, topoFlag, nullptr, nullptr, netDevCtxMap, exchanger,
        para_vector, inputMem, outputMem, true, nullptr, 0, "tag_" + HCCL_MC2_MULTISERVER_SUFFIX);

    ret = comm_mesh->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    const MachineType machineType = MachineType::MACHINE_SERVER_TYPE;
    const u32 dstRank = 0;
    std::vector<std::shared_ptr<HcclSocket>> socketList;
    MachinePara machinePara;

    MOCKER(IsSupportAicpuNormalQP).stubs().with(any()).will(invoke(StupIsSupportAicpuNormalQP));
    
    ret = comm_mesh->SetMachinePara(machineType, tmp_para.serverId, dstRank, socketList, machinePara);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete comm_mesh;
    GlobalMockObject::verify();
}