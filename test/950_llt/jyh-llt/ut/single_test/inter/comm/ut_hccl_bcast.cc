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
#include "rank_consistentcy_checker.h"

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "hccl_comm_pub.h"
#include "sal.h"
#include "config.h"
#include "topoinfo_ranktableParser_pub.h"
#include "dlra_function.h"
#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include "param_check_pub.h"
#include "hcom_private.h"

#include <iostream>
#include <fstream>

using namespace std;
using namespace hccl;

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
    s32 root;
    rtStream_t stream;
    int id;
    volatile s32* sync_addr;
} para_t;

void* inter_broadcast_task(void* parg)
{
    s32 portNum = 7;
    MOCKER(hrtGetHccsPortNum)
        .stubs()
        .with(any(), outBound(portNum))
        .will(returnValue(HCCL_SUCCESS));
    HcclResult ret = HCCL_SUCCESS;
    para_t* para_info = (para_t*)parg;
    s32 rank_num_tmp;

    HcomInfo hcom_info;
    std::string ranktable_file = para_info->file_name;
    std::string rankTableM;
    std::string realFilePath;

    hrtSetDevice(para_info->device_id);
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = NetworkManager::GetInstance(para_info->device_id).Destroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomLoadRanktableFile(ranktable_file.c_str(), rankTableM, realFilePath);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, para_info->identify, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    sal_memcpy(hcom_info.params.id.internal, sizeof(HcclRootInfo), &para_info->rootInfo, sizeof(HcclRootInfo));
    hcom_info.pComm.reset(new(std::nothrow) hccl::hcclComm());
    rtModel_t model = (void*)1;

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task rt_set_device falis", hcom_info.params.rank);
    }
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);
    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task broadcast falis", para_info->device_id);
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
    ret = hcom_info.pComm->GetWorkspaceMemSize("HCCL_KERNEL_OP_TYPE_BROADCAST", para_info->count, para_info->datatype, rankSize, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource("tag_inter_broadcast_task_inter", memptr, memSize, streamList);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//

    bool swapped;

    rank_num_tmp = *(para_info->sync_addr) - 1;

    do
    {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1);
    }
    while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local)
    { sched_yield(); } // linux提供一个系统调用运行进程主动让出执行权

    __sync_synchronize();  // 插入内存屏障，对顺序性有要求，但是有没有使用lock指令的时候

    HCCL_DEBUG("all %d  ranks init ok ,then broadcast", hcom_info.params.totalRanks);
    ret = hcom_info.pComm->Broadcast("tag_inter_broadcast_task_inter",
                              para_info->sendbuff,
                              para_info->count,
                              para_info->datatype,
                              para_info->root,
                              para_info->stream);

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("rank[%d] task broadcast falis", hcom_info.params.rank);
    }
    rt_ret = RT_ERROR_NONE;
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
    }
    for (int i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtStreamDestroy(streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    hrtFree(memptr);
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    return (NULL);
}

void* inter_broadcast_outplace_task(void* parg)
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
    rtModel_t model = (void*)1;
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);
    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task broadcast falis", para_info->device_id);
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
    ret = hcom_info.pComm->GetWorkspaceMemSize("HCCL_KERNEL_OP_TYPE_BROADCAST_OUTPLACE", para_info->count, para_info->datatype, rankSize, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource("tag_inter_broadcast_outplace_task_inter", memptr, memSize, streamList);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//

    bool swapped;

    rank_num_tmp = *(para_info->sync_addr) - 1;

    do
    {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1);
    }
    while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local)
    { sched_yield(); } // linux提供一个系统调用运行进程主动让出执行权

    __sync_synchronize();  // 插入内存屏障，对顺序性有要求，但是有没有使用lock指令的时候

    HCCL_DEBUG("all %d  ranks init ok ,then broadcast", hcom_info.params.totalRanks);
    ret = hcom_info.pComm->BroadcastOutPlace("tag_inter_broadcast_outplace_task_inter",
                              para_info->sendbuff,
                              para_info->count,
                              para_info->datatype,
                              para_info->root,
                              para_info->stream);

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("rank[%d] task broadcast falis", hcom_info.params.rank);
    }
    rt_ret = RT_ERROR_NONE;
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
    }
    for (int i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtStreamDestroy(streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    hrtFree(memptr);

    return (nullptr);
}

class HcclBcastTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "HcclBcastTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "HcclBcastTest TearDown" << std::endl;
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
        DlTdtFunction::GetInstance().DlTdtFunctionInit();
        DlRaFunction::GetInstance().DlRaFunctionInit();
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};


