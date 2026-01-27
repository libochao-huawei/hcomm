#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <mpi.h>

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

#define private public
#include "rank_consistentcy_checker.h"
#undef private

#include "externalinput.h"

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "hcom_pub.h"
#include "hcom.h"
#include "../inc/hccl/hcom_executor.h"
#include "llt_hccl_stub_pub.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "hccl_comm_pub.h"

#include "sal.h"
#include "dlra_function.h"
#include "hccl_comm_pub.h"
#include "config.h"
#include "topo/topoinfo_ranktableParser_pub.h"
#include "externalinput_pub.h"
#include "v80_rank_table.h"
#include "dltdt_function.h"
#include <iostream>
#include <fstream>
#include "opexecounter_pub.h"

#include "param_check_pub.h"

using namespace std;
using namespace hccl;

class MPI_AllreduceNew_Test : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_AllreduceNew_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_AllreduceNew_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        MPI_Barrier(MPI_COMM_WORLD);
        static s32  call_cnt = 0;
        DlTdtFunction::GetInstance().DlTdtFunctionInit();
        TsdOpen(1, 2);
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "UT_MPI_TEST");
        std::cout << "A MPI_AllreduceNew_Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        TsdClose(1);
        MPI_Barrier(MPI_COMM_WORLD);
        std::cout << "A MPI_AllreduceNew_Test TearDown" << std::endl;
    }
};


#define MPI_ALLREDUCE_SLICE_DATA_SIZE 1024*1024*2 +5
#define MPI_ALLREDUCE_DATA_SIZE 1256
#define MPI_ALLREDUCE_ALIGN_DATA_SIZE 215
#define GOUP_DEV_NUM 8

typedef struct para_struct
{
    HcclRootInfo rootInfo;
    std::string identify;
    s32 comm_num;
    s32 device_id;
    s32 ranks_local; //本服务器内的rank数

    char* file_name;
    void* sendbuff;
    void* recvbuff;
    s32 count;
    HcclDataType datatype;
    HcclReduceOp op;
    rtStream_t stream;
    rtStream_t stream1;
    int id;
    bool flag;
    volatile s32* sync_addr;
    const char* tag;
    char* groupName;
    u32 groupRanksNum;
    u32 *pGroupRanks;
} para_t;


int remove_dir(const char *dir) // 删除非空目录
{
    char cur_dir[] = ".";
    char up_dir[] = "..";
    char dir_name[128];
    DIR *dirp;
    struct dirent *dp;
    struct stat dir_stat;

    // 参数传递进来的目录不存在，直接返回
    if ( 0 != access(dir, F_OK) ) {
        return 0;
    }

    // 获取目录属性失败，返回错误
    if ( 0 > stat(dir, &dir_stat) ) {
        perror("get directory stat error");
        return -1;
    }

    if ( S_ISREG(dir_stat.st_mode) ) { // 普通文件直接删除
        remove(dir);
    } else if ( S_ISDIR(dir_stat.st_mode) ) { // 目录文件，递归删除目录中内容
        dirp = opendir(dir);
        while ( (dp=readdir(dirp)) != NULL ) {
        // 忽略 . 和 ..
        if ( (0 == strcmp(cur_dir, dp->d_name)) || (0 == strcmp(up_dir, dp->d_name)) ) {
            continue;
        }
        sprintf(dir_name, "%s/%s", dir, dp->d_name);
        remove_dir(dir_name);   // 递归调用
        }
        closedir(dirp);
        rmdir(dir); // 删除空目录
    } else {
        perror("unknow file type!");
    }
    return 0;
}
#if 1
void* inter_all_reduce_task(void* parg)
{
    HcclResult ret = HCCL_SUCCESS;
    para_t* para_info = (para_t*)parg;
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

    hcom_info.pComm.reset(new(std::nothrow) hccl::hcclComm());
    char* charModel = new char;
    rtModel_t model = (void*)charModel;
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);
    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task all_reduce falis", para_info->device_id);
    }
    //-----------------Set Workspace Resource Start------------------//
    u64 stream_list_size = 0;
    ret = hcom_info.pComm->GetWorkspaceSubStreamNum(stream_list_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("get stream_list_size[%d] success", stream_list_size);
    vector<HcclRtStream> streamList(stream_list_size);

    rtError_t rt_ret;
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

    u64 memSize = 0;
    ret = hcom_info.pComm->GetWorkspaceMemSize("HcomAllReduce", para_info->count, para_info->datatype, rankSize, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource("tag_inter_all_reduce", memptr, memSize, streamList);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//
    bool swapped;
    rank_num_tmp = *(para_info->sync_addr) - 1;

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

    ret =  hcom_info.pComm->AllReduce("tag_inter_all_reduce",
                               para_info->sendbuff,
                               para_info->recvbuff,
                               para_info->count,
                               para_info->datatype,
                               para_info->op,
                               para_info->stream);

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task HcclAllReduce falis", para_info->device_id);
    }

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

    delete charModel;
    charModel = nullptr;
    return (NULL);
}
#endif

