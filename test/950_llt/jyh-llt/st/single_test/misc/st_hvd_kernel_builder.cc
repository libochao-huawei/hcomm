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

#include "externalinput_pub.h"
#include "plugin_manager.h"
#include "external/ge/ge_api_types.h" // ge对内options
#include "framework/common/ge_types.h" // ge对外options
#include "v80_rank_table.h"
#include "hcom_ops_kernel_builder.h"
#include "hvd_ops_kernel_builder.h"
#include <iostream>
#include <fstream>
#include "opexecounter_pub.h"

using namespace std;
using namespace hccl;

class HvdKernelBuilderTest : public testing::Test
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


TEST_F(HvdKernelBuilderTest, st_HVDgenerateTask_Broadcast)
{
    ge::NodePtr nodeptr(new NodeTest);
    ge::Buffer tempBuffer;
    ge::RunContext runContext_dummy;
    hccl::HvdOpsKernelBuilder hvdKernelInfo;
    std::vector<domi::TaskDef> taskDefList;

    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s64 streamId = 10000;
    nodeptr->GetOpDesc()->SetStreamId((s64)streamId);

	std::string name = "HvdTag";
	nodeptr->GetOpDesc()->SetName(name);

    std::string type = HVD_KERNEL_OP_TYPE_BROADCAST;
    nodeptr->GetOpDesc()->SetType(type);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_EVENTID");

    std::string eventID = "event5";
    ge::AttrUtils::SetStr(nodeptr->GetOpDesc(), "event_id", eventID);
    s32 ret = hvdKernelInfo.GenerateTask(*nodeptr,runContext_dummy,taskDefList);
    EXPECT_EQ(ret, ge::SUCCESS);
    u32 result_type = taskDefList[0].type();
    u32 result_stream_id = taskDefList[0].stream_id();
    std::string result_hccl_hccl_type = taskDefList[0].mutable_kernel_hccl()->hccl_type();
    std::string result_private_def = taskDefList[0].private_def();
    char private_def_buf[100];
    sal_memcpy(&private_def_buf[0],sizeof(private_def_buf),result_private_def.c_str(),result_private_def.length());
    HvdKernelInfoPrivateDef *privateDefBuf = (HvdKernelInfoPrivateDef *)&private_def_buf;
    u32 eventid = (privateDefBuf->eventID);
    EXPECT_EQ(result_type, RT_MODEL_TASK_HCCL);
    EXPECT_EQ(result_stream_id, streamId);
    EXPECT_EQ(result_hccl_hccl_type, type);

    EXPECT_EQ(eventid, 5);
}



TEST_F(HvdKernelBuilderTest, st_HVDgenerateTask_AllReduce)
{
    ge::NodePtr nodeptr(new NodeTest);
    ge::Buffer tempBuffer;
    ge::RunContext runContext_dummy;
    hccl::HvdOpsKernelBuilder hvdKernelInfo;
    std::vector<domi::TaskDef> taskDefList;

    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s64 streamId = 10000;
    nodeptr->GetOpDesc()->SetStreamId((s64)streamId);

	std::string name = "HvdTag";
	nodeptr->GetOpDesc()->SetName(name);

    std::string type = HVD_KERNEL_OP_TYPE_ALLREDUCE;
    nodeptr->GetOpDesc()->SetType(type);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_EVENTID");
    // std::string tempStr = HCCL_WORLD_GROUP;
    // ge::AttrUtils::SetStr(nodeptr->GetOpDesc(), "group", tempStr);
    // ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_SRCRANK");
    // ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_DESTRANK");
    // ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_SRTAG");
    // ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_TAG");
    std::string eventID = "event7";
    ge::AttrUtils::SetStr(nodeptr->GetOpDesc(), "event_id", eventID);
    s32 ret = hvdKernelInfo.GenerateTask(*nodeptr,runContext_dummy,taskDefList);
    EXPECT_EQ(ret, ge::SUCCESS);
    u32 result_type = taskDefList[0].type();
    u32 result_stream_id = taskDefList[0].stream_id();
    std::string result_hccl_hccl_type = taskDefList[0].mutable_kernel_hccl()->hccl_type();
    std::string result_private_def = taskDefList[0].private_def();
    char private_def_buf[100];
    sal_memcpy(&private_def_buf[0],sizeof(private_def_buf),result_private_def.c_str(),result_private_def.length());
    HvdKernelInfoPrivateDef *privateDefBuf = (HvdKernelInfoPrivateDef *)&private_def_buf;

    u32 eventid = (privateDefBuf->eventID);
    EXPECT_EQ(result_type, RT_MODEL_TASK_HCCL);
    EXPECT_EQ(result_stream_id, streamId);
    EXPECT_EQ(result_hccl_hccl_type, type);
    EXPECT_EQ(eventid, 7);  

}

TEST_F(HvdKernelBuilderTest, st_HVDgenerateTask_AllGather)
{
    ge::NodePtr nodeptr(new NodeTest);
    ge::Buffer tempBuffer;
    ge::RunContext runContext_dummy;
    hccl::HvdOpsKernelBuilder hvdKernelInfo;
    std::vector<domi::TaskDef> taskDefList;

    rtError_t rt_ret = RT_ERROR_NONE;
    rtStream_t stream;
    s64 streamId = 10000;
    nodeptr->GetOpDesc()->SetStreamId((s64)streamId);

	std::string name = "HvdTag";
	nodeptr->GetOpDesc()->SetName(name);

    std::string type = HVD_KERNEL_OP_TYPE_ALLGATHER;
    nodeptr->GetOpDesc()->SetType(type);
    ge::AttrUtils::HasAttr(nodeptr->GetOpDesc(), "DUMMY_SET_TRUE_EVENTID");
    std::string eventID = "event8";
    ge::AttrUtils::SetStr(nodeptr->GetOpDesc(), "event_id", eventID);
    s32 ret = hvdKernelInfo.GenerateTask(*nodeptr,runContext_dummy,taskDefList);
    EXPECT_EQ(ret, ge::SUCCESS);

    u32 result_type = taskDefList[0].type();
    u32 result_stream_id = taskDefList[0].stream_id();
    std::string result_hccl_hccl_type = taskDefList[0].mutable_kernel_hccl()->hccl_type();
    std::string result_private_def = taskDefList[0].private_def();
    char private_def_buf[sizeof(HCCL_KERNEL_INFO_PRIVATE_DEF)];
    sal_memcpy(&private_def_buf[0],sizeof(private_def_buf),result_private_def.c_str(),result_private_def.length());
    HvdKernelInfoPrivateDef *privateDefBuf = (HvdKernelInfoPrivateDef *)&private_def_buf;
    u32 eventid = (privateDefBuf->eventID);
    EXPECT_EQ(result_type, RT_MODEL_TASK_HCCL);
    EXPECT_EQ(result_stream_id, streamId);
    EXPECT_EQ(result_hccl_hccl_type, type);
    EXPECT_EQ(eventid, 8);
}
