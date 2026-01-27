#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>

#include "stream_pub.h"
#include "mem_host_pub.h"
#include "mem_device_pub.h"
#include "hccl_comm_pub.h"
#include "sal.h"
#include "hccl_impl.h"
#include "llt_hccl_stub_pub.h"
#include "config.h"
#include "topoinfo_ranktableParser_pub.h"
#include "topoinfo_ranktableHeterog.h"
#include "externalinput_pub.h"
#include "plugin_manager.h"
#include "hcom_plugin.h"
#include "external/ge/ge_api_types.h" // ge对内options
#include "framework/common/ge_types.h" // ge对外options
#include "v80_rank_table.h"
#include "hcom_ops_kernel_builder.h"
#include "hccl/hcom_executor.h"
#include <iostream>
#include <fstream>
#include "opexecounter_pub.h"
#include "offline_build_config_parse.h"
#include "env_config.h"

using namespace std;
using namespace hccl;

class HcomKernelBuilderTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {

        std::cout << "HcclKernelBuilderTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "HcclKernelBuilderTest TearDown" << std::endl;
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
        InitEnvParam();
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};


class NodeTest : public ge::Node {
public:
    NodeTest(){;};
    ~NodeTest(){;};    
};


#define HCCL_COM_DATA_SIZE 1024

TEST_F(HcomKernelBuilderTest, st_GetRanktableInfo)
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

TEST_F(HcomKernelBuilderTest, st_generateTask_Broadcast)
{
    ge::NodePtr nodeptr(new NodeTest);
    ge::Buffer tempBuffer;
    ge::RunContext runContext_dummy;
    hccl::HcomOpsKernelBuilder hcomKernelInfo;
    std::vector<domi::TaskDef> taskDefList;

    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s64 streamId = 10000;
    nodeptr->GetOpDesc()->SetStreamId((s64)streamId);

	std::string name = "HcomTag";
	nodeptr->GetOpDesc()->SetName(name);

    std::string type = HCCL_KERNEL_OP_TYPE_BROADCAST;
    nodeptr->GetOpDesc()->SetType(type);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_GROUP");
    std::string tempStr = HCCL_WORLD_GROUP;
    ge::AttrUtils::SetStr(nodeptr->GetOpDesc(), "group", tempStr);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_SRCRANK");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_DESTRANK");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_SRTAG");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_FALSE_TAG");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_FISSION");
    s64 tempInt = 5;
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "dest_rank", tempInt);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "src_rank", tempInt);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "sr_tag", tempInt);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "_fission_factor", 1);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_NEEDMAPRANK");
    ge::AttrUtils::SetBool(nodeptr->GetOpDesc(), "_need_map_rank_id", true);
    s32 ret = hcomKernelInfo.GenerateTask(*nodeptr,runContext_dummy,taskDefList);

    u32 result_type = taskDefList[0].type();
    u32 result_stream_id = taskDefList[0].stream_id();
    std::string result_hccl_hccl_type = taskDefList[0].mutable_kernel_hccl()->hccl_type();
    std::string result_private_def = taskDefList[0].private_def();
    char private_def_buf[sizeof(HCCL_KERNEL_INFO_PRIVATE_DEF)];
    sal_memcpy(&private_def_buf[0],sizeof(private_def_buf),result_private_def.c_str(),sizeof(private_def_buf));
    HCCL_KERNEL_INFO_PRIVATE_DEF *privateDefBuf = (HCCL_KERNEL_INFO_PRIVATE_DEF *)&private_def_buf[0];
    std::string result_group = reinterpret_cast<const char*>(privateDefBuf->group);
    // std::string result_tag = reinterpret_cast<const char*>(privateDefBuf->tag);
    u32 result_srcRank = (privateDefBuf->srcRank);
    u32 result_destRank = (privateDefBuf->destRank);
    u32 result_srTag = (privateDefBuf->srTag);
    EXPECT_EQ(result_type, RT_MODEL_TASK_HCCL);
    EXPECT_EQ(result_stream_id, streamId);
    EXPECT_EQ(result_hccl_hccl_type, type);
    EXPECT_EQ(result_group, tempStr);
    // EXPECT_EQ(result_tag, name);
    EXPECT_EQ(result_srcRank, 0);
    EXPECT_EQ(result_destRank, 0);
    EXPECT_EQ(result_srTag, 0);

}