void* inter_all_reduce_task_do_not_unbind(void* parg)
{
    HcclResult ret = HCCL_SUCCESS;
    para_t* para_info = (para_t*)parg;
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

    hcom_info.pComm.reset(new(std::nothrow) hccl::hcclComm());
    char* charModel = new char;
    rtModel_t model = (void*)charModel;
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);
    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task all_reduce falis", para_info->device_id);
    }
    //-----------------Set Workspace Resource Start------------------//
    u64 stream_list_size = 0;
    ret = hcom_info.pComm->GetWorkspaceSubStreamNum(stream_list_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("get stream_list_size[%d] success", stream_list_size);
    vector<HcclRtStream> streamList(stream_list_size);

    rtError_t rt_ret;
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

    u64 memSize = 0;
    ret = hcom_info.pComm->GetWorkspaceMemSize("HcomAllReduce", para_info->count, para_info->datatype, rankSize, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource("tag_inter_all_reduce_task_do_not", memptr, memSize, streamList);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//
    bool swapped;
    rank_num_tmp = *(para_info->sync_addr) - 1;

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

    ret =  hcom_info.pComm->AllReduce("tag_inter_all_reduce_task_do_not",
                               para_info->sendbuff,
                               para_info->recvbuff,
                               para_info->count,
                               para_info->datatype,
                               para_info->op,
                               para_info->stream);

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task HcclAllReduce falis", para_info->device_id);
    }

    rt_ret = rtStreamSynchronize(para_info->stream);

    if ( rt_ret != RT_ERROR_NONE)
    {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }
    //--------------Resource destroy----------------//
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtStreamDestroy(streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    hrtFree(memptr);

    delete charModel;
    charModel = nullptr;
    return (NULL);
}

void* inter_all_reduce_group_task(void* parg)
{
    HcclResult ret = HCCL_SUCCESS;
    para_t* para_info = (para_t*)parg;
    s32 rank_num_tmp;
    u32 uRankId = -1;
    rtError_t rt_ret = RT_ERROR_NONE;
    HcomInfo hcom_info;
    std::string ranktable_file = para_info->file_name;

    hrtSetDevice(para_info->device_id);
    ret = HcomInitByFile(para_info->file_name, para_info->identify.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    bool swapped;
    rank_num_tmp = *(para_info->sync_addr) - 1;

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

    ret =  HcomAllReduce("tag_inter_all_reduce_group_1",
                               para_info->sendbuff,
                               para_info->recvbuff,
                               para_info->count,
                               para_info->datatype,
                               para_info->op,
                               NULL,
                               para_info->stream1);

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task HcclAllReduce falis", para_info->device_id);
    }

    if(para_info->flag) {
        ret = HcomCreateGroup(para_info->groupName, para_info->groupRanksNum,(u32*)para_info->pGroupRanks);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        // bool swapped;
        rank_num_tmp = *(para_info->sync_addr) - 1;

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

        ret =  HcomAllReduce("tag_inter_all_reduce_group",
                                   para_info->sendbuff,
                                   para_info->recvbuff,
                                   para_info->count,
                                   para_info->datatype,
                                   para_info->op,
                                   para_info->groupName,
                                   para_info->stream);

        if (ret != HCCL_SUCCESS)
        {
            HCCL_ERROR("dev[%d] task HcclAllReduce falis", para_info->device_id);
        }

        rt_ret = rtStreamSynchronize(para_info->stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    rt_ret = rtStreamSynchronize(para_info->stream1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomDestroyGroup(para_info->groupName);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    if ( rt_ret != RT_ERROR_NONE)
    {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }

    return (NULL);
}





void* all_reduce_8p_subgroup_task(void* parg)
{
    HcclResult ret = HCCL_SUCCESS;
    InitEnvVarParam();
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

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr /** &rank_num */, rank_num_tmp, rank_num_tmp + 1);
    }
    while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local)
    {
        sched_yield(); // linux提供一个系统调用运行进程主动让出执行权
    }
    CommConfig commConfig("hccl_world_group");
    ret =  hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task HcclAllReduce falis", para_info->device_id);
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

    u64 memSize = 0;
    ret = hcom_info.pComm->GetWorkspaceMemSize("HCCL_KERNEL_OP_TYPE_ALLREDUCE", para_info->count, para_info->datatype, rankSize, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    //-----------------Set Workspace Resource End------------------//

    if(subGroupFlag) { //是否为group内
        hcom_info.pComm->CreateGroup(para_info->groupName, groupid, hcom_info.params.userRank, groupRanks, pSubComm);

        HCCL_INFO("wordGroupId[%d], groupid[%d], CreateGroup success!",wordGroupId, groupid);
        ret = pSubComm->SetWorkspaceResource("tag_all_reduce_8p_subgroup_1", memptr, memSize, streamList);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        ret =  pSubComm->AllReduce("tag_all_reduce_8p_subgroup_1",
                                   para_info->sendbuff,
                                   para_info->recvbuff,
                                   para_info->count,
                                   para_info->datatype,
                                   para_info->op,
                                   para_info->stream);
        subGroupFlag = false;
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("wordGroupId[%d], groupid[%d], all_reduce success!",wordGroupId, groupid);
         rt_ret = rtStreamSynchronize(para_info->stream);
         if ( rt_ret != RT_ERROR_NONE)
         {
            HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
         }
        HCCL_INFO("wordGroupId[%d], groupid[%d], rtStreamSynchronize success!",wordGroupId, groupid);
    }
    ret = hcom_info.pComm->DestroyGroup(para_info->groupName);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("wordGroupId[%d], groupid[%d], DestroyGroup success!",wordGroupId, groupid);
    //--------------Resource destroy----------------//
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtModelUnbindStream(model, streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    for (int i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtStreamDestroy(streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    hrtFree(memptr);

    return (NULL);
}

#define DEV_NUM_1 1
#define DEV_NUM_2 2
#define DEV_NUM_4 4
#define DEV_NUM_8 8


// ranktable 的读取，直接使用进程
nlohmann::json g_rank_table_16p_1910 = rank_table_910_2server_8rank;
// ranktable 的读取，直接使用进程
nlohmann::json g_rank_table_16p_V80 = rank_table_910_2server_8rank;

nlohmann::json rank_table_4nic_allreduce = rank_table_910_2server_8rank_4nic;
nlohmann::json rank_table_2nic_allreduce = rank_table_910_2server_8rank_2nic;
nlohmann::json rank_table_1nic_allreduce = rank_table_910_2server_8rank_1nic;

#if 1
TEST_F(MPI_AllreduceNew_Test, ut_mpi_allreduce_8p_ring_float32_binary_block_0_interring)
{
    nlohmann::json rank_table = rank_table_board3000_2server_8rank;
    char file_name_t[] = "./ut_mpi_allreduce_8ranks_2server.json";
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

   u32 groupRanks[GOUP_DEV_NUM] = {0,1,2,3,8,9,10,11};

    s32 nnode, rank, errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    float* result_buff[DEV_NUM_4];
    float** recvbuf = (float**)sal_malloc( sizeof(float**) *DEV_NUM_4 );
    float** sendbuf = (float**)sal_malloc( sizeof(float**) *DEV_NUM_4 );
    float* inputbuf[DEV_NUM_4];
    float* outputbuf[DEV_NUM_4];

    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];

    HcclDataType datatype = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = MPI_ALLREDUCE_SLICE_DATA_SIZE;
    s32 ndev = DEV_NUM_4;
    set_board_id(0x3000);
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(float), 0, count * sizeof(float));
        sal_memset(recvbuf[i], count  * sizeof(float), 0,  count * sizeof(float));

        result_buff[i] = (float*)sal_malloc(count * sizeof(float));
        sal_memset(result_buff[i], count * sizeof(float), 0, count * sizeof(float));

        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }
    //sendbuf 赋值
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = 1.0;
        }
    }

    //resultbuf 赋值
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = 2 * ndev * 1.0 ;
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
        //identify << i + ndev * noderank;
        identify << groupRanks[i+ ndev * noderank];
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        // para_info[i].group_id = i;
        para_info[i].ranks_local = ndev;
        para_info[i].op = op;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = inputbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = outputbuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
        para_info[i].groupRanksNum = GOUP_DEV_NUM;
        para_info[i].groupName = "group1" ;
        para_info[i].pGroupRanks = groupRanks;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8p_subgroup_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //获取stream的操作的同步信号量
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    bool subGroupFlag = false;

    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {

        u32 wordGroupId = std::stoi(para_info[j].identify);
        for(u32 i = 0;i < para_info->groupRanksNum;i++) {
            if(wordGroupId == para_info->pGroupRanks[i]) {
                subGroupFlag = true;
            }
        }
        if (subGroupFlag) {
            for (s32 i = 0; i < count; i++)
            {
                float res = result_buff[j][i];
                float recv = outputbuf[j][i];

                if (abs(res - recv) > 1e-6)
                {
                    HCCL_ERROR("worldgroupid[%d] noderank:%d recvbuf[%d][%d]:%f res[%d][%d]:%f\n",wordGroupId, noderank, j, i, recv,j,i,res);
                    errors++;
                    break;
                }
            }
        }
        subGroupFlag = false;
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

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
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        sal_free(result_buff[j]);


    }
    for (s32 j = 0; j < ndev; j++)
    {
        rt_ret = rtStreamDestroy(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    sal_free(sendbuf);
    sal_free(recvbuf);
    set_board_id(0);
    remove(file_name_t);

    EXPECT_EQ(errors, 0);
}
#endif
#if 1
TEST_F(MPI_AllreduceNew_Test, ut_mpi_allreduce_8ranks_2server_int32)
{
    setenv("PROFILING_MODE", "true", 1);
    nlohmann::json rank_table = rank_table_board3000_2server_8rank;
    char file_name_t[] = "./ut_mpi_allreduce_8ranks_2server_int32.json";
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

    u32 groupRanks[GOUP_DEV_NUM] = {0, 1, 2, 3, 8, 9, 10, 11};

    s32 nnode, rank, errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s32* result_buff[DEV_NUM_4];
    s32** recvbuf = (s32**)sal_malloc( sizeof(s32**) *DEV_NUM_4 );
    s32** sendbuf = (s32**)sal_malloc( sizeof(s32**) *DEV_NUM_4 );
    s32* inputBuf[DEV_NUM_4];
    s32* outputBuf[DEV_NUM_4];
    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];

    HcclDataType datatype = HCCL_DATA_TYPE_INT32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = MPI_ALLREDUCE_DATA_SIZE;
    s32 ndev = DEV_NUM_4;
    set_board_id(0x3000);
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(s32));
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(s32));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(s32), 0, count * sizeof(s32));
        sal_memset(recvbuf[i], count  * sizeof(s32), 0,  count * sizeof(s32));

        result_buff[i] = (s32*)sal_malloc(count * sizeof(s32));
        sal_memset(result_buff[i], count * sizeof(s32), 0, count * sizeof(s32));

        inputBuf[i] = sendbuf[i];
        outputBuf[i] = recvbuf[i];
    }
    //sendbuf 赋值
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputBuf[j][i] = 1;
        }
    }

    //resultbuf 赋值
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] =  8;
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
        //identify << i + ndev * noderank;
        identify << groupRanks[i+ ndev * noderank];
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        // para_info[i].group_id = i;
        para_info[i].ranks_local = ndev;
        para_info[i].op = op;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = inputBuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = outputBuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
        para_info[i].groupRanksNum = GOUP_DEV_NUM;
        para_info[i].groupName = "group1" ;
        para_info[i].pGroupRanks = groupRanks;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8p_subgroup_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //获取stream的操作的同步信号量
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    bool subGroupFlag = false;

    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {

        u32 wordGroupId = std::stoi(para_info[j].identify);
        for(u32 i = 0;i < para_info->groupRanksNum;i++) {
            if(wordGroupId == para_info->pGroupRanks[i]) {
                subGroupFlag = true;
            }
        }
        if (subGroupFlag) {
            for (s32 i = 0; i < count; i++)
            {
                s32 res = result_buff[j][i];
                s32 recv = outputBuf[j][i];

                if ( res != recv )
                {
                    HCCL_ERROR("worldgroupid[%d] noderank:%d recvbuf[%d][%d]:%f res[%d][%d]:%f\n",wordGroupId, noderank, j, i, recv,j,i,res);
                    errors++;
                    break;
                }
            }
        }
        subGroupFlag = false; 
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

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
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        sal_free(result_buff[j]);


    }
    for (s32 j = 0; j < ndev; j++)
    {
        rt_ret = rtStreamDestroy(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    sal_free(sendbuf);
    sal_free(recvbuf);
    set_board_id(0);
    remove(file_name_t);
    setenv("PROFILING_MODE", "false", 1);
    EXPECT_EQ(errors, 0);
}
#endif
#if 1
TEST_F(MPI_AllreduceNew_Test, ut_mpi_allreduce_8ranks_2server_tiny_size)
{
    setenv("PROFILING_MODE", "true", 1);
    nlohmann::json rank_table = rank_table_board3000_2server_8rank;
    char file_name_t[] = "./ut_mpi_allreduce_8ranks_2server_int32.json";
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

    u32 groupRanks[GOUP_DEV_NUM] = {0, 1, 2, 3, 8, 9, 10, 11};

    s32 nnode, rank, errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s32* result_buff[DEV_NUM_4];
    s32** recvbuf = (s32**)sal_malloc( sizeof(s32**) *DEV_NUM_4 );
    s32** sendbuf = (s32**)sal_malloc( sizeof(s32**) *DEV_NUM_4 );
    s32* inputBuf[DEV_NUM_4];
    s32* outputBuf[DEV_NUM_4];
    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];

    HcclDataType datatype = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = 1;
    s32 ndev = DEV_NUM_4;
    set_board_id(0x3000);
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(s32));
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(s32));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(s32), 0, count * sizeof(s32));
        sal_memset(recvbuf[i], count  * sizeof(s32), 0,  count * sizeof(s32));

        result_buff[i] = (s32*)sal_malloc(count * sizeof(s32));
        sal_memset(result_buff[i], count * sizeof(s32), 0, count * sizeof(s32));

        inputBuf[i] = sendbuf[i];
        outputBuf[i] = recvbuf[i];
    }
    //sendbuf 赋值
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputBuf[j][i] = 1;
        }
    }

    //resultbuf 赋值
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] =  8;
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
        //identify << i + ndev * noderank;
        identify << groupRanks[i+ ndev * noderank];
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        // para_info[i].group_id = i;
        para_info[i].ranks_local = ndev;
        para_info[i].op = op;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = inputBuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = outputBuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
        para_info[i].groupRanksNum = GOUP_DEV_NUM;
        para_info[i].groupName = "group1" ;
        para_info[i].pGroupRanks = groupRanks;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8p_subgroup_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //获取stream的操作的同步信号量
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    bool subGroupFlag = false;

    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {

        u32 wordGroupId = std::stoi(para_info[j].identify);
        for(u32 i = 0;i < para_info->groupRanksNum;i++) {
            if(wordGroupId == para_info->pGroupRanks[i]) {
                subGroupFlag = true;
            }
        }
        if (subGroupFlag) {
            for (s32 i = 0; i < count; i++)
            {
                s32 res = result_buff[j][i];
                s32 recv = outputBuf[j][i];

                if ( res != recv )
                {
                    HCCL_ERROR("worldgroupid[%d] noderank:%d recvbuf[%d][%d]:%f res[%d][%d]:%f\n",wordGroupId, noderank, j, i, recv,j,i,res);
                    errors++;
                    break;
                }
            }
        }
        subGroupFlag = false; 
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

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
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        sal_free(result_buff[j]);


    }
    for (s32 j = 0; j < ndev; j++)
    {
        rt_ret = rtStreamDestroy(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    sal_free(sendbuf);
    sal_free(recvbuf);
    set_board_id(0);
    remove(file_name_t);
    setenv("PROFILING_MODE", "false", 1);
    EXPECT_EQ(errors, 0);
}
#endif

#if 1
TEST_F(MPI_AllreduceNew_Test, ut_mpi_allreduce_8ranks_2server_ring)
{
    
    int retEnv = setenv("PROFILING_MODE", "true", 1); // 此用例开启Profiling

    retEnv = setenv("PROFILING_OPTIONS", "{\"output\":\"./\",\"training_trace\":\"on\",\"fp_point\":\"\",\"bp_point\":\"\",\"task_trace\":\"on\",\"ai_core_metrics\":\"ArithmeticUtilization\",\"aicpu_trace\":\"on\"}", 1); // 此用例开启Profiling
    char file_name_t[] = "./ut_mpi_allreduce_8ranks_2server_ring.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << g_rank_table_16p_V80 << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

   u32 groupRanks[GOUP_DEV_NUM] = {0,1,2,3,8,9,10,11};

    s32 nnode, rank, errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s8* result_buff[DEV_NUM_4];
    s8* sendbuf[DEV_NUM_4];
    s8* recvbuf[DEV_NUM_4];
    s8* inputBuf[DEV_NUM_4];
    s8* outputBuf[DEV_NUM_4];
    set_board_id(0);
    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = 1000;
    FailureInjectStub(1, TASK_TYPE_NOTIFY_WAIT);
    s32 ndev = DEV_NUM_4;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode ?mpi???,??????
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    /** ????????? */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i],  count * sizeof(s8), 0,  count * sizeof(s8));
        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count  * sizeof(s8), 0,  count * sizeof(s8));
        ret = hrtMalloc((void**)&result_buff[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(result_buff[i], count * sizeof(s8), 0, count * sizeof(s8));
        inputBuf[i] = sendbuf[i];
        outputBuf[i] = recvbuf[i];
    }

    //sendbuf ??
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputBuf[j][i] = 1;
        }
    }

    //resultbuf ??
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = nnode * ndev;
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
        //identify << i + ndev * noderank;
        identify << groupRanks[i+ ndev * noderank];
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        // para_info[i].group_id = i;
        para_info[i].ranks_local = ndev;
        para_info[i].op = op;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = inputBuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = outputBuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
        para_info[i].groupRanksNum = GOUP_DEV_NUM;
        para_info[i].groupName = "group1" ;
        para_info[i].pGroupRanks = groupRanks;
    }

    // ????Dev?allreduce????
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8p_subgroup_task, (void*)&para_info[i]);
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

    MPI_Barrier(MPI_COMM_WORLD);
    bool subGroupFlag = false;

    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {

        u32 wordGroupId = std::stoi(para_info[j].identify);
        for(u32 i = 0;i < para_info->groupRanksNum;i++) {
            if(wordGroupId == para_info->pGroupRanks[i]) {
                subGroupFlag = true;
            }
        }
        if (subGroupFlag) {
            for (s32 i = 0; i < count; i++)
            {
                s8 res = result_buff[j][i];
                s8 recv = outputBuf[j][i];

                if (res != recv)
                {
                    HCCL_ERROR("worldgroupid[%d] noderank:%d recvbuf[%d][%d]:%f res[%d][%d]:%f\n",wordGroupId, noderank, j, i, recv,j,i,res);

                    errors++;
                    break;
                }
            }
        }
        subGroupFlag = false;
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

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
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        hrtFree(result_buff[j]);
    }
    for (s32 j = 0; j < ndev; j++)
    {
        rt_ret = rtStreamDestroy(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0);
    remove(file_name_t);
    unsetenv("PROFILING_MODE"); // 此用例开启Profiling
    unsetenv("PROFILING_OPTIONS"); // 此用例开启Profiling
    EXPECT_EQ(errors, 0);
}
#endif

