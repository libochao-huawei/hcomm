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

#include <securec.h>

#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/mman.h>




#include <runtime/rt.h>

#define private public
#define protected public
#include "hcom_common.h"
#include "hccl_comm_pub.h"
#include "hccl_communicator.h"
#include "topoinfo_parse.h"
#include "topoinfo_ranktableParser_pub.h"
#include "hccl_communicator_attrs.h"
#include "topoinfo_struct.h"
#include "preempt_port_manager.h"
#undef private
#undef protected
#include "hcom_ops_stores.h"
#include "external/hcom/hcom_topo_info.h"

#include "hcom_pub.h"
#include <hccl/hcom.h>

#include "llt_hccl_stub_pub.h"
#include <driver/ascend_hal.h>
#include "adapter_rts.h"
#include "sal.h"
#include "hccl_impl.h"
#include "gradient_segment.h"
#include "config.h"
#include "stream_pub.h"
#include "hccl/hcom.h"
#include "externalinput.h"
#include "v80_rank_table.h"
#include <iostream>
#include <fstream>
#include "opexecounter_pub.h"
#include "param_check_pub.h"
#include "device_capacity.h"
#include "hcom_private.h"
#include "env_config.h"
#include "topoinfo_ranktableStandard.h"
#include "op_base.h"

using namespace std;
using namespace hccl;

extern HcclResult HcomSetGradFusionByIndex(const char *group, u32 segmentNum, const u32 *IdxList);
extern HcclResult HcomSetGradFusionBySize(const char *group, u32 segmentNum, const float *sizeList);
extern HcclResult HcomDestroyBackloggedGroup(const std::string &group);
class HcomTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        nlohmann::json rank_table = rank_table_910_2server_8rank;
        char file_name[] = "./st_hcom.json";
        std::ofstream outfile(file_name, std::ios::out | std::ios::trunc | std::ios::binary);
        if (outfile.is_open()) {
            HCCL_INFO("open %s success", file_name);
        } else {
            HCCL_INFO("open %s failed", file_name);
        }
        outfile << std::setw(4) << rank_table << std::endl;
        outfile.close();
        std::cout << "HcomTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        char file_name[] = "./st_hcom.json";
        remove(file_name);
        std::cout << "HcomTest TearDown" << std::endl;
    }
    virtual void SetUp()
    {
        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        setenv("HCCL_DFS_CONFIG", "connection_fault_detection_time:0", 1);
        setenv("HCCL_OP_RETRY_ENABLE", "L0:0, L1:0, L2:0", 1);
        InitEnvParam();

        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};

TEST_F(HcomTest, st_rt_get_set_Device_v81)
{
    s32 ret = HCCL_SUCCESS;

    DevType  chipType;
    char machinChipVer[CHIP_VERSION_MAX_LEN] = {0};
    set_chip_type_stub(0, static_cast<s32>(DevType::DEV_TYPE_910B));

    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    s32 deviceLogicId = 0;
    ret = hrtGetDevice(&deviceLogicId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    set_chip_type_stub(0, static_cast<s32>(DevType::DEV_TYPE_910));
}


#define HCCL_COM_DATA_SIZE 1024
#if 1
TEST_F(HcomTest, st_hcom_bcast)
{

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_hcom_bcast.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 走1910 4pring
    char* rank_table_file = "./st_hcom_bcast.json";
    char* rank_ID = "0";

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sendbuf = (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendbuf, count * sizeof(s8) , 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcomBroadcast("testbcast", sendbuf, count, HCCL_DATA_TYPE_INT8, 0, NULL, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtSynchronizeStream(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (sendbuf[j] != 2)
        {
            HCCL_ERROR("\n rank:%d sendbuf[%d]:%f", rank, j, sendbuf[j] );
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    rt_ret = aclrtDestroyStream(stream);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}

TEST_F(HcomTest, st_hcom_bcast_cloud)
{
    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"group_count", "1"},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"instance_count", "1"},
                    {"device_count", "1"},
                    {
                        "instance_list",
                        {
                            {   {"pod_name", "tf-bae43"},
                                {"server_id", "10.0.0.10"},
                                {
                                    "devices", {{{"device_id", "0"}, {"device_ip", "192.168.0.12"}}}
                                }
                            },
                        }
                    },
                }
            }
        },
    };

    char file_name_t[] = "./st_hcom_bcast_cloud.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 走1910 4pring
    char* rank_table_file = "./st_hcom_bcast_cloud.json";
    char* pod_name = "tf-bae43";

    ret = HcomInitByFile(rank_table_file, pod_name);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sendbuf = (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendbuf, count * sizeof(s8) , 0, count * sizeof(s8) );

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcomBroadcast("testbcast", sendbuf, count, HCCL_DATA_TYPE_INT8, 0, NULL, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtSynchronizeStream(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (sendbuf[j] != 2)
        {
            HCCL_ERROR("\n rank:%d sendbuf[%d]:%f", rank, j, sendbuf[j] );
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    rt_ret = aclrtDestroyStream(stream);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}

#endif

TEST_F(HcomTest, st_hcom_bcast_cloud_dev1)
{
    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"group_count", "1"},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"instance_count", "1"},
                    {"device_count", "1"},
                    {
                        "instance_list",
                        {
                            {   {"pod_name", "tf-bae41"},
                                {"server_id", "10.0.0.10"},
                                {
                                    "devices", {{{"device_id", "1"}, {"device_ip", "192.168.0.12"}}}
                                }
                            },
                        }
                    },
                }
            }
        },
    };

    char file_name_t[] = "./st_hcom_bcast_cloud_dev1.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s32 rank = 0;
    s32 errors = 0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcom_bcast_cloud_dev1.json";
    char* pod_name = "tf-bae41";

    ret = HcomInitByFile(rank_table_file, pod_name);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sendbuf = (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendbuf, count * sizeof(s8) , 0, count * sizeof(s8) );

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcomBroadcast("testbcast", sendbuf, count, HCCL_DATA_TYPE_INT8, 0, NULL, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtSynchronizeStream(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (sendbuf[j] != 2)
        {
            HCCL_ERROR("\n rank:%d sendbuf[%d]:%f", rank, j, sendbuf[j] );
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    rt_ret = aclrtDestroyStream(stream);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}


#if 1
TEST_F(HcomTest, st_hcom_reduce)
{

    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"deploy_mode", "lab"},
        {"group_count", "1"},
        {"chip_info", "910"},
        {"board_id", "0x0000"},
        {"para_plane_nic_location", "device"},
        {"para_plane_nic_num", "1"},
        {"para_plane_nic_name", {"eth0"}},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"device_num", "1"},
                    {"server_num", "1"},
                    {"instance_count", "1"},
                        {
                            "instance_list",
                            {
                                {   {"rank_id", "0"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "0"}, {"device_ip", "192.168.0.12"}}}
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
                                                {"eth0", "192.168.0.12"},
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

    char file_name_t[] = "./st_hcom_reduce.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors=0;
    s32 count = HCCL_COM_DATA_SIZE;
    set_board_id(0x0000);
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcom_reduce.json";
    char* rank_ID = "0";

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8) , 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8) , 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcomReduce("testreduce", sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, 0, NULL, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtSynchronizeStream(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = aclrtDestroyStream(stream);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    set_board_id(0);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}

TEST_F(HcomTest, st_hcom_reduce_cloud)
{
    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"group_count", "1"},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"instance_count", "1"},
                    {"device_count", "1"},
                    {
                        "instance_list",
                        {
                            {   {"pod_name", "tf-bae43"},
                                {"server_id", "10.0.0.10"},
                                {
                                    "devices", {{{"device_id", "0"}, {"device_ip", "192.168.0.12"}}}
                                }
                            },
                        }
                    },
                }
            }
        },
    };

    char file_name_t[] = "./st_hcom_reduce_cloud.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors=0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcom_reduce_cloud.json";
    char* pod_name = "tf-bae43";

    ret = HcomInitByFile(rank_table_file, pod_name);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8) , 0, count * sizeof(s8) );
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8) , 0, count * sizeof(s8) );

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcomReduce("testreduce", sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, 0, NULL, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtSynchronizeStream(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = aclrtDestroyStream(stream);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}

#endif
#if 1
TEST_F(HcomTest, st_hcom_allreduce)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_hcom_allreduce.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors=0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcom_allreduce.json";
    char* rank_ID = "0";

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
    ret = HcomAllReduce("testallreduce", sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, NULL, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtSynchronizeStream(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = aclrtDestroyStream(stream);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}

TEST_F(HcomTest, st_hcom_allreduce_cloud)
{
    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"group_count", "1"},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"instance_count", "1"},
                    {"device_count", "1"},
                    {
                        "instance_list",
                        {
                            {   {"pod_name", "tf-bae43"},
                                {"server_id", "10.0.0.10"},
                                {
                                    "devices", {{{"device_id", "0"}, {"device_ip", "192.168.0.12"}}}
                                }
                            },
                        }
                    },
                }
            }
        },
    };

    char file_name_t[] = "./st_hcom_allreduce_cloud.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors=0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcom_allreduce_cloud.json";
    char* pod_name = "tf-bae43";

    ret = HcomInitByFile(rank_table_file, pod_name);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
    ret = HcomAllReduce("testallreduce", sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, NULL, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtSynchronizeStream(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = aclrtDestroyStream(stream);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}

#endif
#if 1
TEST_F(HcomTest, st_hcom_allgather)
{

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_hcom_allgather.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors=0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcom_allgather.json";
    char* rank_ID = "0";

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8) , 0, count * sizeof(s8) );
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8) , 0, count * sizeof(s8) );

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcomAllGather("testallgather", sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, NULL, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtSynchronizeStream(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = aclrtDestroyStream(stream);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}

TEST_F(HcomTest, st_hcom_allgather_cloud)
{
    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"group_count", "1"},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"instance_count", "1"},
                    {"device_count", "1"},
                    {
                        "instance_list",
                        {
                            {   {"pod_name", "tf-bae43"},
                                {"server_id", "10.0.0.10"},
                                {
                                    "devices", {{{"device_id", "0"}, {"device_ip", "192.168.0.12"}}}
                                }
                            },
                        }
                    },
                }
            }
        },
    };

    char file_name_t[] = "./st_hcom_allgather_cloud.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors=0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcom_allgather_cloud.json";
    char* pod_name = "tf-bae43";

    ret = HcomInitByFile(rank_table_file, pod_name);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8) , 0, count * sizeof(s8) );
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8) , 0, count * sizeof(s8) );

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }

    ret = HcomAllGather("testallgather", sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, NULL, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtSynchronizeStream(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = aclrtDestroyStream(stream);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}

TEST_F(HcomTest, st_hcom_allgatherv)
{
    HcclCommunicator impl;
    MOCKER_CPP_VIRTUAL(impl, &HcclCommunicator::Init,HcclResult(HcclCommunicator::*)(HcclCommParams &params, const RankTable_t &rankTable))
    .expects(atMost(1))
    .will(returnValue(0));
    char* rank_table_file = "./st_hcom.json";
    char* rank_ID = "0";
    hrtSetDevice(0);
    HcclResult ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    s8* sendbuf = (s8*)sal_malloc(10 * sizeof(s8));
    sal_memset(sendbuf, 10 * sizeof(s8), 0, 10 * sizeof(s8));
    s8* recv = (s8*)sal_malloc(16 * 10 * sizeof(s8));
    sal_memset(recv, 16 * 10 * sizeof(s8), 0, 16 * 10 * sizeof(s8));

    rtStream_t stream;

    rtError_t rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    HcclCommunicator impl2;
    MOCKER_CPP_VIRTUAL(impl2, &HcclCommunicator::AllGatherV)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetRankTableCrc)
    .stubs()
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetAlgType)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&hcclComm::GetRankSize)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetGroupRank)
    .expects(atMost(1))
    .will(returnValue(0));

    // 构造入参
    int32_t rankSize = 2;
    vector<u64> recvCounts(rankSize, 10);
    vector<u64> rdispls(rankSize, 0);
    for (int i = 0; i < rankSize; i++) {
        rdispls[i] = 10 * i;
    }

    ret = HcomAllGatherV("tag", sendbuf, 10,recv, recvCounts.data(), rdispls.data(), HCCL_DATA_TYPE_INT8,NULL, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    aclrtSynchronizeStream(stream);
    rt_ret = aclrtDestroyStream(stream);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sal_free(sendbuf);
    sal_free(recv);
}

#endif

