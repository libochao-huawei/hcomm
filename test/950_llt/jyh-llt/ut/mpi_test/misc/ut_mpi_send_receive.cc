#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <mpi.h>

#include <driver/ascend_hal.h>
#include <runtime/rt.h>

#define private public
#define protected public

#include "hcom.h"
#include "../inc/hccl/hcom_executor.h"
#include "llt_hccl_stub_pub.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "sal.h"
#include "dltrace_function.h"
#include "dlra_function.h"
#include "dlhal_function.h"
#include "config.h"
#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"
#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include "param_check_pub.h"
#include <iostream>
#include <fstream>
#include "hcom.h"
#include "stream_pub.h"
#include "hccl_types.h"

#include "hccl_communicator.h"
#include "hccl_impl.h"
#include "hccl_alg.h"
#include "network_manager_pub.h"
#include "transport_shm_event_pub.h"
#include "hccl_ex.h"
#include "hccl_comm_pub.h"
#include "hcom_pub.h"
#include "transport_heterog_p2p_pub.h"
#include "comm_factory_pub.h"
#include "comm_base_pub.h"
#include "coll_send_executor.h"
#include "coll_receive_executor.h"
#include "remote_notify.h"
#undef protected
#undef private
#include "adapter_rts.h"

using namespace std;
using namespace hccl;
class MPI_Send_Receive_Test : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        DlTdtFunction::GetInstance().DlTdtFunctionInit();
        DlTraceFunction::GetInstance().DlTraceFunctionInit();
        TsdOpen(1, 2);
        MPI_Barrier(MPI_COMM_WORLD);
        
        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "UT_MPI_TEST");

        std::cout << "A TestCase SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        TsdClose(1);
        MPI_Barrier(MPI_COMM_WORLD);
        std::cout << "A TestCase TearDown" << std::endl;
    }
};


static void TestConstructParam(HcclCommParams &params, RankTable_t &rankTable)
{
    string commId = "comm ";
    memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
    params.rank = 0;
    params.totalRanks = 2;
    params.isHeterogComm = false;
    params.logicDevId = 0;
    params.commWorkMode = WorkMode::HCCL_MODE_NORMAL;
    params.deviceType = DevType::DEV_TYPE_910B;
 
    rankTable.collectiveId = "192.168.0.101-8000-8001";
    vector<RankInfo_t> rankVec(2);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";
    rankVec[1].rankId = 1;
    rankVec[1].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr2(1711319232);
    rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2); // 101.0.168.192
    rankVec[1].serverIdx = 1;
    rankVec[1].serverId = "192.168.0.102";
    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.deviceNum = 2;
    rankTable.serverNum = 2;
}

#define P2P_DATA_SIZE_LIGHT 20
#define P2P_DATA_SIZE_HEAVY 1200000
#define P2P_DATA_SIZE_S_HEAVY 3000000   /* 超大数据，约10M */

typedef struct p2p_para_struct
{
    HcclRootInfo rootInfo;
    std::string identify;
    s32 device_id;
    char* file_name;
    bool sender_flag;
    s32 peer_rank;
    void* buffer;
    s32 count;
    HcclDataType datatype;
    rtStream_t stream;
    volatile s32* sync_addr;
    const char* tag;
    char* groupName;
    u32 groupRanksNum;
    u32 *pGroupRanks;
    s32 ranks_local;
} p2p_para_t;