TEST_F(HcomKernelBuilderTest, st_generateTask_AllReduce)
{
    ge::NodePtr nodeptr(new NodeTest);
    ge::Buffer tempBuffer;
    ge::RunContext runContext_dummy;
    hccl::HcomOpsKernelBuilder hcomKernelInfo;
    std::vector<domi::TaskDef> taskDefList;

    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s64 streamId = 10000;
    nodeptr->GetOpDesc()->SetStreamId((s64)streamId);

	std::string name = "HcomTag";
	nodeptr->GetOpDesc()->SetName(name);

    std::string type = HCCL_KERNEL_OP_TYPE_ALLREDUCE;
    nodeptr->GetOpDesc()->SetType(type);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_FALSE_GROUP");
    std::string tempStr = HCCL_WORLD_GROUP;
    ge::AttrUtils::SetStr(nodeptr->GetOpDesc(), "group", tempStr);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_SRCRANK");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_DESTRANK");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_SRTAG");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_TAG");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_FISSION");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_DUMPSIZE");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_DUMPTYPE");
    s64 tempInt = 5;
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "dest_rank", tempInt);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "src_rank", tempInt);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "sr_tag", tempInt);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "_fission_factor", 1);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "global_workspace_size", 1);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "global_workspace_type", 0);
    s32 ret = hcomKernelInfo.GenerateTask(*nodeptr,runContext_dummy,taskDefList);
    EXPECT_EQ(ret, ge::SUCCESS);
    MOCKER(IsOfflineCompilation)
    .stubs()
    .with(any())
    .will(returnValue(true));
    ret = hcomKernelInfo.GenerateTask(*nodeptr,runContext_dummy,taskDefList);
    EXPECT_EQ(ret, ge::SUCCESS);

    u32 result_type = taskDefList[0].type();
    u32 result_stream_id = taskDefList[0].stream_id();
    std::string result_hccl_hccl_type = taskDefList[0].mutable_kernel_hccl()->hccl_type();
    std::string result_private_def = taskDefList[0].private_def();
    char private_def_buf[sizeof(HCCL_KERNEL_INFO_PRIVATE_DEF)];
    sal_memcpy(&private_def_buf[0],sizeof(private_def_buf),result_private_def.c_str(),sizeof(private_def_buf));
    HCCL_KERNEL_INFO_PRIVATE_DEF *privateDefBuf = (HCCL_KERNEL_INFO_PRIVATE_DEF *)&private_def_buf[0];
    std::string result_group = reinterpret_cast<const char*>(privateDefBuf->group);
    // std::string result_tag = reinterpret_cast<const char*>(privateDefBuf->tag);
    u32 result_srcRank = (privateDefBuf->srcRank);
    u32 result_destRank = (privateDefBuf->destRank);
    u32 result_srTag = (privateDefBuf->srTag);
    EXPECT_EQ(result_type, RT_MODEL_TASK_HCCL);
    EXPECT_EQ(result_stream_id, streamId);
    EXPECT_EQ(result_hccl_hccl_type, type);
    EXPECT_EQ(result_group, tempStr);
    // EXPECT_EQ(result_tag, name);
    EXPECT_EQ(result_srcRank, 0);
    EXPECT_EQ(result_destRank, 0);
    EXPECT_EQ(result_srTag, 0);

}