#if 1
TEST_F(HcomTest, st_hcom_reducescatter)
{
    int ret = HCCL_SUCCESS;
    ret = InitEnvVarParam();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_hcom_reducescatter.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors=0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcom_reducescatter.json";
    char* rank_ID = "0";

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8) , 0, count * sizeof(s8) );
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8) , 0, count * sizeof(s8) );

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
    //-----------------Set Workspace Resource Start------------------//
    rtModel_t model = (rtModel_t)NULL;
    rt_ret = rtModelCreate(&model, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    u64 stream_list_size = 0;
    ret = HcomGetWorkspaceSubStreamNum("group_0", stream_list_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(stream_list_size, 0);
    ret = HcomGetWorkspaceSubStreamNum(HCCL_WORLD_GROUP, stream_list_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("get stream_list_size[%d] success", stream_list_size);
    vector<HcclRtStream> streamList(stream_list_size);
    //生成从stream
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = aclrtCreateStreamWithConfig(&streamList[i], 0, ACL_STREAM_PERSISTENT);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        //从流bind到model
        rt_ret = rtModelBindStream(model, streamList[i], RT_MODEL_WAIT_ACTIVE_STREAM);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    u32 rankSize = 0;
    ret = HcomGetRankSize(HCCL_WORLD_GROUP, &rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 memSize = 0;
    ret = HcomGetWorkspaceMemSize("HcomReduceScatter", count, HCCL_DATA_TYPE_INT8, HCCL_WORLD_GROUP, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomSetWorkspaceResource("testreducescatter", HCCL_WORLD_GROUP, streamList.data(), streamList.size(), memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//
    ret = HcomReduceScatter("testreducescatter", sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, HCCL_WORLD_GROUP, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtSynchronizeStream(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            HCCL_ERROR("Err recvbuf[%d] = [%d] ",j,recvbuf[j]);
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = aclrtDestroyStream(stream);
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtModelUnbindStream(model, streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);

        rt_ret = aclrtDestroyStream(streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    ret = HcomDestroy();


    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}

TEST_F(HcomTest, st_hcom_reducescatterv)
{
    nlohmann::json rank_table = rank_table_910_2server_8rank;
    char file_name[] = "./st_hcom.json";
    std::ofstream outfile(file_name, std::ios::out | std::ios::trunc | std::ios::binary);
    if (outfile.is_open()) {
        HCCL_INFO("open %s success", file_name);
    } else {
        HCCL_INFO("open %s failed", file_name);
    }
    outfile << std::setw(4) << rank_table << std::endl;
    outfile.close();

    HcclCommunicator impl;
    MOCKER_CPP_VIRTUAL(impl, &HcclCommunicator::Init,HcclResult(HcclCommunicator::*)(HcclCommParams &params, const RankTable_t &rankTable))
    .expects(atMost(1))
    .will(returnValue(0));
    char* rank_table_file = "./st_hcom.json";
    char* rank_ID = "0";
    hrtSetDevice(0);
    HcclResult ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    s8* sendbuf = (s8*)sal_malloc(16 * 10 * sizeof(s8));
    sal_memset(sendbuf, 16 * 10 * sizeof(s8), 0, 16 * 10 * sizeof(s8));
    s8* recvbuf = (s8*)sal_malloc(10 * sizeof(s8));
    sal_memset(recvbuf, 10 * sizeof(s8), 0, 10 * sizeof(s8));

    rtStream_t stream;
 
    rtError_t rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    HcclCommunicator impl2;
    MOCKER_CPP_VIRTUAL(impl2, &HcclCommunicator::ReduceScatterV)
    .expects(atMost(1))
    .will(returnValue(0));
 
    MOCKER_CPP(&hcclComm::GetRankTableCrc)
    .stubs()
    .will(returnValue(0));
 
    MOCKER_CPP(&hcclComm::GetAlgType)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&hcclComm::GetRankSize)
    .expects(atMost(1))
    .will(returnValue(0));
 
    MOCKER_CPP(&hcclComm::GetGroupRank)
    .expects(atMost(1))
    .will(returnValue(0));
 
    // 构造入参
    int32_t rankSize = 2;
    vector<u64> sendCounts(rankSize, 10);
    vector<u64> sdispls(rankSize, 0);
    for (int i = 0; i < rankSize; i++) {
        sdispls[i] = 10 * i;
    }
 
    ret = HcomReduceScatterV("tag", sendbuf, sendCounts.data(), sdispls.data(), recvbuf, 10,
        HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, HCCL_WORLD_GROUP, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
 
    aclrtSynchronizeStream(stream);
    rt_ret = aclrtDestroyStream(stream);
 
    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    sal_free(sendbuf);
    sal_free(recvbuf);

    remove(file_name);
}

TEST_F(HcomTest, st_hcom_reducescatterv_check_int64)
{
    nlohmann::json rank_table = rank_table_910_2server_8rank;
    char file_name[] = "./st_hcom.json";
    std::ofstream outfile(file_name, std::ios::out | std::ios::trunc | std::ios::binary);
    if (outfile.is_open()) {
        HCCL_INFO("open %s success", file_name);
    } else {
        HCCL_INFO("open %s failed", file_name);
    }
    outfile << std::setw(4) << rank_table << std::endl;
    outfile.close();

    HcclCommunicator impl;
    MOCKER_CPP_VIRTUAL(impl, &HcclCommunicator::Init,HcclResult(HcclCommunicator::*)(HcclCommParams &params, const RankTable_t &rankTable))
    .expects(atMost(1))
    .will(returnValue(0));
    char* rank_table_file = "./st_hcom.json";
    char* rank_ID = "0";
    hrtSetDevice(0);
    HcclResult ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    s8* sendbuf = (s8*)sal_malloc(16 * 10 * sizeof(s8));
    sal_memset(sendbuf, 16 * 10 * sizeof(s8), 0, 16 * 10 * sizeof(s8));
    s8* recvbuf = (s8*)sal_malloc(10 * sizeof(s8));
    sal_memset(recvbuf, 10 * sizeof(s8), 0, 10 * sizeof(s8));

    rtStream_t stream;
 
    rtError_t rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
 
    HcclCommunicator impl2;
    MOCKER_CPP_VIRTUAL(impl2, &HcclCommunicator::ReduceScatterV)
    .expects(atMost(1))
    .will(returnValue(0));
 
    MOCKER_CPP(&hcclComm::GetRankTableCrc)
    .stubs()
    .will(returnValue(0));
 
    MOCKER_CPP(&hcclComm::GetAlgType)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&hcclComm::GetRankSize)
    .expects(atMost(1))
    .will(returnValue(0));
 
    MOCKER_CPP(&hcclComm::GetGroupRank)
    .expects(atMost(1))
    .will(returnValue(0));
 
    // 构造入参
    int32_t rankSize = 2;
    vector<u64> sendCounts(rankSize, 10);
    vector<u64> sdispls(rankSize, 0);
    for (int i = 0; i < rankSize; i++) {
        sdispls[i] = 10 * i;
    }
 
    ret = HcomReduceScatterV("tag", sendbuf, sendCounts.data(), sdispls.data(), recvbuf, 10,
        HCCL_DATA_TYPE_INT64, HCCL_REDUCE_SUM, HCCL_WORLD_GROUP, stream);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
    GlobalMockObject::verify();
 
    aclrtSynchronizeStream(stream);
    rt_ret = aclrtDestroyStream(stream);
 
    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    sal_free(sendbuf);
    sal_free(recvbuf);

    remove(file_name);
}

TEST_F(HcomTest, st_hcom_reducescatter_cloud)
{
    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"group_count", "1"},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"instance_count", "1"},
                    {"device_count", "1"},
                    {
                        "instance_list",
                        {
                            {   {"pod_name", "tf-bae43"},
                                {"server_id", "10.0.0.10"},
                                {
                                    "devices", {{{"device_id", "0"}, {"device_ip", "192.168.0.12"}}}
                                }
                            },
                        }
                    },
                }
            }
        },
    };

    char file_name_t[] = "./st_hcom_reducescatter_cloud.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors=0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcom_reducescatter_cloud.json";
    char* pod_name = "tf-bae43";

    ret = HcomInitByFile(rank_table_file, pod_name);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(sendbuf, count * sizeof(s8) , 0, count * sizeof(s8) );
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
     sal_memset(recvbuf, count * sizeof(s8) , 0, count * sizeof(s8) );

    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
    //-----------------Set Workspace Resource Start------------------//
    rtModel_t model = (rtModel_t)NULL;
    rt_ret = rtModelCreate(&model, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    u64 stream_list_size = 0;
    ret = HcomGetWorkspaceSubStreamNum(HCCL_WORLD_GROUP, stream_list_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_INFO("get stream_list_size[%d] success", stream_list_size);
    vector<HcclRtStream> streamList(stream_list_size);
    //生成从stream
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = aclrtCreateStreamWithConfig(&streamList[i], 0, ACL_STREAM_PERSISTENT);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        //从流bind到model
        rt_ret = rtModelBindStream(model, streamList[i], RT_MODEL_WAIT_ACTIVE_STREAM);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    u32 rankSize = 0;
    ret = HcomGetRankSize(HCCL_WORLD_GROUP, &rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 memSize = 0;
    ret = HcomGetWorkspaceMemSize("HcomReduceScatter", count, HCCL_DATA_TYPE_INT8, HCCL_WORLD_GROUP, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomSetWorkspaceResource("testreducescatter", HCCL_WORLD_GROUP, streamList.data(), streamList.size(), memptr, memSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//
    ret = HcomReduceScatter("testreducescatter", sendbuf, recvbuf, count, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, HCCL_WORLD_GROUP, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtSynchronizeStream(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    for (int j = 0; j < count; j++)
    {
        if (recvbuf[j] != 2)
        {
            HCCL_ERROR("ERR recvbuf[%d] = [%d] ",j,recvbuf[j]);
            errors ++;
            break;
        }
    }

    sal_free(sendbuf);
    sal_free(recvbuf);
    rt_ret = aclrtDestroyStream(stream);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtModelUnbindStream(model, streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);

        rt_ret = aclrtDestroyStream(streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}

#endif

// 覆盖hcom 获取ranksize 和rankid的函数
#if 1
TEST_F(HcomTest, st_hcom_get_rank_info)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_hcom_get_rank_info.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();
    int ret = HCCL_SUCCESS;


    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u32 ranksize = 0;
    u32 rankid = 1;
    u32 localranksize = 0;
    u32 localrankid = 0;
    u32 numHccsLink = 0;
    char* rank_table_file = "./st_hcom_get_rank_info.json";
    char* rank_ID = "0";

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomGetRankSize(HCCL_WORLD_GROUP, &ranksize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomGetRankSize(HCCL_WORLD_GROUP, &ranksize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomGetRankId(HCCL_WORLD_GROUP, &rankid);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomGetLocalRankSize(HCCL_WORLD_GROUP, &localranksize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomGetLocalRankId(HCCL_WORLD_GROUP, &localrankid);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomGetHccsLinkNum(NULL, &numHccsLink);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);

    EXPECT_EQ(ranksize, 1);
    EXPECT_EQ(rankid, 0);
    EXPECT_EQ(localranksize, 1);
    EXPECT_EQ(localrankid, 0);
}

TEST_F(HcomTest, st_hcom_get_rank_info_boardType0_alg0)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_hcom_get_rank_info_boardType0_alg0.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();
    int ret = HCCL_SUCCESS;


    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u32 ranksize = 0;
    u32 rankid = 1;

    setenv("HCCL_ALG_TYPE", "0", 1);

    char* rank_table_file = "./st_hcom_get_rank_info_boardType0_alg0.json";
    char* rank_ID = "0";

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_ALG_TYPE");

    ret = HcomGetRankSize(HCCL_WORLD_GROUP, &ranksize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomGetRankId(HCCL_WORLD_GROUP, &rankid);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);

    EXPECT_EQ(ranksize, 1);
    EXPECT_EQ(rankid, 0);
}
#endif
#if 1
TEST_F(HcomTest, st_hcom_get_data_size)
{
    s32 ret = 0;

    ret = get_data_size(HCCL_DATA_TYPE_INT8);
    EXPECT_EQ(ret, 1);

    ret = get_data_size(HCCL_DATA_TYPE_INT32);
    EXPECT_EQ(ret, 4);

    ret = get_data_size(HCCL_DATA_TYPE_FP16);
    EXPECT_EQ(ret, 2);

    ret = get_data_size(HCCL_DATA_TYPE_FP32);
    EXPECT_EQ(ret, 4);

    ret = get_data_size(HCCL_DATA_TYPE_RESERVED);
    EXPECT_EQ(ret, 0);
}
#endif

TEST_F(HcomTest, st_hcom_cfg_check_file_path_test)
{
    s32 ret = 0;

    std::string file_path = "./testjson.json";
    std::string file_type = ".json";

    ret  = CheckFilePath(file_path, file_type);
    EXPECT_EQ(ret, true);
}


TEST_F(HcomTest, st_hcom_cfg_get_file_name_test)
{
    s32 ret = HCCL_SUCCESS;

    std::string file_path = "./testjson.json";
    std::string file_type = ".json";
    std::string file_name = "";

    ret  = GetFileName(file_path, file_type, file_name);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ((file_name == "testjson"), true);
}

TEST_F(HcomTest, st_hcom_load_rank_table_from_file_to_json_fail)
{
    s32 ret = HCCL_SUCCESS;
    char file_name[] = "./jobstart_hccl_invalid_json_file.json";
    std::ofstream outfile(file_name, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << "invalid json format" << std::endl;
        HCCL_INFO("open %s success", file_name);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name);
    }

    outfile.close();

    char* rank_ID = "0";
    HcomInfo hcom_info;
    std::string rankTableM;
    std::string realFilePath;
    ret = HcomLoadRanktableFile(file_name, rankTableM, realFilePath);
    EXPECT_NE(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, rank_ID, hcom_info.params, hcom_info.rankTable);
    EXPECT_NE(ret, HCCL_SUCCESS);
    remove(file_name);

}
#define MAX_BUF_SIZE (1*1024*1024)
#if 0 //coredump
TEST_F(HcomTest, st_hcom_gradient_segment_no_model)
{
    struct model_feature feature;
    std::vector<u32> segment_index;

    HcclResult ret;

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_hcom_gradient_segment_no_model.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcom_gradient_segment_no_model.json";
    char* rank_ID = "0";
    char group[] = "1";
    char model_name[] = "";
    g_segmentIdxMap.clear();
    g_segmentSizeMap.clear();
    feature.gradient_num = 20;
    feature.gradient_size = (float*)sal_malloc(20 * sizeof(float));
    sal_memset(feature.gradient_size, 20 * sizeof(float), 0, 20 * sizeof(float));
    feature.gradient_time = (float*)sal_malloc(20 * sizeof(float));
    sal_memset(feature.gradient_time, 20 * sizeof(float), 0, 20 * sizeof(float));
    feature.model_name = model_name;

    for (u32 i = 0; i < feature.gradient_num; i++)
    {
        feature.gradient_size[i] = 1.0;
    }

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    bool isConfig = true;
    u32 len = segment_index.size();
    u32 *segment_index_ptr = reinterpret_cast<u32 *>(segment_index.data());
    ret = HcomGetSplitStrategy(group, &feature, &segment_index_ptr, reinterpret_cast<u32 *>(&len), &isConfig);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(segment_index[0], 18);
    EXPECT_EQ(segment_index[1], 19);

    sal_free(feature.gradient_size);
    sal_free(feature.gradient_time);

    ret = HcomDestroy();
    remove(file_name_t);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcomTest, st_hcom_gradient_segment_more_than_8_segment)
{
    struct model_feature feature;
    u32 segment_num = 10;
    std::vector<u32> segment_index;
    HcclResult ret;
    u32 segList[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_hcom_gradient_segment_more_than_8_segment.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcom_gradient_segment_more_than_8_segment.json";
    char* rank_ID = "0";
    char setGroup[] = "1";
    char getGroup[] = "1";
    char model_name[] = "resnet50";
    g_segmentIdxMap.clear();
    g_segmentSizeMap.clear();
    feature.gradient_num = 10;
    feature.gradient_size = (float*)sal_malloc(10 * sizeof(float));
    sal_memset(feature.gradient_size, 10 * sizeof(float), 0, 10 * sizeof(float));
    feature.gradient_time = (float*)sal_malloc(10 * sizeof(float));
    sal_memset(feature.gradient_time, 10 * sizeof(float), 0, 10 * sizeof(float));
    feature.model_name = model_name;

    for (u32 i = 0; i < feature.gradient_num; i++)
    {
        feature.gradient_size[i] = 1.0;
    }

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomSetGradFusionByIndex(setGroup, segment_num, segList);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    bool isConfig = true;
    u32 len = segment_index.size();
    u32 *segment_index_ptr = reinterpret_cast<u32 *>(segment_index.data());
    ret = HcomGetSplitStrategy(getGroup, &feature, &segment_index_ptr, reinterpret_cast<u32 *>(&len), &isConfig);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(segment_index[0], 0);

    sal_free(feature.gradient_size);
    sal_free(feature.gradient_time);

    ret = HcomDestroy();
    remove(file_name_t);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcomTest, st_hcom_gradient_segment_global_set_and_get)
{
    struct model_feature feature;
    std::vector<u32> segment_index;
    HcclResult ret;
    u32 segList[2] = {1, 4};

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_hcom_gradient_segment_global_set_and_get.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcom_gradient_segment_global_set_and_get.json";
    char* rank_ID = "0";

    char setGroup[] = "1";
    char getGroup[] = "1";
    char model_name[] = "resnet50";
    g_segmentIdxMap.clear();
    g_segmentSizeMap.clear();
    feature.gradient_num = 5;
    feature.gradient_size = (float*)sal_malloc(5 * sizeof(float));
    sal_memset(feature.gradient_size, 5 * sizeof(float), 0, 5 * sizeof(float));
    feature.gradient_time = (float*)sal_malloc(5 * sizeof(float));
    sal_memset(feature.gradient_time, 5 * sizeof(float), 0, 5 * sizeof(float));
    feature.model_name = model_name;

    ret = HcomSetGradFusionByIndex(setGroup, 2, segList);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    bool isConfig = true;
    u32 len = segment_index.size();
    u32 *segment_index_ptr = reinterpret_cast<u32 *>(segment_index.data());
    ret = HcomGetSplitStrategy(getGroup, &feature, &segment_index_ptr, reinterpret_cast<u32 *>(&len), &isConfig);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(segment_index[0], 1);
    EXPECT_EQ(segment_index[1], 4);

    sal_free(feature.gradient_size);
    sal_free(feature.gradient_time);

    ret = HcomDestroy();
    remove(file_name_t);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcomTest, st_hcom_gradient_segment_get_and_global_set)
{
    struct model_feature feature;
    std::vector<u32> segment_index;
    HcclResult ret;
    u32 segList[2] = {1, 19};

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_hcom_gradient_segment_get_and_global_set.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcom_gradient_segment_get_and_global_set.json";
    char* rank_ID = "0";

    char setGroup[] = "1";
    char getGroup[] = "1";
    char model_name[] = "resnet50";
    g_segmentIdxMap.clear();
    g_segmentSizeMap.clear();
    feature.gradient_num = 20;
    feature.gradient_size = (float*)sal_malloc(20 * sizeof(float));
    sal_memset(feature.gradient_size, 20 * sizeof(float), 0, 20 * sizeof(float));
    feature.gradient_time = (float*)sal_malloc(20 * sizeof(float));
    sal_memset(feature.gradient_time, 20 * sizeof(float), 0, 20 * sizeof(float));
    feature.model_name = model_name;

    for (u32 i = 0; i < feature.gradient_num; i++)
    {
        feature.gradient_size[i] = 1.0;
    }

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    bool isConfig = true;
    u32 len = segment_index.size();
    u32 *segment_index_ptr = reinterpret_cast<u32 *>(segment_index.data());
    ret = HcomGetSplitStrategy(getGroup, &feature, &segment_index_ptr, reinterpret_cast<u32 *>(&len), &isConfig);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(segment_index[0], 18);
    EXPECT_EQ(segment_index[1], 19);

    ret = HcomSetGradFusionByIndex(setGroup, 2, segList);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sal_free(feature.gradient_size);
    sal_free(feature.gradient_time);

    ret = HcomDestroy();
    remove(file_name_t);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcomTest, st_hcom_gradient_segment_global_size_set_and_get)
{
    struct model_feature feature;
    std::vector<u32> segment_index;
    HcclResult ret;
    float segList[3] = {20, 40, 40};

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_hcom_gradient_segment_global_size_set_and_get.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcom_gradient_segment_global_size_set_and_get.json";
    char* rank_ID = "0";

    char setGroup[] = "1";
    char getGroup[] = "1";
    char model_name[] = "resnet50";
    float gradient_array[30] = {4096,8257536,8704,8704,4194304,2560,2560,9437184,2560,
        2560,4194304,8704,8704,4194304,2560,2560,9437184,2560,2560,4194304,8704,8704,
        8388608,4194304,2560,2560,9437184,2560,2560,2097152
    };
    feature.gradient_num = 30;
    feature.gradient_size = gradient_array;
    feature.gradient_time = (float*)sal_malloc(30 * sizeof(float));
    sal_memset(feature.gradient_time, 30 * sizeof(float), 0, 30 * sizeof(float));
    feature.model_name = model_name;
    g_segmentIdxMap.clear();
    g_segmentSizeMap.clear();
    ret = HcomSetGradFusionBySize(setGroup, 3, segList);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    bool isConfig = true;
    u32 len = segment_index.size();
    u32 *segment_index_ptr = reinterpret_cast<u32 *>(segment_index.data());
    ret = HcomGetSplitStrategy(getGroup, &feature, &segment_index_ptr, reinterpret_cast<u32 *>(&len), &isConfig);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(segment_index[0], 6);
    EXPECT_EQ(segment_index[1], 18);
    EXPECT_EQ(segment_index[2], 29);
    sal_free(feature.gradient_time);

    ret = HcomDestroy();
    remove(file_name_t);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcomTest, st_hcom_gradient_segment_global_size_get_and_set)
{
    struct model_feature feature;
    std::vector<u32> segment_index;
    HcclResult ret;
    float segList[3] = {20, 40, 40};

    nlohmann::json rank_table = rank_table_910_1server_1rank;

    char file_name_t[] = "./st_hcom_gradient_segment_global_size_get_and_set.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcom_gradient_segment_global_size_get_and_set.json";
    char* rank_ID = "0";

    char setGroup[] = "1";
    char getGroup[] = "1";
    char model_name[] = "resnet50";
    float gradient_array[30] = {4096,8257536,8704,8704,4194304,2560,2560,9437184,2560,
        2560,4194304,8704,8704,4194304,2560,2560,9437184,2560,2560,4194304,8704,8704,
        8388608,4194304,2560,2560,9437184,2560,2560,2097152
    };
    feature.gradient_num = 30;
    feature.gradient_size = gradient_array;
    feature.gradient_time = (float*)sal_malloc(30 * sizeof(float));
    sal_memset(feature.gradient_time, 30 * sizeof(float), 0, 30 * sizeof(float));
    feature.model_name = model_name;
    g_segmentSizeMap.clear();
    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    bool isConfig = true;
    u32 len = segment_index.size();
    u32 *segment_index_ptr = reinterpret_cast<u32 *>(segment_index.data());
    ret = HcomGetSplitStrategy(getGroup, &feature, &segment_index_ptr, reinterpret_cast<u32 *>(&len), &isConfig);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomSetGradFusionBySize(setGroup, 3, segList);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(segment_index[0], 25);
    EXPECT_EQ(segment_index[1], 29);
    sal_free(feature.gradient_time);

    ret = HcomDestroy();
    remove(file_name_t);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcomTest, st_hcom_gradient_segment_force_size_mode)
{
    struct model_feature feature;
    u32 segment_num = 10;
    std::vector<u32> segment_index;
    HcclResult ret;
    u32 segList[4] = {5, 10, 15, 19};

    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"deploy_mode", "lab"},
        {"group_count", "1"},
        {"chip_info", "910"},
        {"board_id", "0x0000"},
        {"para_plane_nic_location", "device"},
        {"para_plane_nic_num", "1"},
        {"para_plane_nic_name", {"eth0"}},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"device_num", "1"},
                    {"server_num", "1"},
                    {"instance_count", "1"},
                        {
                            "instance_list",
                            {
                                {   {"rank_id", "0"}, {"server_id", "172.17.1.120"},
                                    {
                                        "devices", {{{"device_id", "0"}, {"device_ip", "192.168.1.120"}}}
                                    }
                                }
                            }
                        },
                }
            }
        }
    };

    char file_name_t[] = "./st_hcom_gradient_segment_force_size_mode.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcom_gradient_segment_force_size_mode.json";
    char* rank_ID = "0";

    char setGroup[] = "1";
    char getGroup[] = "1";
    char model_name[] = "resnet50";
    float gradient_array[20] = {
        4096,4096,4096,4096,4096,
        4096,4096,4096,4096,4096,
        4096,4096,4096,4096,4096,
        4096,4096,4096,4096,4096
    };
    g_segmentIdxMap.clear();
    g_segmentSizeMap.clear();
    feature.gradient_num = 20;
    feature.gradient_size = gradient_array;
    feature.gradient_time = (float*)sal_malloc(20 * sizeof(float));
    sal_memset(feature.gradient_time, 20 * sizeof(float), 0, 20 * sizeof(float));
    feature.model_name = model_name;

    ret = HcomSetGradFusionByIndex(setGroup, 4, segList);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    bool isConfig = true;
    u32 len = segment_index.size();
    u32 *segment_index_ptr = reinterpret_cast<u32 *>(segment_index.data());
    ret = HcomGetSplitStrategy(getGroup, &feature, &segment_index_ptr, reinterpret_cast<u32 *>(&len), &isConfig);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(segment_index[0], 18);
    EXPECT_EQ(segment_index[1], 19);

    sal_free(feature.gradient_time);

    ret = HcomDestroy();
    remove(file_name_t);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#define CHK_BBIT_RET(call)                                 \
    do {                                              \
        s32 ret = call;                        \
        EXPECT_EQ(ret, 0);                                     \
    } while (0)

#define CHK_ST_PTR_NULL(ptr)   \
    do {                       \
        EXPECT_NE(ptr, nullptr);     \
    } while (0)

TEST_F(HcomTest, st_hcom_alltoallv)
{
    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"deploy_mode", "lab"},
        {"group_count", "1"},
        {"chip_info", "910"},
        {"board_id", "0x0000"},
        {"para_plane_nic_location", "device"},
        {"para_plane_nic_num", "1"},
        {"para_plane_nic_name", {"eth0"}},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"device_num", "1"},
                    {"server_num", "1"},
                    {"instance_count", "1"},
                        {
                            "instance_list",
                            {
                                {   {"rank_id", "0"}, {"server_id", "172.17.1.120"},
                                    {
                                        "devices", {{{"device_id", "0"}, {"device_ip", "192.168.1.120"}}}
                                    }
                                }
                            }
                        },
                }
            }
        }
    };

    char file_name_t[] = "./st_hcom_alltoallv.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    char *identify = "0";
    CHK_BBIT_RET(HcomInitByFile(file_name_t, identify));
    ResetInitState();
    u32 rankSize = 0;
    CHK_BBIT_RET(HcomGetRankSize(HCCL_WORLD_GROUP, &rankSize));
    u32 rankID = 0;
    CHK_BBIT_RET(HcomGetRankId(HCCL_WORLD_GROUP, &rankID));

    u64 count = 10;
    u64 memSize = rankSize * count * sizeof(int32_t);
    HostMem inputHostMem = HostMem::alloc(memSize);
    CHK_ST_PTR_NULL(inputHostMem.ptr());
    HostMem comparaMem = HostMem::alloc(memSize);
    for (u32 i = 0; i < count * rankSize; i++) {
        *(reinterpret_cast<int32_t *>(inputHostMem.ptr()) + i) = i / count;
        *(reinterpret_cast<int32_t *>(comparaMem.ptr()) + i) = rankID;
    }
    DeviceMem inputDevMem = DeviceMem::alloc(memSize);
    CHK_ST_PTR_NULL(inputDevMem.ptr());
    CHK_BBIT_RET(hrtMemSyncCopy(inputDevMem.ptr(), memSize, inputHostMem.ptr(), memSize, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));

    DeviceMem outputDevMem =  DeviceMem::alloc(memSize);
    CHK_ST_PTR_NULL(outputDevMem.ptr());

    vector<u64> sendRecvCounts(rankSize, count);
    vector<u64> sendRecvDispls(rankSize, 0);
    for (u32 index = 0; index < sendRecvDispls.size(); index++) {
        sendRecvDispls[index] = index * count;
    }

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    CHK_ST_PTR_NULL(stream.ptr());
    // void *model = nullptr;
    // CHK_BBIT_RET(rtModelCreate(&model, 0));
    // CHK_BBIT_RET(rtModelBindStream(model, stream.ptr(), 0));

    CHK_BBIT_RET(HcomAlltoAllV(inputDevMem.ptr(), sendRecvCounts.data(), sendRecvDispls.data(), HCCL_DATA_TYPE_INT32,
        outputDevMem.ptr(), sendRecvCounts.data(), sendRecvDispls.data(), HCCL_DATA_TYPE_INT32, nullptr, stream.ptr(),
        "bbit_alltoallv"));
    HCCL_INFO("TEST000: hcom alltoallv issue task");

    // CHK_BBIT_RET(rtModelLoadComplete(model));

    // CHK_ST_PTR_NULL(exeStream.ptr());
    // CHK_BBIT_RET(rtModelExecute(model, exeStream.ptr(), 0));
    // CHK_BBIT_RET(hcclStreamSynchronize(exeStream.ptr()));
    CHK_BBIT_RET(hcclStreamSynchronize(stream.ptr()));
    HCCL_INFO("TEST000: execute hcom alltoallv finished");

    HostMem resultHostMem = HostMem::alloc(memSize);
    CHK_ST_PTR_NULL(resultHostMem.ptr());
    CHK_BBIT_RET(hrtMemSyncCopy(resultHostMem.ptr(), memSize, outputDevMem.ptr(), memSize, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_DEVICE_TO_HOST));

    CHK_BBIT_RET(HcomDestroy());
    remove(file_name_t);
    // CHK_BBIT_RET(rtModelUnbindStream(model, stream.ptr()));
    // CHK_BBIT_RET(rtModelDestroy(model));
}

