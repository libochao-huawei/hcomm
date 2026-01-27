#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>
#define private public
#define protected public
#include "../inc/hccl/base.h"
#include "hccl.h"
#include <hccl/hccl_types.h>

#include "stream_pub.h"
#include "mem_host_pub.h"
#include "mem_device_pub.h"
#include "hccl_comm_pub.h"
#include "sal.h"
#include "hccl_impl.h"
#include "llt_hccl_stub_pub.h"
#include "externalinput.h"
#include "config.h"
#include "topoinfo_ranktableParser_pub.h"
#include "topoinfo_ranktableHeterog.h"
#include "hcom_op_utils.h"
#include "hcom_ops_kernel_info_store.h"
#include "external/ge/ge_api_types.h" // ge对内options
#include "framework/common/ge_types.h" // ge对外options
#include "hcom_pub.h"
#include "hcom.h"
#include "hcom_plugin.h"
#include "hccl/hcom_executor.h"
#include "hvd_adapter.h"

#include "graph/debug/ge_attr_define.h"
#include "graph/utils/graph_utils.h"
#include "graph/utils/node_utils.h"
#include "graph/utils/attr_utils.h"
#include "graph/node.h"
#include "graph/op_desc.h"
#include "graph/utils/tensor_utils.h"
#include <iostream>
#include <fstream>
#undef private
#undef protected

using namespace std;
using namespace hccl;

class HcomPluginTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--HcomPluginTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {

        std::cout << "\033[36m--HcomPluginTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};

TEST_F(HcomPluginTest, ut_GetRanktableInfo)
{
    nlohmann::json rank_table =
    {
	    {"collective_id", "192.168.3.3-9527-0001"},
        {"status", "completed"},
        {"heterog_sub_version", "1.2"},
	    {"version","1.1"},
        {"node_list", {
            {
                {"node_addr", "192.168.0.101"},
                {"ranks", {
                    {
                        {"device_id", "0"},
                        {"sdid", "0"},
                        {"rank_id", "0"},
                        {"port", "45000"},
                        {"rank_ip", ""}
                    },
                    {
                        {"device_id", "-1"},
                        {"sdid", "-1"},
                        {"rank_id", "1"},
                        {"port", "40000"},
                        {"rank_ip", ""},
                        {"bind_device_id", "0"}
                    },
                    {
                        {"device_id", "-1"},
                        {"sdid", "-1"},
                        {"rank_id", "2"},
                        {"port", "40001"},
                        {"rank_ip", ""},
                        {"bind_device_id", "0"}
                    },
                    {
                        {"device_id", "-1"},
                        {"sdid", "-1"},
                        {"rank_id", "3"},
                        {"port", "40002"},
                        {"rank_ip", ""},
                        {"bind_device_id", "0"}
                    },
                    {
                        {"device_id", "-1"},
                        {"sdid", "-1"},
                        {"rank_id", "4"},
                        {"port", "40003"},
                        {"rank_ip", ""},
                        {"bind_device_id", "0"}
                    }
                }}
            },
        }
        }
    };
 
    std::string rank_table_string = rank_table.dump();
    std::unique_ptr<TopoinfoRanktableHeterog> pTopoRanktable = nullptr;
    pTopoRanktable.reset(new (std::nothrow) TopoinfoRanktableHeterog(rank_table_string, to_string(0), DevType::DEV_TYPE_910_93));
    RankTable_t rankTbale;
    pTopoRanktable->fileContent_ = rank_table;
    HcclResult ret = pTopoRanktable->GetRanktableInfo(rankTbale);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcomPluginTest, ut_hcom_plugin_exec_timeout_default)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    // options.insert(pair<string,string> ("HCCL_algorithm","ring"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_EQ(ret, ge::SUCCESS);

