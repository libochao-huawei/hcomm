/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <stdio.h>

#include "hccl/base.h"
#include <hccl/hccl_types.h>

#include "sal.h"
#include <cltm.h>
#include "nlohmann/json.hpp"
#include "rank_table_pub.h"
#include "llt_hccl_stub_pub.h"

#include <hccl/hcom.h>
#include "topoinfo_ranktableParser_pub.h"

using namespace std;
using namespace cltm;

class CltmTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--CltmTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--CltmTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};

#define MAX_BUF_SIZE (4*1024*1024)

TEST_F(CltmTest, ut_V80_1group_1server_1dev)
{
    nlohmann::json rank_table =
    {
        {"group_count","1"},
        {
        "allocated_group_resource",
            {
                {
                    {"group_name","world_group"},
                    {"dev_num","1"},
                    {"server_num","1"},
                    {
                    "allocated_resource",
                        {
                            {
                                {"server_id","100.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","0"},
                                {"avail_dev_count","1"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index","0"},
                                            {"device_ip_addr","172.17.1.116"}
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    std::string allocatedResouce = rank_table.dump();
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_strncpy(inputRes, MAX_BUF_SIZE, allocatedResouce.c_str(), allocatedResouce.length());
    char* rankTableBuf = (char*)sal_malloc(MAX_BUF_SIZE);
    unsigned int maxBufSize = MAX_BUF_SIZE;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_SUCCESS);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}
TEST_F(CltmTest, ut_V80_1group_2server_8dev)
{
    nlohmann::json rank_table =
    {
        {"group_count","1"},
        {
        "allocated_group_resource",
            {
                {
                    {"group_name","world_group"},
                    {"dev_num","16"},
                    {"server_num","2"},
                    {
                    "allocated_resource",
                        {
                            {
                                {"server_id","101.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","0"},
                                {"avail_dev_count","8"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","173.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","173.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index","0"},
                                            {"device_ip_addr","173.17.1.116"}
                                            
                                        },
                                        {
                                            {"device_index","1"},
                                            {"device_ip_addr","173.17.1.117"}
                                            
                                        },
                                        {
                                            {"device_index","2"},
                                            {"device_ip_addr","173.17.1.118"}
                                            
                                        },
                                        {
                                            {"device_index","3"},
                                            {"device_ip_addr","173.17.1.119"}
                                            
                                        },
                                        {
                                            {"device_index","4"},
                                            {"device_ip_addr","173.17.1.120"}
                                            
                                        },
                                        {
                                            {"device_index","5"},
                                            {"device_ip_addr","173.17.1.121"}
                                            
                                        },
                                        {
                                            {"device_index","6"},
                                            {"device_ip_addr","173.17.1.122"}
                                            
                                        },
                                        {
                                            {"device_index","7"},
                                            {"device_ip_addr","173.17.1.123"}
                                            
                                        },
                                    }
                                },
                            },
                            {
                                {"server_id","100.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","0"},
                                {"avail_dev_count","8"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index","0"},
                                            {"device_ip_addr","173.17.3.116"}
                                            
                                        },
                                        {
                                            {"device_index","1"},
                                            {"device_ip_addr","173.17.3.117"}
                                            
                                        },
                                        {
                                            {"device_index","2"},
                                            {"device_ip_addr","173.17.3.118"}
                                            
                                        },
                                        {
                                            {"device_index","3"},
                                            {"device_ip_addr","173.17.3.119"}
                                            
                                        },
                                        {
                                            {"device_index","4"},
                                            {"device_ip_addr","173.17.3.120"}
                                            
                                        },
                                        {
                                            {"device_index","5"},
                                            {"device_ip_addr","173.17.3.121"}
                                            
                                        },
                                        {
                                            {"device_index","6"},
                                            {"device_ip_addr","173.17.3.122"}
                                            
                                        },
                                        {
                                            {"device_index","7"},
                                            {"device_ip_addr","173.17.3.123"}
                                            
                                        },
                                    }
                                },
                            }
                        }
                    }
                }
            }
        }
    };
    std::string allocatedResouce = rank_table.dump();
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_strncpy(inputRes, MAX_BUF_SIZE, allocatedResouce.c_str(), allocatedResouce.length());
    char* rankTableBuf = (char*)sal_malloc(MAX_BUF_SIZE);
    unsigned int maxBufSize = MAX_BUF_SIZE;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_SUCCESS);
    if (ret== CLTM_SUCCESS) {
        printf("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}

TEST_F(CltmTest, ut_invalid_ip_addr)
{
    nlohmann::json rank_table =
    {
        {"group_count","1"},
        {
        "allocated_group_resource",
            {
                {
                    {"group_name","world_group"},
                    {"dev_num","1"},
                    {"server_num","1"},
                    {
                    "allocated_resource",
                        {
                            {
                                {"server_id","100.12.4.20000"},
                                {"chip_info","910"},
                                {"board_id","0"},
                                {"avail_dev_count","1"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index","0"},
                                            {"device_ip_addr","172.17.1.116"}
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    std::string allocatedResouce = rank_table.dump();
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_strncpy(inputRes, MAX_BUF_SIZE, allocatedResouce.c_str(), allocatedResouce.length());
    char* rankTableBuf = (char*)sal_malloc(MAX_BUF_SIZE);
    unsigned int maxBufSize = MAX_BUF_SIZE;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_E_PARA);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}

TEST_F(CltmTest, ut_invalid_device_id)
{
    nlohmann::json rank_table =
    {
        {"group_count","1"},
        {
        "allocated_group_resource",
            {
                {
                    {"group_name","world_group"},
                    {"dev_num","1"},
                    {"server_num","1"},
                    {
                    "allocated_resource",
                        {
                            {
                                {"server_id","100.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","0"},
                                {"avail_dev_count","1"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth1"},
                                            {"host_ip_addr","172.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index","8"},
                                            {"device_ip_addr","172.17.1.116"}
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    std::string allocatedResouce = rank_table.dump();
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_strncpy(inputRes, MAX_BUF_SIZE, allocatedResouce.c_str(), allocatedResouce.length());
    char* rankTableBuf = (char*)sal_malloc(MAX_BUF_SIZE);
    unsigned int maxBufSize = MAX_BUF_SIZE;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_E_PARA);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}

TEST_F(CltmTest, ut_same_server_id)
{
    nlohmann::json rank_table =
    {
        {"group_count","1"},
        {
        "allocated_group_resource",
            {
                {
                    {"group_name","world_group"},
                    {"dev_num","1"},
                    {"server_num","2"},
                    {
                    "allocated_resource",
                        {
                            {
                                {"server_id","101.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","0"},
                                {"avail_dev_count","1"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","173.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","173.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index","0"},
                                            {"device_ip_addr","173.17.1.116"}
                                        }
                                    }
                                },
                            },
                            {
                                {"server_id","101.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","0"},
                                {"avail_dev_count","1"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index","0"},
                                            {"device_ip_addr","172.17.1.116"}
                                        }
                                    }
                                },
                            }
                        }
                    }
                }
            }
        }
    };
    std::string allocatedResouce = rank_table.dump();
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_strncpy(inputRes, MAX_BUF_SIZE, allocatedResouce.c_str(), allocatedResouce.length());
    char* rankTableBuf = (char*)sal_malloc(MAX_BUF_SIZE);
    unsigned int maxBufSize = MAX_BUF_SIZE;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_E_PARA);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}

TEST_F(CltmTest, ut_invalid_device_ip_addr)
{
    nlohmann::json rank_table =
    {
        {"group_count","1"},
        {
        "allocated_group_resource",
            {
                {
                    {"group_name","world_group"},
                    {"dev_num","1"},
                    {"server_num","1"},
                    {
                    "allocated_resource",
                        {
                            {
                                {"server_id","100.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","0"},
                                {"avail_dev_count","1"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index","0"},
                                            {"device_ip_addr","172.17.1.256"}
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    std::string allocatedResouce = rank_table.dump();
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_strncpy(inputRes, MAX_BUF_SIZE, allocatedResouce.c_str(), allocatedResouce.length());
    char* rankTableBuf = (char*)sal_malloc(MAX_BUF_SIZE);
    unsigned int maxBufSize = MAX_BUF_SIZE;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_E_PARA);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}

TEST_F(CltmTest, ut_repeat_device_ip_addr)
{
    nlohmann::json rank_table =
    {
        {"group_count","1"},
        {
        "allocated_group_resource",
            {
                {
                    {"group_name","world_group"},
                    {"dev_num","2"},
                    {"server_num","1"},
                    {
                    "allocated_resource",
                        {
                            {
                                {"server_id","100.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","0"},
                                {"avail_dev_count","2"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index","0"},
                                            {"device_ip_addr","172.17.1.116"}
                                        },
                                        {
                                            {"device_index","1"},
                                            {"device_ip_addr","172.17.1.116"}
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    std::string allocatedResouce = rank_table.dump();
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_strncpy(inputRes, MAX_BUF_SIZE, allocatedResouce.c_str(), allocatedResouce.length());
    char* rankTableBuf = (char*)sal_malloc(MAX_BUF_SIZE);
    unsigned int maxBufSize = MAX_BUF_SIZE;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_E_PARA);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}

TEST_F(CltmTest, ut_same_group_name_fail)
{
    nlohmann::json rank_table =
    {
        {"group_count","2"},
        {
        "allocated_group_resource",
            {
                {
                    {"group_name","training"},
                    {"dev_num","1"},
                    {"server_num","2"},
                    {
                    "allocated_resource",
                        {
                            {
                                {"server_id","101.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","0"},
                                {"avail_dev_count","1"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","173.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","173.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index","0"},
                                            {"device_ip_addr","173.17.1.116"}
                                        }
                                    }
                                },
                            },
                            {
                                {"server_id","100.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","0"},
                                {"avail_dev_count","1"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index","0"},
                                            {"device_ip_addr","172.17.1.116"}
                                        }
                                    }
                                },
                            }
                        }
                    }
                },
                {
                    {"group_name","training"},
                    {"dev_num","1"},
                    {"server_num","2"},
                    {
                    "allocated_resource",
                        {
                            {
                                {"server_id","108.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","0"},
                                {"avail_dev_count","1"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","178.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","178.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index","0"},
                                            {"device_ip_addr","178.17.1.116"}
                                        }
                                    }
                                },
                            },
                            {
                                {"server_id","109.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","0"},
                                {"avail_dev_count","1"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","179.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","179.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index","0"},
                                            {"device_ip_addr","179.17.1.116"}
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    std::string allocatedResouce = rank_table.dump();
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_strncpy(inputRes, MAX_BUF_SIZE, allocatedResouce.c_str(), allocatedResouce.length());
    char* rankTableBuf = (char*)sal_malloc(MAX_BUF_SIZE);
    unsigned int maxBufSize = MAX_BUF_SIZE;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_E_PARA);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}

TEST_F(CltmTest, ut_invlid_data_type)
{
    nlohmann::json rank_table =
    {
        {"group_count","1"},
        {
        "allocated_group_resource",
            {
                {
                    {"group_name","world_group"},
                    {"dev_num","1"},
                    {"server_num","1"},
                    {
                    "allocated_resource",
                        {
                            {
                                {"server_id","100.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","0"},
                                {"avail_dev_count",1},       /* 数据类型不是字符串 */
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index","0"},
                                            {"device_ip_addr","172.17.1.116"}
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    std::string allocatedResouce = rank_table.dump();
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_strncpy(inputRes, MAX_BUF_SIZE, allocatedResouce.c_str(), allocatedResouce.length());
    char* rankTableBuf = (char*)sal_malloc(MAX_BUF_SIZE);
    unsigned int maxBufSize = MAX_BUF_SIZE;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_E_PARA);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}


TEST_F(CltmTest, ut_json_property_is_not_exist)
{
    nlohmann::json rank_table =
    {
        {"group_count","1"},
        {
        "allocated_group_resource",
            {
                {
                    {"group_name","world_group"},
                    {"dev_num","1"},
                    {"server_num","1"}
                }
            }
        }
    };
    std::string allocatedResouce = rank_table.dump();
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_strncpy(inputRes, MAX_BUF_SIZE, allocatedResouce.c_str(), allocatedResouce.length());
    char* rankTableBuf = (char*)sal_malloc(MAX_BUF_SIZE);
    unsigned int maxBufSize = MAX_BUF_SIZE;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_E_PARA);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}

TEST_F(CltmTest, ut_invalid_array)   //////////////////////////////////////
{
    nlohmann::json rank_table =
    {
        {"group_count","1"},
        {
        "allocated_group_resource",
            {
                {
                    {"group_name","world_group"},
                    {"dev_num","1"},
                    {"server_num","1"},
                    {
                    "allocated_resource",
                        {
                            {
                                {"server_id","100.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","0"},
                                {"avail_dev_count","1"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {"device_index","0"},
                                        {"device_ip_addr","172.17.1.116"}
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    std::string allocatedResouce = rank_table.dump();
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_strncpy(inputRes, MAX_BUF_SIZE, allocatedResouce.c_str(), allocatedResouce.length());
    char* rankTableBuf = (char*)sal_malloc(MAX_BUF_SIZE);
    unsigned int maxBufSize = MAX_BUF_SIZE;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_E_PARA);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}

TEST_F(CltmTest, ut_json_string_property_is_not_exist)
{
    nlohmann::json rank_table =
    {
        {"group_count","1"},
        {
        "allocated_group_resource",
            {
                {
                    /*{"group_name","world_group"},*/
                    {"dev_num","1"},
                    {"server_num","1"},
                    {
                    "allocated_resource",
                        {
                            {
                                /*{"server_id","100.12.4.1"},*/
                                {"chip_info","910"},
                                {"board_id","0"},
                                {"avail_dev_count","1"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index","0"},
                                            {"device_ip_addr","172.17.1.116"}
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    std::string allocatedResouce = rank_table.dump();
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_strncpy(inputRes, MAX_BUF_SIZE, allocatedResouce.c_str(), allocatedResouce.length());
    char* rankTableBuf = (char*)sal_malloc(MAX_BUF_SIZE);
    unsigned int maxBufSize = MAX_BUF_SIZE;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_E_PARA);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}

TEST_F(CltmTest, ut_json_array_string_property_is_not_exist)
{
    nlohmann::json rank_table =
    {
        {"group_count","1"},
        {
        "allocated_group_resource",
            {
                {
                    {"group_name","world_group"},
                    {"dev_num","1"},
                    {"server_num","1"},
                    {
                    "allocated_resource",
                        {
                            {
                                {"server_id","100.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","0"},
                                {"avail_dev_count","1"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            /*{"device_index","0"},*/
                                            {"device_ip_addr","172.17.1.116"}
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    std::string allocatedResouce = rank_table.dump();
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_strncpy(inputRes, MAX_BUF_SIZE, allocatedResouce.c_str(), allocatedResouce.length());
    char* rankTableBuf = (char*)sal_malloc(MAX_BUF_SIZE);
    unsigned int maxBufSize = MAX_BUF_SIZE;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_E_PARA);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}

TEST_F(CltmTest, ut_json_array_string_property_is_not_string)
{
    nlohmann::json rank_table =
    {
        {"group_count","1"},
        {
        "allocated_group_resource",
            {
                {
                    {"group_name","world_group"},
                    {"dev_num","1"},
                    {"server_num","1"},
                    {
                    "allocated_resource",
                        {
                            {
                                {"server_id","100.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","0"},
                                {"avail_dev_count","1"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index",0},     /* 属性值不为string */
                                            {"device_ip_addr","172.17.1.116"}
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    std::string allocatedResouce = rank_table.dump();
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_strncpy(inputRes, MAX_BUF_SIZE, allocatedResouce.c_str(), allocatedResouce.length());
    char* rankTableBuf = (char*)sal_malloc(MAX_BUF_SIZE);
    unsigned int maxBufSize = MAX_BUF_SIZE;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_E_PARA);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}

TEST_F(CltmTest, ut_json_array_length_not_equal_to_number)
{
    nlohmann::json rank_table =
    {
        {"group_count","1"},
        {
        "allocated_group_resource",
            {
                {
                    {"group_name","world_group"},
                    {"dev_num","1"},
                    {"server_num","3"},
                    {
                    "allocated_resource",
                        {
                            {
                                {"server_id","100.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","0"},
                                {"avail_dev_count","1"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index","0"},
                                            {"device_ip_addr","172.17.1.116"}
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    std::string allocatedResouce = rank_table.dump();
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_strncpy(inputRes, MAX_BUF_SIZE, allocatedResouce.c_str(), allocatedResouce.length());
    char* rankTableBuf = (char*)sal_malloc(MAX_BUF_SIZE);
    unsigned int maxBufSize = MAX_BUF_SIZE;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_E_PARA);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}

TEST_F(CltmTest, ut_too_long_input_json)
{
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE + 2);
    sal_memset(inputRes, MAX_BUF_SIZE + 2, 1, MAX_BUF_SIZE + 2);
    inputRes[MAX_BUF_SIZE + 1] = '\0';

    char* rankTableBuf = (char*)sal_malloc(MAX_BUF_SIZE);
    unsigned int maxBufSize = MAX_BUF_SIZE;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_E_PARA);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}

TEST_F(CltmTest, ut_output_buffer_is_not_enough)
{
    nlohmann::json rank_table =
    {
        {"group_count","1"},
        {
        "allocated_group_resource",
            {
                {
                    {"group_name","world_group"},
                    {"dev_num","1"},
                    {"server_num","1"},
                    {
                    "allocated_resource",
                        {
                            {
                                {"server_id","100.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","0"},
                                {"avail_dev_count","1"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index","0"},
                                            {"device_ip_addr","172.17.1.116"}
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    std::string allocatedResouce = rank_table.dump();
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_strncpy(inputRes, MAX_BUF_SIZE, allocatedResouce.c_str(), allocatedResouce.length());
    char* rankTableBuf = (char*)sal_malloc(10); //  输出缓存很小
    unsigned int maxBufSize = 10;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_E_NO_RESOURCE);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}

TEST_F(CltmTest, ut_illegal_input_para)
{
    std::string allocatedResouce;
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_strncpy(inputRes, MAX_BUF_SIZE, allocatedResouce.c_str(), allocatedResouce.length());
    char* rankTableBuf = (char*)sal_malloc(10); //  输出缓存很小
    unsigned int maxBufSize = 10;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, NULL, maxBufSize, &usedBufSize); /* 输入参数为空 */
    EXPECT_EQ(ret, CLTM_E_PARA);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}

TEST_F(CltmTest, ut_V80_illegal_input_json_info)
{
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_memset(inputRes, MAX_BUF_SIZE, 0,MAX_BUF_SIZE);
    char* rankTableBuf = (char*)sal_malloc(MAX_BUF_SIZE);
    unsigned int maxBufSize = MAX_BUF_SIZE;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_E_PARA);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}

TEST_F(CltmTest, ut_V80_duplicate_deviceid_in_same_server)
{
    nlohmann::json rank_table =
    {
        {"group_count","1"},
        {
        "allocated_group_resource",
            {
                {
                    {"group_name","world_group"},
                    {"dev_num","2"},
                    {"server_num","1"},
                    {
                    "allocated_resource",
                        {
                            {
                                {"server_id","100.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","0"},
                                {"avail_dev_count","2"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index","0"},
                                            {"device_ip_addr","172.17.1.116"}
                                        },
                                        {
                                            {"device_index","0"},
                                            {"device_ip_addr","172.17.1.117"}
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    std::string allocatedResouce = rank_table.dump();
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_strncpy(inputRes, MAX_BUF_SIZE, allocatedResouce.c_str(), allocatedResouce.length());
    char* rankTableBuf = (char*)sal_malloc(MAX_BUF_SIZE);
    unsigned int maxBufSize = MAX_BUF_SIZE;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_E_PARA);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}

TEST_F(CltmTest, ut_group_count_invalid)
{
    nlohmann::json rank_table =
    {
        {"group_count",""},
        {
        "allocated_group_resource",
            {
                {
                    {"group_name","world_group"},
                    {"dev_num","1"},
                    {"server_num","1"},
                    {
                    "allocated_resource",
                        {
                            {
                                {"server_id","100.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","5"},
                                {"avail_dev_count","1"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth1"},
                                            {"host_ip_addr","172.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index","0"},
                                            {"device_ip_addr","172.17.1.116"}
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    std::string allocatedResouce = rank_table.dump();
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_strncpy(inputRes, MAX_BUF_SIZE, allocatedResouce.c_str(), allocatedResouce.length());
    char* rankTableBuf = (char*)sal_malloc(MAX_BUF_SIZE);
    unsigned int maxBufSize = MAX_BUF_SIZE;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_E_PARA);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}

TEST_F(CltmTest, ut_invalid_group_count_invalid1)
{
    nlohmann::json rank_table =
    {
        {"group_count","#"},
        {
        "allocated_group_resource",
            {
                {
                    {"group_name","world_group"},
                    {"dev_num","1"},
                    {"server_num","1"},
                    {
                    "allocated_resource",
                        {
                            {
                                {"server_id","100.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","5"},
                                {"avail_dev_count","1"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth1"},
                                            {"host_ip_addr","172.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index","0"},
                                            {"device_ip_addr","172.17.1.116"}
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    std::string allocatedResouce = rank_table.dump();
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_strncpy(inputRes, MAX_BUF_SIZE, allocatedResouce.c_str(), allocatedResouce.length());
    char* rankTableBuf = (char*)sal_malloc(MAX_BUF_SIZE);
    unsigned int maxBufSize = MAX_BUF_SIZE;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_E_PARA);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}

TEST_F(CltmTest, ut_group_count_too_long)
{
    nlohmann::json rank_table =
    {
        {"group_count","10000000000"},
        {
        "allocated_group_resource",
            {
                {
                    {"group_name","world_group"},
                    {"dev_num","1"},
                    {"server_num","1"},
                    {
                    "allocated_resource",
                        {
                            {
                                {"server_id","100.12.4.1"},
                                {"chip_info","910"},
                                {"board_id","5"},
                                {"avail_dev_count","1"},
                                {"para_plane_eth_count","2"},
                                {
                                "para_plane_eth",
                                    {
                                        {
                                            {"eth_name","eth0"},
                                            {"host_ip_addr","172.17.0.114"}
                                        },
                                        {
                                            {"eth_name","eth1"},
                                            {"host_ip_addr","172.17.0.115"}
                                        }
                                    }
                                },
                                {"avail_dev",
                                    {
                                        {
                                            {"device_index","0"},
                                            {"device_ip_addr","172.17.1.116"}
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    std::string allocatedResouce = rank_table.dump();
    char* inputRes = (char*)sal_malloc(MAX_BUF_SIZE);
    sal_strncpy(inputRes, MAX_BUF_SIZE, allocatedResouce.c_str(), allocatedResouce.length());
    char* rankTableBuf = (char*)sal_malloc(MAX_BUF_SIZE);
    unsigned int maxBufSize = MAX_BUF_SIZE;
    unsigned int usedBufSize= 0;

    cltmResult_t ret = cltm_generate_ranktable(inputRes, rankTableBuf, maxBufSize, &usedBufSize);
    EXPECT_EQ(ret, CLTM_E_PARA);
    if (ret== CLTM_SUCCESS) {
        HCCL_INFO("output json:        %s", rankTableBuf);
    }

    sal_free(inputRes);
    sal_free(rankTableBuf);
    
}