TEST_F(HcomKernelBuilderTest, st_generateTask_AllReduce_by_comm_pytorch)
{
    ge::NodePtr nodeptr(new NodeTest);
    ge::Buffer tempBuffer;
    ge::RunContext runContext_dummy;
    hccl::HcomOpsKernelBuilder hcomKernelInfo;
    std::vector<domi::TaskDef> taskDefList;

    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s64 streamId = 10000;
    nodeptr->GetOpDesc()->SetStreamId((s64)streamId);

	std::string name = "HcomTag";
	nodeptr->GetOpDesc()->SetName(name);

    std::string type = HCCL_KERNEL_OP_TYPE_ALLREDUCE;
    nodeptr->GetOpDesc()->SetType(type);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_COMM");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "comm");
    int64_t hcomComm = 438753439;
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "comm", hcomComm);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_SRCRANK");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_DESTRANK");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_SRTAG");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_TAG");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_FISSION");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_DUMPSIZE");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_DUMPTYPE");
    s64 tempInt = 5;
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "dest_rank", tempInt);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "src_rank", tempInt);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "sr_tag", tempInt);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "_fission_factor", 1);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "global_workspace_size", 1);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "global_workspace_type", 0);
    s32 ret = hcomKernelInfo.GenerateTask(*nodeptr,runContext_dummy,taskDefList);
    EXPECT_EQ(ret, ge::SUCCESS);

    u32 result_type = taskDefList[0].type();
    u32 result_stream_id = taskDefList[0].stream_id();
    std::string result_hccl_hccl_type = taskDefList[0].mutable_kernel_hccl()->hccl_type();
    std::string result_private_def = taskDefList[0].private_def();
    char private_def_buf[sizeof(HCCL_KERNEL_INFO_PRIVATE_DEF)];
    sal_memcpy(&private_def_buf[0],sizeof(private_def_buf),result_private_def.c_str(),sizeof(private_def_buf));
    HCCL_KERNEL_INFO_PRIVATE_DEF *privateDefBuf = (HCCL_KERNEL_INFO_PRIVATE_DEF *)&private_def_buf[0];
    int64_t result_comm = (privateDefBuf->comm);
    u32 result_srcRank = (privateDefBuf->srcRank);
    u32 result_destRank = (privateDefBuf->destRank);
    u32 result_srTag = (privateDefBuf->srTag);
    EXPECT_EQ(result_type, RT_MODEL_TASK_HCCL);
    EXPECT_EQ(result_stream_id, streamId);
    EXPECT_EQ(result_hccl_hccl_type, type);
    EXPECT_EQ(result_comm, hcomComm);
    EXPECT_EQ(result_srcRank, 0);
    EXPECT_EQ(result_destRank, 0);
    EXPECT_EQ(result_srTag, 0);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_FALSE_COMM");
}

TEST_F(HcomKernelBuilderTest, st_generateTask_ReduceScatter)
{
    ge::NodePtr nodeptr(new NodeTest);
    ge::Buffer tempBuffer;
    ge::RunContext runContext_dummy;
    hccl::HcomOpsKernelBuilder hcomKernelInfo;
    std::vector<domi::TaskDef> taskDefList;

    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s64 streamId = 10000;
    nodeptr->GetOpDesc()->SetStreamId((s64)streamId);

	std::string name = "HcomTag";
	nodeptr->GetOpDesc()->SetName(name);

    std::string type = HCCL_KERNEL_OP_TYPE_REDUCESCATTER;
    nodeptr->GetOpDesc()->SetType(type);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_GROUP");
    std::string tempStr = HCCL_WORLD_GROUP;
    ge::AttrUtils::SetStr(nodeptr->GetOpDesc(), "group", tempStr);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_SRCRANK");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_DESTRANK");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_SRTAG");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_TAG");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_FISSION");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_DUMPSIZE");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_DUMPTYPE");
    s64 tempInt = 5;
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "dest_rank", tempInt);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "src_rank", tempInt);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "sr_tag", tempInt);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "_fission_factor", 1);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "global_workspace_size", 1);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "global_workspace_type", 0);
    s32 ret = hcomKernelInfo.GenerateTask(*nodeptr,runContext_dummy,taskDefList);
    EXPECT_EQ(ret, ge::SUCCESS);

    u32 result_type = taskDefList[0].type();
    u32 result_stream_id = taskDefList[0].stream_id();
    std::string result_hccl_hccl_type = taskDefList[0].mutable_kernel_hccl()->hccl_type();
    std::string result_private_def = taskDefList[0].private_def();
    char private_def_buf[sizeof(HCCL_KERNEL_INFO_PRIVATE_DEF)];
    sal_memcpy(&private_def_buf[0],sizeof(private_def_buf),result_private_def.c_str(),sizeof(private_def_buf));
    HCCL_KERNEL_INFO_PRIVATE_DEF *privateDefBuf = (HCCL_KERNEL_INFO_PRIVATE_DEF *)&private_def_buf[0];
    std::string result_group = reinterpret_cast<const char*>(privateDefBuf->group);
    // std::string result_tag = reinterpret_cast<const char*>(privateDefBuf->tag);
    u32 result_srcRank = (privateDefBuf->srcRank);
    u32 result_destRank = (privateDefBuf->destRank);
    u32 result_srTag = (privateDefBuf->srTag);
    EXPECT_EQ(result_type, RT_MODEL_TASK_HCCL);
    EXPECT_EQ(result_stream_id, streamId);
    EXPECT_EQ(result_hccl_hccl_type, type);
    EXPECT_EQ(result_group, tempStr);
    // EXPECT_EQ(result_tag, name);
    EXPECT_EQ(result_srcRank, 0);
    EXPECT_EQ(result_destRank, 0);
    EXPECT_EQ(result_srTag, 0);

}