    EXPECT_EQ(GetExternalInputHcclExecTimeOut(), NOTIFY_DEFAULT_WAIT_TIME);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_exec_timeout_invalid0)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("ge.exec.hcclExecuteTimeOut","0xffffffff"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_EQ(ret, ge::INTERNAL_ERROR);

    EXPECT_EQ(GetExternalInputHcclExecTimeOut(), NOTIFY_DEFAULT_WAIT_TIME);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_exec_timeout_invalid1)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("ge.exec.hcclExecuteTimeOut","0x5a5a"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_EQ(ret, ge::INTERNAL_ERROR);

    EXPECT_EQ(GetExternalInputHcclExecTimeOut(), NOTIFY_DEFAULT_WAIT_TIME);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_exec_timeout_invalid2)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("ge.exec.hcclExecuteTimeOut","this is a test"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_EQ(ret, ge::INTERNAL_ERROR);

    EXPECT_EQ(GetExternalInputHcclExecTimeOut(), NOTIFY_DEFAULT_WAIT_TIME);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_exec_timeout_invalid3)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("ge.exec.hcclExecuteTimeOut","-2555"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_EQ(ret, ge::INTERNAL_ERROR);

    EXPECT_EQ(GetExternalInputHcclExecTimeOut(), NOTIFY_DEFAULT_WAIT_TIME);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_exec_timeout_invalid4)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("ge.exec.hcclExecuteTimeOut","0"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_EQ(ret, ge::INTERNAL_ERROR);

    EXPECT_EQ(GetExternalInputHcclExecTimeOut(), HCCL_EXEC_TIME_OUT_S);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_exec_timeout_invalid5)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("ge.exec.hcclExecuteTimeOut","17341"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_EQ(ret, ge::INTERNAL_ERROR);

    EXPECT_EQ(GetExternalInputHcclExecTimeOut(), HCCL_EXEC_TIME_OUT_S);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_exec_timeout_valid0)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("ge.exec.hcclExecuteTimeOut","68"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_EQ(ret, ge::SUCCESS);

    EXPECT_EQ(GetExternalInputHcclExecTimeOut(), 68);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_exec_timeout_valid1)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("ge.exec.hcclExecuteTimeOut","3600"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_EQ(ret, ge::SUCCESS);

    EXPECT_EQ(GetExternalInputHcclExecTimeOut(), 3536);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_algo_config_hcclalgo_default)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    // options.insert(pair<string,string> ("HCCL_algorithm","ring"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_EQ(ret, ge::SUCCESS);

    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[0], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[1], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[2], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[3], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);

    // std::string algoEnv = SalGetEnv("HCCL_ALGORITHM");
    // HCCL_INFO("HCCL_ALGORITHM: %s", algoEnv.c_str());
    // bool bErr = (algoEnv == "level0:ring;level1:H-D_R;level2:fullmesh;level3:ring") ? false : true;
    // EXPECT_EQ(bErr, false);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_algo_config_hcclalgo_level0)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("HCCL_algorithm","level0:ring"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_EQ(ret, ge::SUCCESS);

    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[0], HcclAlgoType::HCCL_ALGO_TYPE_RING);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[1], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[2], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[3], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_algo_config_hcclalgo_level1)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    // options.insert(pair<string,string> ("level0_algorithm","ring"));
    options.insert(pair<string,string> ("HCCL_algorithm","level1:H-D_R"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_EQ(ret, ge::SUCCESS);

    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[0], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[1], HcclAlgoType::HCCL_ALGO_TYPE_HDR);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[2], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[3], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_algo_config_hcclalgo_level2)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("HCCL_algorithm","level2:fullmesh"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_EQ(ret, ge::SUCCESS);

    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[0], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[1], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[2], HcclAlgoType::HCCL_ALGO_TYPE_FULLMESH);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[3], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_algo_config_hcclalgo_level3)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("HCCL_algorithm","level3:ring"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_EQ(ret, ge::SUCCESS);

    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[0], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[1], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[2], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[3], HcclAlgoType::HCCL_ALGO_TYPE_RING);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_algo_config_hcclalgo_level01)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("HCCL_algorithm","level0:ring;level1:H-D_R"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_EQ(ret, ge::SUCCESS);

    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[0], HcclAlgoType::HCCL_ALGO_TYPE_RING);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[1], HcclAlgoType::HCCL_ALGO_TYPE_HDR);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[2], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[3], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_algo_config_hcclalgo_level03)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("HCCL_algorithm","level0:ring;level3:H-D_R"));


    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_EQ(ret, ge::SUCCESS);

    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[0], HcclAlgoType::HCCL_ALGO_TYPE_RING);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[1], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[2], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[3], HcclAlgoType::HCCL_ALGO_TYPE_HDR);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_algo_config_hcclalgo_level23)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("HCCL_algorithm","level2:ring;level3:H-D_R"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_EQ(ret, ge::SUCCESS);

    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[0], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[1], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[2], HcclAlgoType::HCCL_ALGO_TYPE_RING);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[3], HcclAlgoType::HCCL_ALGO_TYPE_HDR);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_algo_config_hcclalgo_level0123)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("HCCL_algorithm","level0:ring;level1:H-D_R;level2:fullmesh;level3:ring"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_EQ(ret, ge::SUCCESS);

    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[0], HcclAlgoType::HCCL_ALGO_TYPE_RING);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[1], HcclAlgoType::HCCL_ALGO_TYPE_HDR);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[2], HcclAlgoType::HCCL_ALGO_TYPE_FULLMESH);
    EXPECT_EQ(GetExternalInputHcclAlgoConfig()[3], HcclAlgoType::HCCL_ALGO_TYPE_RING);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_algo_config_hcclalgo_foramt_invalid_0)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("HCCL_algorithm","level:ring;level1:H-D_R;level2:fullmesh;level3:ring"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_NE(ret, ge::SUCCESS);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_algo_config_hcclalgo_foramt_invalid_1)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("HCCL_algorithm","levelx:ring;level1:H-D_R;level2:fullmesh;level3:ring"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_NE(ret, ge::SUCCESS);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_algo_config_hcclalgo_foramt_invalid_2)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("HCCL_algorithm","level0:ring;level1:H-D_R;level2:fullmesh;level4:ring"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_NE(ret, ge::SUCCESS);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_algo_config_hcclalgo_foramt_invalid_3)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("HCCL_algorithm","level0:ring;;level2:fullmesh;level4:ring"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_NE(ret, ge::SUCCESS);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_algo_config_hcclalgo_foramt_invalid_4)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("HCCL_algorithm","level0:;level1:H-D_R;level2:fullmesh;level3:ring"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_NE(ret, ge::SUCCESS);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_algo_config_hcclalgo_foramt_invalid_5)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("HCCL_algorithm","level0:8pring;level1:H-D_R;level2:fullmesh;level3:ring"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_NE(ret, ge::SUCCESS);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_algo_config_hcclalgo_foramt_invalid_6)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("HCCL_algorithm","level0:ring;level1:H-D_R;level2:4pmesh;level3:ring"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_NE(ret, ge::SUCCESS);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_algo_config_hcclalgo_foramt_invalid_7)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("HCCL_algorithm","level0:ring;level1:H-D;level2:fullmesh;level3:ring"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_NE(ret, ge::SUCCESS);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_algo_config_hcclalgo_foramt_invalid_8)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    // 未设置 rank table：失败
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    // 实验室场景 设置 rank id
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    // hccl 算法配置
    options.insert(pair<string,string> ("HCCL_algorithm","level0:ring;level0:ring"));

    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_NE(ret, ge::SUCCESS);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_initialize_by_comm_pytorch)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    ret = Initialize(options);
    EXPECT_EQ(ret, ge::SUCCESS);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_HcomPluginFinalize)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"rank_table.json"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));
 
    MOCKER(HcomInitByFile)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
   // EXPECT_NE(ret, ge::SUCCESS);

    g_hvdMode = true;
    MOCKER(HcclCommDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_E_PARA));

    MOCKER_CPP(&HvdAdapter::HvdAdapterDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_E_PARA));

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);
    g_hvdMode = false;

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_HcomSetRankTable)
{
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
                        {"device_id", "1"}
                    }
                }}
            },
            {
                {"node_addr", "192.168.2.101"},
                {"ranks", {
                    {
                        {"rank_id", "2"},
                        {"device_id", "2"},
                    }
                }}
            },
            {
                {"node_addr", "192.168.3.101"},
                {"ranks", {
                    {
                        {"rank_id", "3"},
                        {"device_id", "3"},
                    }
                }}
            }
        }
        }
    };
    std::string rank_table_string = rank_table.dump();
    HcclResult ret = HcomSetRankTable(rank_table_string.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u32 rankSize = 0;
    ret = HcomGetActualRankSize(nullptr, &rankSize);
    HCCL_INFO("rank size[%d]", rankSize);
    EXPECT_EQ(rankSize, 4);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
TEST_F(HcomPluginTest, ut_hcom_plugin_masterinfo)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;

    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_CM_CHIEF_IP,"127.0.0.1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_CM_CHIEF_PORT,"6000"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_CM_WORKER_SIZE,"2"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_CM_CHIEF_DEVICE,"0"));
    MOCKER(HcomInitByMasterInfo)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    // 实验室场景 hcom_init成功：成功
    ret = Initialize(options);
    EXPECT_EQ(ret, ge::SUCCESS);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_congif_deterministic_0)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;
    options.insert(pair<string,string> (ge::DETERMINISTIC,"0"));

    ret = Initialize(options);
    EXPECT_EQ(ret, ge::SUCCESS);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_congif_deterministic_1)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;
    options.insert(pair<string,string> (ge::DETERMINISTIC,"1"));

    ret = Initialize(options);
    EXPECT_EQ(ret, ge::SUCCESS);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_congif_deterministic_2)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;
    options.insert(pair<string,string> (ge::DETERMINISTIC,"2"));

    ret = Initialize(options);
    EXPECT_EQ(ret, ge::SUCCESS);

    MOCKER(HcomDestroy).expects(atMost(1)).will(returnValue(HCCL_SUCCESS));
    ret = Finalize();
    EXPECT_EQ(ret, ge::SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_congif_deterministic_empty)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;
    options.insert(pair<string,string> (ge::DETERMINISTIC,""));

    ret = Initialize(options);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();

    GlobalMockObject::verify();
}