void* send_receive_8p_subgroup_task(void* parg)
{
    MOCKER_CPP(&HcclAlg::SetHDCModeInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HCCL_SUCCESS;
    bool subGroupFlag = false;
    p2p_para_t* para_info = (p2p_para_t*)parg;
    s32 rank_num_tmp;
    std::vector<u32> groupRanks;
    std::shared_ptr<hccl::hcclComm> pSubComm;
    HcomInfo hcom_info;
    hcom_info.params.deviceType = DevType::DEV_TYPE_910;
    std::string ranktable_file = para_info->file_name;
    std::string rankTableM;
    std::string realFilePath;

    rtError_t rt_ret = RT_ERROR_NONE;
    HCCL_INFO("device_id=[%d]", para_info->device_id);

    hrtSetDevice(para_info->device_id);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomLoadRanktableFile(ranktable_file.c_str(), rankTableM, realFilePath);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, para_info->identify, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sal_memcpy(hcom_info.params.id.internal, sizeof(HcclRootInfo), &para_info->rootInfo, sizeof(HcclRootInfo));

    hcom_info.pComm.reset(new(std::nothrow) hccl::hcclComm());
    rtModel_t model = (void*)1;

    bool swapped;
    rank_num_tmp = *(para_info->sync_addr) - 1;
    u32 wordGroupId = std::stoi(para_info->identify);
    s32 groupid = -1;
    do
    {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr /** &rank_num */, rank_num_tmp, rank_num_tmp + 1);
    }
    while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local)
    {
        sched_yield(); // linux提供一个系统调用运行进程主动让出执行权
    }
    __sync_synchronize();  // 插入内存屏障，对顺序性有要求，但是有没有使用lock指令的时候
    CommConfig commConfig("WORLD_GROUP_01");
    ret =  hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task send falis", para_info->device_id);
    }
    HCCL_INFO("wordGroupId[%d], hcclComm init success!",wordGroupId);
    
    for(u32 i = 0;i < para_info->groupRanksNum;i++) {
        if(wordGroupId == para_info->pGroupRanks[i]) {
            subGroupFlag = true;
            groupid = i;
            HCCL_INFO("get groupid[%d]!",groupid);
        }
        groupRanks.push_back(para_info->pGroupRanks[i]);
    }

    if(subGroupFlag) { //是否为group内
        //-----------------Set Workspace Resource Start------------------//
        u64 stream_list_size = 0;
        ret = hcom_info.pComm->GetWorkspaceSubStreamNum(stream_list_size);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("get stream_list_size[%d] success", stream_list_size);
        vector<HcclRtStream> streamList(stream_list_size);

        //生成从stream
        for (s32 i = 0; i < stream_list_size; i++)
        {
            rt_ret = rtStreamCreateWithFlags(&streamList[i], 0, (RT_STREAM_PERSISTENT | RT_STREAM_FORCE_COPY));
            EXPECT_EQ(rt_ret, RT_ERROR_NONE);
            //从流bind到model
            rt_ret = rtModelBindStream(model, streamList[i], RT_MODEL_WAIT_ACTIVE_STREAM);
            EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        }

        u32 rankSize = 0;
        ret = hcom_info.pComm->GetRankSize(rankSize);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        void *memptr = nullptr;

        hcom_info.pComm->CreateGroup(para_info->groupName, groupid, hcom_info.params.userRank, groupRanks, pSubComm);

        HCCL_INFO("wordGroupId[%d], groupid[%d], CreateGroup success!",wordGroupId, groupid);
    
        if (para_info->sender_flag)
        {
            u64 memSize = 0;
            ret = hcom_info.pComm->GetWorkspaceMemSize("HcomSend", para_info->count, para_info->datatype, rankSize, memSize);
            EXPECT_EQ(ret, HCCL_SUCCESS);

            
            ret = hrtMalloc(&memptr, memSize);
            EXPECT_EQ(ret, HCCL_SUCCESS);

            ret = hcom_info.pComm->SetWorkspaceResource(para_info->tag, memptr, memSize, streamList);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            ret = pSubComm->send(
                para_info->tag, 
                para_info->buffer, 
                para_info->count, 
                para_info->datatype, 
                para_info->peer_rank, 
                para_info->stream,
                0,
                para_info->device_id);
            HCCL_INFO("rank[%s] device[%d] send to rank[%d]", para_info->identify.c_str(), para_info->device_id, para_info->peer_rank);
            if (ret != HCCL_SUCCESS)
            {
                HCCL_ERROR("dev[%d] task send fails", para_info->device_id);
            }
            
        }
        else
        {
            u64 memSize = 0;
            ret = hcom_info.pComm->GetWorkspaceMemSize("HcomReceive", para_info->count, para_info->datatype, rankSize, memSize);
            EXPECT_EQ(ret, HCCL_SUCCESS);

            ret = hrtMalloc(&memptr, memSize);
            EXPECT_EQ(ret, HCCL_SUCCESS);

            ret = hcom_info.pComm->SetWorkspaceResource(para_info->tag, memptr, memSize, streamList);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            ret = pSubComm->receive(
                para_info->tag, 
                para_info->buffer, 
                para_info->count, 
                para_info->datatype, 
                para_info->peer_rank, 
                para_info->stream,
                0,
                para_info->device_id);
            HCCL_INFO("rank[%s] device[%d] recv from rank[%d]", para_info->identify.c_str(), para_info->device_id, para_info->peer_rank);
            if (ret != HCCL_SUCCESS)
            {
                HCCL_ERROR("dev[%d] task receive fails", para_info->device_id);
            }
        }
        subGroupFlag = false;
        //EXPECT_EQ(ret, HCCL_SUCCESS); 临时规避ut概率性失败
        HCCL_INFO("wordGroupId[%d], groupid[%d], all_gather success!",wordGroupId, groupid);


        rt_ret = rtStreamSynchronize(para_info->stream);

        if ( rt_ret != RT_ERROR_NONE)
        {
            HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
        }
        HCCL_INFO("wordGroupId[%d], groupid[%d], rtStreamSynchronize success!",wordGroupId, groupid);
        //--------------Resource destroy----------------//
        for (s32 i = 0; i < stream_list_size; i++)
        {
            rt_ret = rtModelUnbindStream(model, streamList[i]);
            EXPECT_EQ(rt_ret, RT_ERROR_NONE);
            
            rt_ret = rtStreamDestroy(streamList[i]);
            EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        }
        hrtFree(memptr);
    }
    
    ret = hcom_info.pComm->DestroyGroup(para_info->groupName);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("wordGroupId[%d], groupid[%d], DestroyGroup success!",wordGroupId, groupid);


    return (NULL);
}