TEST_F(HcomKernelBuilderTest, st_generateTask_AllGather)
{
    ge::NodePtr nodeptr(new NodeTest);
    ge::Buffer tempBuffer;
    ge::RunContext runContext_dummy;
    hccl::HcomOpsKernelBuilder hcomKernelInfo;
    std::vector<domi::TaskDef> taskDefList;

    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s64 streamId = 10000;
    nodeptr->GetOpDesc()->SetStreamId((s64)streamId);

	std::string name = "HcomTag";
	nodeptr->GetOpDesc()->SetName(name);

    std::string type = HCCL_KERNEL_OP_TYPE_ALLGATHER;
    nodeptr->GetOpDesc()->SetType(type);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_GROUP");
    std::string tempStr = HCCL_WORLD_GROUP;
    ge::AttrUtils::SetStr(nodeptr->GetOpDesc(), "group", tempStr);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_SRCRANK");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_DESTRANK");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_SRTAG");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_TAG");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_FISSION");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_DUMPSIZE");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_DUMPTYPE");
    s64 tempInt = 5;
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "dest_rank", tempInt);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "src_rank", tempInt);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "sr_tag", tempInt);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "_fission_factor", 1);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "global_workspace_size", 1);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "global_workspace_type", 0);
    s32 ret = hcomKernelInfo.GenerateTask(*nodeptr,runContext_dummy,taskDefList);
    EXPECT_EQ(ret, ge::SUCCESS);

    u32 result_type = taskDefList[0].type();
    u32 result_stream_id = taskDefList[0].stream_id();
    std::string result_hccl_hccl_type = taskDefList[0].mutable_kernel_hccl()->hccl_type();
    std::string result_private_def = taskDefList[0].private_def();
    char private_def_buf[sizeof(HCCL_KERNEL_INFO_PRIVATE_DEF)];
    sal_memcpy(&private_def_buf[0],sizeof(private_def_buf),result_private_def.c_str(),sizeof(private_def_buf));
    HCCL_KERNEL_INFO_PRIVATE_DEF *privateDefBuf = (HCCL_KERNEL_INFO_PRIVATE_DEF *)&private_def_buf[0];
    std::string result_group = reinterpret_cast<const char*>(privateDefBuf->group);
    // std::string result_tag = reinterpret_cast<const char*>(privateDefBuf->tag);
    u32 result_srcRank = (privateDefBuf->srcRank);
    u32 result_destRank = (privateDefBuf->destRank);
    u32 result_srTag = (privateDefBuf->srTag);
    EXPECT_EQ(result_type, RT_MODEL_TASK_HCCL);
    EXPECT_EQ(result_stream_id, streamId);
    EXPECT_EQ(result_hccl_hccl_type, type);
    EXPECT_EQ(result_group, tempStr);
    // EXPECT_EQ(result_tag, name);
    EXPECT_EQ(result_srcRank, 0);
    EXPECT_EQ(result_destRank, 0);
    EXPECT_EQ(result_srTag, 0);

}