#if 1
TEST_F(MPI_AllreduceNew_Test, ut_mpi_allreduce_8ranks_2server_ring_DMA)
{
    
    
    char file_name_t[] = "./ut_mpi_allreduce_8ranks_2server_ring.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << g_rank_table_16p_V80 << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

   u32 groupRanks[GOUP_DEV_NUM] = {0,1,2,3,8,9,10,11};

    s32 nnode, rank, errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s8* result_buff[DEV_NUM_4];
    s8* sendbuf[DEV_NUM_4];
    s8* recvbuf[DEV_NUM_4];
    s8* inputBuf[DEV_NUM_4];
    s8* outputBuf[DEV_NUM_4];
    set_board_id(0);
    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = 1000;
    FailureInjectStub(1, TASK_TYPE_MEMCPY);
    s32 ndev = DEV_NUM_4;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode ?mpi???,??????
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    /** ????????? */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i],  count * sizeof(s8), 0,  count * sizeof(s8));
        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count  * sizeof(s8), 0,  count * sizeof(s8));
        ret = hrtMalloc((void**)&result_buff[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(result_buff[i], count * sizeof(s8), 0, count * sizeof(s8));
        inputBuf[i] = sendbuf[i];
        outputBuf[i] = recvbuf[i];
    }

    //sendbuf ??
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputBuf[j][i] = 1;
        }
    }

    //resultbuf ??
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = nnode * ndev;
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
        //identify << i + ndev * noderank;
        identify << groupRanks[i+ ndev * noderank];
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        // para_info[i].group_id = i;
        para_info[i].ranks_local = ndev;
        para_info[i].op = op;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = inputBuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = outputBuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
        para_info[i].groupRanksNum = GOUP_DEV_NUM;
        para_info[i].groupName = "group1" ;
        para_info[i].pGroupRanks = groupRanks;
    }

    // ????Dev?allreduce????
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8p_subgroup_task, (void*)&para_info[i]);
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

    MPI_Barrier(MPI_COMM_WORLD);
    bool subGroupFlag = false;

    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {

        u32 wordGroupId = std::stoi(para_info[j].identify);
        for(u32 i = 0;i < para_info->groupRanksNum;i++) {
            if(wordGroupId == para_info->pGroupRanks[i]) {
                subGroupFlag = true;
            }
        }
        if (subGroupFlag) {
            for (s32 i = 0; i < count; i++)
            {
                s8 res = result_buff[j][i];
                s8 recv = outputBuf[j][i];

                if (res != recv)
                {
                    HCCL_ERROR("worldgroupid[%d] noderank:%d recvbuf[%d][%d]:%f res[%d][%d]:%f\n",wordGroupId, noderank, j, i, recv,j,i,res);

                    errors++;
                    break;
                }
            }
        }
        subGroupFlag = false;
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

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
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        hrtFree(result_buff[j]);
    }
    for (s32 j = 0; j < ndev; j++)
    {
        rt_ret = rtStreamDestroy(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0);
    remove(file_name_t);
    unsetenv("PROFILING_MODE"); // 此用例开启Profiling
    unsetenv("PROFILING_OPTIONS"); // 此用例开启Profiling
    EXPECT_EQ(errors, 0);
}
#endif
#if 1
TEST_F(MPI_AllreduceNew_Test, ut_mpi_allreduce_8ranks_2server_ring_reduce)
{
    
  
    char file_name_t[] = "./ut_mpi_allreduce_8ranks_2server_ring.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << g_rank_table_16p_V80 << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

   u32 groupRanks[GOUP_DEV_NUM] = {0,1,2,3,8,9,10,11};

    s32 nnode, rank, errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s8* result_buff[DEV_NUM_4];
    s8* sendbuf[DEV_NUM_4];
    s8* recvbuf[DEV_NUM_4];
    s8* inputBuf[DEV_NUM_4];
    s8* outputBuf[DEV_NUM_4];
    set_board_id(0);
    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = 1000;
    FailureInjectStub(1, TASK_TYPE_REDUCE);
    s32 ndev = DEV_NUM_4;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode ?mpi???,??????
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    /** ????????? */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i],  count * sizeof(s8), 0,  count * sizeof(s8));
        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count  * sizeof(s8), 0,  count * sizeof(s8));
        ret = hrtMalloc((void**)&result_buff[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(result_buff[i], count * sizeof(s8), 0, count * sizeof(s8));
        inputBuf[i] = sendbuf[i];
        outputBuf[i] = recvbuf[i];
    }

    //sendbuf ??
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputBuf[j][i] = 1;
        }
    }

    //resultbuf ??
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = nnode * ndev;
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
        //identify << i + ndev * noderank;
        identify << groupRanks[i+ ndev * noderank];
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        // para_info[i].group_id = i;
        para_info[i].ranks_local = ndev;
        para_info[i].op = op;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = inputBuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = outputBuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
        para_info[i].groupRanksNum = GOUP_DEV_NUM;
        para_info[i].groupName = "group1" ;
        para_info[i].pGroupRanks = groupRanks;
    }

    // ????Dev?allreduce????
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8p_subgroup_task, (void*)&para_info[i]);
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

    MPI_Barrier(MPI_COMM_WORLD);
    bool subGroupFlag = false;

    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {

        u32 wordGroupId = std::stoi(para_info[j].identify);
        for(u32 i = 0;i < para_info->groupRanksNum;i++) {
            if(wordGroupId == para_info->pGroupRanks[i]) {
                subGroupFlag = true;
            }
        }
        if (subGroupFlag) {
            for (s32 i = 0; i < count; i++)
            {
                s8 res = result_buff[j][i];
                s8 recv = outputBuf[j][i];

                if (res != recv)
                {
                    HCCL_ERROR("worldgroupid[%d] noderank:%d recvbuf[%d][%d]:%f res[%d][%d]:%f\n",wordGroupId, noderank, j, i, recv,j,i,res);

                    errors++;
                    break;
                }
            }
        }
        subGroupFlag = false;
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

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
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        hrtFree(result_buff[j]);
    }
    for (s32 j = 0; j < ndev; j++)
    {
        rt_ret = rtStreamDestroy(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0);
    remove(file_name_t);
    unsetenv("PROFILING_MODE"); // 此用例开启Profiling
    unsetenv("PROFILING_OPTIONS"); // 此用例开启Profiling
    EXPECT_EQ(errors, 0);
}
#endif
#if 1
TEST_F(MPI_AllreduceNew_Test, ut_mpi_allreduce_8ranks_2server_float32_binary_block_0)
{
    char file_name_t[] = "./ut_mpi_allreduce_8ranks_2server_float32_binary_block_0.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << g_rank_table_16p_V80 << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();
    ResetInitState();
    s32 nPID = SalGetPid();
    string strPID = std::to_string(nPID);
    string config_file_name = "./config_file_st_mpi_allreduce_8ranks_2server_float32_binary_block_0" + strPID;
    ResetConfigFilePathName(config_file_name);
    remove(config_file_name.c_str());
    ofstream configfile(config_file_name);
    bool bRes = configfile.is_open();
    if(bRes)
    {
        HCCL_INFO("open %s success", config_file_name.c_str());
    }
    else
    {
        HCCL_ERROR("open %s failed", config_file_name.c_str());
    }
    EXPECT_EQ(bRes, true);
    configfile << "halvingDoublingType=0" << endl;
    configfile.close();
   u32 groupRanks[GOUP_DEV_NUM] = {0,1,2,3,8,9,10,11};

    s32 nnode, rank, errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    float* result_buff[DEV_NUM_4];
    float** recvbuf = (float**)sal_malloc( sizeof(float**) *DEV_NUM_4 );
    float** sendbuf = (float**)sal_malloc( sizeof(float**) *DEV_NUM_4 );
    float* inputbuf[DEV_NUM_4];
    float* outputbuf[DEV_NUM_4];
    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];
    HcclDataType datatype = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = MPI_ALLREDUCE_SLICE_DATA_SIZE;
    s32 ndev = DEV_NUM_4;

    set_board_id(0);
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(float), 0, count * sizeof(float));
        sal_memset(recvbuf[i], count  * sizeof(float), 0,  count * sizeof(float));

        result_buff[i] = (float*)sal_malloc(count * sizeof(float));
        sal_memset(result_buff[i], count * sizeof(float), 0, count * sizeof(float));

        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = 1.0;
        }
    }

    //resultbuf 赋值
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = 2 * ndev * 1.0 ;
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
        //identify << i + ndev * noderank;
        identify << groupRanks[i+ ndev * noderank];
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        // para_info[i].group_id = i;
        para_info[i].ranks_local = ndev;
        para_info[i].op = op;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = inputbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = outputbuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
        para_info[i].groupRanksNum = GOUP_DEV_NUM;
        para_info[i].groupName = "group1" ;
        para_info[i].pGroupRanks = groupRanks;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8p_subgroup_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //获取stream的操作的同步信号量
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    s32 resRemove = remove(config_file_name.c_str());
    EXPECT_EQ(resRemove, 0);
    configfile.open(config_file_name);
    bRes = configfile.is_open();
    if(bRes)
    {
        HCCL_INFO("open %s success", config_file_name.c_str());
    }
    else
    {
        HCCL_ERROR("open %s failed", config_file_name.c_str());
    }
    EXPECT_EQ(bRes, true);
    configfile << "halvingDoublingType=1" << endl;
    configfile.close();
    InitConfigFileParam(config_file_name);
    resRemove = remove(config_file_name.c_str());
    EXPECT_EQ(resRemove, 0);
    ResetInitState();

    MPI_Barrier(MPI_COMM_WORLD);
    bool subGroupFlag = false;

    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {

        u32 wordGroupId = std::stoi(para_info[j].identify);
        for(u32 i = 0;i < para_info->groupRanksNum;i++) {
            if(wordGroupId == para_info->pGroupRanks[i]) {
                subGroupFlag = true;
            }
        }
        if (subGroupFlag) {
            for (s32 i = 0; i < count; i++)
            {
                float res = result_buff[j][i];
                float recv = outputbuf[j][i];

                if (abs(res - recv) > 1e-6)
                {
                    HCCL_ERROR("worldgroupid[%d] noderank:%d recvbuf[%d][%d]:%f res[%d][%d]:%f\n",wordGroupId, noderank, j, i, recv,j,i,res);

                    errors++;
                    break;
                }
            }
        }
        subGroupFlag = false;
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

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
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        sal_free(result_buff[j]);


    }
    for (s32 j = 0; j < ndev; j++)
    {
        rt_ret = rtStreamDestroy(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0);
    sal_free(sendbuf);
    sal_free(recvbuf);
    remove(file_name_t);

    EXPECT_EQ(errors, 0);
}
#endif
#if 1
TEST_F(MPI_AllreduceNew_Test, ut_mpi_allreduce_8ranks_2server_float32_binary_block_1)
{
       char file_name_t[] = "./ut_mpi_allreduce_8ranks_2server_float32_binary_block_1.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << g_rank_table_16p_V80 << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();
    ResetInitState();
    s32 nPID = SalGetPid();
    string strPID = std::to_string(nPID);
    string config_file_name = "./config_file_st_mpi_allreduce_8ranks_2server_float32_binary_block_1" + strPID;
    ResetConfigFilePathName(config_file_name);
    remove(config_file_name.c_str());
    ofstream configfile(config_file_name);
    bool bRes = configfile.is_open();
    if(bRes)
    {
        HCCL_INFO("open %s success", config_file_name.c_str());
    }
    else
    {
        HCCL_ERROR("open %s failed", config_file_name.c_str());
    }
    EXPECT_EQ(bRes, true);
    configfile << "halvingDoublingType=0" << endl;
    configfile.close();
   u32 groupRanks[GOUP_DEV_NUM] = {0,1,2,3,8,9,10,11};

    s32 nnode, rank, errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    float* result_buff[DEV_NUM_4];
    float** recvbuf = (float**)sal_malloc( sizeof(float**) *DEV_NUM_4 );
    float** sendbuf = (float**)sal_malloc( sizeof(float**) *DEV_NUM_4 );
    float* inputbuf[DEV_NUM_8];
    float* outputbuf[DEV_NUM_8];
    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];
    HcclDataType datatype = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = MPI_ALLREDUCE_SLICE_DATA_SIZE;
    s32 ndev = DEV_NUM_4;

    set_board_id(0);
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(float), 0, count * sizeof(float));
        sal_memset(recvbuf[i], count  * sizeof(float), 0,  count * sizeof(float));

        result_buff[i] = (float*)sal_malloc(count * sizeof(float));
        sal_memset(result_buff[i], count * sizeof(float), 0, count * sizeof(float));

        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = 1.0;
        }
    }

    //resultbuf 赋值
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = 2 * ndev * 1.0 ;
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
        //identify << i + ndev * noderank;
        identify << groupRanks[i+ ndev * noderank];
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        // para_info[i].group_id = i;
        para_info[i].ranks_local = ndev;
        para_info[i].op = op;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = inputbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = outputbuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
        para_info[i].groupRanksNum = GOUP_DEV_NUM;
        para_info[i].groupName = "group1" ;
        para_info[i].pGroupRanks = groupRanks;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8p_subgroup_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //获取stream的操作的同步信号量
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    s32 resRemove = remove(config_file_name.c_str());
    EXPECT_EQ(resRemove, 0);
    configfile.open(config_file_name);
    bRes = configfile.is_open();
    if(bRes)
    {
        HCCL_INFO("open %s success", config_file_name.c_str());
    }
    else
    {
        HCCL_ERROR("open %s failed", config_file_name.c_str());
    }
    EXPECT_EQ(bRes, true);
    configfile << "halvingDoublingType=1" << endl;
    configfile.close();
    InitConfigFileParam(config_file_name);
    resRemove = remove(config_file_name.c_str());
    EXPECT_EQ(resRemove, 0);
    ResetInitState();

    MPI_Barrier(MPI_COMM_WORLD);
    bool subGroupFlag = false;

    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {

        u32 wordGroupId = std::stoi(para_info[j].identify);
        for(u32 i = 0;i < para_info->groupRanksNum;i++) {
            if(wordGroupId == para_info->pGroupRanks[i]) {
                subGroupFlag = true;
            }
        }
        if (subGroupFlag) {
            for (s32 i = 0; i < count; i++)
            {
                float res = result_buff[j][i];
                float recv = outputbuf[j][i];

                if (abs(res - recv) > 1e-6)
                {
                    HCCL_ERROR("worldgroupid[%d] noderank:%d recvbuf[%d][%d]:%f res[%d][%d]:%f\n",wordGroupId, noderank, j, i, recv,j,i,res);

                    errors++;
                    break;
                }
            }
        }
        subGroupFlag = false;
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

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
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        sal_free(result_buff[j]);


    }
    for (s32 j = 0; j < ndev; j++)
    {
        rt_ret = rtStreamDestroy(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0);
    sal_free(sendbuf);
    sal_free(recvbuf);
    remove(file_name_t);

    EXPECT_EQ(errors, 0);
}
#endif
#if 1
TEST_F(MPI_AllreduceNew_Test, ut_mpi_allreduce_8p_ring_float32_binary_block_0)
{
    nlohmann::json rank_table = rank_table_910_2server_8rank;
    char file_name_t[] = "./st_mpi_allreduce_8p_ring_float32_binary_block_0.json";
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

    ResetInitState();
    s32 nPID = SalGetPid();
    string strPID = std::to_string(nPID);
    string config_file_name = "./config_file_st_mpi_allreduce_8p_ring_float32_binary_block_0" + strPID;
    ResetConfigFilePathName(config_file_name);
    remove(config_file_name.c_str());
    ofstream configfile(config_file_name);
    bool bRes = configfile.is_open();
    if(bRes)
    {
        HCCL_INFO("open %s success", config_file_name.c_str());
    }
    else
    {
        HCCL_ERROR("open %s failed", config_file_name.c_str());
    }
    EXPECT_EQ(bRes, true);
    configfile << "halvingDoublingType=0" << endl;
    configfile.close();
    setenv("HCCL_CONNECT_TIMEOUT", "180", 1);
    s32 nnode, rank, errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    float* result_buff[DEV_NUM_8];
    float** recvbuf = (float**)sal_malloc( sizeof(float**) *DEV_NUM_8 );
    float** sendbuf = (float**)sal_malloc( sizeof(float**) *DEV_NUM_8 );
    float* inputbuf[DEV_NUM_8];
    float* outputbuf[DEV_NUM_8];
    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];
    HcclDataType datatype = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = MPI_ALLREDUCE_SLICE_DATA_SIZE;
    s32 ndev = DEV_NUM_8;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(float), 0, count * sizeof(float));
        sal_memset(recvbuf[i], count  * sizeof(float), 0,  count * sizeof(float));

        result_buff[i] = (float*)sal_malloc(count * sizeof(float));
        sal_memset(result_buff[i], count * sizeof(float), 0, count * sizeof(float));

        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = 1.0;
        }
    }

    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = 2 * ndev * 1.0 ;
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
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;
        para_info[i].op = op;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = inputbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = outputbuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_all_reduce_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    s32 resRemove = remove(config_file_name.c_str());
    EXPECT_EQ(resRemove, 0);
    configfile.open(config_file_name);
    bRes = configfile.is_open();
    if(bRes)
    {
        HCCL_INFO("open %s success", config_file_name.c_str());
    }
    else
    {
        HCCL_ERROR("open %s failed", config_file_name.c_str());
    }
    EXPECT_EQ(bRes, true);
    configfile << "halvingDoublingType=1" << endl;
    configfile.close();
    InitConfigFileParam(config_file_name);
    resRemove = remove(config_file_name.c_str());
    EXPECT_EQ(resRemove, 0);
    ResetInitState();

    MPI_Barrier(MPI_COMM_WORLD);

    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count; i++)
        {
            float res = result_buff[j][i];
            float recv = outputbuf[j][i];

            if (abs(res - recv) > 1e-6)
            {
                HCCL_ERROR("node:%d result[%d][%d]:%f sendbuf:%f\n", noderank, j, i, outputbuf[j][i] , inputbuf[j][i]);
                errors++;
                break;
            }
        }
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

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
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        sal_free(result_buff[j]);
        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
	unsetenv("HCCL_CONNECT_TIMEOUT");
    remove(file_name_t);
    sal_free(sendbuf);
    sal_free(recvbuf);
    EXPECT_EQ(errors, 0);
}
#endif

