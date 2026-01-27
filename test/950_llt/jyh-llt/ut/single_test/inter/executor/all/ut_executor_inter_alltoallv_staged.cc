#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <runtime/rt.h>

#include <assert.h>
#include <semaphore.h>
#include <signal.h>
#include <syscall.h>
#include <sys/prctl.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <cce/dnn.h>
#include <securec.h>

#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <driver/ascend_hal.h>
#include <runtime/rt.h>

#include "dlra_function.h"
#include "param_check_pub.h"

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "hccl_comm_pub.h"
#include "externalinput_pub.h"
#include "externalinput.h"
#include "workflow_pub.h"
#include "rank_consistentcy_checker.h"
#include "alg_template_register.h"
#define private public
#include "alltoallv_staged_pairwise.h"
#undef private
#include "sal.h"
#include "config.h"
#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"

#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include "param_check_pub.h"
#include "hcom_private.h"

#include <iostream>
#include <fstream>

using namespace std;
using namespace hccl;

class Inter_AlltoAllV_Staged_Test : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "Inter_AlltoAllV_Staged_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "Inter_AlltoAllV_Staged_Test TearDown" << std::endl;
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
        TsdOpen(1, 2);
    
        static s32 call_cnt = 0;
        string name = std::to_string(call_cnt++) + "_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name.c_str());
        std::cout << "A Inter_AlltoAllV_Staged_Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        TsdClose(1);
        std::cout << "A Inter_AlltoAllV_Staged_Test TearDown" << std::endl;
    }
    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;

};
HcclDispatcher Inter_AlltoAllV_Staged_Test::dispatcherPtr = nullptr;
DispatcherPub *Inter_AlltoAllV_Staged_Test::dispatcher = nullptr;


#define GOUP_DEV_NUM 8

typedef struct para_struct {
    HcclRootInfo rootInfo;
    std::string identify;
    s32 userRankSize;
    s32 device_id;
    s32 ranks_local;  //??????rank?

    char *file_name;
    void *sendbuff;
    void *sendCounts;
    void *sdispls;
    void *recvbuff;
    void *recvCounts;
    void *rdispls;
    HcclDataType sendDataType;
    HcclDataType recvDataType;

    rtStream_t stream;

    int id;
    bool flag;
    volatile s32 *sync_addr;
    const char *tag;
    char *groupName;
    u32 groupRanksNum;
    u32 *pGroupRanks;
} para_t;