TEST_F(HcomKernelBuilderTest, st_generateTask_Send)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    g_hvdMode = false;
    char file_name_t[] = "./st_generateTask_Send.json";
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

    ge ::Status ge_ret;
    std::map<std::string,std::string> options;
    
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"./st_generateTask_Send.json"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));
    ge_ret = Initialize(options);
    EXPECT_EQ(ge_ret, ge::SUCCESS);

    int ret = HCCL_SUCCESS;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors=0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_generateTask_Send.json";
    char* rank_ID = "0";

    ge::NodePtr nodeptr(new NodeTest);
    ge::Buffer tempBuffer;
    ge::RunContext runContext_dummy;
    hccl::HcomOpsKernelBuilder hcomKernelInfo;
    std::vector<domi::TaskDef> taskDefList;

    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s64 streamId = 10000;
    nodeptr->GetOpDesc()->SetStreamId((s64)streamId);

    std::string type = HCCL_KERNEL_OP_TYPE_SEND;
    nodeptr->GetOpDesc()->SetType(type);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_GROUP");
    std::string tempStr = HCCL_WORLD_GROUP;
    ge::AttrUtils::SetStr(nodeptr->GetOpDesc(), "group", tempStr);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_SRCRANK");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_DESTRANK");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_SRTAG");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_FALSE_TAG");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_FISSION");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_DUMPSIZE");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_DUMPTYPE");
    s64 tempInt = 5;
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "dest_rank", tempInt);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "src_rank", tempInt);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "sr_tag", tempInt);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "_fission_factor", 1);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "global_workspace_size", 1);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "global_workspace_type", 0);
    ge_ret = hcomKernelInfo.GenerateTask(*nodeptr,runContext_dummy,taskDefList);
    EXPECT_EQ(ge_ret, ge::SUCCESS);

    u32 result_type = taskDefList[0].type();
    u32 result_stream_id = taskDefList[0].stream_id();
    std::string result_hccl_hccl_type = taskDefList[0].mutable_kernel_hccl()->hccl_type();
    std::string result_private_def = taskDefList[0].private_def();
    char private_def_buf[sizeof(HCCL_KERNEL_INFO_PRIVATE_DEF)];
    sal_memcpy(&private_def_buf[0],sizeof(private_def_buf),result_private_def.c_str(),sizeof(private_def_buf));
    HCCL_KERNEL_INFO_PRIVATE_DEF *privateDefBuf = (HCCL_KERNEL_INFO_PRIVATE_DEF *)&private_def_buf[0];
    std::string result_group = reinterpret_cast<const char*>(privateDefBuf->group);
    // std::string result_tag = reinterpret_cast<const char*>(privateDefBuf->tag);
    u32 result_srcRank = (privateDefBuf->srcRank);
    u32 result_destRank = (privateDefBuf->destRank);
    u32 result_srTag = (privateDefBuf->srTag);
    EXPECT_EQ(result_type, RT_MODEL_TASK_HCCL);
    EXPECT_EQ(result_stream_id, streamId);
    EXPECT_EQ(result_hccl_hccl_type, type);
    EXPECT_EQ(result_group, tempStr);
    std::string tmpTag = result_group+"5"+"0"+"5";
    // EXPECT_EQ(result_tag, tmpTag);
    EXPECT_EQ(result_srcRank, 0);
    EXPECT_EQ(result_destRank, tempInt);
    EXPECT_EQ(result_srTag, tempInt);

    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_FALSE_DESTRANK");
    ge_ret = hcomKernelInfo.GenerateTask(*nodeptr,runContext_dummy,taskDefList);
    EXPECT_EQ(ge_ret, ge::INTERNAL_ERROR);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_DESTRANK");

    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_FALSE_SRTAG");
    ge_ret = hcomKernelInfo.GenerateTask(*nodeptr,runContext_dummy,taskDefList);
    EXPECT_EQ(ge_ret, ge::INTERNAL_ERROR);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_SRTAG");

    ge_ret = Finalize();
    EXPECT_EQ(ge_ret, ge::SUCCESS);

    remove(file_name_t);
}