#if 1
TEST_F(MPI_AllreduceNew_Test, ut_mpi_allreduce_8ranks_2server_char_sum_ring_checksum)
{
    char file_name_t[] = "./ut_mpi_allreduce_8ranks_2server_char_sum_ring_checksum.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << g_rank_table_16p_V80 << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

     //  写入配置文件参数
    ResetInitState();
    string strPID = std::to_string(SalGetPid());
    string config_file_name = "./config_file_ut_mpi_allreduce_8ranks_2server_char_sum_ring_checksum" + strPID;
    ResetConfigFilePathName(config_file_name);

    ofstream configfile(config_file_name);
    bool bRes = configfile.is_open();
    if(bRes)
    {
        HCCL_INFO("open %s success", config_file_name.c_str());
    }
    else
    {
        HCCL_ERROR("open %s failed", config_file_name.c_str());
    }
    EXPECT_EQ(bRes, true);
    configfile << "ResultCheckSumSwitch=1" << endl;
    configfile.close();

    u32 groupRanks[GOUP_DEV_NUM] = {0,1,2,3,8,9,10,11};

    s32 nnode, rank, errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s8* result_buff[DEV_NUM_4];
    s8* sendbuf[DEV_NUM_4];
    s8* recvbuf[DEV_NUM_4];
    s8* inputbuf[DEV_NUM_4];
    s8* outputbuf[DEV_NUM_4];
    set_board_id(0);
    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = 11;

    s32 ndev = DEV_NUM_4;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode ?mpi???,??????
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i],  count * sizeof(s8), 0,  count * sizeof(s8));
        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count  * sizeof(s8), 0,  count * sizeof(s8));
        ret = hrtMalloc((void**)&result_buff[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(result_buff[i], count * sizeof(s8), 0, count * sizeof(s8));

        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    //sendbuf 赋值
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = 3;
        }
    }

    //resultbuf 赋值
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = nnode * ndev*3;
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
        //identify << i + ndev * noderank;
        identify << groupRanks[i+ ndev * noderank];
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        // para_info[i].group_id = i;
        para_info[i].ranks_local = ndev;
        para_info[i].op = op;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = inputbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = outputbuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
        para_info[i].groupRanksNum = GOUP_DEV_NUM;
        para_info[i].groupName = "group1" ;
        para_info[i].pGroupRanks = groupRanks;
    }

    // ????Dev?allreduce????
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8p_subgroup_task, (void*)&para_info[i]);
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

    s32 resRemove = remove(config_file_name.c_str());
    EXPECT_EQ(resRemove, 0);
    configfile.open(config_file_name);
    bRes = configfile.is_open();
    if(bRes)
    {
        HCCL_INFO("open %s success", config_file_name.c_str());
    }
    else
    {
        HCCL_ERROR("open %s failed", config_file_name.c_str());
    }
    EXPECT_EQ(bRes, true);
    configfile << "ResultCheckSumSwitch=0" << endl;
    configfile.close();
    InitConfigFileParam(config_file_name);
    resRemove = remove(config_file_name.c_str());
    EXPECT_EQ(resRemove, 0);
    ResetInitState();

    MPI_Barrier(MPI_COMM_WORLD);
    bool subGroupFlag = false;

    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {

        u32 wordGroupId = std::stoi(para_info[j].identify);
        for(u32 i = 0;i < para_info->groupRanksNum;i++) {
            if(wordGroupId == para_info->pGroupRanks[i]) {
                subGroupFlag = true;
            }
        }
        if (subGroupFlag) {
            for (s32 i = 0; i < count; i++)
            {
                s8 res = result_buff[j][i];
                s8 recv = outputbuf[j][i];
                HCCL_INFO("worldgroupid[%d] noderank:%d recvbuf[%d][%d]:%d res[%d][%d]:%d\n",wordGroupId, noderank, j, i, recv,j,i,res);
                if (res != recv)
                {
                    HCCL_ERROR("worldgroupid[%d] noderank:%d recvbuf[%d][%d]:%d res[%d][%d]:%d\n",wordGroupId, noderank, j, i, recv,j,i,res);

                    errors++;
                    break;
                }
            }
        }
        subGroupFlag = false;
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

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
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        hrtFree(result_buff[j]);

    }
    for (s32 j = 0; j < ndev; j++)
    {
        rt_ret = rtStreamDestroy(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0);
    remove(file_name_t);

    EXPECT_EQ(errors, 0);
}
#endif