TEST_F(HcomTest, st_hcom_alltoallv_null_input)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_hcom_alltoallv_null_input.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();
    s32 deviceId = 0;
    char *identify = "0";
    s32 rankSize = 1;
    s32 rank = atoi(identify);
    u64 count = 2;
    HCCL_INFO("alltoall_int32 : identify[%d], count[%llu]", rank, count);
    u32 devLogicId = 0xFFFF;
    HcclResult ret = hrtGetDeviceIndexByPhyId(deviceId, devLogicId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hrtSetDevice(devLogicId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char *rankTableFile = "./st_hcom_alltoallv_null_input.json";
    ret = HcomInitByFile(rankTableFile, identify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ResetInitState();

    const int COUNT_PER_RANK = count;
    u64 memSize = COUNT_PER_RANK * rankSize * sizeof(s32);

    // 构造入参
    vector<u64> sendCounts(rankSize, 0);
    vector<u64> recvCounts(rankSize, 0);
    vector<u64> sdispls(rankSize, 0);
    vector<u64> rdispls(rankSize, 0);

    hccl::Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    ret = HcomAlltoAllV(nullptr, sendCounts.data(), sdispls.data(), HCCL_DATA_TYPE_INT32, nullptr,
        recvCounts.data(), rdispls.data(), HCCL_DATA_TYPE_INT32, nullptr, stream.ptr(), "hcom_alltoallv");
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcclStreamSynchronize(stream.ptr());

    HCCL_INFO("HcomDestory start");

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    (void)aclrtResetDevice(0);
}

#if 1
TEST_F(HcomTest, st_hcom_creatgroup)
{
    nlohmann::json rank_table_group =
    {
            {"status", "completed"},
            {"deploy_mode", "lab"},
            {"group_count", "1"},
            {"chip_info", "910"},
            {"board_id", "0x3011"},
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
                                            "devices", {{{"device_id", "0"}, {"device_ip", "192.168.0.31"}}}
                                        }
                                    },

                                    {   {"rank_id", "1"}, {"server_id", "10.0.0.10"},
                                        {
                                            "devices", {{{"device_id", "1"}, {"device_ip", "192.168.0.32"}}}
                                        }
                                    },
                                    {   {"rank_id", "2"}, {"server_id", "10.0.0.10"},
                                        {
                                            "devices", {{{"device_id", "2"}, {"device_ip", "192.168.0.33"}}}
                                        }
                                    },

                                    {   {"rank_id", "3"}, {"server_id", "10.0.0.10"},
                                        {
                                            "devices", {{{"device_id", "3"}, {"device_ip", "192.168.0.34"}}}
                                        }
                                    },
                                    {   {"rank_id", "4"}, {"server_id", "10.0.0.10"},
                                        {
                                            "devices", {{{"device_id", "4"}, {"device_ip", "192.168.0.35"}}}
                                        }
                                    },

                                    {   {"rank_id", "5"}, {"server_id", "10.0.0.10"},
                                        {
                                            "devices", {{{"device_id", "5"}, {"device_ip", "192.168.0.36"}}}
                                        }
                                    },
                                    {   {"rank_id", "6"}, {"server_id", "10.0.0.10"},
                                        {
                                            "devices", {{{"device_id", "6"}, {"device_ip", "192.168.0.37"}}}
                                        }
                                    },

                                    {   {"rank_id", "7"}, {"server_id", "10.0.0.10"},
                                        {
                                            "devices", {{{"device_id", "7"}, {"device_ip", "192.168.0.38"}}}
                                        }
                                    },
                                     {  {"rank_id", "8"}, {"server_id", "10.0.0.11"},
                                        {
                                            "devices", {{{"device_id", "0"}, {"device_ip", "192.168.0.40"}}}
                                        }
                                    },

                                    {   {"rank_id", "9"}, {"server_id", "10.0.0.11"},
                                        {
                                            "devices", {{{"device_id", "1"}, {"device_ip", "192.168.0.41"}}}
                                        }
                                    },
                                    {   {"rank_id", "10"}, {"server_id", "10.0.0.11"},
                                        {
                                            "devices", {{{"device_id", "2"}, {"device_ip", "192.168.0.42"}}}
                                        }
                                    },

                                    {   {"rank_id", "11"}, {"server_id", "10.0.0.11"},
                                        {
                                            "devices", {{{"device_id", "3"}, {"device_ip", "192.168.0.43"}}}
                                        }
                                    },
                                    {   {"rank_id", "12"}, {"server_id", "10.0.0.11"},
                                        {
                                            "devices", {{{"device_id", "4"}, {"device_ip", "192.168.0.44"}}}
                                        }
                                    },

                                    {   {"rank_id", "13"}, {"server_id", "10.0.0.11"},
                                        {
                                            "devices", {{{"device_id", "5"}, {"device_ip", "192.168.0.45"}}}
                                        }
                                    },
                                    {   {"rank_id", "14"}, {"server_id", "10.0.0.11"},
                                        {
                                            "devices", {{{"device_id", "6"}, {"device_ip", "192.168.0.46"}}}
                                        }
                                    },

                                    {   {"rank_id", "15"}, {"server_id", "10.0.0.11"},
                                        {
                                            "devices", {{{"device_id", "7"}, {"device_ip", "192.168.0.47"}}}
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
                                                    {"eth1", "192.168.201.2"},
                                                },
                                                {
                                                    {"eth2", "192.168.202.2"},
                                                },
                                                {
                                                    {"eth3", "192.168.203.2"},
                                                },
                                                {
                                                    {"eth4", "192.168.204.2"},
                                                },
                                                {
                                                    {"eth5", "192.168.205.2"},
                                                },
                                                {
                                                    {"eth6", "192.168.206.2"},
                                                },
                                                {
                                                    {"eth7", "192.168.207.2"},
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
                                                    {"eth1", "192.168.211.3"},
                                                },
                                                {
                                                    {"eth2", "192.168.212.3"},
                                                },
                                                {
                                                    {"eth3", "192.168.213.3"},
                                                },
                                                {
                                                    {"eth4", "192.168.214.3"},
                                                },
                                                {
                                                    {"eth5", "192.168.215.3"},
                                                },
                                                {
                                                    {"eth6", "192.168.216.3"},
                                                },
                                                {
                                                    {"eth7", "192.168.217.3"},
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

    char file_name_t[] = "./st_hcom_creatgroup.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table_group << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    const u32 groupRanksNum = 1;
    char* strGroup1 = "group1";
    char* strGroup2 = "";
    char* strGroupErr = "group_err";
    u32 rankNum = 1;
    u32 worldRank;
    u32 groupRank;
    //std::vector<u32> groupRanks;
    u32 groupRanks[1] = {0};
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    rtStream_t stream1;
    s32 rank = 0;
    s32 errors=0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcom_creatgroup.json";
    char* pod_name = "0";
    set_board_id(0x3011);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomInitByFile(rank_table_file, pod_name);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    //groupRanks.push_back(0);
    //groupRanks.push_back(4);

    HCCL_INFO("this is hcom_group");
    ret = HcomCreateGroup(strGroup1, groupRanksNum,(u32*)groupRanks);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomDestroyGroup(strGroup1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HCCL_INFO("this is hcom_group end");

    rt_ret = aclrtDestroyStream(stream);
    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    set_board_id(0);
    remove(file_name_t);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    EXPECT_EQ(errors, 0);
}
#endif
TEST_F(HcomTest, st_hcom_init_by_string)
{
    GlobalMockObject::verify();
    s32 portNum = -1;
    MOCKER(hrtGetHccsPortNum)
        .stubs()
        .with(any(), outBound(portNum))
        .will(returnValue(HCCL_SUCCESS));
    nlohmann::json rank_table =
    {
	    {"collective_id", "192.168.3.3-9527-0001"},
        {"master_ip", "192.168.0.100"},
        {"master_port", "18000"},
        {"status", "completed"},
	    {"version","1.1"},
        {"node_list", {
            {
                {"node_addr", "192.168.0.101"},
                {"ranks", {
                    {
                        {"rank_id", "0"},
                        {"device_id", "0"}
                    }
                }}
            },
            {
                {"node_addr", "192.168.1.101"},
                {"ranks", {
                    {
                        {"rank_id", "1"},
                        {"device_id", "0"}
                    }
                }}
            },
            {
                {"node_addr", "192.168.2.101"},
                {"ranks", {
                    {
                        {"rank_id", "2"},
                        {"device_id", "0"}
                    }
                }}
            },
            {
                {"node_addr", "192.168.3.101"},
                {"ranks", {
                    {
                        {"rank_id", "3"},
                        {"device_id", "0"}
                    }
                }}
            }
        }
        }
    };

    MOCKER(Is310PDevice)
    .stubs()
    .will(returnValue(true));

    std::string rank_table_string = rank_table.dump();
    HcclResult ret;

    ret = HcomInitByString(rank_table_string.c_str(), "0");
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomInitByString(rank_table_string.c_str(), "1");
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string rank_table_string_invalid(40*1024*1024+1,'a');
    ret = HcomInitByString(rank_table_string_invalid.c_str(), "2");
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcomTest, st_HcomGetWorkspaceSubStreamNum)
{
    const char *group = "testgroup";
    u64 streamNum = 0;
    HcomInfo &hcomInfo = HcomGetCtxHomInfo();
    HcclGroupParams groupParamsTem;
    HcclResult ret;
    ret = HcomGetWorkspaceSubStreamNum(group, streamNum);
    HcomDestroy();
    GlobalMockObject::verify();
}

TEST_F(HcomTest, st_hcom_backlog_group)
{
    const u32 groupRanksNum = 4;
    char* strGroup = "group1";
    u32 groupRanks[4] = {0,1,2,3};
    int ret = HCCL_SUCCESS;
    ret = HcomCreateGroup(strGroup, groupRanksNum,(u32*)groupRanks);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomDestroyGroup(strGroup);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomCreateGroup(strGroup, groupRanksNum,(u32*)groupRanks);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // GROUP 已存在
    ret = HcomCreateGroup(strGroup, groupRanksNum,(u32*)groupRanks);
    EXPECT_EQ(ret, HCCL_E_PARA);

    nlohmann::json rank_table = rank_table_1server_8rank;
    char file_name_t[] = "./st_hcom_test_rank_table_1server_8rank.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open()) {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    } else {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();
    s32 deviceId = 0;
    char *identify = "0";
    s32 rankSize = 1;
    s32 rank = atoi(identify);
    u32 devLogicId = 0;
    ret = hrtSetDevice(devLogicId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char *rankTableFile = "./st_hcom_test_rank_table_1server_8rank.json";
    ret = HcomInitByFile(rankTableFile, identify);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // GROUP 已存在
    ret = HcomCreateGroup(strGroup, groupRanksNum,(u32*)groupRanks);
    EXPECT_EQ(ret, HCCL_E_PARA);

    ret = HcomDestroyGroup(strGroup);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
}

TEST_F(HcomTest, st_HcclCommGraphAllGather)
{
    hrtSetDevice(0);
    s8* sendbuf = (s8*)sal_malloc(10 * sizeof(s8));
    sal_memset(sendbuf, 10 * sizeof(s8), 0, 10 * sizeof(s8));
    s8* recv = (s8*)sal_malloc(16 * 10 * sizeof(s8));
    sal_memset(recv, 16 * 10 * sizeof(s8), 0, 16 * 10 * sizeof(s8));

    rtStream_t stream;

    rtError_t rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    hccl::hcclComm *comm = new hccl::hcclComm(1, 1, "123");
    s64 opBaseHcom = (s64)comm;

    MOCKER_CPP(&hcclComm::GetRankTableCrc)
    .stubs()
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetRankSize)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetGroupRank)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetAlgType)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&hcclComm::AllGather)
    .expects(atMost(1))
    .will(returnValue(0));

    int ret = HCCL_SUCCESS;
    ret = HcclCommGraphAllGather("tag", sendbuf, recv, 10, HCCL_DATA_TYPE_INT8, opBaseHcom, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    aclrtSynchronizeStream(stream);
    rt_ret = aclrtDestroyStream(stream);

    sal_free(sendbuf);
    sal_free(recv);
    delete comm;
}

TEST_F(HcomTest, st_HcclCommGraphAllReduce)
{
    HcclResult ret = hrtSetDevice(0);

    rtStream_t stream;
    rtError_t rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    s8* sendbuf = (s8*)sal_malloc(10 * sizeof(s8));
    sal_memset(sendbuf, 10 * sizeof(s8), 0, 10 * sizeof(s8));
    s8* recv = (s8*)sal_malloc(10 * sizeof(s8));
    sal_memset(recv, 10 * sizeof(s8), 0, 10 * sizeof(s8));

    hccl::hcclComm *comm = new hccl::hcclComm(1, 1, "123");
    s64 opBaseHcom = (s64)comm;

    MOCKER_CPP(&hcclComm::AllReduce)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetRankTableCrc)
    .stubs()
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetAlgType)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&hcclComm::GetRankSize)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetGroupRank)
    .expects(atMost(1))
    .will(returnValue(0));

    ret = HcclCommGraphAllReduce("tag", sendbuf, recv, 10, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, opBaseHcom, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    aclrtSynchronizeStream(stream);
    rt_ret = aclrtDestroyStream(stream);

    sal_free(sendbuf);
    sal_free(recv);
    delete comm;
}