#define HCC_REDUCE_DATA_SIZE 10
#define HCC_REDUCE_DATA_SIZE_2M (1024*1024*2+2)

#define DEV_NUM_4 4
#define DEV_NUM_2 2
#define DEV_NUM_8 8

#define HCCL_ALLGATHER_DATA_SIZE 10
#define HCC_ALLGATHER_SIZE_2M (1024*1024*2+3)

#if 1
TEST_F(HcclBcastTest, ut_bcast_8ranks_1server_ring_float)
{
    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"deploy_mode", "lab"},
        {"group_count", "1"},
        {"chip_info", "910"},
        {"board_id", "0x0000"},
        {"para_plane_nic_location", "device"},
        {"para_plane_nic_num", "8"},
        {"para_plane_nic_name", {"eth0", "eth1", "eth2", "eth3","eth4", "eth5", "eth6", "eth7"}},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"device_num", "8"},
                    {"server_num", "1"},
                    {"instance_count", "8"},
                        {
                            "instance_list",
                            {
                                {   {"rank_id", "0"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "0"}, {"device_ip", "192.168.0.11"}}}
                                    }
                                },

                                {   {"rank_id", "1"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "1"}, {"device_ip", "192.168.0.12"}}}
                                    }
                                },
                                {   {"rank_id", "2"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "2"}, {"device_ip", "192.168.0.13"}}}
                                    }
                                },

                                {   {"rank_id", "3"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "3"}, {"device_ip", "192.168.0.14"}}}
                                    }
                                },
                                {   {"rank_id", "4"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "4"}, {"device_ip", "192.168.0.15"}}}
                                    }
                                },

                                {   {"rank_id", "5"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "5"}, {"device_ip", "192.168.0.16"}}}
                                    }
                                },
                                {   {"rank_id", "6"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "6"}, {"device_ip", "192.168.0.17"}}}
                                    }
                                },

                                {   {"rank_id", "7"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "7"}, {"device_ip", "192.168.0.18"}}}
                                    }
                                },
                            }
                        },
                        {
                            "server_list",
                            {
                                {
                                    {"server_id", "10.0.0.10"},
                                    {
                                        "para_plane_info",
                                        {{
                                                {"eth0", "192.168.200.2"},
                                            },
                                            {
                                                {"eth1", "192.168.200.2"},
                                            },
                                            {
                                                {"eth2", "192.168.200.2"},
                                            },
                                            {
                                                {"eth3", "192.168.200.2"},
                                            },
                                            {
                                                {"eth4", "192.168.200.2"},
                                            },
                                            {
                                                {"eth5", "192.168.200.2"},
                                            },
                                            {
                                                {"eth6", "192.168.200.2"},
                                            },
                                            {
                                                {"eth7", "192.168.200.2"},
                                            },
                                        }
                                    }

                                }, 
                            }
                        }
                }
            }
        }
    };
    char file_name_t[] = "./ut_bcast_8ranks_1server_ring_float.json";
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
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    s32 errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    float* sendbuf[DEV_NUM_8];
    s32 sync_value = 0;
    rtStream_t stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];
    HcclDataType datatype = HCCL_DATA_TYPE_FP32;
    s32 ndev = DEV_NUM_8;
    s32 count = 128;
    set_board_id(0x0000);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(float), 0, count * sizeof(float));

        if (i == 2) {
            for (u32 j = 0; j < count; j++)
            {
                sendbuf[2][j] = 1.0;
            }
            for (u32 j = count/4; j < count/2; j++)
            {
                sendbuf[2][j] = 2.0;
            }
            for (u32 j = count/2; j < count/4*3; j++)
            {
                sendbuf[2][j] = 3.0;
            }
            for (u32 j = count/4*3; j < count; j++)
            {
                sendbuf[2][j] = 4.0;
            }
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].root = 2;
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_broadcast_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }


    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count/4; i++)
        {
            float recv = sendbuf[j][i];

            if (abs(recv - 1.0) > 1e-6)
            {
       //         printf("node:%d dev:%d recv[%d]:%f \n", noderank, j, i, recv);
                errors ++;
            }
        }
        for (s32 i = count/4; i < count/2; i++)
        {
            float recv = sendbuf[j][i];

            if (abs(recv - 2.0) > 1e-6)
            {
       //         printf("node:%d dev:%d recv[%d]:%f \n", noderank, j, i, recv);
                errors ++;
            }
        }
        for (s32 i = count/2; i < count/4*3; i++)
        {
            float recv = sendbuf[j][i];

            if (abs(recv - 3.0) > 1e-6)
            {
       //         printf("node:%d dev:%d recv[%d]:%f \n", noderank, j, i, recv);
                errors ++;
            }
        }
        for (s32 i = count/4*3; i < count; i++)
        {
            float recv = sendbuf[j][i];

            if (abs(recv - 4.0) > 1e-6)
            {
       //         printf("node:%d dev:%d recv[%d]:%f \n", noderank, j, i, recv);
                errors ++;
            }
        }
    }

        if (errors)
        {
            HCCL_ERROR("%d errors. Test FAILED.\n", errors);
        }
        else
        {
            HCCL_INFO("Test PASSED.\n");
        }

    for (s32 i = 0; i < ndev; ++i)
    {
        hrtFree(sendbuf[i]);
        rt_ret = rtStreamDestroy(stream[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    unsetenv("HCCL_CONNECT_TIMEOUT");
    set_board_id(0);
    remove(file_name_t);

    EXPECT_EQ(errors, 0);
}
#endif

#if 1
TEST_F(HcclBcastTest, ut_bcast_8ranks_1server_ring_char)
{
    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"deploy_mode", "lab"},
        {"group_count", "1"},
        {"chip_info", "910"},
        {"board_id", "0x0000"},
        {"para_plane_nic_location", "device"},
        {"para_plane_nic_num", "8"},
        {"para_plane_nic_name", {"eth0", "eth1", "eth2", "eth3","eth4", "eth5", "eth6", "eth7"}},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"device_num", "8"},
                    {"server_num", "1"},
                    {"instance_count", "8"},
                        {
                            "instance_list",
                            {
                                {   {"rank_id", "0"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "0"}, {"device_ip", "192.168.0.11"}}}
                                    }
                                },

                                {   {"rank_id", "1"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "1"}, {"device_ip", "192.168.0.12"}}}
                                    }
                                },
                                {   {"rank_id", "2"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "2"}, {"device_ip", "192.168.0.13"}}}
                                    }
                                },

                                {   {"rank_id", "3"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "3"}, {"device_ip", "192.168.0.14"}}}
                                    }
                                },
                                {   {"rank_id", "4"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "4"}, {"device_ip", "192.168.0.15"}}}
                                    }
                                },

                                {   {"rank_id", "5"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "5"}, {"device_ip", "192.168.0.16"}}}
                                    }
                                },
                                {   {"rank_id", "6"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "6"}, {"device_ip", "192.168.0.17"}}}
                                    }
                                },

                                {   {"rank_id", "7"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "7"}, {"device_ip", "192.168.0.18"}}}
                                    }
                                },
                            }
                        },
                        {
                            "server_list",
                            {
                                {
                                    {"server_id", "10.0.0.10"},
                                    {
                                        "para_plane_info",
                                        {{
                                                {"eth0", "192.168.200.2"},
                                            },
                                            {
                                                {"eth1", "192.168.200.2"},
                                            },
                                            {
                                                {"eth2", "192.168.200.2"},
                                            },
                                            {
                                                {"eth3", "192.168.200.2"},
                                            },
                                            {
                                                {"eth4", "192.168.200.2"},
                                            },
                                            {
                                                {"eth5", "192.168.200.2"},
                                            },
                                            {
                                                {"eth6", "192.168.200.2"},
                                            },
                                            {
                                                {"eth7", "192.168.200.2"},
                                            },
                                        }
                                    }

                                }, 
                            }
                        }
                }
            }
        }
    };
    char file_name_t[] = "./ut_bcast_8ranks_1server_ring_char.json";
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
    s32 errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    char* sendbuf[DEV_NUM_8];
    s32 sync_value = 0;
    rtStream_t stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];
    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    s32 ndev = DEV_NUM_8;
    s32 count = 129;
    set_board_id(0x0000);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(char));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(char), 0, count * sizeof(char));

        if (i == 2) {
            for (u32 j = 0; j < count; j++)
            {
                sendbuf[2][j] = 1;
            }
            for (u32 j = count/4; j < count/2; j++)
            {
                sendbuf[2][j] = 2;
            }
            for (u32 j = count/2; j < count/4*3; j++)
            {
                sendbuf[2][j] = 3;
            }
            for (u32 j = count/4*3; j < count; j++)
            {
                sendbuf[2][j] = 4;
            }
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].root = 2;
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_broadcast_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }


    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count/4; i++)
        {
            char recv = sendbuf[j][i];

            if (recv != 1)
            {

                errors ++;
            }
        }
        for (s32 i = count/4; i < count/2; i++)
        {
            char recv = sendbuf[j][i];

            if (recv != 2)
            {

                errors ++;
            }
        }
        for (s32 i = count/2; i < count/4*3; i++)
        {
            char recv = sendbuf[j][i];

            if (recv != 3)
            {

                errors ++;
            }
        }
        for (s32 i = count/4*3; i < count; i++)
        {
            char recv = sendbuf[j][i];

            if (recv != 4)
            {

                errors ++;
            }
        }
    }

        if (errors)
        {
            HCCL_ERROR("%d errors. Test FAILED.\n", errors);
        }
        else
        {
            HCCL_INFO("Test PASSED.\n");
        }

    for (s32 i = 0; i < ndev; ++i)
    {
        hrtFree(sendbuf[i]);
        rt_ret = rtStreamDestroy(stream[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    set_board_id(0);
    remove(file_name_t);

    EXPECT_EQ(errors, 0);
}
#endif

#if 1
TEST_F(HcclBcastTest, ut_bcast_8ranks_1server_ring_float_outplace)
{
    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"deploy_mode", "lab"},
        {"group_count", "1"},
        {"chip_info", "910"},
        {"board_id", "0x0000"},
        {"para_plane_nic_location", "device"},
        {"para_plane_nic_num", "8"},
        {"para_plane_nic_name", {"eth0", "eth1", "eth2", "eth3","eth4", "eth5", "eth6", "eth7"}},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"device_num", "8"},
                    {"server_num", "1"},
                    {"instance_count", "8"},
                        {
                            "instance_list",
                            {
                                {   {"rank_id", "0"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "0"}, {"device_ip", "192.168.0.11"}}}
                                    }
                                },

                                {   {"rank_id", "1"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "1"}, {"device_ip", "192.168.0.12"}}}
                                    }
                                },
                                {   {"rank_id", "2"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "2"}, {"device_ip", "192.168.0.13"}}}
                                    }
                                },

                                {   {"rank_id", "3"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "3"}, {"device_ip", "192.168.0.14"}}}
                                    }
                                },
                                {   {"rank_id", "4"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "4"}, {"device_ip", "192.168.0.15"}}}
                                    }
                                },

                                {   {"rank_id", "5"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "5"}, {"device_ip", "192.168.0.16"}}}
                                    }
                                },
                                {   {"rank_id", "6"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "6"}, {"device_ip", "192.168.0.17"}}}
                                    }
                                },

                                {   {"rank_id", "7"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "7"}, {"device_ip", "192.168.0.18"}}}
                                    }
                                },
                            }
                        },
                        {
                            "server_list",
                            {
                                {
                                    {"server_id", "10.0.0.10"},
                                    {
                                        "para_plane_info",
                                        {{
                                                {"eth0", "192.168.200.2"},
                                            },
                                            {
                                                {"eth1", "192.168.200.2"},
                                            },
                                            {
                                                {"eth2", "192.168.200.2"},
                                            },
                                            {
                                                {"eth3", "192.168.200.2"},
                                            },
                                            {
                                                {"eth4", "192.168.200.2"},
                                            },
                                            {
                                                {"eth5", "192.168.200.2"},
                                            },
                                            {
                                                {"eth6", "192.168.200.2"},
                                            },
                                            {
                                                {"eth7", "192.168.200.2"},
                                            },
                                        }
                                    }

                                }, 
                            }
                        }
                }
            }
        }
    };
    char file_name_t[] = "./ut_bcast_8ranks_1server_ring_float.json";
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
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    s32 errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    float* sendbuf[DEV_NUM_8];
    s32 sync_value = 0;
    rtStream_t stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];
    HcclDataType datatype = HCCL_DATA_TYPE_FP32;
    s32 ndev = DEV_NUM_8;
    s32 count = 4096;
    set_board_id(0x0000);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(float), 0, count * sizeof(float));

        if (i == 2) {
            for (u32 j = 0; j < count; j++)
            {
                sendbuf[2][j] = 1.0;
            }
            for (u32 j = count/4; j < count/2; j++)
            {
                sendbuf[2][j] = 2.0;
            }
            for (u32 j = count/2; j < count/4*3; j++)
            {
                sendbuf[2][j] = 3.0;
            }
            for (u32 j = count/4*3; j < count; j++)
            {
                sendbuf[2][j] = 4.0;
            }
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
        identify << i;
        para_info[i].identify = identify.str();
        para_info[i].comm_num = ndev;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;
        para_info[i].count = count;
        para_info[i].datatype = datatype;
        para_info[i].sendbuff = sendbuf[i];
        para_info[i].stream = stream[i];
        para_info[i].root = 2;
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_broadcast_outplace_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }


    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count/4; i++)
        {
            float recv = sendbuf[j][i];

            if (abs(recv - 1.0) > 1e-6)
            {
                errors ++;
            }
        }
        for (s32 i = count/4; i < count/2; i++)
        {
            float recv = sendbuf[j][i];

            if (abs(recv - 2.0) > 1e-6)
            {
                errors ++;
            }
        }
        for (s32 i = count/2; i < count/4*3; i++)
        {
            float recv = sendbuf[j][i];

            if (abs(recv - 3.0) > 1e-6)
            {
                errors ++;
            }
        }
        for (s32 i = count/4*3; i < count; i++)
        {
            float recv = sendbuf[j][i];

            if (abs(recv - 4.0) > 1e-6)
            {
                errors ++;
            }
        }
    }

        if (errors)
        {
            HCCL_ERROR("%d errors. Test FAILED.\n", errors);
        }
        else
        {
            HCCL_INFO("Test PASSED.\n");
        }

    for (s32 i = 0; i < ndev; ++i)
    {
        hrtFree(sendbuf[i]);
        rt_ret = rtStreamDestroy(stream[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    unsetenv("HCCL_CONNECT_TIMEOUT");
    set_board_id(0);
    remove(file_name_t);

}
#endif