#if 1
TEST_F(MPI_AllreduceNew_Test, ut_mpi_allreduce_8p_ring_s8_step_mem_sampler)
{
    nlohmann::json rank_table = rank_table_910_2server_8rank;
    char file_name_t[] = "./ut_mpi_allreduce_8p_ring_s8_step_mem_sampler.json";
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
    ResetInitState();
    setenv("HCCL_MEM_SAMPLER_PARAM", "0x20000000", 1);

    s32 nnode, rank, errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s8* result_buff[DEV_NUM_8];
    s8** recvbuf = (s8**)sal_malloc( sizeof(s8**) *DEV_NUM_8 );
    s8** sendbuf = (s8**)sal_malloc( sizeof(s8**) *DEV_NUM_8 );
    s8* inputbuf[DEV_NUM_8];
    s8* outputbuf[DEV_NUM_8];
    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];
    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = 128;
    s32 ndev = DEV_NUM_8;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(s8), 0, count * sizeof(s8));
        sal_memset(recvbuf[i], count  * sizeof(s8), 0,  count * sizeof(s8));

        result_buff[i] = (s8*)sal_malloc(count * sizeof(s8));
        sal_memset(result_buff[i], count * sizeof(s8), 0, count * sizeof(s8));

        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = 1;
        }
    }

    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] =  1*nnode*ndev;
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
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;
        para_info[i].op = op;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = inputbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = outputbuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_all_reduce_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    unsetenv("HCCL_MEM_SAMPLER_PARAM");
    ResetInitState();

    MPI_Barrier(MPI_COMM_WORLD);

    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count; i++)
        {
            float res = result_buff[j][i];
            float recv = outputbuf[j][i];

            if (res != recv)
            {
                errors++;
                break;
            }
        }
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

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
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        sal_free(result_buff[j]);
        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    sal_free(sendbuf);
    sal_free(recvbuf);
    EXPECT_EQ(errors, 0);
}
#endif

