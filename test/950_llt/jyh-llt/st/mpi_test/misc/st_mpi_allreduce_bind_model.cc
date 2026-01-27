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


#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "hccl_comm_pub.h"

#include "sal.h"
#include "hccl_impl.h"
#include "config.h"
#include "topoinfo_ranktableParser_pub.h"


#include <iostream>
#include <fstream>
#include "network_manager_pub.h"
#include "opexecounter_pub.h"

using namespace std;
using namespace hccl;

class MPI_AllreduceBindModel_Test : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_AllreduceBindModel_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_AllreduceBindModel_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        MPI_Barrier(MPI_COMM_WORLD);

        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "ST_MPI_TEST");
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A MPI_AllreduceBindModel_Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        std::cout << "A MPI_AllreduceBindModel_Test TearDown" << std::endl;
    }
};


#define MPI_ALLREDUCE_SLICE_DATA_SIZE 1024*1024*2 +5
#define MPI_ALLREDUCE_DATA_SIZE 1256
#define MPI_ALLREDUCE_ALIGN_DATA_SIZE 215
typedef struct para_struct
{
    HcclRootInfo rootInfo;
    s32 comm_id;
    s32 comm_num;
    s32 device_id;
    s32 ranks_local; //本服务器内的rank数
    s32 round;

    char* file_name;
    const void* sendbuff;
    void* recvbuff;
    s32 count;
    HcclDataType datatype;
    HcclReduceOp op;
    rtStream_t stream;
    int id;
    volatile s32* sync_addr;
} para_t;


#define ALLREDUCE_NUM 4
#define ALLREDUCE_SIZE 10

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

    hcom_info.params.rank = para_info->comm_id;
    hcom_info.dev_id = hcom_info.rankTable.rankList[para_info->comm_id].deviceInfo.devicePhyId;
    hcom_info.params.totalRanks = hcom_info.rankTable.rankNum;
    sal_memset(hcom_info.params.id.internal, HCCL_ROOT_INFO_BYTES, 0, sizeof(hcom_info.params.id.internal));

    sal_memcpy(hcom_info.params.id.internal, sizeof(HcclRootInfo), &para_info->rootInfo, sizeof(HcclRootInfo));

    hcom_info.pComm.reset(new(std::nothrow) hccl::hcclComm());
    rtModel_t model = (void*)1;

    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);
    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task HcclAllReduce falis", para_info->device_id);
    }

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


    for(int i = 0; i < para_info->round; ++i)
    {
        std:string tag = "tag_inter_all_reduce";
        char index[128] = {0};
        sprintf(index, "%d", i);
        tag += index;

        sal_memset((void*)para_info->sendbuff, para_info->count * sizeof(char), 1, para_info->count * sizeof(char));
        sal_memset((void*)para_info->recvbuff, para_info->count * sizeof(char), 0, para_info->count * sizeof(char));

        ret =  hcom_info.pComm->AllReduce(tag.c_str(),
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

        rtError_t rt_ret = RT_ERROR_NONE;
        rt_ret = rtStreamSynchronize(para_info->stream);

        if ( rt_ret != RT_ERROR_NONE)
        {
            HCCL_ERROR("rank[%d] task allgather falis", para_info->comm_id);
        }

    }

    return (NULL);
}