TEST_F(HcomTest, st_HcclCommGraphReduce)
{
    HcclResult ret = hrtSetDevice(0);

    s8* sendbuf = (s8*)sal_malloc(10 * sizeof(s8));
    sal_memset(sendbuf, 10 * sizeof(s8), 0, 10 * sizeof(s8));
    s8* recv = (s8*)sal_malloc(10 * sizeof(s8));
    sal_memset(recv, 10 * sizeof(s8), 0, 10 * sizeof(s8));

    rtStream_t stream;

    rtError_t rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    hccl::hcclComm *comm = new hccl::hcclComm(1, 1, "123");
    s64 opBaseHcom = (s64)comm;

    MOCKER_CPP(&hcclComm::Reduce)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetRankTableCrc)
    .stubs()
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetRankSize)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetGroupRank)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER(HcomCheckUserRank)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetAlgType)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    ret = HcclCommGraphReduce("tag", sendbuf, recv, 10, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, 0, opBaseHcom, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    aclrtSynchronizeStream(stream);

    rt_ret = aclrtDestroyStream(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    sal_free(sendbuf);
    sal_free(recv);
    delete comm;
}

TEST_F(HcomTest, st_HcclCommGraphBroadcast)
{
    HcclResult ret = hrtSetDevice(0);

    s8* sendbuf = (s8*)sal_malloc(10 * sizeof(s8));
    sal_memset(sendbuf, 10 * sizeof(s8), 0, 10 * sizeof(s8));

    rtStream_t stream;

    rtError_t rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    hccl::hcclComm *comm = new hccl::hcclComm(1, 1, "123");
    s64 opBaseHcom = (s64)comm;

    MOCKER_CPP(&hcclComm::Broadcast)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetRankTableCrc)
    .stubs()
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetRankSize)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetGroupRank)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER(HcomCheckUserRank)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetAlgType)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&hcclComm::GetBlockDim)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    ret = HcclCommGraphBroadcast("tag", sendbuf, 10, HCCL_DATA_TYPE_INT8, 0, opBaseHcom, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    aclrtSynchronizeStream(stream);

    rt_ret = aclrtDestroyStream(stream);

    sal_free(sendbuf);
    delete comm;
}

TEST_F(HcomTest, st_HcclCommGraphReduceScatter)
{
    HcclResult ret = hrtSetDevice(0);

    s8* sendbuf = (s8*)sal_malloc(16 * 10 * sizeof(s8));
    sal_memset(sendbuf, 16 * 10 * sizeof(s8), 0, 16 * 10 * sizeof(s8));
    s8* recv = (s8*)sal_malloc(10 * sizeof(s8));
    sal_memset(recv, 10 * sizeof(s8), 0, 10 * sizeof(s8));

    rtStream_t stream;

    rtError_t rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    hccl::hcclComm *comm = new hccl::hcclComm(1, 1, "123");
    s64 opBaseHcom = (s64)comm;

    MOCKER_CPP(&hcclComm::GetRankTableCrc)
    .stubs()
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetAlgType)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&hcclComm::ReduceScatter)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER(HcomCheckUserRank)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetRankSize)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetGroupRank)
    .expects(atMost(1))
    .will(returnValue(0));

    ret = HcclCommGraphReduceScatter("tag", sendbuf, recv, 10, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, opBaseHcom, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    aclrtSynchronizeStream(stream);

    rt_ret = aclrtDestroyStream(stream);


    sal_free(sendbuf);
    sal_free(recv);
    delete comm;
}

TEST_F(HcomTest, st_HcclCommGraphSendRecv)
{

    HcclResult ret = hrtSetDevice(0);

    rtStream_t stream;
    rtError_t rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    rtStream_t stream2;
    rt_ret = aclrtCreateStream(&stream2);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    s8* sendbuf = (s8*)sal_malloc(10 * sizeof(s8));
    sal_memset(sendbuf, 10 * sizeof(s8), 0, 10 * sizeof(s8));
    s8* recv = (s8*)sal_malloc(10 * sizeof(s8));
    sal_memset(recv, 10 * sizeof(s8), 0, 10 * sizeof(s8));

    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    hccl::hcclComm *comm = new hccl::hcclComm(1, 1, "123");
    s64 opBaseHcom = (s64)comm;

    MOCKER_CPP(&hcclComm::GetRankTableCrc)
    .stubs()
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetAlgType)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&hcclComm::send)
    .stubs()
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetRankSize)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetUserRank)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER(HcomCheckUserRank)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER(HcclCommGraphGetRankId)
    .expects(atMost(1))
    .will(returnValue(0));

    ret = HcclCommGraphSend("tag", sendbuf, 10, HCCL_DATA_TYPE_INT8, 8,0, opBaseHcom, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    MOCKER_CPP(&hcclComm::receive)
    .stubs()
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetRankTableCrc)
    .stubs()
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetRankSize)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetUserRank)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER(HcomCheckUserRank)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER(HcclCommGraphGetRankId)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetAlgType)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    ret = HcclCommGraphReceive("tag", recv, 10, HCCL_DATA_TYPE_INT8, 8,0, opBaseHcom, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    aclrtSynchronizeStream(stream);

    rt_ret = aclrtDestroyStream(stream);
    rt_ret = aclrtDestroyStream(stream2);

    sal_free(sendbuf);
    sal_free(recv);
    delete comm;
}

TEST_F(HcomTest, st_HcclCommGraphAlltoAllV)
{
    s32 deviceId = 0;
    char *identify = "0";
    s32 rankSize = 1;
    s32 rank = atoi(identify);
    u64 count = 2;
    HCCL_INFO("alltoall_int32 : identify[%d], count[%llu]", rank, count);
    u32 devLogicId = 0xFFFF;
    HcclResult ret = hrtGetDeviceIndexByPhyId(deviceId, devLogicId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hrtSetDevice(devLogicId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    const int COUNT_PER_RANK = count;
    u64 memSize = COUNT_PER_RANK * rankSize * sizeof(s32);
    HostMem hostSendMem = HostMem::alloc(memSize);
    memset_s(hostSendMem.ptr(), memSize, 0, COUNT_PER_RANK * rankSize);
    for (u32 i = 0; i < COUNT_PER_RANK * rankSize; i++) {
        *((s32 *)hostSendMem.ptr() + i) = rank + 1;
    }

    // 构造入参
    vector<u64> sendCounts(rankSize, COUNT_PER_RANK);
    vector<u64> recvCounts(rankSize, COUNT_PER_RANK);
    vector<u64> sdispls(rankSize, 0);
    vector<u64> rdispls(rankSize, 0);
    for (int i = 0; i < rankSize; i++) {
        sdispls[i] = COUNT_PER_RANK * i;
        rdispls[i] = COUNT_PER_RANK * i;
        HCCL_INFO("num[%d] displs[%d]", i, COUNT_PER_RANK * i);
    }

    DeviceMem sendMem = DeviceMem::alloc(memSize);
    ret = hrtMemSyncCopy(sendMem.ptr(), memSize, hostSendMem.ptr(), memSize, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem recvMem = DeviceMem::alloc(memSize);

    hccl::Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    hccl::hcclComm *comm = new hccl::hcclComm(1, 1, "123");
    s64 opBaseHcom = (s64)comm;

    MOCKER_CPP(&hcclComm::AlltoAllV)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetRankTableCrc)
    .stubs()
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetRankSize)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetUserRank)
    .expects(atMost(1))
    .will(returnValue(0));

    MOCKER_CPP(&hcclComm::GetAlgType)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&hcclComm::GetBlockDim)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    ret = HcclCommGraphAlltoAllV(sendMem.ptr(), sendCounts.data(), sdispls.data(), HCCL_DATA_TYPE_INT32, recvMem.ptr(),
        recvCounts.data(), rdispls.data(), HCCL_DATA_TYPE_INT32, opBaseHcom, stream.ptr(), "hcom_alltoallv");
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcclStreamSynchronize(stream.ptr());

    EXPECT_EQ(ret, HCCL_SUCCESS);
    (void)aclrtResetDevice(0);
    delete comm;
}