#if 1
TEST_F(MPI_AllreduceNew_Test, ut_mpi_allreduce_8p_ring_s8_step_mem_sampler_destruction)
{
    nlohmann::json rank_table = rank_table_910_2server_8rank;
    char file_name_t[] = "./ut_mpi_allreduce_8p_ring_s8_step_mem_sampler.json";
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
    ResetInitState();
    setenv("HCCL_MEM_SAMPLER_PARAM", "0x20000000", 1);

    s32 nnode, rank, errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s8* result_buff[DEV_NUM_8];
    s8** recvbuf = (s8**)sal_malloc( sizeof(s8**) *DEV_NUM_8 );
    s8** sendbuf = (s8**)sal_malloc( sizeof(s8**) *DEV_NUM_8 );
    s8* inputbuf[DEV_NUM_8];
    s8* outputbuf[DEV_NUM_8];
    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];
    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = 128;
    s32 ndev = DEV_NUM_8;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(s8), 0, count * sizeof(s8));
        sal_memset(recvbuf[i], count  * sizeof(s8), 0,  count * sizeof(s8));

        result_buff[i] = (s8*)sal_malloc(count * sizeof(s8));
        sal_memset(result_buff[i], count * sizeof(s8), 0, count * sizeof(s8));

        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = 1;
        }
    }

    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] =  1*nnode*ndev;
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
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;
        para_info[i].op = op;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = inputbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = outputbuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_all_reduce_task_do_not_unbind, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    unsetenv("HCCL_MEM_SAMPLER_PARAM");
    ResetInitState();

    MPI_Barrier(MPI_COMM_WORLD);

    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count; i++)
        {
            float res = result_buff[j][i];
            float recv = outputbuf[j][i];

            if (res != recv)
            {
                errors++;
                break;
            }
        }
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

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
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        sal_free(result_buff[j]);
        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    sal_free(sendbuf);
    sal_free(recvbuf);
    EXPECT_EQ(errors, 0);
}
#endif

#if 1
TEST_F(MPI_AllreduceNew_Test, ut_mpi_allreduce_8p_ring_fp16_sum_padding)
{
    nlohmann::json rank_table = rank_table_910_2server_8rank;
    char file_name_t[] = "./ut_mpi_allreduce_8p_ring_fp16_sum_padding.json"; 
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
    setenv("HCCL_CONNECT_TIMEOUT", "180", 1);
    s32 nnode, rank, errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    u16* result_buff[DEV_NUM_8];
    u16* recvbuf[DEV_NUM_8];
    u16* sendbuf[DEV_NUM_8];
    u16* inputbuf[DEV_NUM_8];
    u16* outputbuf[DEV_NUM_8];
    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];
    HcclDataType datatype = HCCL_DATA_TYPE_FP16;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = 128;
    s32 ndev = DEV_NUM_8;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&inputbuf[i], count * sizeof(u16));
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = hrtMalloc((void**)&outputbuf[i], count * sizeof(u16));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(inputbuf[i], count * sizeof(u16), 0, count * sizeof(u16));
        sal_memset(outputbuf[i], count  * sizeof(u16), 0,  count * sizeof(u16));

        result_buff[i] = (u16*)sal_malloc(count * sizeof(u16));
        sal_memset(result_buff[i], count * sizeof(u16), 0, count * sizeof(u16));

        sendbuf[i] = inputbuf[i];
        recvbuf[i] = outputbuf[i];
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[j][i] = fp16_ieee_from_fp32_value(1.0f);
        }
    }

    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] =  fp16_ieee_from_fp32_value(1.0f*nnode*ndev);
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
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;
        para_info[i].op = op;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_all_reduce_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count; i++)
        {
            u16 res = result_buff[j][i];
            u16 recv = recvbuf[j][i];

            if (res != recv)
            {
                errors++;
                break;
            }
        }
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

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
        hrtFree(inputbuf[j]);
        hrtFree(outputbuf[j]);
        sal_free(result_buff[j]);
        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
	unsetenv("HCCL_CONNECT_TIMEOUT");
 //   sal_free(sendbuf);
 //   sal_free(recvbuf);
    EXPECT_EQ(errors, 0);
}
#endif