#if 1
void* all_reduce_8pring_task(void* parg)
{
    HcclResult ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    para_t* para_info = (para_t*)parg;
    s32 rank_num_tmp;
    Stream exeStream(StreamType::STREAM_TYPE_OFFLINE);

    HcomInfo hcom_info;
    std::string ranktable_file = para_info->file_name;

    hrtSetDevice(para_info->device_id);
    ret = CfgGetClusterInfo(ranktable_file, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcom_info.params.rank = para_info->comm_id;
    hcom_info.dev_id = hcom_info.rankTable.rankList[para_info->comm_id].deviceInfo.devicePhyId;
    hcom_info.params.totalRanks = hcom_info.rankTable.rankNum;
    sal_memset(hcom_info.params.id.internal, HCCL_ROOT_INFO_BYTES, 0, sizeof(hcom_info.params.id.internal));

    sal_memcpy(hcom_info.params.id.internal, sizeof(HcclRootInfo), &para_info->rootInfo, sizeof(HcclRootInfo));

    hcom_info.pComm.reset(new(std::nothrow) hccl::hcclComm());

    rtModel_t model = (rtModel_t)0;
    rt_ret = rtModelCreate(&model, 0);
    if ((rt_ret != RT_ERROR_NONE) || (0 == model))
    {
        HCCL_ERROR("rank[%d] rtModelCreate fail!", para_info->comm_id);
    }

    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);
    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task HcclAllReduce falis", para_info->device_id);
    }

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


    rt_ret = rtModelBindStream(model, para_info->stream, 0);
    if ( rt_ret != RT_ERROR_NONE)
    {
        HCCL_ERROR("rank[%d] bind model fail!", para_info->comm_id);
    }
    hcom_info.params.mapStreamMode.insert(std::make_pair(para_info->stream, model));

    ret = hcom_info.pComm->SaveMapStreamModel(para_info->stream, model);
    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("rank[%d] comm get map streamModel fail!", para_info->comm_id);
    }
    ret =  hcom_info.pComm->AllReduce("tag_all_reduce_8pring",
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

    rt_ret = rtModelExecute(model, exeStream.ptr(), 0);
    if ( rt_ret != RT_ERROR_NONE)
    {
        HCCL_ERROR("rank[%d] rtModelExecute fail!", para_info->comm_id);
    }

    rt_ret = rtStreamSynchronize(para_info->stream);

    if ( rt_ret != RT_ERROR_NONE)
    {
        HCCL_ERROR("rank[%d] task allgather falis", para_info->comm_id);
    }

    ret = hcom_info.pComm->unbind_model(model);
    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("rank[%d] hcom_unbind_model fail!", para_info->comm_id);
    }

    rt_ret = rtModelUnbindStream(model, para_info->stream);
    if ( rt_ret != RT_ERROR_NONE)
    {
        HCCL_ERROR("rank[%d] rtModelUnbindStream fail!", para_info->comm_id);
    }

    return (NULL);
}
#endif

