/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

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

#include <securec.h>

#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <driver/ascend_hal.h>
#include <runtime/rt.h>

#include "hcom_pub.h"

#include "hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "sal.h"
#include "config.h"
#include "dlra_function.h"
#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"
#include "opexecounter_pub.h"

#include <iostream>
#include <fstream>
#include "network_manager_pub.h"
#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include "param_check_pub.h"
#include "externalinput.h"
#include "rank_consistentcy_checker.h"
#include "hccl_communicator.h"
#include "env_config.h"

using namespace std;
using namespace hccl;

typedef struct para_struct
{
    HcclRootInfo rootInfo;
    std::string identify;
    s32 comm_num;
    s32 device_id;
    s32 ranks_local;

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


void* inter_reduce_task_ffts(void* parg)
{
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
    hcom_info.params.deviceType = DevType::DEV_TYPE_910B;
    SetFftsSwitch(true);
    InitEnvVarParam();
    rtModel_t model = (void*)1;

    hrtSetDevice(para_info->device_id);
    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task rt_set_device fails", hcom_info.params.rank);
    }

    CommConfig commConfig("hccl_world_group"); 
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);
    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("dev[%d] task reduce fails", para_info->device_id);
    }

    bool swapped;
    //-----------------Get Workspace Resource Start------------------//
    u64 stream_list_size = 0;
    ret = hcom_info.pComm->GetWorkspaceSubStreamNum(stream_list_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("get stream_list_size[%d] success", stream_list_size);
    vector<HcclRtStream> streamList(stream_list_size);

    rtError_t rt_ret;
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = aclrtCreateStreamWithConfig(&streamList[i], 0, ACL_STREAM_PERSISTENT);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        rt_ret = rtModelBindStream(model, streamList[i], RT_MODEL_WAIT_ACTIVE_STREAM);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    u32 rankSize = 0;
    ret = hcom_info.pComm->GetRankSize(rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 memSize = 0;
    ret = hcom_info.pComm->GetWorkspaceMemSize("HcomReduce_ffts", para_info->count, para_info->datatype, rankSize, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcom_info.pComm->SetWorkspaceResource("tag_inter_reduce_task_ffts", memptr, memSize, streamList);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Get Workspace Resource End------------------//
    rank_num_tmp = *(para_info->sync_addr) - 1;

    do
    {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1);
    }
    while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local)
    { sched_yield(); } 

    __sync_synchronize();  

    HCCL_DEBUG("all %d  ranks init ok ,then reduce", hcom_info.params.totalRanks);

    ret = hcom_info.pComm->ReduceOutPlace("tag_inter_reduce_task_ffts", para_info->sendbuff,
                                   para_info->recvbuff,
                                   para_info->count,
                                   para_info->datatype,
                                   para_info->op,
                                   para_info->root,
                                   para_info->stream);


    SetFftsSwitch(false);
    InitEnvVarParam();

    if (ret != HCCL_SUCCESS)
    {
        HCCL_ERROR("rank[%d] task reduce fails", hcom_info.params.rank);
    }

    rt_ret = RT_ERROR_NONE;
    rt_ret = aclrtSynchronizeStream(para_info->stream);

    if ( rt_ret != RT_ERROR_NONE)
    {
        HCCL_ERROR("rank[%d] task allgather fails", hcom_info.params.rank);
    }
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtModelUnbindStream(model, streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    for (int i = 0; i < stream_list_size; i++)
    {
        rt_ret = aclrtDestroyStream(streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    hrtFree(memptr);
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    return (NULL);
}

class HcclInterTestFFTS : public testing::Test
{
protected:
    static void SetUpTestCase()
    {

        std::cout << "HcclInterFFTSTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "HcclInterFFTSTest TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        static s32  call_cnt = 0;
        DlTdtFunction::GetInstance().DlTdtFunctionInit();
        TsdOpen(1, 2);
        std::cout << "tsd open" << std::endl;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        setenv("HCCL_DFS_CONFIG", "connection_fault_detection_time:0", 1);
        InitEnvParam();
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        TsdClose(1);
        std::cout << "tsd close" << std::endl;
        std::cout << "A Test TearDown" << std::endl;
    }
};


#define HCC_REDUCE_DATA_SIZE 10
#define HCC_REDUCE_DATA_SIZE_1024 1024
#define HCC_REDUCE_DATA_SIZE_2M (1024*1024*2)

#define DEV_NUM_4 4
#define DEV_NUM_2 2
#define DEV_NUM_8 8
#if 0 //执行失败double-free
TEST_F(HcclInterTestFFTS, st_reduce_inter_sum_char_ffts)
{

    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"deploy_mode", "lab"},
        {"group_count", "1"},
        {"chip_info", "910"},
        {"board_id", "0x0000"},
        {"para_plane_nic_location", "device"},
        {"para_plane_nic_num", "4"},
        {"para_plane_nic_name", {"eth0", "eth1","eth2", "eth3"}},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"device_num", "4"},
                    {"server_num", "1"},
                    {"instance_count", "4"},
                        {
                            "instance_list",
                            {
                                {   {"rank_id", "0"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "0"}, {"device_ip", "192.168.0.12"}}}
                                    }
                                },

                                {   {"rank_id", "1"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "1"}, {"device_ip", "192.168.0.14"}}}
                                    }
                                },
                                {   {"rank_id", "2"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "2"}, {"device_ip", "192.168.0.16"}}}
                                    }
                                },

                                {   {"rank_id", "3"}, {"server_id", "10.0.0.10"},
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
                                    {"server_id", "192.168.10.2"},
                                    {
                                        "para_plane_info",
                                        {{
                                                {"eth1", "192.168.210.2"},
                                            },
                                            {
                                                {"eth0", "192.168.200.2"},
                                            }
                                        }
                                    }

                                },
                            }
                        }
                }
            }
        }
    };

    char file_name_t[] = "./st_reduce_inter_sum_char_ffts.json";
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

    s8* result_buff[DEV_NUM_4];
    s8* sendbuf[DEV_NUM_4];
    s8* recvbuf[DEV_NUM_4];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;

    HcclReduceOp op = HCCL_REDUCE_SUM;
    s32 count = HCC_REDUCE_DATA_SIZE;
    s32 ndev = DEV_NUM_4;

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(s8), 0, count * sizeof(s8));
        ret = hrtMalloc((void **)&recvbuf[i] , count * sizeof(s8));
        sal_memset(recvbuf[i], count * sizeof(s8), 0, count * sizeof(s8));
        ret = hrtMalloc((void **)&result_buff[i] ,count * sizeof(s8));
        sal_memset(result_buff[i], count * sizeof(s8), 0, count * sizeof(s8));
    }
   
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[j][i] = 1;
        }
    }

    for (u32 j = 0; j < count; j++)
     {
            result_buff[0][j] = ndev;
     }

    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = aclrtCreateStream(&stream[i]);
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
        para_info[i].recvbuff = recvbuf[i];
        para_info[i].op = op;
        para_info[i].root = 0;
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }
    MOCKER_CPP(&HcclSocketManager::ServerInit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    for (s32 i = 0; i < ndev; ++i)
    {
        set_chip_type_stub(i, static_cast<s32>(DevType::DEV_TYPE_910B));
        tid[i] = sal_thread_create("thread_ffts", inter_reduce_task_ffts, (void*)&para_info[i]);
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
        rt_ret = aclrtSynchronizeStream(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    if (errors)
    {
        HCCL_ERROR("%d errors. Test FAILED.\n", errors);
    }
    else
    {
        HCCL_INFO("Test PASSED.\n");
    }
    for (s32 i = 0; i < ndev; i++)
   {
    hrtFree(sendbuf[i]);
    hrtFree(recvbuf[i]);
    hrtFree(result_buff[i]);
    rt_ret = aclrtDestroyStream(stream[i]);

    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    ret = NetworkManager::GetInstance(i).Destroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    set_chip_type_stub(i, static_cast<s32>(DevType::DEV_TYPE_910));
   }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}
#endif

#if 0 //coredump
TEST_F(HcclInterTestFFTS, st_reduce_inter_prod_char)
{

    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"deploy_mode", "lab"},
        {"group_count", "1"},
        {"chip_info", "910"},
        {"board_id", "0x0000"},
        {"para_plane_nic_location", "device"},
        {"para_plane_nic_num", "4"},
        {"para_plane_nic_name", {"eth0", "eth1","eth2", "eth3"}},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"device_num", "4"},
                    {"server_num", "1"},
                    {"instance_count", "4"},
                        {
                            "instance_list",
                            {
                                {   {"rank_id", "0"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "0"}, {"device_ip", "192.168.0.12"}}}
                                    }
                                },

                                {   {"rank_id", "1"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "1"}, {"device_ip", "192.168.0.14"}}}
                                    }
                                },
                                {   {"rank_id", "2"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "2"}, {"device_ip", "192.168.0.16"}}}
                                    }
                                },

                                {   {"rank_id", "3"}, {"server_id", "10.0.0.10"},
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
                                    {"server_id", "192.168.10.2"},
                                    {
                                        "para_plane_info",
                                        {{
                                                {"eth1", "192.168.210.2"},
                                            },
                                            {
                                                {"eth0", "192.168.200.2"},
                                            }
                                        }
                                    }

                                },
                            }
                        }
                }
            }
        }
    };

    char file_name_t[] = "./st_reduce_inter_prod_char.json";
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

    s8* result_buff[DEV_NUM_4];
    s8* sendbuf[DEV_NUM_4];
    s8* recvbuf[DEV_NUM_4];

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];

    HcclDataType datatype = HCCL_DATA_TYPE_INT8;

    HcclReduceOp op = HCCL_REDUCE_PROD;
    s32 count = HCC_REDUCE_DATA_SIZE;
    s32 ndev = DEV_NUM_4;

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    for (s32 i = 0; i < ndev; i++ )
    {
        ret = hrtMalloc((void **)&sendbuf[i], count * sizeof(s8));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(sendbuf[i], count * sizeof(s8), 0, count * sizeof(s8));
        ret = hrtMalloc((void **)&recvbuf[i] , count * sizeof(s8));
        sal_memset(recvbuf[i], count * sizeof(s8), 0, count * sizeof(s8));
        ret = hrtMalloc((void **)&result_buff[i] ,count * sizeof(s8));
        sal_memset(result_buff[i], count * sizeof(s8), 0, count * sizeof(s8));
    }
   
    for (u32 j = 0; j < ndev; j++)
    {
        for (u32 i = 0; i < count; i++)
        {
            sendbuf[j][i] = 2;
        }
    }

    for (u32 j = 0; j < count; j++)
     {
            result_buff[0][j] = 16;
     }

    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = aclrtCreateStream(&stream[i]);
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
        para_info[i].recvbuff = recvbuf[i];
        para_info[i].op = op;
        para_info[i].root = 0;
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;
    }
    
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread_ffts", inter_reduce_task_ffts, (void*)&para_info[i]);
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
        rt_ret = aclrtSynchronizeStream(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    if (errors)
    {
        HCCL_ERROR("%d errors. Test FAILED.\n", errors);
    }
    else
    {
        HCCL_INFO("Test PASSED.\n");
    }
    for (s32 i = 0; i < ndev; i++)
   {
    hrtFree(sendbuf[i]);
    hrtFree(recvbuf[i]);
    hrtFree(result_buff[i]);
    rt_ret = aclrtDestroyStream(stream[i]);

    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    ret = NetworkManager::GetInstance(i).Destroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
   }
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
}
#endif