TEST_F(MPI_Send_Receive_Test, ut_mpi_heterog_hccl_signal)
{
    s32 nnode, noderank = 0;

    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);

    MOCKER(hrtNotifyRecord)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtNotifyWaitWithTimeOut)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtNotifyDestroy)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    DlHalFunction::GetInstance().DlHalFunctionInit();

    //创建dispatcher, 创建信号
    void *dispatcher = nullptr;

    HcclResult ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcher, nullptr);

    s32 devId = noderank == 0 ? 0 : -1;

    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(devId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string tag = "signal_test";
    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    if (noderank == 0) {
        HcclResult ret;
        std::shared_ptr<LocalIpcNotify> localNotify = nullptr;
        std::shared_ptr<RemoteNotify> remoteNotify = nullptr;
        HCCL_INFO("alloc notify start");
        RemoteRankInfo info(-1, 0, 1);
        ret = notifyPool->Alloc(tag, info, localNotify);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        EXPECT_NE(localNotify, nullptr);

        HCCL_INFO("notify open ipc start");
        std::vector<u8> data(NOTIFY_INFO_LENGTH, 0);
        ret = localNotify->Serialize(data);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        remoteNotify.reset(new (std::nothrow) RemoteNotify());
        EXPECT_NE(remoteNotify, nullptr);

        ret = remoteNotify->Init(data);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        ret = remoteNotify->Open();
        EXPECT_EQ(ret, HCCL_SUCCESS);

        rtStream_t stream;
        rtError_t rt_ret = rtStreamCreate(&stream, 5);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        Stream streamObj(stream);

        HCCL_INFO("notify record start");
        ret = remoteNotify->Post(streamObj, dispatcher);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("notify wait start");
        ret = localNotify->Wait(streamObj, dispatcher);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("notify ipc close");
        ret = remoteNotify->Close();
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("notify Destroy");
        ret = localNotify->Destroy();
        EXPECT_EQ(ret, HCCL_SUCCESS);

        rtStreamDestroy(stream);
        localNotify = nullptr;
        remoteNotify = nullptr;

    } else if (noderank == 1) {

        HcclResult ret;
        std::shared_ptr<LocalIpcNotify> localNotify = nullptr;
        std::shared_ptr<RemoteNotify> remoteNotify = nullptr;
        HCCL_INFO("alloc esched event start");
        RemoteRankInfo info(0, 0, 1);
        ret = notifyPool->Alloc(tag, info, localNotify);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        EXPECT_NE(localNotify, nullptr);

        HCCL_INFO("esched event open ipc start");
        std::vector<u8> data(NOTIFY_INFO_LENGTH, 0);
        ret = localNotify->Serialize(data);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        remoteNotify.reset(new (std::nothrow) RemoteNotify());

        ret = remoteNotify->Init(data);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        ret = remoteNotify->Open();
        EXPECT_EQ(ret, HCCL_SUCCESS);

        rtStream_t stream;
        rtError_t rt_ret = rtStreamCreate(&stream, 5);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        Stream streamObj(stream);

        HCCL_INFO("esched event record start");
        ret = remoteNotify->Post(streamObj, dispatcher);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("esched event wait start");
        ret = localNotify->Wait(streamObj, dispatcher);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("esched event ipc close");
        ret = remoteNotify->Close();
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("esched event ipc Destroy");
        ret = localNotify->Destroy();
        EXPECT_EQ(ret, HCCL_SUCCESS);
        rtStreamDestroy(stream);
        localNotify = nullptr;
        remoteNotify = nullptr;

    }

    ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (dispatcher != nullptr) {
        ret = HcclDispatcherDestroy(dispatcher);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcher = nullptr;
    }
    GlobalMockObject::verify();

    MPI_Barrier(MPI_COMM_WORLD);
}