#if 1
TEST_F(MPI_AllreduceNew_Test, ut_mpi_allreduce_8p_ring_float32_binary_block_0_4nic)
{
    char file_name_t[] = "./st_mpi_allreduce_8p_ring_float32_binary_block_0_4nic.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << rank_table_4nic_allreduce << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    ResetInitState();
    s32 nPID = SalGetPid();
    string strPID = std::to_string(nPID);
    string config_file_name = "./config_file_st_mpi_allreduce_8p_ring_float32_binary_block_0_4nic" + strPID;
    ResetConfigFilePathName(config_file_name);
    remove(config_file_name.c_str());
    ofstream configfile(config_file_name);
    bool bRes = configfile.is_open();
    if(bRes)
    {
        HCCL_INFO("open %s success", config_file_name.c_str());
    }
    else
    {
        HCCL_ERROR("open %s failed", config_file_name.c_str());
    }
    EXPECT_EQ(bRes, true);
    configfile << "halvingDoublingType=0" << endl;
    configfile.close();

    s32 nnode, rank, errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    float* result_buff[DEV_NUM_8];
    float** recvbuf = (float**)sal_malloc( sizeof(float**) *DEV_NUM_8 );
    float** sendbuf = (float**)sal_malloc( sizeof(float**) *DEV_NUM_8 );
    float* inputbuf[DEV_NUM_8];
    float* outputbuf[DEV_NUM_8];
    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];
    HcclDataType datatype = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_MAX;
    s32 count = MPI_ALLREDUCE_SLICE_DATA_SIZE;
    s32 ndev = DEV_NUM_8;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(float), 0, count * sizeof(float));
        sal_memset(recvbuf[i], count  * sizeof(float), 0,  count * sizeof(float));

        result_buff[i] = (float*)sal_malloc(count * sizeof(float));
        sal_memset(result_buff[i], count * sizeof(float), 0, count * sizeof(float));

        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = 1.0;
        }
    }

    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = 1.0 ;
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
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;
        para_info[i].op = op;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = inputbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = outputbuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_all_reduce_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    s32 resRemove = remove(config_file_name.c_str());
    EXPECT_EQ(resRemove, 0);
    configfile.open(config_file_name);
    bRes = configfile.is_open();
    if(bRes)
    {
        HCCL_INFO("open %s success", config_file_name.c_str());
    }
    else
    {
        HCCL_ERROR("open %s failed", config_file_name.c_str());
    }
    EXPECT_EQ(bRes, true);
    configfile << "halvingDoublingType=1" << endl;
    configfile.close();
    InitConfigFileParam(config_file_name);
    resRemove = remove(config_file_name.c_str());
    EXPECT_EQ(resRemove, 0);
    ResetInitState();

    MPI_Barrier(MPI_COMM_WORLD);

    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count; i++)
        {
            float res = result_buff[j][i];
            float recv = outputbuf[j][i];

            if (abs(res - recv) > 1e-6)
            {
                HCCL_ERROR("node:%d result[%d][%d]:%f sendbuf:%f\n", noderank, j, i, outputbuf[j][i] , inputbuf[j][i]);
                errors++;
                break;
            }
        }
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

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
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        sal_free(result_buff[j]);
        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    sal_free(sendbuf);
    sal_free(recvbuf);
    EXPECT_EQ(errors, 0);
}

TEST_F(MPI_AllreduceNew_Test, ut_mpi_allreduce_8p_ring_float32_binary_block_0_2nic)
{
    char file_name_t[] = "./st_mpi_allreduce_8p_ring_float32_binary_block_0_2nic.json";
    // nlohmann::json rank_table = rank_table_910_2server_8rank;
    // char file_name_t[] = "./st_mpi_allreduce_8p_ring_float32_binary_block_0.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << rank_table_910_2server_8rank << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    ResetInitState();
    setenv("HCCL_ALGO", "1", 1);
    s32 nPID = SalGetPid();
    string strPID = std::to_string(nPID);
    string config_file_name = "./config_file_st_mpi_allreduce_8p_ring_float32_binary_block_0" + strPID;
    ResetConfigFilePathName(config_file_name);
    remove(config_file_name.c_str());
    ofstream configfile(config_file_name);
    bool bRes = configfile.is_open();
    if(bRes)
    {
        HCCL_INFO("open %s success", config_file_name.c_str());
    }
    else
    {
        HCCL_ERROR("open %s failed", config_file_name.c_str());
    }
    EXPECT_EQ(bRes, true);
    configfile << "halvingDoublingType=0" << endl;
    configfile.close();

    s32 nnode, rank, errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    float* result_buff[DEV_NUM_8];
    float** recvbuf = (float**)sal_malloc( sizeof(float**) *DEV_NUM_8 );
    float** sendbuf = (float**)sal_malloc( sizeof(float**) *DEV_NUM_8 );
    float* inputbuf[DEV_NUM_8];
    float* outputbuf[DEV_NUM_8];
    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];
    HcclDataType datatype = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = MPI_ALLREDUCE_SLICE_DATA_SIZE;
    s32 ndev = DEV_NUM_8;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(float), 0, count * sizeof(float));
        sal_memset(recvbuf[i], count  * sizeof(float), 0,  count * sizeof(float));

        result_buff[i] = (float*)sal_malloc(count * sizeof(float));
        sal_memset(result_buff[i], count * sizeof(float), 0, count * sizeof(float));

        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = 1.0;
        }
    }

    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = 2 * ndev * 1.0 ;
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
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;
        para_info[i].op = op;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = inputbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = outputbuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_all_reduce_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    s32 resRemove = remove(config_file_name.c_str());
    EXPECT_EQ(resRemove, 0);
    configfile.open(config_file_name);
    bRes = configfile.is_open();
    if(bRes)
    {
        HCCL_INFO("open %s success", config_file_name.c_str());
    }
    else
    {
        HCCL_ERROR("open %s failed", config_file_name.c_str());
    }
    EXPECT_EQ(bRes, true);
    configfile << "halvingDoublingType=1" << endl;
    configfile.close();
    InitConfigFileParam(config_file_name);
    resRemove = remove(config_file_name.c_str());
    EXPECT_EQ(resRemove, 0);
    ResetInitState();

    MPI_Barrier(MPI_COMM_WORLD);

    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count; i++)
        {
            float res = result_buff[j][i];
            float recv = outputbuf[j][i];

            if (abs(res - recv) > 1e-6)
            {
                HCCL_ERROR("node:%d result[%d][%d]:%f sendbuf:%f\n", noderank, j, i, outputbuf[j][i] , inputbuf[j][i]);
                errors++;
                break;
            }
        }
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

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
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        sal_free(result_buff[j]);
        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    unsetenv("HCCL_ALGO");
    remove(file_name_t);
    sal_free(sendbuf);
    sal_free(recvbuf);
    // EXPECT_EQ(errors, 0);
}
#endif
#if 1
TEST_F(MPI_AllreduceNew_Test, ut_mpi_allreduce_8ranks_2server_float_sum_inline_mesh)
{
    char file_name_t[] = "./ut_mpi_allreduce_8ranks_2server_float_sum_inline_mesh.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << g_rank_table_16p_V80 << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();
    setenv("HCCL_ALGO", "1", 1);
   u32 groupRanks[GOUP_DEV_NUM] = {0,1,2,3,8,9,10,11};

    s32 nnode, rank, errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    float* result_buff[DEV_NUM_4];
    float** recvbuf;
    float** sendbuf;
    float* inputBuf[DEV_NUM_4];
    float* outputBuf[DEV_NUM_4];
    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];

    HcclDataType datatype = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = MPI_ALLREDUCE_SLICE_DATA_SIZE;
    s32 ndev = DEV_NUM_4;

    set_board_id(0);
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    ret = hrtMalloc((void**)&recvbuf, ( sizeof(float**) *DEV_NUM_4 ));
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hrtMalloc((void**)&sendbuf, ( sizeof(float**) *DEV_NUM_4 ));
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], (count * sizeof(float)));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(float), 0, count * sizeof(float));

        ret = hrtMalloc((void**)&recvbuf[i], ( count * sizeof(float)));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count  * sizeof(float), 0,  count * sizeof(float));

        ret = hrtMalloc((void**)&result_buff[i], (count * sizeof(float)));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(result_buff[i], count * sizeof(float), 0, count * sizeof(float));

        inputBuf[i] = sendbuf[i];
        outputBuf[i] = recvbuf[i];
    }
    //sendbuf 赋值
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputBuf[j][i] = 1.0 * i;
        }
    }

    //resultbuf 赋值
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] =  8 * 1.0 * j;
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
        //identify << i + ndev * noderank;
        identify << groupRanks[i+ ndev * noderank];
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        // para_info[i].group_id = i;
        para_info[i].ranks_local = ndev;
        para_info[i].op = op;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = inputBuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = inputBuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
        para_info[i].groupRanksNum = GOUP_DEV_NUM;
        para_info[i].groupName = "group1" ;
        para_info[i].pGroupRanks = groupRanks;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8p_subgroup_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //获取stream的操作的同步信号量
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    bool subGroupFlag = false;

    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {

        u32 wordGroupId = std::stoi(para_info[j].identify);
        for(u32 i = 0;i < para_info->groupRanksNum;i++) {
            if(wordGroupId == para_info->pGroupRanks[i]) {
                subGroupFlag = true;
            }
        }
        if (subGroupFlag) {
            for (s32 i = 0; i < count; i++)
            {
                float res = result_buff[j][i];
                float recv = inputBuf[j][i];

                if (abs(res - recv) > 1e-6)
                {
                    HCCL_ERROR("worldgroupid[%d] noderank:%d recvbuf[%d][%d]:%f res[%d][%d]:%f\n",wordGroupId, noderank, j, i, recv,j,i,res);

                    errors++;
                    break;
                }
            }
        }
        subGroupFlag = false;
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

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
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        hrtFree(result_buff[j]);


    }
    for (s32 j = 0; j < ndev; j++)
    {
        rt_ret = rtStreamDestroy(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0);
    remove(file_name_t);
    unsetenv("HCCL_ALGO");
    hrtFree(sendbuf);
    hrtFree(recvbuf);
    EXPECT_EQ(errors, 0);
}
#endif
#if 1
TEST_F(MPI_AllreduceNew_Test, ut_mpi_allreduce_8ranks_2server_ring_virtual_machine)
{    char file_name_t[] = "./ut_mpi_allreduce_8ranks_2server_ring.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << g_rank_table_16p_V80 << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

   u32 groupRanks[GOUP_DEV_NUM] = {0,1,2,3,8,9,10,11};

    s32 nnode, rank, errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s8* result_buff[DEV_NUM_4];
    s8* sendbuf[DEV_NUM_4];
    s8* recvbuf[DEV_NUM_4];
    s8* inputBuf[DEV_NUM_4];
    s8* outputBuf[DEV_NUM_4];
    set_board_id(0x001E);
    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = 1000;

    s32 ndev = DEV_NUM_4;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode ?mpi???,??????
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    /** ????????? */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i],  count * sizeof(s8), 0,  count * sizeof(s8));
        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count  * sizeof(s8), 0,  count * sizeof(s8));
        ret = hrtMalloc((void**)&result_buff[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(result_buff[i], count * sizeof(s8), 0, count * sizeof(s8));
        inputBuf[i] = sendbuf[i];
        outputBuf[i] = recvbuf[i];
    }

    //sendbuf ??
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputBuf[j][i] = 1;
        }
    }

    //resultbuf ??
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = nnode * ndev;
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
        //identify << i + ndev * noderank;
        identify << groupRanks[i+ ndev * noderank];
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        // para_info[i].group_id = i;
        para_info[i].ranks_local = ndev;
        para_info[i].op = op;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = inputBuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = outputBuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
        para_info[i].groupRanksNum = GOUP_DEV_NUM;
        para_info[i].groupName = "group1" ;
        para_info[i].pGroupRanks = groupRanks;
    }

    // ????Dev?allreduce????
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8p_subgroup_task, (void*)&para_info[i]);
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

    MPI_Barrier(MPI_COMM_WORLD);
    bool subGroupFlag = false;

    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {

        u32 wordGroupId = std::stoi(para_info[j].identify);
        for(u32 i = 0;i < para_info->groupRanksNum;i++) {
            if(wordGroupId == para_info->pGroupRanks[i]) {
                subGroupFlag = true;
            }
        }
        if (subGroupFlag) {
            for (s32 i = 0; i < count; i++)
            {
                s8 res = result_buff[j][i];
                s8 recv = outputBuf[j][i];

                if (res != recv)
                {
                    HCCL_ERROR("worldgroupid[%d] noderank:%d recvbuf[%d][%d]:%f res[%d][%d]:%f\n",wordGroupId, noderank, j, i, recv,j,i,res);

                    errors++;
                    break;
                }
            }
        }
        subGroupFlag = false;
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

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
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        hrtFree(result_buff[j]);
    }
    for (s32 j = 0; j < ndev; j++)
    {
        rt_ret = rtStreamDestroy(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0);
    remove(file_name_t);

    EXPECT_EQ(errors, 0);
}
#endif