void *inter_alltoallv_staged_task(void *parg)
{
    HcclResult ret = HCCL_SUCCESS;
    para_t *para_info = (para_t *)parg;
    s32 rank_num_tmp;

    HcomInfo hcom_info;
    std::string ranktable_file = para_info->file_name;
    std::string rankTableM;
    std::string realFilePath;

    hrtSetDevice(para_info->device_id);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomLoadRanktableFile(ranktable_file.c_str(), rankTableM, realFilePath);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, para_info->identify, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sal_memcpy(hcom_info.params.id.internal, sizeof(HcclRootInfo), &para_info->rootInfo, sizeof(HcclRootInfo));

    hcom_info.pComm.reset(new (std::nothrow) hccl::hcclComm());
    rtModel_t model = (void *)1;
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);
    HcclWorkflowMode mode = GetWorkflowMode();
    InitWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB);

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("dev[%d] task all_gather falis", para_info->device_id);
    }

    //-----------------Set Workspace Resource Start------------------//
    u64 stream_list_size = 0;
    ret = hcom_info.pComm->GetWorkspaceSubStreamNum(stream_list_size);
    HCCL_INFO("==TMP== stream_list_size=%d", stream_list_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("get stream_list_size[%d] success", stream_list_size);
    vector<HcclRtStream> streamList(stream_list_size);
    rtError_t rt_ret;
    //???stream
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtStreamCreateWithFlags(&streamList[i], 0, (RT_STREAM_PERSISTENT | RT_STREAM_FORCE_COPY));
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        //??bind?model
        rt_ret = rtModelBindStream(model, streamList[i], RT_MODEL_WAIT_ACTIVE_STREAM);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    // ??workspace????
    string tag = "tag-alltoallv-staged";
    u64 memSize = 0;

    HCCL_INFO("==TMP==  workspace memSize[%llu]", memSize);

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource(tag, memptr, memSize, streamList);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//

    bool swapped;
    rank_num_tmp = *(para_info->sync_addr) - 1;

    do {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1);
    } while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local) {
        sched_yield();
    }  // linux

    ret = hcom_info.pComm->AlltoAllV(para_info->sendbuff,
        para_info->sendCounts,
        para_info->sdispls,
        para_info->sendDataType,
        para_info->recvbuff,
        para_info->recvCounts,
        para_info->rdispls,
        para_info->recvDataType,
        para_info->stream,
        tag);

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }

    rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamSynchronize(para_info->stream);

    if (rt_ret != RT_ERROR_NONE) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }
    //--------------Resource destroy----------------//
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtModelUnbindStream(model, streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);

        rt_ret = rtStreamDestroy(streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    ret = hcom_info.pComm->ClearOpResource(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hrtFree(memptr);
    SetWorkflowMode(mode);

    return (nullptr);
}

// ?opbase??alltoallv 4p
void *inter_alltoallv_staged_task1(void *parg)
{
    HcclResult ret = HCCL_SUCCESS;
    para_t *para_info = (para_t *)parg;
    s32 rank_num_tmp;

    HcomInfo hcom_info;
    std::string ranktable_file = para_info->file_name;
    std::string rankTableM;
    std::string realFilePath;

    hrtSetDevice(para_info->device_id);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomLoadRanktableFile(ranktable_file.c_str(), rankTableM, realFilePath);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, para_info->identify, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sal_memcpy(hcom_info.params.id.internal, sizeof(HcclRootInfo), &para_info->rootInfo, sizeof(HcclRootInfo));

    hcom_info.pComm.reset(new (std::nothrow) hccl::hcclComm());
    rtModel_t model = (void *)1;
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("dev[%d] task all_gather falis", para_info->device_id);
    }

    ret = hcom_info.pComm->CreateCommCCLbuffer();
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("dev[%d] task all_gather falis", para_info->device_id);
    }

    HcclWorkflowMode mode = GetWorkflowMode();
    InitWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);

    //-----------------Set Workspace Resource Start------------------//
    // alltoallv ??????stream
    std::vector<rtStream_t> subStreams;
    // ??workspace????
    string tag = "tag-alltoallv-staged-opbase";
    u64 memSize = 0;

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    HCCL_INFO("==TMP==  workspace[%p] memSize[%llu]", memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource(tag, memptr, memSize, subStreams);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//

    bool swapped;
    rank_num_tmp = *(para_info->sync_addr) - 1;

    do {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1);
    } while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local) {
        sched_yield();
    }  // linux???????????????????

    ret = hcom_info.pComm->AlltoAllV(para_info->sendbuff,
        para_info->sendCounts,
        para_info->sdispls,
        para_info->sendDataType,
        para_info->recvbuff,
        para_info->recvCounts,
        para_info->rdispls,
        para_info->recvDataType,
        para_info->stream,
        tag);

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }

    rtError_t rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamSynchronize(para_info->stream);

    if (rt_ret != RT_ERROR_NONE) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }
    //--------------Resource destroy----------------//

    hrtFree(memptr);
    SetWorkflowMode(mode);

    return (nullptr);
}