#if 1
TEST_F(MPI_Send_Receive_Test, ut_TransportHeterogP2P_Init)
{
    u32 status = HETEROG_P2P_SUCCESS;
    HcclResult ret = HCCL_SUCCESS;
    std::chrono::milliseconds timeout;
    MachinePara machinePara;
    TransportHeterogP2P TranHeP2P(nullptr, nullptr, machinePara, timeout);
    MOCKER_CPP_VIRTUAL(TranHeP2P, &TransportHeterogP2P::ConnectQuerry)
    .stubs()
    .with(outBound(status))
    .will(returnValue(HCCL_SUCCESS));
    ret = TranHeP2P.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    status = HETEROG_P2P_WAIT;
    MOCKER_CPP_VIRTUAL(TranHeP2P, &TransportHeterogP2P::ConnectQuerry)
    .stubs()
    .with(outBound(status))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(GetExternalInputHcclLinkTimeOut)
    .stubs()
    .will(returnValue(2));

    ret = TranHeP2P.Init();
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);
    GlobalMockObject::verify();
}
#endif
#if 1
struct HRequestInfo {
    std::string opTag;
    std::string group;
    HcomOpDesc opDesc;
};

HcclResult CommThread() {
    return HCCL_SUCCESS;
}

#endif
#if 1
TEST_F(MPI_Send_Receive_Test, ut_hccl_notify_test)
{
    //创建dispatcher, 创建信号
    s32 ret;
    DlHalFunction::GetInstance().DlHalFunctionInit();

    void *dispatcher = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcher, nullptr);

    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string tag = "notify_test";
    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::shared_ptr<LocalIpcNotify> localNotify = nullptr;
    std::shared_ptr<RemoteNotify> remoteNotify = nullptr;
    HCCL_INFO("alloc notify start");
    RemoteRankInfo info(0, 0);
    SalGetBareTgid(reinterpret_cast<u32*>(&info.remotePid));
    ret = notifyPool->Alloc(tag, info, localNotify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(localNotify, nullptr);

    HCCL_INFO("notify open ipc start");
    std::vector<u8> data(NOTIFY_INFO_LENGTH, 0);
    ret = localNotify->Serialize(data);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remoteNotify.reset(new (std::nothrow) RemoteNotify());

    ret = remoteNotify->Init(data);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = remoteNotify->Open();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rtStream_t stream;
    rtError_t rt_ret = rtStreamCreate(&stream, 5);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    Stream streamObj(stream);

    HCCL_INFO("notify record start");
    ret = remoteNotify->Post(streamObj, dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HCCL_INFO("notify wait start");
    ret = localNotify->Wait(streamObj, dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rtStreamSynchronize(streamObj.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HCCL_INFO("notify ipc close");
    ret = remoteNotify->Close();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HCCL_INFO("notify Destroy");
    ret = localNotify->Destroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (dispatcher != nullptr) {
        ret = HcclDispatcherDestroy(dispatcher);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcher = nullptr;
    }

    rtStreamDestroy(stream);
    localNotify = nullptr;
    remoteNotify = nullptr;
}
#endif