TEST_F(HcomTest, st_hcom_get_dev_phy_id)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_hcom_get_dev_phy_id.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();
    s32 deviceId = 0;
    char *identify = "0";
    s32 rankSize = 1;
    s32 rank = atoi(identify);
    u64 count = 2;
    HCCL_INFO("alltoall_int32 : identify[%d], count[%llu]", rank, count);
    u32 devLogicId = 0xFFFF;
    HcclResult ret = hrtGetDeviceIndexByPhyId(deviceId, devLogicId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hrtSetDevice(devLogicId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char *rankTableFile = "./st_hcom_get_dev_phy_id.json";
    ret = HcomInitByFile(rankTableFile, identify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    const char *group = HCCL_WORLD_GROUP;
    s32 devId = 0;
    ret = HcomGetDevId(group, &devId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HCCL_INFO("HcomDestory start");

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
    (void)aclrtResetDevice(0);
}

TEST_F(HcomTest, st_hcom_HcclCommGraphGetDevId)
{
    hccl::hcclComm *comm = new hccl::hcclComm(1, 1, "123");
    comm->communicator_.reset(new (std::nothrow) HcclCommunicator());
    s64 opBaseHcom = (s64)comm;
    s32 devId = 0;
    HcclResult ret = HcclCommGraphGetDevId(opBaseHcom, &devId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete comm;
}

TEST_F(HcomTest, st_hcom_get_dev_phy_id_group)
{
    const char *group = "test_group";
    s32 devId = 0;

    MOCKER(HcomGetRankId)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(HcomGetWorldRankFromGroupRank)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HcomGetDevId(group, &devId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(HcomTest, st_HcclCommGraphUnloadTask)
{
    hccl::hcclComm *comm = new hccl::hcclComm(1, 1, "123");
    s64 opBaseHcom = (s64)comm;
    std::string tag = "test_tag";
    MOCKER_CPP(&hcclComm::ClearOpResource)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    HcclResult ret = HcclCommGraphUnloadTask(opBaseHcom, tag.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
    delete comm;
}

TEST_F(HcomTest, st_HcclCommGlobalWorkSpace)
{
    hccl::hcclComm *comm = new hccl::hcclComm(1, 1, "123");
    s64 opBaseHcom = (s64)comm;
    std::vector<void *> globalWorkSpaceAddr;
    MOCKER_CPP(&hcclComm::SetGlobalWorkSpace)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    HcclResult ret = HcclCommSetGlobalWorkSpace(opBaseHcom, globalWorkSpaceAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
    delete comm;
}

#if 1
TEST_F(HcomTest, st_hcom_get_new_ranktable_info_serverId)
{

    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"version", "1.0"},
        {"server_count", "1"},
        {
            "server_list",
            {
                {
                    {"server_id", "167772170"},
                    {"host_nic_ip", "192.168.0.12:0,192.168.1.12:199"},
                    {
                        "device",
                        {
                            {   {"rank_id", "2"},
                                {"device_id", "0"},
                                {"device_ip", "192.168.0.12,192.168.0.13"}

                            },
                            {   {"rank_id", "1"},
                                {"device_id", "1"},
                                {"device_ip", "192.168.1.12,192.168.1.13"}

                            },
                            {   {"rank_id", "0"},
                                {"device_id", "2"},
                                {"device_ip", "192.168.2.12,192.168.2.13"}

                            },
                        }
                    },
                }
            }
        }
    };

    char file_name_t[] = "./st_hcom_get_new_ranktable_info_serverId.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();
    int ret = HCCL_SUCCESS;


    ret = hrtSetDevice(2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    string rank_table_file("./st_hcom_get_new_ranktable_info_serverId.json");
    string rank_ID("0");
    std::string rankTableM;
    std::string realFilePath;

    HcomInfo hcom_info;
    ret = HcomLoadRanktableFile(rank_table_file.c_str(), rankTableM, realFilePath);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, rank_ID, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hrtSetDevice(0);
    remove(file_name_t);
}
#endif

#if 1
TEST_F(HcomTest, st_hcom_get_new_ranktable_info_empty_serverId)
{

    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"version", "1.0"},
        {"server_count", "1"},
        {
            "server_list",
            {
                {
                    {"server_id", ""},
                    {"host_nic_ip", "192.168.0.12:0,192.168.1.12:199"},
                    {
                        "device",
                        {
                            {   {"rank_id", "2"},
                                {"device_id", "0"},
                                {"device_ip", "192.168.0.12,192.168.0.13"}

                            },
                            {   {"rank_id", "1"},
                                {"device_id", "1"},
                                {"device_ip", "192.168.1.12,192.168.1.13"}

                            },
                            {   {"rank_id", "0"},
                                {"device_id", "2"},
                                {"device_ip", "192.168.2.12,192.168.2.13"}

                            },
                        }
                    },
                }
            }
        }
    };

    char file_name_t[] = "./st_hcom_get_new_ranktable_info_empty_serverId.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();
    int ret = HCCL_SUCCESS;


    ret = hrtSetDevice(2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    string rank_table_file("./st_hcom_get_new_ranktable_info_empty_serverId.json");
    string rank_ID("0");
    std::string rankTableM;
    std::string realFilePath;

    HcomInfo hcom_info;
    ret = HcomLoadRanktableFile(rank_table_file.c_str(), rankTableM, realFilePath);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, rank_ID, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_E_PARA);
    ret = hrtSetDevice(0);
    remove(file_name_t);
}
#endif

#if 1
TEST_F(HcomTest, st_hcom_get_new_ranktable_info_exception_serverId)
{

    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"version", "1.0"},
        {"server_count", "1"},
        {
            "server_list",
            {
                {
                    {"server_id", "4294967296"},
                    {"host_nic_ip", "192.168.0.12:0,192.168.1.12:199"},
                    {
                        "device",
                        {
                            {   {"rank_id", "2"},
                                {"device_id", "0"},
                                {"device_ip", "192.168.0.12,192.168.0.13"}

                            },
                            {   {"rank_id", "1"},
                                {"device_id", "1"},
                                {"device_ip", "192.168.1.12,192.168.1.13"}

                            },
                            {   {"rank_id", "0"},
                                {"device_id", "2"},
                                {"device_ip", "192.168.2.12,192.168.2.13"}

                            },
                        }
                    },
                }
            }
        }
    };

    char file_name_t[] = "./st_hcom_get_new_ranktable_info_exception_serverId.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();
    int ret = HCCL_SUCCESS;


    ret = hrtSetDevice(2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    string rank_table_file("./st_hcom_get_new_ranktable_info_exception_serverId.json");
    string rank_ID("0");
    std::string rankTableM;
    std::string realFilePath;

    HcomInfo hcom_info;
    ret = HcomLoadRanktableFile(rank_table_file.c_str(), rankTableM, realFilePath);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, rank_ID, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hrtSetDevice(0);
    remove(file_name_t);
}
#endif

#if 1
TEST_F(HcomTest, st_rank_inner_server_4p_select_full)
{
    int ret = 0;
    TopoInfoParse test;
    test.deviceNum_ = 4;
    test.deviceNumPerServer_ = 4;
    test.serverNum_ = 1;
    test.serverId_ = "1";
    test.deviceType_ = DevType::DEV_TYPE_910;
    for (int i = 0; i < 8; ++i)
    for (int j = i + 1; j < 8; ++j)
    for (int k = j + 1; k < 8; ++k)
    for (int m = k + 1; m < 8; ++m)
    {
        cout << i << " " << j << " " << k << " " << m << endl;
        RankInfo tmp;
        tmp.serverId = "1";
        tmp.devicePhyId = i;
        test.rankList_.push_back(tmp);
        tmp.devicePhyId = j;
        test.rankList_.push_back(tmp);
        tmp.devicePhyId = k;
        test.rankList_.push_back(tmp);
        tmp.devicePhyId = m;
        test.rankList_.push_back(tmp);
        if (test.CheckServerInnerRankInfo() == 0) ret++;
        test.rankList_.clear();
    }
    EXPECT_EQ(ret, 8);
}
#endif
TEST_F(HcomTest, st_HcclCommGraphGetRankId)
{

    HcclResult ret = hrtSetDevice(0);

    rtStream_t stream;
    rtError_t rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    hccl::hcclComm *comm = new hccl::hcclComm(1, 1, "123");
    s64 opBaseHcom = (s64)comm;

    ret = HcclCommGraphGetRankId(opBaseHcom, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);

    aclrtSynchronizeStream(stream);
    rt_ret = aclrtDestroyStream(stream);
    delete comm;
}


HcclResult Stub_GetAlgType_DEFAULT(hcclComm* comm, AlgType &algType)
{
    HCCL_INFO("==TMP== point1");
    algType = AlgType();
    return HCCL_SUCCESS;
}

HcclResult Stub_GetAlgType_Reserved(hcclComm* comm, AlgType &algType)
{
    algType = AlgType::Reserved();
    return HCCL_SUCCESS;
}

HcclResult Stub_GetAlgType_ALG_DOUBLE_RING_PLUS_HD(hcclComm* comm, AlgType &algType)
{
    algType.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_NP_DOUBLE_RING;
    algType.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_HD;
    return HCCL_SUCCESS;
}

HcclResult Stub_GetAlgType_ALG_DOUBLE_RING_PLUS_RING(hcclComm* comm, AlgType &algType)
{
    algType.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_NP_DOUBLE_RING;
    algType.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    return HCCL_SUCCESS;
}

HcclResult Stub_GetAlgType_mesh_plus_ring(hcclComm* comm, AlgType &algType)
{
    algType.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_4P_MESH;
    algType.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    return HCCL_SUCCESS;
}

HcclResult Stub_GetAlgType_pipeline(HcclCommunicator* comm, AlgType &algType, HcclCMDType opType)
{
    algType.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_NP_MESH;
    algType.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_PIPELINE;
    return HCCL_SUCCESS;
}

HcclResult Stub_GetAlgType_ALG_ALLGATHER_REDUCESCATTER_GRAPH_PIPELINE(
    HcclCommunicator *comm, AlgType &algType, HcclCMDType opType)
{
    algType.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_NP_MESH;
    
    return HCCL_SUCCESS;
}

TEST_F(HcomTest, st_HcomGetAlgorithm)
{
    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"chip_info", "910"},
        {"group_count", "1"},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"instance_count", "1"},
                    {"device_count", "1"},
                    {
                        "instance_list",
                        {
                            {   {"pod_name", "tf-bae43"},
                                {"server_id", "10.0.0.10"},
                                {
                                    "devices", {{{"device_id", "0"}, {"device_ip", "192.168.0.12"}}}
                                }
                            },
                        }
                    },
                }
            }
        },
    };

    char file_name_t[] = "./st_HcomGetAlgorithm.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_HcomGetAlgorithm.json";
    char* pod_name = "tf-bae43";

    ret = HcomInitByFile(rank_table_file, pod_name);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u32 level = 1;
    std::string algo;
    char *algoch = const_cast<char *>(algo.c_str());
    MOCKER_CPP(&hcclComm::GetAlgType)
              .stubs()
              .will(invoke(Stub_GetAlgType_DEFAULT));
    ret = HcomGetAlgorithm(level, &algoch);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    level = 0;
    MOCKER_CPP(&hcclComm::GetAlgType)
                .stubs()
              .will(invoke(Stub_GetAlgType_mesh_plus_ring));
    ret = HcomGetAlgorithm(level, &algoch);
    EXPECT_EQ(ret, HCCL_SUCCESS);
        GlobalMockObject::verify();

    level = 1;
    MOCKER_CPP(&hcclComm::GetAlgType)
                .stubs()
              .will(invoke(Stub_GetAlgType_mesh_plus_ring));
    ret = HcomGetAlgorithm(level, &algoch);
    EXPECT_EQ(ret, HCCL_SUCCESS);
        GlobalMockObject::verify();

    level = 0;
    MOCKER_CPP(&hcclComm::GetAlgType)
                .stubs()
              .will(invoke(Stub_GetAlgType_Reserved));
    ret = HcomGetAlgorithm(level, &algoch);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
        GlobalMockObject::verify();

    level = 1;
    MOCKER_CPP(&hcclComm::GetAlgType)
                .stubs()
              .will(invoke(Stub_GetAlgType_Reserved));
    ret = HcomGetAlgorithm(level, &algoch);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
        GlobalMockObject::verify();

    level = 1;
    MOCKER_CPP(&hcclComm::GetAlgType)
                .stubs()
              .will(invoke(Stub_GetAlgType_ALG_DOUBLE_RING_PLUS_HD));
    ret = HcomGetAlgorithm(level, &algoch);
    EXPECT_EQ(ret, HCCL_SUCCESS);
        GlobalMockObject::verify();

    level = 1;
    MOCKER_CPP(&hcclComm::GetAlgType)
                .stubs()
              .will(invoke(Stub_GetAlgType_ALG_DOUBLE_RING_PLUS_RING));
    ret = HcomGetAlgorithm(level, &algoch);
    EXPECT_EQ(ret, HCCL_SUCCESS);
        GlobalMockObject::verify();

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);

}

TEST_F(HcomTest, st_hcom_test_config)
{
    HcomInfo hcom_info;
    RankInfo_t myRank;
    myRank.deviceInfo.devicePhyId = 0;
    hcom_info.rankTable.rankList.push_back(myRank);
    int ret = CheckDeviceNumValid(hcom_info.rankTable.rankList, 2, 1);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

#if 1
TEST_F(HcomTest, st_check_and_assign_nic_info)
{
    int ret = 0;
    TopoInfoParse test;
    std::vector<u32> nicIdx;

    test.serverNum_ = 2;
    test.deviceNum_ = 16;
    test.deviceNumPerServer_ = 8;
    nicIdx.push_back(0);
    nicIdx.push_back(1);
    for (u32 i = 0; i < 16; i++) {
        RankInfo tmp;
        tmp.devicePhyId = i % 8;
        test.rankList_.push_back(tmp);
    }
    for (u32 i = 0; i < 8; i++) {
        test.rankList_[i].serverId = "1";
        test.rankList_[i].nicIp.push_back(HcclIpAddress(0));
    }
    for (u32 i = 8; i < 16; i++) {
        test.rankList_[i].serverId = "2";
        test.rankList_[i].nicIp.push_back(HcclIpAddress(0));
    }

    test.rankList_[0].nicIp[0] = HcclIpAddress(3232261989);
    ret = test.CheckAndAssignNicInfo(nicIdx);
    EXPECT_EQ(ret, HCCL_E_PARA);

    test.rankList_[1].nicIp[0] = HcclIpAddress(3232261989);
    test.rankList_[8].nicIp[0] = HcclIpAddress(3232261989);
    test.rankList_[9].nicIp[0] = HcclIpAddress(3232261989);

    ret = test.CheckAndAssignNicInfo(nicIdx);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    test.rankList_.clear();
    nicIdx.clear();
}
#endif

void* hcom_get_cur_hcom_ctx(void* parg)
{
    const char *group = "test_group";
    s32 devId = 0;
    HcclResult ret = HCCL_SUCCESS;

    ret = hrtSetDevice(MAX_MODULE_DEVICE_NUM);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER(HcomGetRankId)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(HcomGetWorldRankFromGroupRank)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    ret = HcomGetDevId(group, &devId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    return (NULL);
}

void* hcom_get_cur_hcom_ctx_second(void* parg)
{
    const char *group = "test_group";
    s32 devId = 0;
    HcclResult ret = HCCL_SUCCESS;

    ret = hrtSetDevice(15);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER(HcomGetRankId)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(HcomGetWorldRankFromGroupRank)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    ret = HcomGetDevId(group, &devId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    return (NULL);
}

TEST_F(HcomTest, st_HcomGetCurHcomCtx)
{
    sal_thread_t tid;
    tid = sal_thread_create("thread", hcom_get_cur_hcom_ctx, (void*)NULL);
    EXPECT_NE(tid, (sal_thread_t )NULL);

    while (sal_thread_is_running(tid))
    {
        SaluSleep(SAL_MILLISECOND_USEC * 10);
    }

    tid = sal_thread_create("thread", hcom_get_cur_hcom_ctx_second, (void*)NULL);
    EXPECT_NE(tid, (sal_thread_t )NULL);

    while (sal_thread_is_running(tid))
    {
        SaluSleep(SAL_MILLISECOND_USEC * 10);
    }

    GlobalMockObject::verify();
}

#if 1
nlohmann::json super_pod_ranktable_1_2_4 =
{
    {"status", "completed"},
    {"version", "1.2"},
    {"server_list",
        {
            {
                {"server_id", "10.155.111.140"},
                {"device",
                    {
                        {{"rank_id", "0"},{"device_id", "0"},{"super_device_id", "0"},{"device_ip", "192.1.27.6"}},
                        {{"rank_id", "1"},{"device_id", "1"},{"super_device_id", "1"},{"device_ip", "192.2.27.6"}},
                        {{"rank_id", "2"},{"device_id", "2"},{"super_device_id", "2"},{"device_ip", "192.3.27.6"}},
                        {{"rank_id", "3"},{"device_id", "3"},{"super_device_id", "3"},{"device_ip", "192.4.27.6"}},
                    }
                },
            },
            {
                {"server_id", "10.155.111.141"},
                {"device",
                    {
                        {{"rank_id", "4"},{"device_id", "0"},{"super_device_id", "4"},{"device_ip", "192.1.27.7"}},
                        {{"rank_id", "5"},{"device_id", "1"},{"super_device_id", "5"},{"device_ip", "192.2.27.7"}},
                        {{"rank_id", "6"},{"device_id", "2"},{"super_device_id", "6"},{"device_ip", "192.3.27.7"}},
                        {{"rank_id", "7"},{"device_id", "3"},{"super_device_id", "7"},{"device_ip", "192.4.27.7"}},
                    }
                },
            }
        }
    },
    {"super_pod_list",
        {
            {
                {"super_pod_id", "0"},
                {"server_list",
                    {
                        {{"server_id", "10.155.111.140"}},
                        {{"server_id", "10.155.111.141"}},
                    }
                },
            }
        }
    }
};

TEST_F(HcomTest, st_hcom_get_hcom_info_podnameEmpty)
{
    nlohmann::json rank_table =
    {
       {"status", "completed"},
        {"group_count", "2"},
        {
            "group_list",
            {
                {
                    {"group_name", "group1"},
                    {"instance_count", "1"},
                    {"device_count", "8"},
                    {
                        "instance_list",
                        {
                            {   {"pod_name", ""},
                                {"server_id", "10.0.0.10"},
                                {
                                    "devices",
                                    {
                                        {   {"device_id", "0"},
                                            {"device_ip", "192.168.0.10"}
                                        },
                                        {   {"device_id", "1"},
                                            {"device_ip", "192.168.0.11"}
                                        },
                                        {   {"device_id", "2"},
                                            {"device_ip", "192.168.0.12"}
                                        },
                                        {   {"device_id", "3"},
                                            {"device_ip", "192.168.0.13"}
                                        },
                                         {   {"device_id", "4"},
                                            {"device_ip", "192.168.0.14"}
                                        },
                                        {   {"device_id", "5"},
                                            {"device_ip", "192.168.0.15"}
                                        },
                                        {   {"device_id", "6"},
                                            {"device_ip", "192.168.0.16"}
                                        },
                                        {   {"device_id", "7"},
                                            {"device_ip", "192.168.0.17"}
                                        }
                                    }
                                }
                            }
                        }
                    },
                },
                {
                    {"group_name", "1"},
                    {"instance_count", "1"},
                    {"device_count", "8"},
                    {
                        "instance_list",
                        {
                            {   {"pod_name", "tf-1"},
                                {"server_id", "10.0.0.11"},
                                {
                                    "devices",
                                    {
                                        {   {"device_id", "0"},
                                            {"device_ip", "192.168.0.21"}
                                        },
                                        {   {"device_id", "1"},
                                            {"device_ip", "192.168.0.22"}
                                        },
                                        {   {"device_id", "2"},
                                            {"device_ip", "192.168.0.23"}
                                        },
                                        {   {"device_id", "3"},
                                            {"device_ip", "192.168.0.24"}
                                        },
                                         {   {"device_id", "4"},
                                            {"device_ip", "192.168.0.20"}
                                        },
                                        {   {"device_id", "5"},
                                            {"device_ip", "192.168.0.25"}
                                        },
                                        {   {"device_id", "6"},
                                            {"device_ip", "192.168.0.26"}
                                        },
                                        {   {"device_id", "7"},
                                            {"device_ip", "192.168.0.27"}
                                        }
                                    }
                                }
                            },
                        }
                    },
                }
            }
        },
    };

    char file_name[] = "./st_hcom_get_hcom_info_podnameEmpty.json";
    std::ofstream outfile(file_name, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name);
    }

    outfile.close();

    std::string identify = "0";
    s32 ret = HCCL_SUCCESS;
    HcomInfo hcom_info;
    std::string ranktable_file(file_name);
    std::string rankTableM;
    std::string realFilePath;

    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomLoadRanktableFile(ranktable_file.c_str(), rankTableM, realFilePath);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, identify, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_E_PARA);

    ret = hrtResetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    remove(file_name);

}

TEST_F(HcomTest, st_hcom_91093_InitByFile)
{
    setenv("HCCL_INTER_HCCS_DISABLE", "false", 1);

    char file_name_t[] = "./super_pod_ranktable_1_2_4.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << super_pod_ranktable_1_2_4 << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();
    int ret = HCCL_SUCCESS;

    DevType type91093 = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(type91093))
    .will(returnValue(HCCL_SUCCESS));
	MOCKER(hrtRaGetSingleSocketVnicIpInfo)
	.stubs()
	.with(any())
	.will(invoke(stub_hrtRaGetSingleSocketVnicIpInfo));
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 localrankid = 0;
    u32 localranksize = 0;
    char* rank_table_file = "./super_pod_ranktable_1_2_4.json";
    char* rank_ID = "0";

    MOCKER_CPP(&HcclCommunicatorAttrs::CheckSuperDeviceId, HcclResult(HcclCommunicatorAttrs::*)(const RankTable_t &rankTable))
	.stubs()
	.with(any())
	.will(returnValue(HCCL_SUCCESS));

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomGetLocalRankSize(NULL, &localranksize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomGetLocalRankId(NULL, &localrankid);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);

    EXPECT_EQ(localranksize, 4);
    EXPECT_EQ(localrankid, 0);
    ret = hrtResetDevice(0);
    unsetenv("HCCL_INTER_HCCS_DISABLE");
    GlobalMockObject::verify();
}