/*
void* all_gather_8p_subgroup_task(void* parg)
{
    HcclResult ret = HCCL_SUCCESS;
    bool subGroupFlag = false;
    para_t* para_info = (para_t*)parg;
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

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1); //* &rank_num
    }
    while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local)
    {
        sched_yield(); // linux???????????????????
    }
    __sync_synchronize();  // ??????????????????????lock?????

    ret =  hcom_info.pComm->init(hcom_info.params, hcom_info.rankTable);

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task all_gather falis", para_info->device_id);
    }
    else
    {
        HCCL_INFO("wordGroupId[%d], hcclComm init success!",wordGroupId);
    }
    for(u32 i = 0;i < para_info->groupRanksNum;i++) {
        if(wordGroupId == para_info->pGroupRanks[i]) {
            subGroupFlag = true;
            groupid = i;
            HCCL_INFO("get groupid[%d]!",groupid);
        }
        groupRanks.push_back(para_info->pGroupRanks[i]);
    }

    //-----------------Set Workspace Resource Start------------------//
    u64 stream_list_size = 0;
    ret = hcom_info.pComm->GetWorkspaceSubStreamNum(stream_list_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("get stream_list_size[%d] success", stream_list_size);
    vector<HcclRtStream> streamList(stream_list_size);

    //???stream
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtStreamCreateWithFlags(&streamList[i], 0, (RT_STREAM_PERSISTENT | RT_STREAM_FORCE_COPY));
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        //??bind?model
        rt_ret = rtModelBindStream(model, streamList[i], RT_MODEL_WAIT_ACTIVE_STREAM);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    u32 rankSize = 0;
    ret = hcom_info.pComm->GetRankSize(rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 memSize = 0;
    ret = hcom_info.pComm->GetWorkspaceMemSize(HCCL_KERNEL_OP_TYPE_ALLGATHER, para_info->count, para_info->datatype,
rankSize, memSize); EXPECT_EQ(ret, HCCL_SUCCESS);

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource("tag1", memptr, memSize, streamList);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//

    if(subGroupFlag) { //???group?
        hcom_info.pComm->CreateGroup(para_info->groupName, groupid, groupRanks, pSubComm);

        HCCL_INFO("wordGroupId[%d], groupid[%d], CreateGroup success!",wordGroupId, groupid);

        ret =  pSubComm->AllGather("tag1",
                                   para_info->sendbuff,
                                   para_info->recvbuff,
                                   para_info->count,
                                   para_info->datatype,
                                   para_info->stream);
        if (ret != HCCL_SUCCESS)
        {
            HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
        }
        subGroupFlag = false;
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("wordGroupId[%d], groupid[%d], all_gather success!",wordGroupId, groupid);

        HCCL_INFO("wordGroupId[%d], groupid[%d], rtStreamSynchronize success!",wordGroupId, groupid);
    }

   // rtError_t rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamSynchronize(para_info->stream);

    if ( rt_ret != RT_ERROR_NONE)
    {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }
    //--------------Resource destroy----------------//
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtModelUnbindStream(model, streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);

        rt_ret = rtStreamDestroy(streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    hrtFree(memptr);

    ret = hcom_info.pComm->DestroyGroup(para_info->groupName);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("wordGroupId[%d], groupid[%d], DestroyGroup success!",wordGroupId, groupid);

    return (NULL);
}
*/


#define DEV_NUM_2 2
#define DEV_NUM_4 4
#define DEV_NUM_8 8