#if 1
#define DEV_NUM_4 4
TEST_F(MPI_AllreduceBindModel_Test, st_mpi_allreduce_8ranks_2server_ring_muti_tag)
{
    // ranktable 的读取，直接使用进程
    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"deploy_mode", "lab"},
        {"group_count", "1"},
        {"chip_info", "910"},
        {"board_id", "0x0000"},
        {"para_plane_nic_location", "device"},
        {"para_plane_nic_num", "4"},
        {"para_plane_nic_name", {"eth0", "eth1", "eth2", "eth3"}},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"device_num", "8"},
                    {"server_num", "2"},
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
                                {   {"rank_id", "4"}, {"server_id", "10.0.0.11"},
                                    {
                                        "devices", {{{"device_id", "0"}, {"device_ip", "192.168.0.15"}}}
                                    }
                                },

                                {   {"rank_id", "5"}, {"server_id", "10.0.0.11"},
                                    {
                                        "devices", {{{"device_id", "1"}, {"device_ip", "192.168.0.16"}}}
                                    }
                                },
                                {   {"rank_id", "6"}, {"server_id", "10.0.0.11"},
                                    {
                                        "devices", {{{"device_id", "2"}, {"device_ip", "192.168.0.17"}}}
                                    }
                                },

                                {   {"rank_id", "7"}, {"server_id", "10.0.0.11"},
                                    {
                                        "devices", {{{"device_id", "3"}, {"device_ip", "192.168.0.18"}}}
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
                                        }
                                    }

                                },
                                {
                                    {"server_id", "10.0.0.11"},
                                    {
                                        "para_plane_info",
                                        {{
                                                {"eth0", "192.168.210.3"},
                                            },
                                            {
                                                {"eth1", "192.168.210.3"},
                                            },
                                            {
                                                {"eth2", "192.168.200.3"},
                                            },
                                            {
                                                {"eth3", "192.168.210.3"},
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

    char file_name_t[] = "./st_mpi_allreduce_8ranks_2server_float_max_ring.json";
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

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s8* result_buff[DEV_NUM_4];
    s8* sendbuf[DEV_NUM_4];
    s8* recvbuf[DEV_NUM_4];
    s8* inputbuf[DEV_NUM_4];
    s8* outputbuf[DEV_NUM_4];

    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = 10;

    s32 ndev = DEV_NUM_4;

    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
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
        sal_memset(sendbuf[i], count * sizeof(s8), 0, count * sizeof(s8));

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
            inputbuf[j][i] = 1;
        }
    }

    //resultbuf 赋值
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
        para_info[i].comm_id = i + ndev * noderank;
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
        para_info[i].round = 2;
    }

    // 创建每个Dev的allreduce任务线程
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

    //获取stream的操作的同步信号量
    for (s32 j = 0; j < ndev; j++)
    {
        rt_ret = rtStreamSynchronize(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count; i++)
        {
            s8 res = result_buff[j][i];
            s8 recv = outputbuf[j][i];

            if (res != recv)
            {
                HCCL_ERROR("noderank:%d recvbuf[%d][%d]:%d\n", noderank, j, i, recv);
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
        hrtFree(result_buff[j]);
        rt_ret = rtStreamDestroy(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    remove(file_name_t);

    EXPECT_EQ(errors, 0);
}
#endif


nlohmann::json rank_table_16ranks_2server =
{
        {"status", "completed"},
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
                    {"device_num", "16"},
                    {"server_num", "2"},
                    {"instance_count", "16"},
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
                                 {  {"rank_id", "8"}, {"server_id", "10.0.0.11"},
                                    {
                                        "devices", {{{"device_id", "0"}, {"device_ip", "192.168.0.20"}}}
                                    }
                                },

                                {   {"rank_id", "9"}, {"server_id", "10.0.0.11"},
                                    {
                                        "devices", {{{"device_id", "1"}, {"device_ip", "192.168.0.21"}}}
                                    }
                                },
                                {   {"rank_id", "10"}, {"server_id", "10.0.0.11"},
                                    {
                                        "devices", {{{"device_id", "2"}, {"device_ip", "192.168.0.22"}}}
                                    }
                                },

                                {   {"rank_id", "11"}, {"server_id", "10.0.0.11"},
                                    {
                                        "devices", {{{"device_id", "3"}, {"device_ip", "192.168.0.23"}}}
                                    }
                                },
                                {   {"rank_id", "12"}, {"server_id", "10.0.0.11"},
                                    {
                                        "devices", {{{"device_id", "4"}, {"device_ip", "192.168.0.24"}}}
                                    }
                                },

                                {   {"rank_id", "13"}, {"server_id", "10.0.0.11"},
                                    {
                                        "devices", {{{"device_id", "5"}, {"device_ip", "192.168.0.25"}}}
                                    }
                                },
                                {   {"rank_id", "14"}, {"server_id", "10.0.0.11"},
                                    {
                                        "devices", {{{"device_id", "6"}, {"device_ip", "192.168.0.26"}}}
                                    }
                                },

                                {   {"rank_id", "15"}, {"server_id", "10.0.0.11"},
                                    {
                                        "devices", {{{"device_id", "7"}, {"device_ip", "192.168.0.27"}}}
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
                                {
                                    {"server_id", "10.0.0.11"},
                                    {
                                        "para_plane_info",
                                        {{
                                                {"eth0", "192.168.210.3"},
                                            },
                                            {
                                                {"eth1", "192.168.210.3"},
                                            },
                                            {
                                                {"eth2", "192.168.200.3"},
                                            },
                                            {
                                                {"eth3", "192.168.210.3"},
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


#if 1
#define DEV_NUM_8 8
TEST_F(MPI_AllreduceBindModel_Test, st_mpi_allreduce_16ranks_2server_char_sum_4rings_bind_model)
{
    // ranktable 的读取，直接使用进程

    char file_name_t[] = "./st_mpi_allreduce_16ranks_2server_char_sum_4rings_slicecount.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << rank_table_16ranks_2server << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    s32 nnode, rank, errors = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    s8* result_buff[DEV_NUM_8];
    s8* sendbuf[DEV_NUM_8];
    s8* recvbuf[DEV_NUM_8];
    s8* inputbuf[DEV_NUM_8];
    s8* outputbuf[DEV_NUM_8];

    s32 sync_value = 0;
    s32 noderank = 0;
    rtStream_t stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    para_t para_info[DEV_NUM_8];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = 100;

    s32 ndev = DEV_NUM_8;

    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
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
        sal_memset(sendbuf[i], count * sizeof(s8), 0, count * sizeof(s8));
        ret = hrtMalloc((void**)&recvbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuf[i], count  * sizeof(s8), 0,  count * sizeof(s8));
        result_buff[i] = (s8*)sal_mallo(ccount * sizeof(s8));
        sal_memset(result_buff[i], count * sizeof(s8), 0, count * sizeof(s8));
        inputbuf[i] = sendbuf[i];
        outputbuf[i] = recvbuf[i];
    }

    //sendbuf 赋值
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            inputbuf[j][i] = 1;
        }
    }

    //resultbuf 赋值
    for (u32 i = 0; i < ndev; i++)
    {
        for (u32 j = 0; j < count; j++)
        {
            result_buff[i][j] = ndev * nnode ;
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
        identify << i + ndev * noderank;
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

    // 创建每个Dev的allreduce任务线程
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", all_reduce_8pring_task, (void*)&para_info[i]);
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

    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count; i++)
        {
            s8 res = result_buff[j][i];
            s8 recv = outputbuf[j][i];

            if (res !=recv)
            {
               HCCL_ERROR("noderank:%d recvbuf[%d][%d]:%d res[%d][%d]:%d\n", noderank, j, i, recv,j,i,res);
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

    EXPECT_EQ(errors, 0);
}
#endif