TEST_F(HcomTest, st_setAttachedStream)
{
    hcclComm h;
    h.communicator_ = std::make_unique<HcclCommunicator>();

    std::vector<void *> emptyStreams;
    s64 base = reinterpret_cast<s64>(&h);
    EXPECT_EQ(HcclCommSetAttachedStream(base, emptyStreams), HCCL_SUCCESS);

    MOCKER(&HcomGetCommByGroup).stubs().will(returnValue(HCCL_E_INTERNAL));
    HcclResult ret = HcomSetAttachedStream(nullptr, emptyStreams.data(), emptyStreams.size());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomTest, st_Destroy_backlogged_group)
{
    std::vector<u32> ranklist;
    HcomInfo &hcomInfo = HcomGetCtxHomInfo();
    hcomInfo.isHcomInit = true;

    HcclResult ret;
    std::string group = "testgroup";
    ret = HcomDestroyBackloggedGroup(group);
    EXPECT_EQ(ret, HCCL_E_PARA);
    hcomInfo.isHcomInit = false;
    ret = HcomDestroyBackloggedGroup(group);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}
#endif



TEST_F(HcomTest, hcclComm_SetRanksPort)
{
    HcclCommunicator hcclCommunicator;
    hcclCommunicator.commPortConfig_.devPortSwitchOn = true;
    HcclIpAddress localIp{"10.10.10.10"};
    hcclCommunicator.userRankSize_ = 1;
    std::vector<RankInfo_t> rankLists= {};
    RankInfo_t node;
    node.rankId = 0;
    node.deviceInfo.port = 50000;
    node.deviceInfo.vnicPort = 50001;
    rankLists.push_back(node);
    HcclResult ret ;
    ret = hcclCommunicator.SetRanksPort(rankLists);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcomTest, hcclCommAttr_SetRanksPort)
{
    HcclCommunicatorAttrs hcclCommunicator;
    MOCKER(GetExternalInputNpuPortSwitch).stubs().will(returnValue(true));
    HcclIpAddress localIp{"10.10.10.10"};
    hcclCommunicator.userRankSize_ = 1;
    std::vector<RankInfo_t> rankLists= {};
    RankInfo_t node;
    node.rankId = 0;
    node.deviceInfo.port = 50000;
    node.deviceInfo.vnicPort = 50001;
    rankLists.push_back(node);
    HcclResult ret ;
    ret = hcclCommunicator.SetRanksPort(rankLists);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcomTest, hcclComm_ReleasePreemptSocket)
{
    MOCKER_CPP(&PreemptPortManager::Release).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclNetCloseDev).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclNetDeInit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtGetPairDevicePhyId).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtGetDeviceIndexByPhyId).stubs().will(returnValue(HCCL_SUCCESS));

    HcclCommunicator hcclCommunicator;
    HcclIpAddress remoteIp1{"10.10.10.10"};
    std::shared_ptr<HcclSocket> listenSocket1(new (std::nothrow)HcclSocket("my tag1", nullptr, remoteIp1, 0,
        HcclSocketRole::SOCKET_ROLE_SERVER));
    HcclNetDevCtx  ctx1 ;
    hcclCommunicator.commPortConfig_.devNicListen = std::make_pair(listenSocket1, ctx1);

    HcclIpAddress remoteIp2{"10.10.10.11"};
    std::shared_ptr<HcclSocket> listenSocket2(new (std::nothrow)HcclSocket("my tag2", nullptr, remoteIp2, 0,
        HcclSocketRole::SOCKET_ROLE_SERVER));
    HcclNetDevCtx  ctx2;
    hcclCommunicator.commPortConfig_.devVnicListen = std::make_pair(listenSocket2, ctx2);

    HcclIpAddress remoteIp3{"10.10.10.12"};
    std::shared_ptr<HcclSocket> listenSocket3(new (std::nothrow)HcclSocket("my tag3", nullptr, remoteIp3, 0,
        HcclSocketRole::SOCKET_ROLE_SERVER));
    HcclNetDevCtx  ctx3 ;
    hcclCommunicator.commPortConfig_.backupDevNicListen = std::make_pair(listenSocket3, ctx3);

    hcclCommunicator.deviceBackUpLogicId_ = 1;

    HcclResult ret ;
    ret = hcclCommunicator.ReleasePreemptSocket();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomTest, hcclComm_InitRankInfoSubGroup_devPortSwitchOn_branch)
{
    MOCKER_CPP(&HcclCommunicatorAttrs::GetInlineReduceSwitchOn).stubs().with(any()).will(returnValue(true));
    MOCKER_CPP(&HcclCommunicatorAttrs::GetMeshAggregationRankSize).stubs().with(any()).will(returnValue(1));
    MOCKER_CPP(&HcclCommunicatorAttrs::GetUsedRdmaLevel0).stubs().with(any()).will(returnValue(false));
    MOCKER_CPP(&HcclCommunicator::SetWorldGroupInfo).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(SetRetryEnable).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(IsHostUseDevNic).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    std::vector<RankInfo> rankLists= {};
    RankInfo node;
    node.worldRank = 0;
    node.userRank = 0;
    rankLists.push_back(node);
    WorldGroupInfo groupCommonData;
    groupCommonData.devPortSwitchOn = true;

    HcclCommunicator hcclCommunicator;
    hcclCommunicator.commPortConfig_.devPortSwitchOn = true;
    hcclCommunicator.vnicRanksPort_.push_back(50000);
    HcclResult ret ;
    ret = hcclCommunicator.InitRankInfoSubGroup(rankLists, groupCommonData);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomTest, hcclComm_InitRaResource_devVnicSocket_branch)
{
    MOCKER(IsHostUseDevNic).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclNetInit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(Is310PDevice).stubs().with(any()).will(returnValue(false));
    MOCKER_CPP(&HcclCommunicator::IsEnableBackupLink).stubs().with(any()).will(returnValue(false));
    MOCKER_CPP(& HcclCommunicator::InitSocketManager).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicatorAttrs::IsEnableRoce).stubs().with(any()).will(returnValue(false));
    MOCKER_CPP(&HcclSocketManager::ServerInit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclSocketManager::ServerDeInit, HcclResult(HcclSocketManager::*)(const HcclNetDevCtx, u32)).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicatorAttrs::GenSupportRdmaLite).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicatorAttrs::GetSupportRdmaLite).stubs().with(any()).will(returnValue(false));
    MOCKER_CPP(&HcclCommunicator::ReleasePreemptSocket).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    HcclCommunicator hcclCommunicator;
    hcclCommunicator.userRankSize_ = 2;
    hcclCommunicator.devicePhyId_ = 0;
    hcclCommunicator.nicDeployment_ = NICDeployment::NIC_DEPLOYMENT_HOST; 
    hcclCommunicator.isHaveCpuRank_ = false;
    hcclCommunicator.socketManager_.reset(new (std::nothrow) HcclSocketManager(NICDeployment::NIC_DEPLOYMENT_HOST, 0, 0, 0));

    HcclIpAddress remoteIp{"10.10.10.11"};
    std::shared_ptr<HcclSocket> listenSocket(new (std::nothrow)HcclSocket("my tag2", nullptr, remoteIp, 0,
        HcclSocketRole::SOCKET_ROLE_SERVER));
    HcclIpAddress  localIp{"127.0.0.1"};
    listenSocket->localIp_ = localIp;
    listenSocket->localPort_ = 50000;

    HcclNetDevCtx  ctx;
    hcclCommunicator.commPortConfig_.devVnicListen = std::make_pair(listenSocket, ctx);

    MOCKER_CPP(&NetworkManager::StopVnic)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcclResult ret ;
    ret = hcclCommunicator.InitRaResource();
    EXPECT_EQ(ret, HCCL_E_PTR);
    
    hcclCommunicator.GetRanksPort();

    hcclCommunicator.raResourceInit_ = false;
    hcclCommunicator.nicInitialized_ = 0;
    GlobalMockObject::verify();
}

TEST_F(HcomTest, hcclComm_InitNic_IsEnableBackupLink_branch1)
{
    setenv("HCCL_INTRA_ROCE_ENABLE", "1", 1);
    MOCKER_CPP(&HcclCommunicator::IsEnableBackupLink).stubs().with(any()).will(returnValue(true));
    MOCKER_CPP(&HcclSocketManager::ServerInit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclSocketManager::ServerDeInit, HcclResult(HcclSocketManager::*)(const HcclNetDevCtx, u32)).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicatorAttrs::SetNeedInitNicFlag).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(Is310PDevice).stubs().with(any()).will(returnValue(false));
    MOCKER_CPP(&HcclCommunicator::ReleasePreemptSocket).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    
    HcclIpAddress remoteIp{"10.10.10.11"};
    std::shared_ptr<HcclSocket> listenSocket(new (std::nothrow)HcclSocket("my tag2", nullptr, remoteIp, 0,
        HcclSocketRole::SOCKET_ROLE_SERVER));
    HcclIpAddress  localIp{"127.0.0.1"};
    listenSocket->localIp_ = localIp;
    listenSocket->localPort_ = 50000;
    HcclNetDevCtx ctx1;
    HcclResult ret = HcclNetOpenDev(&ctx1, NicType::DEVICE_NIC_TYPE, 0, 0,
        HcclIpAddress("1.1.1.1"));
    
    HcclIpAddress remoteIp2{"10.10.10.12"};
    std::shared_ptr<HcclSocket> listenSocket2(new (std::nothrow)HcclSocket("my tag2", nullptr, remoteIp2, 0,
        HcclSocketRole::SOCKET_ROLE_SERVER));
    HcclIpAddress  localIp2{"127.0.0.1"};
    listenSocket2->localIp_ = localIp2;
    listenSocket2->localPort_ = 50001;
    HcclNetDevCtx  ctx2;

    HcclCommunicator hcclCommunicator;
    hcclCommunicator.nicDeployment_ = NICDeployment::NIC_DEPLOYMENT_RESERVED;
    hcclCommunicator.commPortConfig_.devNicListen = std::make_pair(listenSocket, ctx1);
    hcclCommunicator.commPortConfig_.backupDevNicListen = std::make_pair(listenSocket2, ctx2);

    ret = hcclCommunicator.InitNic();
    EXPECT_EQ(ret, HCCL_E_PARA);
    hcclCommunicator.nicInitialized_ = 0;
    hcclCommunicator.raResourceInit_ = false;
    unsetenv("HCCL_INTRA_ROCE_ENABLE");
    GlobalMockObject::verify();
}

TEST_F(HcomTest, hcclComm_InitRankInfoSubGroup_devicePortSwitchOn)
{
    MOCKER_CPP(&HcclCommunicatorAttrs::SethbRankInfo).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicatorAttrs::TransformRankList).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicatorAttrs::SetServerNum).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicatorAttrs::SetModuleInfo).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclCommunicatorAttrs::SetSuperPodInfo).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicatorAttrs::SetLocalRankInfoSubGroup).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicatorAttrs::SetInterModeInSuperPod).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclCommunicatorAttrs::UpdateNicList).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicatorAttrs::CheckLocalRankInfo).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicatorAttrs::CalAndSetMeshAggRankSize).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicatorAttrs::IsEnableRoce).stubs().with(any()).will(returnValue(false));
    MOCKER_CPP(&HcclCommunicatorAttrs::SetWorldGroupInfo).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    MOCKER(HcclCheckLogLevel).stubs().with(any()).will(returnValue(false));
    MOCKER(IsHostUseDevNic).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclCommunicatorAttrs::SetInnerServerAverageDevice,
        HcclResult (HcclCommunicatorAttrs::*)(const std::vector<RankInfo> &rankList))
        .stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicatorAttrs::InitTopoInfo,
        HcclResult (HcclCommunicatorAttrs::*)(const std::vector<RankInfo> &rankList))
        .stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    HcclCommunicatorAttrs hcomattr;
    
    std::vector<RankInfo> rankLists ;
    RankInfo node;
    node.worldRank = 0;
    node.userRank = 0;
    rankLists.push_back(node);
    WorldGroupInfo groupCommonData;
    groupCommonData.devPortSwitchOn = true;
 
    hcomattr.vnicRanksPort_.push_back(50000);
    HcclResult ret ;
    ret = hcomattr.InitRankInfoSubGroup(rankLists, groupCommonData);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(HcomTest, should_return_substream_num_when_910b_graph_allreduce_pipeline)
{
    HcclCommunicator comm;
    comm.deviceType_ = DevType::DEV_TYPE_910B;
    comm.userRankSize_ = 8;
    comm.moduleNum_ = 2;

    u64 streamNum = 0;
    u64 dataSize = 0;
    HcclCMDType opType = HcclCMDType::HCCL_CMD_ALLREDUCE;

    MOCKER_CPP_VIRTUAL(comm, &HcclCommunicator::GetAlgType)
            .stubs()
            .will(invoke(Stub_GetAlgType_pipeline));

    HcclResult result = comm.GetWorkspaceSubStreamNum(streamNum, dataSize, opType);

    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_EQ(streamNum, comm.userRankSize_ / comm.moduleNum_ - 1);
}

TEST_F(HcomTest, should_return_substream_num_when_910b_allgather_graph_pipeline)
{
    HcclCommunicator comm;
    comm.deviceType_ = DevType::DEV_TYPE_910B;
    comm.moduleNum_ = 2;
    comm.deviceNumPerAggregation_ = 8;
    comm.userRankSize_ = 16;

    u64 dataSize = HCCL_SMALL_COUNT_1_MB + 1;
    u64 streamNum = 0;

    HcclCMDType opType = HcclCMDType::HCCL_CMD_ALLGATHER;

    MOCKER_CPP_VIRTUAL(comm, &HcclCommunicator::GetAlgType)
        .stubs()
        .will(invoke(Stub_GetAlgType_ALG_ALLGATHER_REDUCESCATTER_GRAPH_PIPELINE));

    HcclResult result = comm.GetWorkspaceSubStreamNum(streamNum, dataSize, opType);

    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_EQ(streamNum, comm.userRankSize_ / comm.moduleNum_);
}

TEST_F(HcomTest, should_return_substream_num_when_910b_reducescatter_graph_pipeline)
{
    HcclCommunicator comm;
    comm.deviceType_ = DevType::DEV_TYPE_910B;
    comm.moduleNum_ = 2;
    comm.deviceNumPerAggregation_ = 4;
    comm.userRankSize_ = 8;

    u64 dataSize = HCCL_SMALL_COUNT_1_MB - 1;
    u64 streamNum = 0;

    HcclCMDType opType = HcclCMDType::HCCL_CMD_REDUCE_SCATTER;

    MOCKER_CPP_VIRTUAL(comm, &HcclCommunicator::GetAlgType)
        .stubs()
        .will(invoke(Stub_GetAlgType_ALG_ALLGATHER_REDUCESCATTER_GRAPH_PIPELINE));

    HcclResult result = comm.GetWorkspaceSubStreamNum(streamNum, dataSize, opType);

    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_EQ(streamNum, comm.userRankSize_ / comm.moduleNum_);
}

