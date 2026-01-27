#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub.h"

#include "comm_halving_doubling_pub.h"
#include "sal.h"
#include "dlra_function.h"
#include <vector>
#include "comm_factory_pub.h"
#include "profiler_manager.h"
#include "network_manager_pub.h"

using namespace std;
using namespace hccl;


class CommBinaryBlocksHDTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--CommBinaryBlocksHDTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--CommBinaryBlocksHDTest TearDown--\033[0m" << std::endl;
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
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};

class CommBinaryBlocksHDTmp : public CommHalvingDoubling
{
public:
    explicit CommBinaryBlocksHDTmp(const std::string& collectiveId,
            const u32 userRank,
            const u32 user_rank_size,
            const u32 rank,
            const u32 rank_size,
            const TopoType topoFlag,
            const HcclDispatcher dispatcher,
            std::map<HcclIpAddress, HcclNetDevCtx> &netDevCtxMap,
            const IntraExchanger &exchanger,
            const std::vector<RankInfo> para_vector,
            const DeviceMem& inputMem, const DeviceMem& outputMem,
            const u64 comm_attribute = 0,
            const std::string& tag = "");
    virtual ~CommBinaryBlocksHDTmp();
    std::vector<std::shared_ptr<Transport> > link_info_hd_;  // 当前rank与其他rank对应的link信息
    std::unique_ptr<Transport> get_link()
    {
        MachinePara machinePara;
        std::chrono::milliseconds timeout;
        const std::string tag;

        std::unique_ptr<Transport> link = nullptr;
        link.reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));

        return link;
    }

    HcclResult set_err_type_link()
    {
        // link_info_hd_[0] = get_link();
        transportType_[0] = TransportType::TRANS_TYPE_RESERVED;
        return HCCL_SUCCESS;
    }
    DispatcherPub *dispatcher;

};

    CommBinaryBlocksHDTmp::CommBinaryBlocksHDTmp(const std::string& collectiveId,
            const u32 userRank,
            const u32 user_rank_size,
            const u32 rank,
            const u32 rank_size,
            const TopoType topoFlag,
            const HcclDispatcher dispatcher,
            std::map<HcclIpAddress, HcclNetDevCtx> &netDevCtxMap,
            const IntraExchanger &exchanger,
            const std::vector<RankInfo> para_vector,
            const DeviceMem& inputMem, const DeviceMem& outputMem,
            const u64 comm_attribute,
            const std::string& tag)
        : CommHalvingDoubling(collectiveId, userRank, user_rank_size, rank, rank_size, topoFlag, dispatcher, nullptr,
            netDevCtxMap, exchanger, para_vector, inputMem, outputMem, false, nullptr, 0, tag), dispatcher(reinterpret_cast<DispatcherPub*>(dispatcher))
    {
    }

    CommBinaryBlocksHDTmp::~CommBinaryBlocksHDTmp()
    {
    }

typedef struct innerpara_struct_hd
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
    IntraExchanger *exchanger;
    std::vector<RankInfo> para_vector;
    DeviceMem inputMem = DeviceMem();
    DeviceMem outputMem = DeviceMem();
    std::shared_ptr<CommBinaryBlocksHDTmp> comm_binary_blocks_H_D;
} innerpara_t_hd;

HcclDispatcher get_H_D_dispatcher(s32 devid, std::shared_ptr<hccl::ProfilerManager> &profilerManager)
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

void* comm_binary_blocks_H_D_task_handle(void* para)
{
    HcclResult ret = HCCL_SUCCESS;
    innerpara_t_hd* para_info = (innerpara_t_hd*)para;

    IntraExchanger exchanger {};

     //不需要exchanger
    //ret = exchanger->init();
    //EXPECT_EQ(ret, HCCL_SUCCESS);
    
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    TopoType topoFlag = TopoType::TOPO_TYPE_8P_RING;
    para_info->comm_binary_blocks_H_D.reset(new CommBinaryBlocksHDTmp(para_info->collectiveId,
                                para_info->userRank,
                                para_info->user_rank_size,
                                para_info->rank,
                                para_info->rank_size,
                                topoFlag,
                                para_info->dispatcher,
                                netDevCtxMap,
                                exchanger,
                                para_info->para_vector,
                                para_info->inputMem,
                                para_info->outputMem));

    //ret = para_info->comm_binary_blocks_H_D->Init();
    //EXPECT_EQ(ret, HCCL_SUCCESS);
    return (NULL);
}