#if 1
// cann 两个版本一致
TEST_F(MPI_AllreduceNew_Test, ut_mpi_allreduce_8ranks_2server_cann1_int32)
{
    setenv("PROFILING_MODE", "true", 1);

    nlohmann::json rank_table = rank_table_board3000_2server_8rank;
    char file_name_t[] = "./ut_mpi_allreduce_8ranks_2server_cann1_int32.json";
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

    u32 groupRanks[GOUP_DEV_NUM] = {0, 1, 2, 3, 8, 9, 10, 11};

    s32 nnode, rank, errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s32* result_buff[DEV_NUM_4];
    s32** recvbuf = (s32**)sal_malloc( sizeof(s32**) *DEV_NUM_4 );
    s32** sendbuf = (s32**)sal_malloc( sizeof(s32**) *DEV_NUM_4 );
    s32* inputBuf[DEV_NUM_4];
    s32* outputBuf[DEV_NUM_4];
    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];

    HcclDataType datatype = HCCL_DATA_TYPE_INT32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = MPI_ALLREDUCE_DATA_SIZE;
    s32 ndev = DEV_NUM_4;
    set_board_id(0x3000);
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    // 添加cann版本信息
    RankConsistentcyChecker::GetInstance().SetCheckCannVersionSwitch(true);
    std::string cannEnv = (getenv("LD_LIBRARY_PATH") != nullptr) : getenv("LD_LIBRARY_PATH") : "EmptyString";
    std::string cannPath = "./rank/runtime";
    string newCannEnv = cannEnv + ":./rank/runtime";
    HCCL_RUN_INFO("LD_LIBRARY_PATH : %s ", newCannEnv.c_str());
    char version_file_path[] = "./rank/runtime/version.info";
    setenv("LD_LIBRARY_PATH", newCannEnv.c_str(), 1); // 增加CANN环境变量
    int mkdirCannPath = 0;
    if (0 != access(cannPath.c_str(), 0)) {
        mkdir("./rank", S_IRWXU | S_IRGRP | S_IXGRP);
        mkdirCannPath = mkdir(cannPath.c_str(), S_IRWXU | S_IRGRP | S_IXGRP); // 创建成功返回0，失败返回-1
    }
    if (mkdirCannPath != -1) {
        HCCL_RUN_INFO("mkdir %s success", cannPath.c_str());
        std::ofstream cannVerOutfile(version_file_path, std::ios::out | std::ios::trunc);
        if (cannVerOutfile.is_open())
        {
            cannVerOutfile << "Version=1.83.T6.0.B114" << std::endl;
            HCCL_RUN_INFO("open %s success", version_file_path);
        }
        else
        {
            HCCL_RUN_INFO("open %s failed", version_file_path);
        }
        cannVerOutfile.close();
    } else {
        HCCL_RUN_INFO("mkdir %s failed", cannPath.c_str());
    }
    InitEnvVarParam();
    memset(RankConsistentcyChecker::GetInstance().mCannVersion_,'\0',sizeof(RankConsistentcyChecker::GetInstance().mCannVersion_));
    RankConsistentcyChecker::GetInstance().RecordVerInfo(GetExternalInputCannVersion());
    HCCL_INFO("RankConsistentcyChecker information set SUCCESS");

    /** 初始化输入输出缓存 */
    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void**)&sendbuf[i], count * sizeof(s32));
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(s32));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(s32), 0, count * sizeof(s32));
        sal_memset(recvbuf[i], count  * sizeof(s32), 0,  count * sizeof(s32));

        result_buff[i] = (s32*)sal_malloc(count * sizeof(s32));
        sal_memset(result_buff[i], count * sizeof(s32), 0, count * sizeof(s32));

        inputBuf[i] = sendbuf[i];
        outputBuf[i] = recvbuf[i];
    }
    //sendbuf 赋值
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputBuf[j][i] = 1;
        }
    }

    //resultbuf 赋值
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] =  8;
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
        //identify << i + ndev * noderank;
        identify << groupRanks[i+ ndev * noderank];
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev * nnode;
        para_info[i].device_id = i ;
        // para_info[i].group_id = i;
        para_info[i].ranks_local = ndev;
        para_info[i].op = op;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = inputBuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = outputBuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
        para_info[i].groupRanksNum = GOUP_DEV_NUM;
        para_info[i].groupName = "group1" ;
        para_info[i].pGroupRanks = groupRanks;
    }

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8p_subgroup_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //获取stream的操作的同步信号量
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    bool subGroupFlag = false;

    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {

        u32 wordGroupId = std::stoi(para_info[j].identify);
        for(u32 i = 0;i < para_info->groupRanksNum;i++) {
            if(wordGroupId == para_info->pGroupRanks[i]) {
                subGroupFlag = true;
            }
        }
        if (subGroupFlag) {
            for (s32 i = 0; i < count; i++)
            {
                s32 res = result_buff[j][i];
                s32 recv = outputBuf[j][i];

                if ( res != recv )
                {
                    HCCL_ERROR("worldgroupid[%d] noderank:%d recvbuf[%d][%d]:%f res[%d][%d]:%f\n",wordGroupId, noderank, j, i, recv,j,i,res);
                    errors++;
                    break;
                }
            }
        }
        subGroupFlag = false; 
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

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
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        sal_free(result_buff[j]);
    }
    for (s32 j = 0; j < ndev; j++)
    {
        rt_ret = rtStreamDestroy(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    sal_free(sendbuf);
    sal_free(recvbuf);
    set_board_id(0);
    remove(file_name_t);
    setenv("PROFILING_MODE", "false", 1);

    remove_dir("./rank");
    setenv("LD_LIBRARY_PATH", cannEnv.c_str(), 1); // 恢复CANN环境变量
    memset_s(RankConsistentcyChecker::GetInstance().mCannVersion_, MAX_CANN_VERSION_LEN, 0, MAX_CANN_VERSION_LEN);
    RankConsistentcyChecker::GetInstance().SetCheckCannVersionSwitch(false);

    EXPECT_EQ(errors, 0);
}
#endif