TEST_F(HcomTest, should_return_substream_num_when_910b_allgather_graph_pipeline_level1algo)
{
    HcclCommunicator comm;
    comm.deviceType_ = DevType::DEV_TYPE_910B;
    comm.moduleNum_ = 8;
    comm.deviceNumPerAggregation_ = 8;
    comm.userRankSize_ = 64;

    u64 dataSize = HCCL_SMALL_COUNT_1_MB - 1;
    u64 streamNum = 0;

    HcclCMDType opType = HcclCMDType::HCCL_CMD_ALLGATHER;

    MOCKER_CPP_VIRTUAL(comm, &HcclCommunicator::GetAlgType)
        .stubs()
        .will(invoke(Stub_GetAlgType_pipeline));

    HcclResult result = comm.GetWorkspaceSubStreamNum(streamNum, dataSize, opType);

    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_EQ(streamNum, comm.userRankSize_ / comm.moduleNum_);
}

TEST_F(HcomTest, should_return_substream_num_when_910c_graph_allgather_pipeline)
{
    HcclCommunicator comm;
    comm.deviceType_ = DevType::DEV_TYPE_910_93;
    comm.userRankSize_ = 8;
    comm.moduleNum_ = 2;
    comm.serverNum_ = 1;

    u64 streamNum = 0;
    constexpr u64 streamForSmallCount = 3;
    u64 dataSize = HCCL_SMALL_COUNT_512_KB - 1;
    HcclCMDType opType = HcclCMDType::HCCL_CMD_ALLGATHER;

    MOCKER_CPP_VIRTUAL(comm, &HcclCommunicator::GetAlgType)
            .stubs()
            .will(invoke(Stub_GetAlgType_pipeline));

    HcclResult result = comm.GetWorkspaceSubStreamNum(streamNum, dataSize, opType);

    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_EQ(streamNum, streamForSmallCount);
}

TEST_F(HcomTest, should_return_substream_num_when_910b_reducescatter_graph_pipeline_level1algo)
{
    HcclCommunicator comm;
    comm.deviceType_ = DevType::DEV_TYPE_910B;
    comm.moduleNum_ = 8;
    comm.deviceNumPerAggregation_ = 4;
    comm.userRankSize_ = 32;

    u64 dataSize = HCCL_SMALL_COUNT_1_MB - 1;
    u64 streamNum = 0;

    HcclCMDType opType = HcclCMDType::HCCL_CMD_REDUCE_SCATTER;

    MOCKER_CPP_VIRTUAL(comm, &HcclCommunicator::GetAlgType)
        .stubs()
        .will(invoke(Stub_GetAlgType_pipeline));

HcclResult result = comm.GetWorkspaceSubStreamNum(streamNum, dataSize, opType);

    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_EQ(streamNum, comm.userRankSize_ / comm.moduleNum_);
}

TEST_F(HcomTest, hcclComm_ra_deinit)
{
    s32 ret = HCCL_SUCCESS;

    u32 devLogicId = 0;

    bool supportMultiProcHCCP = true;
    MOCKER_CPP(&NetworkManager::TsdCapabilityGet)
    .stubs()
    .with(outBound(supportMultiProcHCCP))
    .will(returnValue(HCCL_SUCCESS));

    NetworkManager::GetInstance(devLogicId).deviceNicInitRef_.Ref();

    NetworkManager::GetInstance(devLogicId).DeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE);
    GlobalMockObject::verify();
}

TEST_F(HcomTest, should_return_substream_num_when_910b_reduce_order_preservation)
{
    HcclCommunicator comm;
    comm.deviceType_ = DevType::DEV_TYPE_910B;
    comm.userRankSize_ = 5;
    comm.deviceNumPerAggregation_ = 5;
    comm.moduleNum_ = 1;

    u64 streamNum = 0;
    u64 dataSize = 0;
    HcclCMDType opType = HcclCMDType::HCCL_CMD_ALLREDUCE;

    MOCKER_CPP_VIRTUAL(comm, &HcclCommunicator::GetAlgType).stubs().will(invoke(Stub_GetAlgType_pipeline));
    MOCKER(GetExternalInputHcclDeterministicV2).stubs().will(returnValue(2));

    HcclResult result = comm.GetWorkspaceSubStreamNum(streamNum, dataSize, opType);
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_EQ(streamNum, 7);
    GlobalMockObject::verify();
}

TEST_F(HcomTest, st_group_fail_test)
{
    hcclComm comm(0, 0, "tag");
    std::string group = ""; 
    u32 groupRank = 0;
    u32 userRank = 0;
    std::vector<u32> groupRanks;
    std::shared_ptr<hcclComm> groupComm = nullptr;
    HcclResult ret = comm.CreateGroup(group, groupRank, userRank, groupRanks, groupComm);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcomTest, DataTypeforAIV)
{
    // 列出所有已知枚举值
    EXPECT_EQ(IsSupportAIVCopy(HCCL_DATA_TYPE_FP16), true);
    EXPECT_EQ(IsSupportAIVCopy(HCCL_DATA_TYPE_INT16), true);
    EXPECT_EQ(IsSupportAIVCopy(HCCL_DATA_TYPE_UINT16), true);
    EXPECT_EQ(IsSupportAIVCopy(HCCL_DATA_TYPE_FP32), true);
    EXPECT_EQ(IsSupportAIVCopy(HCCL_DATA_TYPE_INT32), true);
    EXPECT_EQ(IsSupportAIVCopy(HCCL_DATA_TYPE_UINT32), true);
    EXPECT_EQ(IsSupportAIVCopy(HCCL_DATA_TYPE_INT8), true);
    EXPECT_EQ(IsSupportAIVCopy(HCCL_DATA_TYPE_UINT8), true);
    EXPECT_EQ(IsSupportAIVCopy(HCCL_DATA_TYPE_BFP16), true);
}