TEST_F(CommBinaryBlocksHDTest, destructor_D0)
{
    std::string rootInfo = "test_collective";
    IntraExchanger exchanger {};
    std::vector<RankInfo> para_vector(1);

    TopoType topoFlag = TopoType::TOPO_TYPE_8P_RING;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    CommHalvingDoubling* comm_binary_blocks_H_D = new CommHalvingDoubling(rootInfo, 0, 1, 0, 1, topoFlag, nullptr, nullptr,
        netDevCtxMap, exchanger, para_vector, DeviceMem(), DeviceMem(), true, nullptr, 0);

    delete comm_binary_blocks_H_D;
}

TEST_F(CommBinaryBlocksHDTest, destructor_err)
{
    HcclResult ret = HCCL_SUCCESS;

    std::string rootInfo = "test_collective";
    IntraExchanger exchanger {};
    std::vector<RankInfo> para_vector(1);

    TopoType topoFlag = TopoType::TOPO_TYPE_8P_RING;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    CommBinaryBlocksHDTmp* comm_halving_doubling_tmp = new CommBinaryBlocksHDTmp(rootInfo, 0, 1, 0, 1, topoFlag, nullptr,
        netDevCtxMap, exchanger, para_vector, DeviceMem(), DeviceMem(), true);
    ret = comm_halving_doubling_tmp->set_err_type_link();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete comm_halving_doubling_tmp;
}


TEST_F(CommBinaryBlocksHDTest, ut_comm_B_B_H_D_init_3_thread_sameip)
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
    tmp_para_2.nicIp.push_back(HcclIpAddress("10.21.78.206"));
    // tmp_para_2.inputMem = DeviceMem::alloc(1024);
    // tmp_para_2.outputMem = DeviceMem::alloc(1024);

    std::vector<RankInfo> para_vector;
    para_vector.push_back(tmp_para_0);
    para_vector.push_back(tmp_para_1);
    para_vector.push_back(tmp_para_2);

    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string collective_id_tmp = collectiveId;

    const s32 dev_num = 3;
    s32 dev_list[dev_num] = {0, 1, 2};
    std::vector<s32> device_ids(dev_list, dev_list+dev_num);

    std::vector<u32> ip_list;
    for (int i = 0;i<dev_num;i++ )
    {
        u32 ipAddr = 0;
        (void)rt_get_dev_ip(0, i, &ipAddr);
        ip_list.push_back(ipAddr);
    }

    const u32 rank_list[dev_num] = {0, 1, 2};
    std::vector<u32> user_ranks(rank_list, rank_list+dev_num);

    std::shared_ptr<CommBinaryBlocksHDTmp> comm_binary_blocks_H_D = nullptr;

    sal_thread_t tid[3];
    innerpara_t_hd para_info[3];
    s32 ndev = 3;
    std::shared_ptr<ProfilerManager> profilerManager[3] = {nullptr};
    for (s32 i = 0; i < ndev; i++)
    {
        para_info[i].collectiveId = collective_id_tmp;
        para_info[i].userRank = i;
        para_info[i].user_rank_size = ndev;
        para_info[i].rank = i;
        para_info[i].rank_size = ndev;
        para_info[i].devicePhyId = dev_list[i];
        para_info[i].dispatcher= get_H_D_dispatcher(i, profilerManager[i]);
        para_info[i].device_ids.assign(device_ids.begin(), device_ids.end());
        para_info[i].device_ips.assign(ip_list.begin(), ip_list.end());
        para_info[i].user_ranks.assign(user_ranks.begin(), user_ranks.end());
        para_info[i].tag = "ut_comm_B_B_H_D_init_3_thread_sameip";
        para_info[i].para_vector = para_vector;
        para_info[i].inputMem = DeviceMem::alloc(1024);
        para_info[i].outputMem = DeviceMem::alloc(1024);
        para_info[i].comm_binary_blocks_H_D = comm_binary_blocks_H_D;
    }

    tid[0] = sal_thread_create("HalvingDoubling rank0 thread", comm_binary_blocks_H_D_task_handle, (void*)&para_info[0]);

    tid[1] = sal_thread_create("HalvingDoubling rank1 thread", comm_binary_blocks_H_D_task_handle, (void*)&para_info[1]);

    tid[2] = sal_thread_create("HalvingDoubling rank2 thread", comm_binary_blocks_H_D_task_handle, (void*)&para_info[2]);

    while (sal_thread_is_running(tid[1]) || sal_thread_is_running(tid[2])
           || sal_thread_is_running(tid[0]))
    {
        SaluSleep(SAL_MILLISECOND_USEC * 10);
        NetworkManager::GetInstance(dev_list[0]).Destroy();
        NetworkManager::GetInstance(dev_list[1]).Destroy();
        NetworkManager::GetInstance(dev_list[2]).Destroy();
    }
    for (s32 i = 0; i < ndev; i++)
    {
        if (para_info[i].dispatcher != nullptr) {
            ret = HcclDispatcherDestroy(para_info[i].dispatcher);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            para_info[i].dispatcher = nullptr;
        }
    }
}

