#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#define private public
#define protected public
#include "hcom_all_reduce_fusion.h"
#include "hvd_all_reduce_fusion.h"
#include "hcom_broadcast_fusion.h"
#include "hcom_reduce_fusion.h"
#include "hcom_alltoallvc_fusion.h"
#include "hcom_allgather_fusion.h"
#include "hcom_reducescatter_fusion.h"
#include "hvd_graph_optimizer.h"
#include "hcom_graph_optimizer.h"
#include "hcom_fusion_optimizer.h"
#include "offline_build_config_parse.h"

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "v80_rank_table.h"
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

#include "plugin_manager.h"
#include "hcom_ops_kernel_info_store.h"
#include "external/ge/ge_api_types.h" // ge对内options
#include "framework/common/ge_types.h" // ge对外options
#include "graph/ge_local_context.h"
#include "hcom_pub.h"
#include "hccl_communicator.h"

#include "graph/debug/ge_attr_define.h"
#include "graph/utils/graph_utils.h"
#include "graph/utils/node_utils.h"
#include "graph/utils/attr_utils.h"
#include "graph/node.h"
#include "graph/ge_tensor.h"
#include "graph/ge_context.h"
#include <iostream>
#include <fstream>
#include <hccl/hccl.h>
#include "hcom_graph_mc2.h"
#include "adapter_rts.h"
#include "env_config.h"

#undef private
#undef protected

using namespace std;
using namespace hccl;

class HcomGraphMc2Test : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "HcomGraphMc2Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {

        std::cout << "HcomGraphMc2Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
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

TEST_F(HcomGraphMc2Test, st_mc2_creatComResourceErr)
{
    std::vector<void *> commContext{};
    ge::graphStatus ge_ret;
    HcclRootInfo id;
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(newcomm);
 
    const ge::OpDescPtr opDescPtr = std::make_shared<ge::OpDesc>();
    opDescPtr->SetType("MatmulAllReduce");
    ge::AttrUtils::SetStr(opDescPtr, "group", hcclComm->GetIdentifier());
 
    ge_ret = hccl::HcomCreateComResource(opDescPtr, commContext);
    EXPECT_EQ(ge_ret, ge::GRAPH_FAILED);
 
    shared_ptr<std::vector<void *>> rt_resource_list = std::make_shared<std::vector<void *>>();
    opDescPtr->SetExtAttr("_rt_resource_list", rt_resource_list);
    ge_ret = hccl::HcomCreateComResource(opDescPtr, commContext);
    EXPECT_EQ(ge_ret, ge::GRAPH_FAILED);
}

TEST_F(HcomGraphMc2Test, st_mc2_creatComResource)
{
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::HcclGetCmdTimeout)
    .stubs()
    .will(returnValue(50));

    std::vector<void *> commContext{};
    ge::graphStatus ge_ret;
    rtStream_t stream;
    rtNotify_t notify;
    HcclRootInfo id;
    s32 devicePhyId = 0;
    rtError_t rt_ret = RT_ERROR_NONE;
    HcclResult ret = hrtGetDevice(&devicePhyId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(newcomm);
 
    ge::OpDescPtr opDescPtr = std::make_shared<ge::OpDesc>();
    opDescPtr->SetType("MatmulAllReduce");
    ge::AttrUtils::SetStr(opDescPtr, "group", hcclComm->GetIdentifier());
 
    shared_ptr<std::vector<void *>> rt_resource_list = std::make_shared<std::vector<void *>>();
 
    rt_ret = rtStreamCreate(&stream, 5);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    ret = hrtNotifyCreate(0, &notify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    rt_resource_list->push_back((void *)stream);
    opDescPtr->SetExtAttr("_rt_resource_list", rt_resource_list);
 
    ge_ret = hccl::HcomCreateComResource(opDescPtr, commContext);
    EXPECT_EQ(ge_ret, ge::GRAPH_SUCCESS);
 
    rt_ret = rtStreamDestroy(stream);
    EXPECT_EQ(ret, RT_ERROR_NONE);
    ret = hrtNotifyDestroy(notify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}