TEST_F(HcomTest, st_hcom_GetInstanceList_error)
{
    string rank_ID("0");
    std::string rankTableM;

    TopoinfoRanktableStandard myTopoinfoRanktableStandard(rankTableM, rank_ID);
    string propValue = "";

    HcclCommParams params = {};
    RankTable_t rankTable;
    nlohmann::json rank_table = nlohmann::json::array({
        { {"instance_id", 0}, {"device_name", "device0"}, {"server_id", ""}}
    });
    s32 ret = myTopoinfoRanktableStandard.GetInstanceList(rank_table, params, rankTable, 1 , 1);
    EXPECT_EQ(ret, HCCL_E_PARA);
    nlohmann::json rank_table2 = nlohmann::json::array({
        { {"instance_id", 0}, {"device_name", "device0"}, {"server_id",
            "12345678910123456789101234567891012345678910123456789101234567891"}}
    });
    ret = myTopoinfoRanktableStandard.GetInstanceList(rank_table2, params, rankTable, 1 , 1);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

HcclResult HcomSetGroupToTopoInfoStub(const char *group, uint32_t rankSize)
{
    return HCCL_SUCCESS;
}

void HcomUnsetGroupToTopoInfoStub(const char *group)
{
    return;
}

TEST_F(HcomTest, st_hcom_HcomTopoInfoRegCallback)
{
    // HcomInfo hcomInfo;
    // hcomInfo.groupRankNumMap.clear();
    // MOCKER(HcomGetCtxHomInfo)
    // .stubs()
    // .with(any())
    // .will(returnValue(hcomInfo));
    
    HcomTopoInfoRegCallback(HcomSetGroupToTopoInfoStub, HcomUnsetGroupToTopoInfoStub);
}

TEST_F(HcomTest, st_hcom_HcomSetExecTimeOut)
{
    char *execTimeOut = nullptr;
    HcclResult ret = HcomSetExecTimeOut(execTimeOut);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcomTest, st_hcom_HcomSetAlgorithm)
{
    char *algo = nullptr;
    HcclResult ret = HcomSetAlgorithm(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcomTest, st_hcom_HcomSetDeterministic)
{
    MOCKER(SetDeterministic)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    u8 deterministic = 1;
    HcclResult ret = HcomSetDeterministic(deterministic);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomTest, st_hcom_SetAivCoreLimit)
{
    MOCKER_CPP(&hcclComm::SetAivCoreLimit)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    std::shared_ptr<hccl::hcclComm> h = std::make_shared<hccl::hcclComm>();
    h->communicator_ = std::make_unique<HcclCommunicator>();
 
    s64 base = 0;
    u32 blockDim = 4;
    EXPECT_EQ(HcclCommGraphSetAivCoreLimit(base, blockDim), HCCL_E_PARA);
 
    base = reinterpret_cast<s64>(h.get());
    blockDim = 0;
    EXPECT_EQ(HcclCommGraphSetAivCoreLimit(base, blockDim), HCCL_E_PARA);
 
    blockDim = 4;
    EXPECT_EQ(HcclCommGraphSetAivCoreLimit(base, blockDim), HCCL_SUCCESS);
    // EXPECT_EQ(h->communicator_->blockDim_, 4U);
 
    blockDim = 0;
    EXPECT_EQ(HcomSetAivCoreLimit(nullptr, blockDim), HCCL_E_PARA);
 
    MOCKER(HcomGetCommByGroup).stubs().with(any(), outBound(h)).will(returnValue(HCCL_SUCCESS));
    blockDim = 5;
    EXPECT_EQ(HcomSetAivCoreLimit(nullptr, blockDim), HCCL_SUCCESS);
    // EXPECT_EQ(h->communicator_->blockDim_, 5U);
 
    std::string group("hcom_group_1");
    blockDim = 6;
    EXPECT_EQ(HcomSetAivCoreLimit(group.c_str(), blockDim), HCCL_SUCCESS);
    // EXPECT_EQ(h->communicator_->blockDim_, 6U);
 
    GlobalMockObject::verify();
}
 
TEST_F(HcomTest, st_hcom_GetInitStatus)
{
    bool initiated = 0;
    HcclResult ret = HcomGetInitStatus(&initiated);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
 
TEST_F(HcomTest, st_hcom_GetServerNumAndDeviceNumPerServer)
{
    u32 serverNum;
    u32 deviceNumPerServer;
    u32 deviceNumPerAggregation;
 
    HcomInfo &hcomInfo = HcomGetCtxHomInfo();
    hcomInfo.pComm = std::make_shared<hccl::hcclComm>();
    hcomInfo.rankTable.serverNum = 1;
    hcomInfo.rankTable.deviceNum = 8;
    HcclResult ret;
 
    MOCKER_CPP(&hcclComm::GetDeviceNumPerAggregation)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    ret = HcomGetServerNumAndDeviceNumPerServer(&serverNum, &deviceNumPerServer, &deviceNumPerAggregation);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    GlobalMockObject::verify();
}
 
TEST_F(HcomTest, st_hcom_SetWorkflowMode)
{
    HcclResult ret;
    ret = HcomSetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclWorkflowMode WorkflowMode;
    WorkflowMode = HcomGetWorkflowMode();
 
    EXPECT_EQ(WorkflowMode, HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB);
}
 
TEST_F(HcomTest, st_hcom_CreateComResourceByComm)
{
    MOCKER(HcclCreateComResourceByComm)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));

    HcclComm comm;
    u32 streamMode;
    bool isOpbaseMode;
    void* commContext;
    bool isMC2;
    HcclResult ret;
    HcomSetLaunchKernelMode(false);

    ret = HcomCreateComResourceByComm(comm, streamMode, isOpbaseMode, &commContext, isMC2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    GlobalMockObject::verify();
}
 
TEST_F(HcomTest, st_hcom_GenerateCclOpTag)
{
    s64 hcomComm;
    char* tag = new char[256];
    std::string sCollectiveType;
    HcclResult ret;
 
    MOCKER(GenerateCclOpTag)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
    ret = HcomGenerateCclOpTag("optype", hcomComm, "group", tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    delete[] tag;
 
    GlobalMockObject::verify();
}
 
TEST_F(HcomTest, st_hcom_GetOpScratchMemSize)
{
    bool isOfflineCompilation;
    HcclCMDType hcclOpType;
    HcomOpParam hcomOpParam;
    u64 opMemSize;
    u32 dataTypeSize;
    s32 rankSize;
    s32 serverNum;
 
    hcomOpParam.opType = "opType";
    hcomOpParam.socVersion = "soc";
 
    HcclResult ret;
 
    MOCKER(HcomGetDevId)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(hrtCtxGetCurrent)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(hrtSetDevice)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(GetAlltoAllvStagedScratchMemSize)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(GetAlltoAllvcStagedScratchMemSize)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(hrtResetDevice)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(hrtCtxSetCurrent)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(hrtGetDeviceTypeBySocVersion)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(GetDeterministic)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(GetAllReduceScratchMemSize)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));

    MOCKER(GetRedcueScatterVScratchMemSize)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
 
    ret = GetOpScratchMemSize(isOfflineCompilation, hcclOpType, &hcomOpParam, opMemSize, dataTypeSize, rankSize, serverNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    hcclOpType = HCCL_CMD_REDUCE_SCATTER_V;
    ret = GetOpScratchMemSize(isOfflineCompilation, hcclOpType, &hcomOpParam, opMemSize, dataTypeSize, 3, serverNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hcclOpType = HCCL_CMD_REDUCE_SCATTER;
    ret = GetOpScratchMemSize(isOfflineCompilation, hcclOpType, &hcomOpParam, opMemSize, dataTypeSize, 3, serverNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hcclOpType = HCCL_CMD_BROADCAST;
    ret = GetOpScratchMemSize(isOfflineCompilation, hcclOpType, &hcomOpParam, opMemSize, dataTypeSize, 3, serverNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = GetOpScratchMemSize(false, hcclOpType, &hcomOpParam, opMemSize, dataTypeSize, 3, serverNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hcclOpType = HCCL_CMD_ALLTOALLVC;
    ret = GetOpScratchMemSize(false, hcclOpType, &hcomOpParam, opMemSize, dataTypeSize, 3, serverNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hcclOpType = HCCL_CMD_ALLREDUCE;
    ret = GetOpScratchMemSize(false, hcclOpType, &hcomOpParam, opMemSize, dataTypeSize, 3, serverNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    GlobalMockObject::verify();
}
 
TEST_F(HcomTest, st_hcom_GetOpWorkspaceMemSize)
{
    bool isOfflineCompilation;
    HcclCMDType hcclOpType;
    s32 serverNum;
    u64 opMemSize;
    HcomOpParam hcomOpParam;
    hcomOpParam.opType = "HcomAllGather";
    hcomOpParam.dataType = HcclDataType::HCCL_DATA_TYPE_INT8;
    hcomOpParam.count = 1024;
    hcomOpParam.group = "group_0";
    hcomOpParam.rankSize = 8;
    HcclResult ret;
 
    MOCKER(GetOpScratchMemSize)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    ret = GetOpWorkspaceMemSize(isOfflineCompilation, hcclOpType, &hcomOpParam, serverNum, opMemSize);
 
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    GlobalMockObject::verify();
}
 
TEST_F(HcomTest, st_hcom_HcomGetMemType)
{
    const char *group;
    u32 memType = 0;
    bool isTsMen = false;
 
    HcclResult ret;
 
    MOCKER(HcomGetRankSize)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(HcomGetHccsLinkNum)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    ret = HcomGetMemType(group, "Ascend310P3", true, &memType, &isTsMen, false, true);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = HcomGetMemType(group, "Ascend310P3", true, &memType, &isTsMen, false, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomGetMemType(group, "Ascend310P1", true, &memType, &isTsMen, false, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomGetMemType(group, "Ascend910", true, &memType, &isTsMen, false, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    MOCKER(Is310PDevice).stubs().will(returnValue(true));
    ret = HcomGetMemType(group, "Ascend310P3", true, &memType, &isTsMen, false, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = HcomGetMemType(group, "Ascend310P3", true, &memType, &isTsMen, false, true);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = HcomGetMemType(group, "Ascend910", true, &memType, &isTsMen, false, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcomGetMemType(group, "Ascend310P3", false, &memType, &isTsMen, false, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = HcomGetMemType(group, "Ascend310P3", true, &memType, &isTsMen, true, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    GlobalMockObject::verify();
}
 
TEST_F(HcomTest, st_hcom_GetAlltoAllvStagedScratchMemSize)
{
    HcomOpParam hcomOpParam;
    u64 getMemSize;
 
    hcomOpParam.All2AllDataDes.sendCounts = malloc(sizeof(u64));
    hcomOpParam.All2AllDataDes.sendDispls = malloc(sizeof(u64));
    hcomOpParam.All2AllDataDes.recvCounts = malloc(sizeof(u64));
    hcomOpParam.All2AllDataDes.recvDispls = malloc(sizeof(u64));
 
    HcclResult ret;
 
    MOCKER(HcomGetAlltoAllStagedWorkSpaceMemSize)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    ret = GetAlltoAllvStagedScratchMemSize(&hcomOpParam, 1, getMemSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = GetAlltoAllvStagedScratchMemSize(&hcomOpParam, 257, getMemSize);
    EXPECT_EQ(ret, HCCL_E_PARA);
 
    free(hcomOpParam.All2AllDataDes.sendCounts);
    free(hcomOpParam.All2AllDataDes.sendDispls);
    free(hcomOpParam.All2AllDataDes.recvCounts);
    free(hcomOpParam.All2AllDataDes.recvDispls);
 
    GlobalMockObject::verify();
}
 
TEST_F(HcomTest, st_hcom_GetAlltoAllvcStagedScratchMemSize)
{
    HcomOpParam hcomOpParam;
    u64 getMemSize;
 
    hcomOpParam.All2AllDataDes.sendCountMatrix = malloc(sizeof(u64));
 
    HcclResult ret;
 
    MOCKER(HcomGetAlltoAllvcStagedWorkSpaceMemSize)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(SalGetDataTypeSize)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    ret = GetAlltoAllvcStagedScratchMemSize(&hcomOpParam, 1, getMemSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = GetAlltoAllvcStagedScratchMemSize(&hcomOpParam, 257, getMemSize);
    EXPECT_EQ(ret, HCCL_E_PARA);
 
    free(hcomOpParam.All2AllDataDes.sendCountMatrix);
 
    GlobalMockObject::verify();
}
 
TEST_F(HcomTest, st_hcom_GetAllReduceScratchMemSize)
{
    HcomOpParam hcomOpParam;
    u64 getMemSize;
 
    hcomOpParam.dataType = HCCL_DATA_TYPE_INT8;
 
    hcomOpParam.All2AllDataDes.sendCountMatrix = malloc(sizeof(u64));
 
    HcclResult ret;
 
    MOCKER(GetAllReduceScratchSizeWithoutDev)
    .stubs()
    .will(returnValue(HcclResult::HCCL_E_PARA));
 
    MOCKER(HcomGetAllReduceScratchSize)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
 
    ret = GetAllReduceScratchMemSize(false, &hcomOpParam, 1, 1, getMemSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = GetAllReduceScratchMemSize(true, &hcomOpParam, 1, 1, getMemSize);
    EXPECT_EQ(ret, HCCL_E_PARA);
 
    free(hcomOpParam.All2AllDataDes.sendCountMatrix);
 
    GlobalMockObject::verify();
}
 
TEST_F(HcomTest, st_hcom_GetAllReduceScratchSizeWithoutDev)
{
    HcomOpParam hcomOpParam;
    u64 scratchSize;
 
    hcomOpParam.socVersion = "soc";
    hcomOpParam.dataType = HCCL_DATA_TYPE_INT8;
 
    HcclResult ret;
 
    MOCKER(GetOffDeviceTypeWithoutDev)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(GetDeterministic)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
 
    ret = GetAllReduceScratchSizeWithoutDev(&hcomOpParam, 1, 1, scratchSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    GlobalMockObject::verify();
}

TEST_F(HcomTest, st_hcom_GetRedcueScatterVScratchMemSize)
{
    HcomOpParam hcomOpParam;
    u64 scratchSize;
 
    hcomOpParam.socVersion = "Ascend910B";
    hcomOpParam.dataType = HcclDataType::HCCL_DATA_TYPE_INT8;
    hcomOpParam.geDeterministic = 0;
    hcomOpParam.rankSize = 2;
    hcomOpParam.count = 1;
 
    HcclResult ret;
    ret = GetRedcueScatterVScratchMemSize(&hcomOpParam, scratchSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcomOpParam.geDeterministic = 1;
    ret = GetRedcueScatterVScratchMemSize(&hcomOpParam, scratchSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hcomOpParam.socVersion = "Ascend910_9391";
    ret = GetRedcueScatterVScratchMemSize(&hcomOpParam, scratchSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}
 
TEST_F(HcomTest, st_hcom_SplitHcclOpTypeConfig)
{
    std::string opType = "aa";
    std::string specificAlgoConfig;
 
    HcclResult ret;
 
    ret = SplitHcclOpTypeConfig("/aaa", opType, specificAlgoConfig);
    EXPECT_EQ(ret, HCCL_E_PARA);
 
    ret = SplitHcclOpTypeConfig("a/aaa", opType, specificAlgoConfig);
    EXPECT_EQ(ret, HCCL_E_PARA);
 
    ret = SplitHcclOpTypeConfig("aaa", opType, specificAlgoConfig);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = SplitHcclOpTypeConfig("a=a/a", opType, specificAlgoConfig);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = SplitHcclOpTypeConfig("aa=a/a", opType, specificAlgoConfig);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
 
TEST_F(HcomTest, st_hcom_GetToSlaveStreamTaskNum)
{
    u32 taskNum;
 
    HcclResult ret;
 
    ret = GetToSlaveStreamTaskNum("HcomAllReduce", 1, 2, taskNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = GetToSlaveStreamTaskNum("HcomAllGather", 1, 2, taskNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
 
TEST_F(HcomTest, st_hcom_GetToMasterStreamTaskNum)
{
    u32 taskNum;
 
    HcclResult ret;
 
    ret = GetToMasterStreamTaskNum("HcomAllReduce", taskNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = GetToMasterStreamTaskNum("HcomAllGather", taskNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
 
TEST_F(HcomTest, st_hcom_GetCombineComTaskNum)
{
    u32 intraTaskNum;
    u32 interTaskNum;
 
    HcclResult ret;
 
    ret = GetCombineComTaskNum("HcomAllReduce", 1, 1, intraTaskNum, interTaskNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = GetCombineComTaskNum("HcomAllGather", 1, 1, intraTaskNum, interTaskNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = GetCombineComTaskNum("HcomReduceScatter", 1, 1, intraTaskNum, interTaskNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = GetCombineComTaskNum("HcomAllToAll", 1, 1, intraTaskNum, interTaskNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = GetCombineComTaskNum("Hcom", 1, 1, intraTaskNum, interTaskNum);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}
 
TEST_F(HcomTest, st_hcom_GetIntraComTaskNum)
{
    AlgType algType;
    u32 taskNum;
 
    HcclResult ret;
 
    algType.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    ret = GetIntraComTaskNum("HcomAllReduce", 1, 1, algType, taskNum, 60000);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    algType.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_NP_DOUBLE_RING;
    ret = GetIntraComTaskNum("HcomAllGather", 1, 1, algType, taskNum, 60000);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    algType.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_4P_MESH;
    ret = GetIntraComTaskNum("HcomReduceScatter", 1, 1, algType, taskNum, 60000);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = GetIntraComTaskNum("HcomAllToAll", 1, 1, algType, taskNum, 70000);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = GetIntraComTaskNum("Hcom", 1, 1, algType, taskNum, 1);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}
 
TEST_F(HcomTest, st_hcom_GetInterComTaskNum)
{   
    DevType devType;
    u32 taskNum;
 
    HcclResult ret;
 
    ret = GetInterComTaskNum("HcomAllReduce", 1, 2, devType, taskNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = GetInterComTaskNum("HcomAllGather", 3, 2, devType, taskNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = GetInterComTaskNum("HcomReduceScatter", 9, 2, devType, taskNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = GetInterComTaskNum("HcomAllToAll", 3, 2, devType, taskNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = GetInterComTaskNum("Hcom", 3, 2, devType, taskNum);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}
 
TEST_F(HcomTest, st_hcom_CalcTaskNum)
{   
    HcomOpParam hcomOpParam;
    u64 streamNum;
    s32 deviceNumPerServer;
    s32 serverNum;
    u32 taskNum;
    DevType devType;
 
    HcclResult ret;
 
    MOCKER(IsNeedCalTaskNum)
    .stubs()
    .will(returnValue(false));
 
    MOCKER(GetAlgType)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(SalGetDataTypeSize)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(CalculatePiplineSliceNum)
    .stubs()
    .will(returnValue(0ULL));
 
    MOCKER(GetDfxTaskNum)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(GetToSlaveStreamTaskNum)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(GetToMasterStreamTaskNum)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(GetCombineComTaskNum)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(GetIntraComTaskNum)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(GetInterComTaskNum)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    hcomOpParam.opType = "op";
    hcomOpParam.socVersion = "soc";
 
    ret = CalcTaskNum(&hcomOpParam, streamNum, deviceNumPerServer, serverNum, false, taskNum, devType);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    deviceNumPerServer = 1;
 
    ret = CalcTaskNum(&hcomOpParam, streamNum, deviceNumPerServer, serverNum, false, taskNum, devType);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    MOCKER(CalculatePiplineSliceNum)
    .stubs()
    .will(returnValue(2ULL));
 
    ret = CalcTaskNum(&hcomOpParam, streamNum, deviceNumPerServer, serverNum, true, taskNum, devType);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    MOCKER(IsNeedCalTaskNum)
    .stubs()
    .will(returnValue(true));
 
    ret = CalcTaskNum(&hcomOpParam, streamNum, deviceNumPerServer, serverNum, false, taskNum, devType);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hcomOpParam.opType = "HcomSend";
 
    ret = CalcTaskNum(&hcomOpParam, streamNum, deviceNumPerServer, serverNum, false, taskNum, devType);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    GlobalMockObject::verify();
}

TEST_F(HcomTest, st_hcom_GetDeterministic)
{
    DevType devType = DevType::DEV_TYPE_910_93;
    u8 deterministic;
 
    HcclResult ret;
 
    ret = GetDeterministic(devType, 1, deterministic);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    devType = DevType::DEV_TYPE_910B;
    ret = GetDeterministic(devType, 1, deterministic);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = GetDeterministic(devType, 2, deterministic);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    setenv("HCCL_DETERMINISTIC","TRUE",1);
    ret = GetDeterministic(devType, 1, deterministic);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    setenv("HCCL_DETERMINISTIC","FALSE",1);
    ret = GetDeterministic(devType, 1, deterministic);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    setenv("HCCL_DETERMINISTIC","STRICT",1);
    ret = GetDeterministic(devType, 1, deterministic);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    setenv("HCCL_DETERMINISTIC","SS",1);
    ret = GetDeterministic(devType, 1, deterministic);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcomTest, st_hcom_HcomCalcOpOnline)
{
    MOCKER(HcomGetWorkspaceSubStreamNum)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(GetOpWorkspaceMemSize)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(CalcTaskNum)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(GetModuleInfo)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    HcomInfo &hcomInfo = HcomGetCtxHomInfo();
    hcomInfo.pComm = std::make_shared<hccl::hcclComm>();
    hcomInfo.rankTable.serverNum = 1;
    hcomInfo.rankTable.deviceNum = 8;
 
    HcomOpParam hcomOpParam;
    HcomResResponse hcomResResponse;
    hcomOpParam.opType = "HcomAllGather";
    hcomOpParam.socVersion = "Ascend910_9391"; // "Ascend310P3" "Ascend910A" "Ascend910B"
    hcomOpParam.dataType = HcclDataType::HCCL_DATA_TYPE_INT8;
    hcomOpParam.count = 1024;
    hcomOpParam.group = nullptr;
    hcomOpParam.rankSize = 0;
 
    HcclResult ret =  HcomCalcOpOnline(&hcomOpParam, &hcomResResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    GlobalMockObject::verify();
}
 
TEST_F(HcomTest, st_hcom_HcomCalcOpResOffline)
{
    MOCKER(GetStreamNumOfflineComp)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(GetOpWorkspaceMemSize)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    MOCKER(CalcTaskNum)
    .stubs()
    .will(returnValue(HcclResult::HCCL_SUCCESS));
 
    std::string rankTableString = R"({"status": "completed","version": "1.1","node_list":[{"node_id": "0","rank_list":[
        {"rank_id": "0","item_id": "0","rank_ip":"192.168.2.10"},
        {"rank_id": "1","item_id": "0","rank_ip":"192.168.2.11"},
        {"rank_id": "2","item_id": "0","rank_ip":"192.168.2.11"},
        {"rank_id": "3","item_id": "0","rank_ip":"192.168.2.11"},
        {"rank_id": "4","item_id": "0","rank_ip":"192.168.2.11"},
        {"rank_id": "5","item_id": "0","rank_ip":"192.168.2.11"},
        {"rank_id": "6","item_id": "0","rank_ip":"192.168.2.11"},
        {"rank_id": "7","item_id": "0","rank_ip":"192.168.2.11"}]}]})";
 
    u32 groupRankList[] = {0, 1, 2, 3, 4, 5, 6, 7};
 
    HcomOpParam hcomOpParam;
    HcomResResponse hcomResResponse;
    hcomOpParam.opType = "HcomAllGather";
    hcomOpParam.socVersion = "Ascend910";
    hcomOpParam.dataType = HcclDataType::HCCL_DATA_TYPE_INT8;
    hcomOpParam.count = 1024;
    hcomOpParam.group = "group_0";
    hcomOpParam.rankSize = 0;
    hcomOpParam.rankTable = const_cast<char *>(rankTableString.c_str());
    hcomOpParam.groupListSize = 8;
    hcomOpParam.groupList = groupRankList;
 
    HcclResult ret =  HcomCalcOpResOffline(&hcomOpParam, &hcomResResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hcomOpParam.groupListSize == 0;
    ret =  HcomCalcOpResOffline(&hcomOpParam, &hcomResResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    GlobalMockObject::verify();
}
 
TEST_F(HcomTest, st_hcom_GetClusterInfoAndDeviceNum)
{
    RankTable_t clusterInfo;
    s32 deviceNumPerServer = 0;
    std::string rankTableString = R"({"status": "completed","version": "1.1","node_list":[{"node_id": "0","rank_list":[
        {"rank_id": "0","item_id": "0","rank_ip":"192.168.2.10"},
        {"rank_id": "1","item_id": "0","rank_ip":"192.168.2.11"},
        {"rank_id": "2","item_id": "0","rank_ip":"192.168.2.11"},
        {"rank_id": "3","item_id": "0","rank_ip":"192.168.2.11"},
        {"rank_id": "4","item_id": "0","rank_ip":"192.168.2.11"},
        {"rank_id": "5","item_id": "0","rank_ip":"192.168.2.11"},
        {"rank_id": "6","item_id": "0","rank_ip":"192.168.2.11"},
        {"rank_id": "7","item_id": "0","rank_ip":"192.168.2.11"}]}]})";
 
    HcclResult ret =  GetClusterInfoAndDeviceNum(rankTableString, clusterInfo, deviceNumPerServer);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(deviceNumPerServer, 8);
    EXPECT_EQ(clusterInfo.serverNum, 1);
}
 
TEST_F(HcomTest, st_hcom_GetServerAndDevNumFromGroupList)
{
    std::string rankTableString = R"({"status": "completed","version": "1.1","node_list":[{"node_id": "0","rank_list":[
        {"rank_id": "0","item_id": "0","rank_ip":"192.168.2.10"},
        {"rank_id": "1","item_id": "0","rank_ip":"192.168.2.11"},
        {"rank_id": "2","item_id": "0","rank_ip":"192.168.2.11"},
        {"rank_id": "3","item_id": "0","rank_ip":"192.168.2.11"},
        {"rank_id": "4","item_id": "0","rank_ip":"192.168.2.11"},
        {"rank_id": "5","item_id": "0","rank_ip":"192.168.2.11"},
        {"rank_id": "6","item_id": "0","rank_ip":"192.168.2.11"},
        {"rank_id": "7","item_id": "0","rank_ip":"192.168.2.11"}]}]})";
 
    u32 groupList[] = {0, 1, 2, 3, 4, 5, 6, 7};
    u32 groupListSize = 8;
    DevType devType = DevType::DEV_TYPE_910B;
    s32 serverNum = 0;
    s32 deviceNumPerServer = 0;
    bool multiModuleDiffDeviceNumMode = true;
 
    HcclResult ret =  GetServerAndDevNumFromGroupList(groupList, groupListSize, rankTableString, devType,
        serverNum, deviceNumPerServer, multiModuleDiffDeviceNumMode);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(serverNum, 1);
    EXPECT_EQ(deviceNumPerServer, 8);
    EXPECT_EQ(multiModuleDiffDeviceNumMode, false);
}
 
TEST_F(HcomTest, st_hcom_GetStreamNumOfflineComp)
{
    HcclCMDType hcclOpType;
    s32 serverNum = 1;
    s32 deviceNumPerServer = 8;
    DevType devType;
    u64 streamNum;
    HcclResult ret;
 
    devType = DevType::DEV_TYPE_310P3;
    ret = GetStreamNumOfflineComp(hcclOpType, serverNum, deviceNumPerServer, devType, streamNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(streamNum, 0);
 
    devType = DevType::DEV_TYPE_NOSOC;
    ret = GetStreamNumOfflineComp(hcclOpType, serverNum, deviceNumPerServer, devType, streamNum);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
 
    devType = DevType::DEV_TYPE_910_93;
    hcclOpType = HcclCMDType::HCCL_CMD_SEND;
    ret = GetStreamNumOfflineComp(hcclOpType, serverNum, deviceNumPerServer, devType, streamNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(streamNum, 0);
 
    MOCKER(SatisfyIntraSuperPod)
    .stubs()
    .will(returnValue(true))
    .then(returnValue(false));
 
    MOCKER(FullmeshPairwiseSatisfyHighPerfAlltoallMeshCondition)
    .stubs()
    .will(returnValue(true));
 
    devType = DevType::DEV_TYPE_910_93;
    hcclOpType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    ret = GetStreamNumOfflineComp(hcclOpType, serverNum, deviceNumPerServer, devType, streamNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(streamNum, 7);
 
    ret = GetStreamNumOfflineComp(hcclOpType, serverNum, deviceNumPerServer, devType, streamNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(streamNum, 7);
 
    GlobalMockObject::verify();
}