TEST_F(CommBinaryBlocksHDTest, ut_comm_B_B_H_D_init_13_thread_sameip)
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

    RankInfo tmp_para_1;

    tmp_para_1.userRank = 1;
    tmp_para_1.devicePhyId = 1;
    tmp_para_1.deviceType = DevType::DEV_TYPE_910;
    tmp_para_1.serverIdx = 0;
    tmp_para_1.serverId = "10.21.78.208";
    tmp_para_1.nicIp.push_back(HcclIpAddress("10.21.78.206"));
    // tmp_para_1.inputMem = DeviceMem::alloc(1024);
    // tmp_para_1.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_2;

    tmp_para_2.userRank = 2;
    tmp_para_2.deviceType = DevType::DEV_TYPE_910;
    tmp_para_2.devicePhyId = 2;
    tmp_para_2.serverIdx = 0;
    tmp_para_2.serverId = "10.21.78.208";
    tmp_para_2.nicIp.push_back(HcclIpAddress("10.21.78.207"));
    // tmp_para_2.inputMem = DeviceMem::alloc(1024);
    // tmp_para_2.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_3;

    tmp_para_3.userRank = 3;
    tmp_para_3.deviceType = DevType::DEV_TYPE_910;
    tmp_para_3.devicePhyId = 3;
    tmp_para_3.serverIdx = 0;
    tmp_para_3.serverId = "10.21.78.208";
    tmp_para_3.nicIp.push_back(HcclIpAddress("10.21.78.204"));
    // tmp_para_3.inputMem = DeviceMem::alloc(1024);
    // tmp_para_3.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_4;

    tmp_para_4.userRank = 4;
    tmp_para_4.deviceType = DevType::DEV_TYPE_910;
    tmp_para_4.devicePhyId = 4;
    tmp_para_4.serverIdx = 0;
    tmp_para_4.serverId = "10.21.78.208";
    tmp_para_4.nicIp.push_back(HcclIpAddress("10.21.78.205"));
    // tmp_para_4.inputMem = DeviceMem::alloc(1024);
    // tmp_para_4.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_5;

    tmp_para_5.userRank = 5;
    tmp_para_5.deviceType = DevType::DEV_TYPE_910;
    tmp_para_5.devicePhyId = 5;
    tmp_para_5.serverIdx = 0;
    tmp_para_5.serverId = "10.21.78.208";
    tmp_para_5.nicIp.push_back(HcclIpAddress("10.21.78.203"));
    // tmp_para_5.inputMem = DeviceMem::alloc(1024);
    // tmp_para_5.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_6;
    tmp_para_6.userRank = 6;
    tmp_para_6.deviceType = DevType::DEV_TYPE_910;
    tmp_para_6.devicePhyId = 6;
    tmp_para_6.serverIdx = 0;
    tmp_para_6.serverId = "10.21.78.208";
    tmp_para_6.nicIp.push_back(HcclIpAddress("10.21.78.202"));
    // tmp_para_6.inputMem = DeviceMem::alloc(1024);
    // tmp_para_6.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_7;
    tmp_para_7.userRank = 7;
    tmp_para_7.deviceType = DevType::DEV_TYPE_910;
    tmp_para_7.devicePhyId = 7;
    tmp_para_7.serverIdx = 0;
    tmp_para_7.serverId = "10.21.78.208";
    tmp_para_7.nicIp.push_back(HcclIpAddress("10.21.78.201"));
    // tmp_para_7.inputMem = DeviceMem::alloc(1024);
    // tmp_para_7.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_8;
    tmp_para_8.userRank = 8;
    tmp_para_8.deviceType = DevType::DEV_TYPE_910;
    tmp_para_8.devicePhyId = 8;
    tmp_para_8.serverIdx = 0;
    tmp_para_8.serverId = "10.21.78.208";
    tmp_para_8.nicIp.push_back(HcclIpAddress("10.21.78.200"));
    // tmp_para_8.inputMem = DeviceMem::alloc(1024);
    // tmp_para_8.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_9;
    tmp_para_9.userRank = 9;
    tmp_para_9.deviceType = DevType::DEV_TYPE_910;
    tmp_para_9.devicePhyId = 9;
    tmp_para_9.serverIdx = 0;
    tmp_para_9.serverId = "10.21.78.208";
    tmp_para_9.nicIp.push_back(HcclIpAddress("10.21.78.218"));
    // tmp_para_9.inputMem = DeviceMem::alloc(1024);
    // tmp_para_9.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_10;
    tmp_para_10.userRank = 10;
    tmp_para_10.deviceType = DevType::DEV_TYPE_910;
    tmp_para_10.devicePhyId = 10;
    tmp_para_10.serverIdx = 0;
    tmp_para_10.serverId = "10.21.78.208";
    tmp_para_10.nicIp.push_back(HcclIpAddress("10.21.78.228"));
    // tmp_para_10.inputMem = DeviceMem::alloc(1024);
    // tmp_para_10.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_11;
    tmp_para_11.deviceType = DevType::DEV_TYPE_910;
    tmp_para_11.userRank = 11;
    tmp_para_11.devicePhyId = 11;
    tmp_para_11.serverIdx = 0;
    tmp_para_11.serverId = "10.21.78.208";
    tmp_para_11.nicIp.push_back(HcclIpAddress("10.21.78.219"));
    // tmp_para_11.inputMem = DeviceMem::alloc(1024);
    // tmp_para_11.outputMem = DeviceMem::alloc(1024);

    RankInfo tmp_para_12;
    tmp_para_12.deviceType = DevType::DEV_TYPE_910;
    tmp_para_12.userRank = 12;
    tmp_para_12.devicePhyId = 12;
    tmp_para_12.serverIdx = 0;
    tmp_para_12.serverId = "10.21.78.208";
    tmp_para_12.nicIp.push_back(HcclIpAddress("10.21.78.229"));
    // tmp_para_12.inputMem = DeviceMem::alloc(1024);
    // tmp_para_12.outputMem = DeviceMem::alloc(1024);

    std::vector<RankInfo> para_vector;
    para_vector.push_back(tmp_para_0);
    para_vector.push_back(tmp_para_1);
    para_vector.push_back(tmp_para_2);
    para_vector.push_back(tmp_para_3);
    para_vector.push_back(tmp_para_4);
    para_vector.push_back(tmp_para_5);
    para_vector.push_back(tmp_para_6);
    para_vector.push_back(tmp_para_7);
    para_vector.push_back(tmp_para_8);
    para_vector.push_back(tmp_para_9);
    para_vector.push_back(tmp_para_10);
    para_vector.push_back(tmp_para_11);
    para_vector.push_back(tmp_para_12);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    char collectiveId[SAL_UNIQUE_ID_BYTES];
    ret = SalGetUniqueId(collectiveId);

    std::string collective_id_tmp = collectiveId;

    const s32 dev_num = 13;
    s32 dev_list[dev_num] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    std::vector<s32> device_ids(dev_list, dev_list+dev_num);

    std::vector<u32> ip_list;
    for (int i = 0;i<dev_num;i++ )
    {
        u32 ipAddr = 0;
        (void)rt_get_dev_ip(0, i, &ipAddr);
        ip_list.push_back(ipAddr);
    }

    const u32 rank_list[dev_num] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    std::vector<u32> user_ranks(rank_list, rank_list+dev_num);

    std::shared_ptr<CommBinaryBlocksHDTmp> comm_binary_blocks_H_D = nullptr;

    sal_thread_t tid[13];
    innerpara_t_hd para_info[13];
    s32 ndev = 13;
    std::shared_ptr<ProfilerManager> profilerManager[13];
    for (s32 i = 0; i < ndev; i++)
    {
        para_info[i].collectiveId = collective_id_tmp;
        para_info[i].userRank = i;
        para_info[i].user_rank_size = ndev;
        para_info[i].rank = i;
        para_info[i].rank_size = ndev;
        para_info[i].dispatcher = get_H_D_dispatcher(i, profilerManager[i]);
        para_info[i].device_ids.assign(device_ids.begin(), device_ids.end());
        para_info[i].device_ips.assign(ip_list.begin(), ip_list.end());
        para_info[i].user_ranks.assign(user_ranks.begin(), user_ranks.end());
        para_info[i].tag = "ut_comm_B_B_H_D_init_13_thread_sameip";
        para_info[i].para_vector = para_vector;
        para_info[i].inputMem = DeviceMem::alloc(1024);
        para_info[i].outputMem = DeviceMem::alloc(1024);
        para_info[i].comm_binary_blocks_H_D = comm_binary_blocks_H_D;
    }

    tid[0] = sal_thread_create("HalvingDoubling rank0 thread", comm_binary_blocks_H_D_task_handle, (void*)&para_info[0]);

    tid[1] = sal_thread_create("HalvingDoubling rank1 thread", comm_binary_blocks_H_D_task_handle, (void*)&para_info[1]);

    tid[2] = sal_thread_create("HalvingDoubling rank2 thread", comm_binary_blocks_H_D_task_handle, (void*)&para_info[2]);

    tid[3] = sal_thread_create("HalvingDoubling rank3 thread", comm_binary_blocks_H_D_task_handle, (void*)&para_info[3]);

    tid[4] = sal_thread_create("HalvingDoubling rank4 thread", comm_binary_blocks_H_D_task_handle, (void*)&para_info[4]);

    tid[5] = sal_thread_create("HalvingDoubling rank5 thread", comm_binary_blocks_H_D_task_handle, (void*)&para_info[5]);

    tid[6] = sal_thread_create("HalvingDoubling rank6 thread", comm_binary_blocks_H_D_task_handle, (void*)&para_info[6]);

    tid[7] = sal_thread_create("HalvingDoubling rank7 thread", comm_binary_blocks_H_D_task_handle, (void*)&para_info[7]);

    tid[8] = sal_thread_create("HalvingDoubling rank8 thread", comm_binary_blocks_H_D_task_handle, (void*)&para_info[8]);

    tid[9] = sal_thread_create("HalvingDoubling rank9 thread", comm_binary_blocks_H_D_task_handle, (void*)&para_info[9]);

    tid[10] = sal_thread_create("HalvingDoubling rank10 thread", comm_binary_blocks_H_D_task_handle, (void*)&para_info[10]);

    tid[11] = sal_thread_create("HalvingDoubling rank11 thread", comm_binary_blocks_H_D_task_handle, (void*)&para_info[11]);

    tid[12] = sal_thread_create("HalvingDoubling rank12 thread", comm_binary_blocks_H_D_task_handle, (void*)&para_info[12]);

    while (sal_thread_is_running(tid[0]) || sal_thread_is_running(tid[1])|| sal_thread_is_running(tid[2])
           || sal_thread_is_running(tid[3]) || sal_thread_is_running(tid[4])|| sal_thread_is_running(tid[5])
           || sal_thread_is_running(tid[6]) || sal_thread_is_running(tid[7])|| sal_thread_is_running(tid[8])
           || sal_thread_is_running(tid[9]) || sal_thread_is_running(tid[10])|| sal_thread_is_running(tid[11])
           || sal_thread_is_running(tid[12]))
    {
        SaluSleep(SAL_MILLISECOND_USEC * 10);;
    }

    for (s32 j = 0; j < ndev; j++)
    {
        (void)sal_thread_destroy(tid[j]);
        NetworkManager::GetInstance(dev_list[j]).Destroy();
        if (para_info[j].dispatcher != nullptr) {
            ret = HcclDispatcherDestroy(para_info[j].dispatcher);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            para_info[j].dispatcher = nullptr;
        }
    }
}

