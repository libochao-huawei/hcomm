#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
//#include <mpi.h>

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
#include "rank_consistentcy_checker.h"
#include "workflow_pub.h"
#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "sal.h"
#include "config.h"
//#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"
#include "dlra_function.h"
#include "param_check_pub.h"
#include "hcom_private.h"

#include <iostream>
#include <fstream>


using namespace std;
using namespace hccl;


#define MPI_AllGather_SLICE_DATA_SIZE 1024*1024*2 +5
#define MPI_AllGather_DATA_SIZE 1256
#define MPI_AllGather_ALIGN_DATA_SIZE 215
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
    u32 localDieID;
} para_t;

void* intra_chip_all_gather_task(void* parg)
{
    HcclResult ret = HCCL_SUCCESS;
    para_t* para_info = (para_t*)parg;
    s32 rank_num_tmp;

    HcomInfo hcom_info;
    std::string ranktable_file = para_info->file_name;
    std::string rankTableM;
    std::string realFilePath;
    HCCL_INFO("<multiDie> device_id[%d] localDieID[%d]", para_info->device_id, para_info->localDieID);
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
    //ret = hcom_info.pComm->GetWorkspaceSubStreamNum(stream_list_size);
    //EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("get stream_list_size[%d] success", stream_list_size);
    vector<HcclRtStream> streamList;

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
    ret = hcom_info.pComm->GetWorkspaceMemSize("tag_intra_chip_all_gather_task_inter", para_info->count, para_info->datatype, rankSize, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hcom_info.pComm->SetWorkspaceResource("tag_intra_chip_all_gather_task_inter", memptr, memSize, streamList);
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

    ret =  hcom_info.pComm->AllGather("tag_intra_chip_all_gather_task_inter",
                               para_info->sendbuff,
                               para_info->recvbuff,
                               para_info->count,
                               para_info->datatype,
                               para_info->stream);
    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task HcclAllGather falis", para_info->device_id);
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
    HCCL_INFO("rank[%d] task allgather success");
    delete charModel;
    charModel = nullptr;
    return (NULL);
}
#define DEV_NUM_1 1

class AllGatherMultiDie_Test : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "AllGatherMultiDie_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "AllGatherMultiDie_Test SetUP" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
	static s32 call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(0, "UT_TEST");
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};

#define DEV_NUM_2 2
#define DEV_NUM_4 4
#define DEV_NUM_8 8

#if 0
TEST_F(AllGatherMultiDie_Test, ut_mpi_AllGather_multiDie)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank_for_multiDie;
    char file_name_t[] = "./ut_mpi_allreduce_multi_die.json";
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
    s32 rank, errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s8* result_buff[DEV_NUM_2];
    s8* sendbuf[DEV_NUM_2];
    s8* recvbuf[DEV_NUM_2];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_2];
    sal_thread_t tid[DEV_NUM_2];
    para_t para_info[DEV_NUM_2];

    HcclDataType datatype = HCCL_DATA_TYPE_FP32;
    s32 count = 10;

    s32 ndev = DEV_NUM_2;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(float), 0, count * sizeof(float));

        ret = hrtMalloc((void **)&recvbuf[i], ndev * count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], ndev * count * sizeof(float), 0, ndev  * count * sizeof(float));

        result_buff[i] = (s8*)sal_malloc(ndev * count * sizeof(float));
        sal_memset(result_buff[i], ndev  * count * sizeof(float), 0, ndev * count * sizeof(float));
    }

    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[j][i] = 1;
        }
    }

    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < ndev * count; j++)
        {
            result_buff[i][j] = 1 ;
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
        identify << 0;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = 1;
        para_info[i].device_id = 0 ;
        para_info[i].ranks_local = i;
        //para_info[i].op = op;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuf[i];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
        para_info[i].localDieID = i;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", intra_chip_all_gather_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }
    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }
    HCCL_INFO("<multiDie> thread end");
    /*
    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count * ndev; i++)
        {
            s8 res = result_buff[j][i];
            s8 recv = recvbuf[j][i];

            if (res != recv)
            {
                HCCL_ERROR("recvbuf[%d][%d]:%d \n", j, i, recv);
                errors++;
                break;
            }
        }
    }
    */
    for (s32 j = 0; j < ndev; j++)
    {
        hrtFree(sendbuf[j]);
        hrtFree(recvbuf[j]);
        sal_free(result_buff[j]);
        rt_ret = rtStreamDestroy(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
    HCCL_INFO("==TMP== multi die allreduce finished");
}
#endif