#if 1
TEST_F(Inter_AlltoAllV_Staged_Test, ut_inter_alltoallv_staged_2ranks_1server_int64)
{
    HcclResult ret;
    std::string algo = "level0:pairwise;level1:pairwise";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    nlohmann::json rank_table = rank_table_910_1server_2rank;
    char file_name_t[] = "./ut_inter_alltoallv_staged_2ranks_1server_int64.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    setenv("HCCL_DEBUG_CONFIG", "alg", 1);

    s32 nnode, rank, errors = 0;
    nnode = 1;
    ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s64* result_buff[DEV_NUM_2];
    s64* sendbuff[DEV_NUM_2];
    s64* recvbuff[DEV_NUM_2];
    u64* sendCounts[DEV_NUM_2];
    u64* sdispls[DEV_NUM_2];
    u64* recvCounts[DEV_NUM_2];
    u64* rdispls[DEV_NUM_2];

    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_2];
    sal_thread_t tid[DEV_NUM_2];
    para_t para_info[DEV_NUM_2];
    HcclDataType sendDataType = HCCL_DATA_TYPE_INT64;
    HcclDataType recvDataType = HCCL_DATA_TYPE_INT64;

    s32 ndev = DEV_NUM_2;
    set_board_id(0x0000);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 totalRecvCounts[DEV_NUM_2];

    /** ????????? */
    u64 count = 1048576;
    for (u32 i = 0; i < ndev; i++ )
    {
        u64 totalSendCounts = 0;
        sendCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            sendCounts[i][j] = j * count;
            totalSendCounts += sendCounts[i][j];
        }

        HCCL_INFO("==TMP== totalSendCounts[%llu]", totalSendCounts);

        sdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curSDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            sdispls[i][j] = curSDispl;
            curSDispl += sendCounts[i][j];
        }

        totalRecvCounts[i] = 0;
        recvCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            recvCounts[i][j] =  i * count;
            totalRecvCounts[i] += recvCounts[i][j];
        }
        HCCL_INFO("==TMP== totalRecvCounts[%llu]", totalRecvCounts[i]);

        rdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curRDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            rdispls[i][j] = curRDispl;
            curRDispl += recvCounts[i][j];
        }

        ret = hrtMalloc((void**)&sendbuff[i], totalSendCounts * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        for (u32 j = 0; j < totalSendCounts; j++) {
            sendbuff[i][j] = 1;
        }

        ret = hrtMalloc((void**)&recvbuff[i], totalRecvCounts[i] * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuff[i], totalRecvCounts[i] * sizeof(s64), 0, totalRecvCounts[i] * sizeof(s64));

        result_buff[i] = (s64*)sal_malloc(totalRecvCounts[i] * sizeof(s64));
        for (u32 j = 0; j < totalRecvCounts[i]; j++) {
            result_buff[i][j] = 1;
        }


    }


    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = rtStreamCreate(&stream[i], 5);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        sal_memcpy(&para_info[i].rootInfo, sizeof(HcclRootInfo), &rootInfo, sizeof(HcclRootInfo));
        std::ostringstream identify("");
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].userRankSize = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;


        para_info[i].sendbuff = sendbuff[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuff[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;

        para_info[i].sendCounts = sendCounts[i];
        para_info[i].sdispls = sdispls[i];

        para_info[i].recvCounts = recvCounts[i];
        para_info[i].rdispls = rdispls[i];

        para_info[i].sendDataType = sendDataType;
        para_info[i].recvDataType = recvDataType;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_alltoallv_staged_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //??stream?????????
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < totalRecvCounts[j]; i++)
        {
            s64 res = result_buff[j][i];
            s64 recv = recvbuff[j][i];

            if (res != recv)
            {
                HCCL_ERROR("node:%d result[%d][%d]:% sendbuf:%\n", noderank, j, i, recvbuff[j][i] , sendbuff[j][i]);
                errors++;
                break;
            }
        }
    }

  /*
    if (noderank == 0)
        for (s32 j = 0; j < ndev; j++) {
            for (s32 i = 0; i < count * ndev * nnode; i++)
            {
                printf("recvbuf[%d][%d] = %f \n", j, i, recvbuf[j][i]);
            }
        }
*/

    if (errors)
    {
        HCCL_ERROR("%d errors. Test FAILED.\n", errors);
    }
    else
    {
        HCCL_INFO("Test PASSED.\n");
    }

    for (s32 j = 0; j < ndev; j++)
    {
        hrtFree(sendbuff[j]);
        hrtFree(recvbuff[j]);
        sal_free(result_buff[j]);

        sal_free(sendCounts[j]);
        sal_free(sdispls[j]);
        sal_free(recvCounts[j]);
        sal_free(rdispls[j]);

        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0x0000);
    remove(file_name_t);

    algo = "level0:ring;level1:H-D_R";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_DEBUG_CONFIG");
}
#endif