TEST_F(HcomPluginTest, ut_hcom_plugin_congif_deterministic)
{
    ge ::Status ret;
    std::map<std::string,std::string> options;
    options.insert(pair<string,string> (ge::DETERMINISTIC,"1"));
    setenv("HCCL_DETERMINISTIC", "TRUE", 1);

    ret = Initialize(options);

    MOCKER(HcomDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));
    ret = Finalize();

    GlobalMockObject::verify();
    unsetenv("HCCL_DETERMINISTIC");
}

/**
 * @tc.name  : GetOpsKernelInfoPtr_ShouldReturnValidPtr_WhenPtrIsNotNull
 * @tc.number: HcomPluginTest_003
 * @tc.desc  : Test when hcomOpsKernelInfoStoreInfoPtr_ is not null, GetOpsKernelInfoPtr should return it
 */
TEST_F(HcomPluginTest, GetOpsKernelInfoPtr_ShouldReturnValidPtr_WhenPtrIsNotNull)
{
    HcomPlugin *plugin_;
    plugin_ = &HcomPlugin::Instance();
    plugin_->hcomOpsKernelInfoStoreInfoPtr_ = std::make_shared<HcomOpsKernelInfoStore>();
    HcomOpsKernelInfoStorePtr opsKernelInfoStorePtr;
    plugin_->GetOpsKernelInfoPtr(opsKernelInfoStorePtr);
    
    // 检查返回的指针是否有效
    EXPECT_NE(opsKernelInfoStorePtr, nullptr);

    GlobalMockObject::verify();
}

/**
 * @tc.name  : GetOpsKernelInfoPtr_ShouldReturnNull_WhenPtrIsNull
 * @tc.number: HcomPluginTest_004
 * @tc.desc  : Test when hcomOpsKernelInfoStoreInfoPtr_ is null, GetOpsKernelInfoPtr should return null
 */
TEST_F(HcomPluginTest, GetOpsKernelInfoPtr_ShouldReturnNull_WhenPtrIsNull)
{
    HcomPlugin *plugin_;
    plugin_ = &HcomPlugin::Instance();
    plugin_->hcomOpsKernelInfoStoreInfoPtr_ = std::make_shared<HcomOpsKernelInfoStore>();

    // 设置hcomOpsKernelInfoStoreInfoPtr_为null
    plugin_->hcomOpsKernelInfoStoreInfoPtr_ = nullptr;
    
    HcomOpsKernelInfoStorePtr opsKernelInfoStorePtr;
    plugin_->GetOpsKernelInfoPtr(opsKernelInfoStorePtr);
    
    // 检查返回的指针是否为null
    EXPECT_EQ(opsKernelInfoStorePtr, nullptr);

    GlobalMockObject::verify();
}