TEST_F(HcomKernelBuilderTest, st_generateTask_Receive)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_generateTask_Receive.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);
    g_hvdMode = false;
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

    ge ::Status ge_ret;
    std::map<std::string,std::string> options;
    
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"./st_generateTask_Receive.json"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,"0"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));
    ge_ret = Initialize(options);
    EXPECT_EQ(ge_ret, ge::SUCCESS);

    int ret = HCCL_SUCCESS;
    s8* sendbuf;
    s8* recvbuf;
    s32 rank = 0;
    s32 errors=0;
    s32 count = HCCL_COM_DATA_SIZE;
    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* rank_table_file = "./st_generateTask_Receive.json";
    char* rank_ID = "0";

    ge::NodePtr nodeptr(new NodeTest);
    ge::Buffer tempBuffer;
    ge::RunContext runContext_dummy;
    hccl::HcomOpsKernelBuilder hcomKernelInfo;
    std::vector<domi::TaskDef> taskDefList;

    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s64 streamId = 10000;
    nodeptr->GetOpDesc()->SetStreamId((s64)streamId);

    std::string type = HCCL_KERNEL_OP_TYPE_RECEIVE;
    nodeptr->GetOpDesc()->SetType(type);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_GROUP");
    std::string tempStr = HCCL_WORLD_GROUP;
    ge::AttrUtils::SetStr(nodeptr->GetOpDesc(), "group", tempStr);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_SRCRANK");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_DESTRANK");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_SRTAG");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_FALSE_TAG");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_FISSION"); 
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_DUMPSIZE");
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_DUMPTYPE");
    s64 tempInt = 5;
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "dest_rank", tempInt);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "src_rank", tempInt);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "sr_tag", tempInt);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "_fission_factor", 1);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "global_workspace_size", 1);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "global_workspace_type", 0);
    ge_ret = hcomKernelInfo.GenerateTask(*nodeptr,runContext_dummy,taskDefList);
    EXPECT_EQ(ge_ret, ge::SUCCESS);

    u32 result_type = taskDefList[0].type();
    u32 result_stream_id = taskDefList[0].stream_id();
    std::string result_hccl_hccl_type = taskDefList[0].mutable_kernel_hccl()->hccl_type();
    std::string result_private_def = taskDefList[0].private_def();
    char private_def_buf[sizeof(HCCL_KERNEL_INFO_PRIVATE_DEF)];
    sal_memcpy(&private_def_buf[0],sizeof(private_def_buf),result_private_def.c_str(),sizeof(private_def_buf));
    HCCL_KERNEL_INFO_PRIVATE_DEF *privateDefBuf = (HCCL_KERNEL_INFO_PRIVATE_DEF *)&private_def_buf[0];
    std::string result_group = reinterpret_cast<const char*>(privateDefBuf->group);
    // std::string result_tag = reinterpret_cast<const char*>(privateDefBuf->tag);
    u32 result_srcRank = (privateDefBuf->srcRank);
    u32 result_destRank = (privateDefBuf->destRank);
    u32 result_srTag = (privateDefBuf->srTag);
    EXPECT_EQ(result_type, RT_MODEL_TASK_HCCL);
    EXPECT_EQ(result_stream_id, streamId);
    EXPECT_EQ(result_hccl_hccl_type, type);
    EXPECT_EQ(result_group, tempStr);
    // std::string tmpTag = result_group+"5"+"5"+"0";
    // EXPECT_EQ(result_tag, tmpTag);
    EXPECT_EQ(result_srcRank, tempInt);
    EXPECT_EQ(result_destRank, 0);
    EXPECT_EQ(result_srTag, tempInt);

    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_FALSE_SRCRANK");
    ge_ret = hcomKernelInfo.GenerateTask(*nodeptr,runContext_dummy,taskDefList);
    EXPECT_EQ(ge_ret, ge::INTERNAL_ERROR);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_SRCRANK");

    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_FALSE_SRTAG");
    ge_ret = hcomKernelInfo.GenerateTask(*nodeptr,runContext_dummy,taskDefList);
    EXPECT_EQ(ge_ret, ge::INTERNAL_ERROR);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_SRTAG");

    ge_ret = Finalize();
    EXPECT_EQ(ge_ret, ge::SUCCESS);

    remove(file_name_t);
}

TEST_F(HcomKernelBuilderTest, st_GenerateTask_AlltoAllV)
{
    ge::NodePtr nodeptr(new NodeTest);
    HcomOpsKernelBuilder hcomKernelInfo;

    s64 streamId = 10000;
    nodeptr->GetOpDesc()->SetStreamId((s64)streamId);

    std::string name = "HcomTag";
    nodeptr->GetOpDesc()->SetName(name);

    nodeptr->GetOpDesc()->SetType(HCCL_KERNEL_OP_TYPE_ALLTOALLV);
    ge::AttrUtils::SetStr(nodeptr->GetOpDesc(), "group", HCCL_WORLD_GROUP);
    ge::AttrUtils::SetInt(nodeptr->GetOpDesc(), "sr_tag", 5);
    std::vector<int64_t> sendCounts;
    ge::AttrUtils::SetListInt(nodeptr->GetOpDesc(), "send_counts", sendCounts);

    ge::RunContext runContext;
    std::vector<domi::TaskDef> taskDefList;
    s32 ret = hcomKernelInfo.GenerateTask(*nodeptr, runContext, taskDefList);
    EXPECT_EQ(ret, ge::SUCCESS);
}

TEST_F(HcomKernelBuilderTest, st_HcomSetRankTable)
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
                        {"rank_id", "0"}
                    }
                }}
            },
            {
                {"node_addr", "192.168.1.101"},
                {"ranks", {
                    {
                        {"rank_id", "1"}
                    }
                }}
            },
            {
                {"node_addr", "192.168.2.101"},
                {"ranks", {
                    {
                        {"rank_id", "2"}
                    }
                }}
            },
            {
                {"node_addr", "192.168.3.101"},
                {"ranks", {
                    {
                        {"rank_id", "3"}
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