#if 1
TEST_F(Inter_AlltoAllV_Staged_Test, ut_inter_opbase_alltoallv_staged_4ranks_1server_int64)
{
    HcclResult ret;
    std::string algo = "level0:pairwise;level1:pairwise";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();

    nlohmann::json rank_table = rank_table_910_1server_4rank;
    char file_name_t[] = "./ut_inter_alltoallv_staged_4ranks_1server_int64.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    s32 nnode, rank, errors = 0;
    nnode = 1;
    ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s64* result_buff[DEV_NUM_4];
    s64* sendbuff[DEV_NUM_4];
    s64* recvbuff[DEV_NUM_4];
    u64* sendCounts[DEV_NUM_4];
    u64* sdispls[DEV_NUM_4];
    u64* recvCounts[DEV_NUM_4];
    u64* rdispls[DEV_NUM_4];

    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];
    HcclDataType sendDataType = HCCL_DATA_TYPE_INT64;
    HcclDataType recvDataType = HCCL_DATA_TYPE_INT64;

    s32 ndev = DEV_NUM_4;
    set_board_id(0x0000);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 totalRecvCounts[DEV_NUM_4];

    /** ????????? */
    u64 count = 1048576;
    for (u32 i = 0; i < ndev; i++ )
    {
        u64 totalSendCounts = 0;
        sendCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            sendCounts[i][j] = (j + 1) * count;
            totalSendCounts += sendCounts[i][j];
        }

        HCCL_INFO("==TMP== rank%u totalSendCounts[%llu]", i, totalSendCounts);

        sdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curSDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            sdispls[i][j] = curSDispl;
            curSDispl += sendCounts[i][j];
        }

        totalRecvCounts[i] = 0;
        recvCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            recvCounts[i][j] = (1 + i) * count;
            totalRecvCounts[i] += recvCounts[i][j];
        }
        HCCL_INFO("==TMP== rank%u totalRecvCounts[%llu]", i, totalRecvCounts[i]);

        rdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curRDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            rdispls[i][j] = curRDispl;
            curRDispl += recvCounts[i][j];
        }

        ret = hrtMalloc((void**)&sendbuff[i], totalSendCounts * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        for (u32 j = 0; j < totalSendCounts; j++) {
            sendbuff[i][j] = 1;
        }

        ret = hrtMalloc((void**)&recvbuff[i], totalRecvCounts[i] * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuff[i], totalRecvCounts[i] * sizeof(s64), 0, totalRecvCounts[i] * sizeof(s64));

        result_buff[i] = (s64*)sal_malloc(totalRecvCounts[i] * sizeof(s64));
        for (u32 j = 0; j < totalRecvCounts[i]; j++) {
            result_buff[i][j] = 1;
        }


    }


    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = rtStreamCreate(&stream[i], 5);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        sal_memcpy(&para_info[i].rootInfo, sizeof(HcclRootInfo), &rootInfo, sizeof(HcclRootInfo));
        std::ostringstream identify("");
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].userRankSize = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;


        para_info[i].sendbuff = sendbuff[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuff[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;

        para_info[i].sendCounts = sendCounts[i];
        para_info[i].sdispls = sdispls[i];

        para_info[i].recvCounts = recvCounts[i];
        para_info[i].rdispls = rdispls[i];

        para_info[i].sendDataType = sendDataType;
        para_info[i].recvDataType = recvDataType;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_alltoallv_staged_task1, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //??stream?????????
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < totalRecvCounts[j]; i++)
        {
            s64 res = result_buff[j][i];
            s64 recv = recvbuff[j][i];

            if (res != recv)
            {
                HCCL_ERROR("node:%d result[%d][%d]:%llu recvbuff:%llu\n", noderank, j, i, result_buff[j][i] , recvbuff[j][i]);
                errors++;
                break;
            }
        }
    }

  /*
    if (noderank == 0)
        for (s32 j = 0; j < ndev; j++) {
            for (s32 i = 0; i < count * ndev * nnode; i++)
            {
                printf("recvbuf[%d][%d] = %f \n", j, i, recvbuf[j][i]);
            }
        }
*/

    if (errors)
    {
        HCCL_ERROR("%d errors. Test FAILED.\n", errors);
    }
    else
    {
        HCCL_INFO("Test PASSED.\n");
    }

    SaluSleep(SAL_SECOND_USEC);

    for (s32 j = 0; j < ndev; j++)
    {
        hrtFree(sendbuff[j]);
        hrtFree(recvbuff[j]);
        sal_free(result_buff[j]);

        sal_free(sendCounts[j]);
        sal_free(sdispls[j]);
        sal_free(recvCounts[j]);
        sal_free(rdispls[j]);

        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0x0000);
    remove(file_name_t);

    algo = "level0:ring;level1:H-D_R";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(Inter_AlltoAllV_Staged_Test, ut_opbase_alltoallv_set)
{
    HcclResult ret;
    // ????????????????
    setenv("HCCL_ALGO", "level0:pairwise;level1:pairwise", 1);
    ret = InitExternalInput();
    unsetenv("HCCL_ALGO");
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(Inter_AlltoAllV_Staged_Test, ut_send_zero_check)
{
    s32 device_id = 0;
    Stream stream;
    DeviceMem sendMem;
    DeviceMem recvMem;
    DeviceMem scratchInputMem;
    DeviceMem scratchOutputMem;
    StageAlltoAllVAddrInfo sendAddrInfo;
    StageAlltoAllVAddrInfo recvAddrInfo;
    std::shared_ptr<AlgTemplateBase> tempBasePtr = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_2_ALL_V_STAGED_PAIRWISE, dispatcher);
    EXPECT_EQ(HcclResult::HCCL_SUCCESS, tempBasePtr->Prepare(sendMem, recvMem, scratchInputMem, scratchOutputMem,
        sendAddrInfo, recvAddrInfo, true, stream));
    std::shared_ptr<AlltoAllVStagedPairwise> tempPtr = std::dynamic_pointer_cast<AlltoAllVStagedPairwise>(tempBasePtr);

    StageAlltoAllVAddrInfo testSendRecvInfo;
    std::list<OneSendRecvAddrInfo> testList;
    testList.push_back({0, 0, 0, 0});
    testSendRecvInfo.insert({0, testList});

    std::vector<std::list<OneSendRecvAddrInfo>> testVector;
    tempPtr->LoadPolicies(0, testSendRecvInfo, testVector);
    EXPECT_EQ(testVector.empty(), true);
}
#endif

#if 1
TEST_F(Inter_AlltoAllV_Staged_Test, ut_inter_alltoallv_staged_mesh_4ranks_1server_int64)
{
    HcclResult ret;
    std::string algo = "level0:fullmesh;level1:pairwise";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    nlohmann::json rank_table = rank_table_910_1server_4rank;
    char file_name_t[] = "./ut_inter_alltoallv_staged_4ranks_1server_int64.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    s32 nnode, rank, errors = 0;
    nnode = 1;
    ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s64* result_buff[DEV_NUM_4];
    s64* sendbuff[DEV_NUM_4];
    s64* recvbuff[DEV_NUM_4];
    u64* sendCounts[DEV_NUM_4];
    u64* sdispls[DEV_NUM_4];
    u64* recvCounts[DEV_NUM_4];
    u64* rdispls[DEV_NUM_4];

    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];
    HcclDataType sendDataType = HCCL_DATA_TYPE_INT64;
    HcclDataType recvDataType = HCCL_DATA_TYPE_INT64;

    s32 ndev = DEV_NUM_4;
    set_board_id(0x0000);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 totalRecvCounts[DEV_NUM_4];

    /** ????????? */
    u64 count = 1048576;
    for (u32 i = 0; i < ndev; i++ )
    {
        u64 totalSendCounts = 0;
        sendCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            sendCounts[i][j] = j * count;
            totalSendCounts += sendCounts[i][j];
        }

        HCCL_INFO("==TMP== totalSendCounts[%llu]", totalSendCounts);

        sdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curSDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            sdispls[i][j] = curSDispl;
            curSDispl += sendCounts[i][j];
        }

        totalRecvCounts[i] = 0;
        recvCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            recvCounts[i][j] =  i * count;
            totalRecvCounts[i] += recvCounts[i][j];
        }
        HCCL_INFO("==TMP== totalRecvCounts[%llu]", totalRecvCounts[i]);

        rdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curRDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            rdispls[i][j] = curRDispl;
            curRDispl += recvCounts[i][j];
        }

        ret = hrtMalloc((void**)&sendbuff[i], totalSendCounts * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        for (u32 j = 0; j < totalSendCounts; j++) {
            sendbuff[i][j] = 1;
        }

        ret = hrtMalloc((void**)&recvbuff[i], totalRecvCounts[i] * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuff[i], totalRecvCounts[i] * sizeof(s64), 0, totalRecvCounts[i] * sizeof(s64));

        result_buff[i] = (s64*)sal_malloc(totalRecvCounts[i] * sizeof(s64));
        for (u32 j = 0; j < totalRecvCounts[i]; j++) {
            result_buff[i][j] = 1;
        }


    }


    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = rtStreamCreate(&stream[i], 5);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        sal_memcpy(&para_info[i].rootInfo, sizeof(HcclRootInfo), &rootInfo, sizeof(HcclRootInfo));
        std::ostringstream identify("");
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].userRankSize = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;


        para_info[i].sendbuff = sendbuff[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuff[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;

        para_info[i].sendCounts = sendCounts[i];
        para_info[i].sdispls = sdispls[i];

        para_info[i].recvCounts = recvCounts[i];
        para_info[i].rdispls = rdispls[i];

        para_info[i].sendDataType = sendDataType;
        para_info[i].recvDataType = recvDataType;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_alltoallv_staged_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //??stream?????????
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < totalRecvCounts[j]; i++)
        {
            s64 res = result_buff[j][i];
            s64 recv = recvbuff[j][i];

            if (res != recv)
            {
                HCCL_ERROR("node:%d result[%d][%d]:% sendbuf:%\n", noderank, j, i, recvbuff[j][i] , sendbuff[j][i]);
                errors++;
                break;
            }
        }
    }

  /*
    if (noderank == 0)
        for (s32 j = 0; j < ndev; j++) {
            for (s32 i = 0; i < count * ndev * nnode; i++)
            {
                printf("recvbuf[%d][%d] = %f \n", j, i, recvbuf[j][i]);
            }
        }
*/

    if (errors)
    {
        HCCL_ERROR("%d errors. Test FAILED.\n", errors);
    }
    else
    {
        HCCL_INFO("Test PASSED.\n");
    }

    for (s32 j = 0; j < ndev; j++)
    {
        hrtFree(sendbuff[j]);
        hrtFree(recvbuff[j]);
        sal_free(result_buff[j]);

        sal_free(sendCounts[j]);
        sal_free(sdispls[j]);
        sal_free(recvCounts[j]);
        sal_free(rdispls[j]);

        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0x0000);
    remove(file_name_t);

    algo = "level0:ring;level1:H-D_R";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(Inter_AlltoAllV_Staged_Test, ut_inter_alltoallv_staged_mesh_2ranks_1server_int64)
{
    HcclResult ret;
    std::string algo = "level0:fullmesh;level1:pairwise";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    nlohmann::json rank_table = rank_table_910_1server_2rank;
    char file_name_t[] = "./ut_inter_alltoallv_staged_2ranks_1server_int64.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    s32 nnode, rank, errors = 0;
    nnode = 1;
    ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s64* result_buff[DEV_NUM_2];
    s64* sendbuff[DEV_NUM_2];
    s64* recvbuff[DEV_NUM_2];
    u64* sendCounts[DEV_NUM_2];
    u64* sdispls[DEV_NUM_2];
    u64* recvCounts[DEV_NUM_2];
    u64* rdispls[DEV_NUM_2];

    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_2];
    sal_thread_t tid[DEV_NUM_2];
    para_t para_info[DEV_NUM_2];
    HcclDataType sendDataType = HCCL_DATA_TYPE_INT64;
    HcclDataType recvDataType = HCCL_DATA_TYPE_INT64;

    s32 ndev = DEV_NUM_2;
    set_board_id(0x0000);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 totalRecvCounts[DEV_NUM_2];

    /** ????????? */
    u64 count = 1048576;
    for (u32 i = 0; i < ndev; i++ )
    {
        u64 totalSendCounts = 0;
        sendCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            sendCounts[i][j] = j * count;
            totalSendCounts += sendCounts[i][j];
        }

        HCCL_INFO("==TMP== totalSendCounts[%llu]", totalSendCounts);

        sdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curSDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            sdispls[i][j] = curSDispl;
            curSDispl += sendCounts[i][j];
        }

        totalRecvCounts[i] = 0;
        recvCounts[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        for (u32 j = 0; j < ndev; j++) {
            recvCounts[i][j] =  i * count;
            totalRecvCounts[i] += recvCounts[i][j];
        }
        HCCL_INFO("==TMP== totalRecvCounts[%llu]", totalRecvCounts[i]);

        rdispls[i] = (u64*)sal_malloc(ndev * sizeof(u64));
        u64 curRDispl = 0;
        for (u32 j = 0; j < ndev; j++) {
            rdispls[i][j] = curRDispl;
            curRDispl += recvCounts[i][j];
        }

        ret = hrtMalloc((void**)&sendbuff[i], totalSendCounts * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        for (u32 j = 0; j < totalSendCounts; j++) {
            sendbuff[i][j] = 1;
        }

        ret = hrtMalloc((void**)&recvbuff[i], totalRecvCounts[i] * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuff[i], totalRecvCounts[i] * sizeof(s64), 0, totalRecvCounts[i] * sizeof(s64));

        result_buff[i] = (s64*)sal_malloc(totalRecvCounts[i] * sizeof(s64));
        for (u32 j = 0; j < totalRecvCounts[i]; j++) {
            result_buff[i][j] = 1;
        }


    }


    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = rtStreamCreate(&stream[i], 5);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        sal_memcpy(&para_info[i].rootInfo, sizeof(HcclRootInfo), &rootInfo, sizeof(HcclRootInfo));
        std::ostringstream identify("");
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].userRankSize = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;


        para_info[i].sendbuff = sendbuff[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuff[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;

        para_info[i].sendCounts = sendCounts[i];
        para_info[i].sdispls = sdispls[i];

        para_info[i].recvCounts = recvCounts[i];
        para_info[i].rdispls = rdispls[i];

        para_info[i].sendDataType = sendDataType;
        para_info[i].recvDataType = recvDataType;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_alltoallv_staged_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //??stream?????????
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < totalRecvCounts[j]; i++)
        {
            s64 res = result_buff[j][i];
            s64 recv = recvbuff[j][i];

            if (res != recv)
            {
                HCCL_ERROR("node:%d result[%d][%d]:% sendbuf:%\n", noderank, j, i, recvbuff[j][i] , sendbuff[j][i]);
                errors++;
                break;
            }
        }
    }

  /*
    if (noderank == 0)
        for (s32 j = 0; j < ndev; j++) {
            for (s32 i = 0; i < count * ndev * nnode; i++)
            {
                printf("recvbuf[%d][%d] = %f \n", j, i, recvbuf[j][i]);
            }
        }
*/

    if (errors)
    {
        HCCL_ERROR("%d errors. Test FAILED.\n", errors);
    }
    else
    {
        HCCL_INFO("Test PASSED.\n");
    }

    for (s32 j = 0; j < ndev; j++)
    {
        hrtFree(sendbuff[j]);
        hrtFree(recvbuff[j]);
        sal_free(result_buff[j]);

        sal_free(sendCounts[j]);
        sal_free(sdispls[j]);
        sal_free(recvCounts[j]);
        sal_free(rdispls[j]);

        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0x0000);
    remove(file_name_t);

    algo = "level0:ring;level1:H-D_R";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif