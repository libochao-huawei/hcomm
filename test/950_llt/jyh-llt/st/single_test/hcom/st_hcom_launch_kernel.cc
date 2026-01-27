#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <iostream>
#include <fstream>

#define private public
#include "hcom_launch_kernel.h"
#include "hcom_build_graph.h"
#include "hcom_node_converter.h"
#include "exe_graph/lowering/dev_mem_value_holder.h"
#include "hccl.h"
#include "llt_hccl_stub_gert.h"
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub_ge.h"
#include "mem_host_pub.h"
#include "mem_device_pub.h"
#include "v80_rank_table.h"
#include "hcom_pub.h"
#include "op_base.h"
#include "remote_access.h"
#include "hccl_comm_pub.h"
#include "workflow_pub.h"
#include "lowering_global_data.h"
#include "graph/debug/ge_attr_define.h"


#include <stdio.h>

#include "hccl_communicator.h"
#include "hcom_ops_kernel_info_store.h"
#include "hcom_ops_kernel_builder.h"
#include "hcom_graph_optimizer.h"

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_kernel_run_ctx_faker.h"
#include "ge/ge_allocator.h"

#include "hcom_plugin.h"
#include "env_config.h"
#undef private

using namespace std;
using namespace hccl;
namespace hccl {
extern HcclResult GetCountByShape(const gert::Shape &shape, HcclDataType dataType, uint64_t &count);
}

class HcomAllToAllKernelTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化默认的HcomOpLaunchArgs
        strcpy(launchArgs_.opAttr.group, "hccl_world_group");
        launchArgs_.opAttr.dataType = HCCL_DATA_TYPE_INT8;
        launchArgs_.stream = nullptr;
        launchArgs_.inputNum = 1;
        launchArgs_.outputNum = 1;
        launchArgs_.inputAddrs.push_back(reinterpret_cast<void*>(0x1000));
        launchArgs_.inputAddrs.push_back(reinterpret_cast<void*>(0x1000));
        launchArgs_.inputAddrs.push_back(reinterpret_cast<void*>(0x1000));
        launchArgs_.inputAddrs.push_back(reinterpret_cast<void*>(0x1000));
        launchArgs_.inputAddrs.push_back(reinterpret_cast<void*>(0x1000));
        launchArgs_.outputAddrs.push_back(reinterpret_cast<void*>(0x2000));
        launchArgs_.outputAddrs.push_back(reinterpret_cast<void*>(0x2000));
        launchArgs_.outputAddrs.push_back(reinterpret_cast<void*>(0x2000));
        launchArgs_.outputAddrs.push_back(reinterpret_cast<void*>(0x2000));
        launchArgs_.outputAddrs.push_back(reinterpret_cast<void*>(0x2000));
        // 假设inputShapes和outputShapes为有效的形状
        gert::Shape inputShape{2, 2};
        gert::Shape outputShape{2, 2};
        launchArgs_.inputShapes.push_back(inputShape);
        launchArgs_.outputShapes.push_back(outputShape);
    }

    void TearDown() override {
        // 清理资源
        launchArgs_.inputAddrs.clear();
        launchArgs_.outputAddrs.clear();
        launchArgs_.inputShapes.clear();
        launchArgs_.outputShapes.clear();
    }

    HcomOpLaunchArgs launchArgs_;
};

TEST_F(HcomAllToAllKernelTest, HcomAllToAllKernel_ShouldReturnError_WhenShapesMismatch)
{
    // 设置输入和输出形状不一致
    gert::Shape inputShape{2, 2};
    gert::Shape outputShape{3, 3};
    launchArgs_.inputShapes[0] = inputShape;
    launchArgs_.outputShapes[0] = outputShape;
    uint64_t count = 300 * 1024 * 1024;
    MOCKER(GetCountByShape).stubs().with(any(), any(), outBound(count)).will(returnValue(HCCL_SUCCESS));
    std::shared_ptr<hccl::hcclComm> comm;
    comm.reset(new (std::nothrow) hccl::hcclComm());
    MOCKER(HcomGetCommByGroup).stubs().with(any(), outBound(comm)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hcclComm::SetAivCoreLimit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclAlltoAll).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    HcomOpInputStruct inputStruct;
    HcclResult result = HcomAllToAllKernel(launchArgs_, &inputStruct);
    EXPECT_EQ(result, HCCL_SUCCESS);
    int addr = 10;
    std::vector<void*> Addrs;
    Addrs.push_back(&addr);
    result = HcomLaunchAllToAllKernel(&inputStruct, Addrs, Addrs);
    EXPECT_EQ(result, HCCL_SUCCESS);
}

TEST_F(HcomAllToAllKernelTest, HcomAllToAllVKernel_ShouldReturnError_WhenShapesMismatch)
{
    // 设置输入和输出形状不一致
    gert::Shape inputShape{2, 2};
    gert::Shape outputShape{3, 3};
    launchArgs_.inputShapes[0] = inputShape;
    launchArgs_.outputShapes[0] = outputShape;
    launchArgs_.opAttr.op.alltoallv.recvType = HCCL_DATA_TYPE_INT8;
    std::shared_ptr<hccl::hcclComm> comm;
    comm.reset(new (std::nothrow) hccl::hcclComm());
    MOCKER(HcomGetCommByGroup).stubs().with(any(), outBound(comm)).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclAlltoAllV).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    HcomOpInputStruct inputStruct;
    HcclResult result = HcomAllToAllVKernel(launchArgs_, &inputStruct);
    EXPECT_EQ(result, HCCL_SUCCESS);
    int addr = 10;
    std::vector<void*> Addrs;
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    result = HcomLaunchAllToAllVKernel(&inputStruct, Addrs, Addrs);
    EXPECT_EQ(result, HCCL_SUCCESS);
}

TEST_F(HcomAllToAllKernelTest, HcomAllToAllVCKernel_ShouldReturnError_WhenShapesMismatch)
{
    // 设置输入和输出形状不一致
    gert::Shape inputShape{2, 2};
    gert::Shape outputShape{3, 3};
    launchArgs_.inputShapes[0] = inputShape;
    launchArgs_.outputShapes[0] = outputShape;
    uint64_t count = 300 * 1024 * 1024;
    MOCKER(GetCountByShape).stubs().with(any(), any(), outBound(count)).will(returnValue(HCCL_SUCCESS));
    std::shared_ptr<hccl::hcclComm> comm;
    comm.reset(new (std::nothrow) hccl::hcclComm());
    MOCKER(HcomGetCommByGroup).stubs().with(any(), outBound(comm)).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclAlltoAllVC).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    HcomOpInputStruct inputStruct;
    HcclResult result = HcomAllToAllVCKernel(launchArgs_, &inputStruct);
    EXPECT_EQ(result, HCCL_SUCCESS);
    int addr = 10;
    std::vector<void*> Addrs;
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    result = HcomLaunchAllToAllVCKernel(&inputStruct, Addrs, Addrs);
    EXPECT_EQ(result, HCCL_SUCCESS);
}

TEST_F(HcomAllToAllKernelTest, HcomGatherAllToAllVKernel_ShouldReturnError_WhenShapesMismatch)
{
    // 设置输入和输出形状不一致
    gert::Shape inputShape{2, 2};
    gert::Shape outputShape{3, 3};
    launchArgs_.inputShapes[0] = inputShape;
    launchArgs_.outputShapes[0] = outputShape;
    uint64_t count = 300 * 1024 * 1024;
    MOCKER(GetCountByShape).stubs().with(any(), any(), outBound(count)).will(returnValue(HCCL_SUCCESS));
    std::shared_ptr<hccl::hcclComm> comm;
    comm.reset(new (std::nothrow) hccl::hcclComm());
    MOCKER(HcomGetCommByGroup).stubs().with(any(), outBound(comm)).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclGatherAlltoAllV).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    HcomOpInputStruct inputStruct;
    HcclResult result = HcomGatherAllToAllVKernel(launchArgs_, &inputStruct);
    EXPECT_EQ(result, HCCL_SUCCESS);
    int addr = 10;
    std::vector<void*> Addrs;
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    result = HcomLaunchGatherAllToAllVKernel(&inputStruct, Addrs, Addrs);
    EXPECT_EQ(result, HCCL_SUCCESS);
}

TEST_F(HcomAllToAllKernelTest, HcomAllGahterKernel_ShouldReturnError_WhenShapesMismatch)
{
    // 设置输入和输出形状不一致
    gert::Shape inputShape{2, 2};
    gert::Shape outputShape{3, 3};
    launchArgs_.inputShapes[0] = inputShape;
    launchArgs_.outputShapes[0] = outputShape;
    uint64_t count = 300 * 1024 * 1024;
    MOCKER(GetCountByShape).stubs().with(any(), any(), outBound(count)).will(returnValue(HCCL_SUCCESS));
    std::shared_ptr<hccl::hcclComm> comm;
    comm.reset(new (std::nothrow) hccl::hcclComm());
    MOCKER(HcomGetCommByGroup).stubs().with(any(), outBound(comm)).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclAllGather).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    HcomOpInputStruct inputStruct;
    HcclResult result = HcomAllGatherKernel(launchArgs_, &inputStruct);
    EXPECT_EQ(result, HCCL_SUCCESS);
    int addr = 10;
    std::vector<void*> Addrs;
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    result = HcomLaunchAllGatherKernel(&inputStruct, Addrs, Addrs);
    EXPECT_EQ(result, HCCL_SUCCESS);
}

TEST_F(HcomAllToAllKernelTest, HcomAllGahterKernelV_ShouldReturnError_WhenShapesMismatch)
{
    // 设置输入和输出形状不一致
    gert::Shape inputShape{2, 2};
    gert::Shape outputShape{3, 3};
    launchArgs_.inputShapes[0] = inputShape;
    launchArgs_.outputShapes[0] = outputShape;
    uint64_t count = 300 * 1024 * 1024;
    MOCKER(GetCountByShape).stubs().with(any(), any(), outBound(count)).will(returnValue(HCCL_SUCCESS));
    std::shared_ptr<hccl::hcclComm> comm;
    comm.reset(new (std::nothrow) hccl::hcclComm());
    MOCKER(HcomGetCommByGroup).stubs().with(any(), outBound(comm)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hcclComm::GetRankSize)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclAllGatherV).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    HcomOpInputStruct inputStruct;
    HcclResult result = HcomAllGatherVKernel(launchArgs_, &inputStruct);
    EXPECT_EQ(result, HCCL_SUCCESS);
    int addr = 10;
    std::vector<void*> Addrs;
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    result = HcomLaunchAllGatherVKernel(&inputStruct, Addrs, Addrs);
    EXPECT_EQ(result, HCCL_SUCCESS);
}

TEST_F(HcomAllToAllKernelTest, HcomBroadcastKernel_ShouldReturnError_WhenShapesMismatch)
{
    // 设置输入和输出形状不一致
    gert::Shape inputShape{2, 2};
    gert::Shape outputShape{3, 3};
    launchArgs_.inputShapes[0] = inputShape;
    launchArgs_.outputShapes[0] = outputShape;
    uint64_t count = 300 * 1024 * 1024;
    MOCKER(GetCountByShape).stubs().with(any(), any(), outBound(count)).will(returnValue(HCCL_SUCCESS));
    std::shared_ptr<hccl::hcclComm> comm;
    comm.reset(new (std::nothrow) hccl::hcclComm());
    MOCKER(HcomGetCommByGroup).stubs().with(any(), outBound(comm)).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclBroadcast).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    HcomOpInputStruct inputStruct;
    HcclResult result = HcomBroadcastKernel(launchArgs_, &inputStruct);
    EXPECT_EQ(result, HCCL_SUCCESS);
    int addr = 10;
    std::vector<void*> Addrs;
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    result = HcomLaunchBroadcastKernel(&inputStruct, Addrs, Addrs);
    EXPECT_EQ(result, HCCL_SUCCESS);
}

TEST_F(HcomAllToAllKernelTest, HcomReduceScatter_ShouldReturnError_WhenShapesMismatch)
{
    // 设置输入和输出形状不一致
    gert::Shape inputShape{2, 2};
    gert::Shape outputShape{3, 3};
    launchArgs_.inputShapes[0] = inputShape;
    launchArgs_.outputShapes[0] = outputShape;
    uint64_t count = 300 * 1024 * 1024;
    MOCKER(GetCountByShape).stubs().with(any(), any(), outBound(count)).will(returnValue(HCCL_SUCCESS));
    std::shared_ptr<hccl::hcclComm> comm;
    comm.reset(new (std::nothrow) hccl::hcclComm());
    MOCKER(HcomGetCommByGroup).stubs().with(any(), outBound(comm)).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclReduceScatter).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    HcomOpInputStruct inputStruct;
    HcclResult result = HcomReduceScatterKernel(launchArgs_, &inputStruct);
    EXPECT_EQ(result, HCCL_SUCCESS);
    int addr = 10;
    std::vector<void*> Addrs;
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    result = HcomLaunchReduceScatterKernel(&inputStruct, Addrs, Addrs);
    EXPECT_EQ(result, HCCL_SUCCESS);
}

TEST_F(HcomAllToAllKernelTest, HcomReduceScatterV_ShouldReturnError_WhenShapesMismatch)
{
    // 设置输入和输出形状不一致
    gert::Shape inputShape{2, 2};
    gert::Shape outputShape{3, 3};
    launchArgs_.inputShapes[0] = inputShape;
    launchArgs_.outputShapes[0] = outputShape;
    uint64_t count = 300 * 1024 * 1024;
    MOCKER(GetCountByShape).stubs().with(any(), any(), outBound(count)).will(returnValue(HCCL_SUCCESS));
    std::shared_ptr<hccl::hcclComm> comm;
    comm.reset(new (std::nothrow) hccl::hcclComm());
    MOCKER(HcomGetCommByGroup).stubs().with(any(), outBound(comm)).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclReduceScatterV).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    HcomOpInputStruct inputStruct;
    HcclResult result = HcomReduceScatterVKernel(launchArgs_, &inputStruct);
    EXPECT_EQ(result, HCCL_SUCCESS);
    int addr = 10;
    std::vector<void*> Addrs;
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    result = HcomLaunchReduceScatterVKernel(&inputStruct, Addrs, Addrs);
    EXPECT_EQ(result, HCCL_SUCCESS);
}

TEST_F(HcomAllToAllKernelTest, HcomAllGahterKernel_v2_When_Normal_Expect_ReturnlsHCCL_SUCCESS)
{
#ifdef OPEN_BUILD_PROJECT
#undef OPEN_BUILD_PROJECT
#endif
    // 设置输入和输出形状不一致
    gert::Shape inputShape{2, 2};
    gert::Shape outputShape{3, 3};
    launchArgs_.inputShapes[0] = inputShape;
    launchArgs_.outputShapes[0] = outputShape;
    uint64_t count = 300 * 1024 * 1024;
    MOCKER(GetCountByShape).stubs().with(any(), any(), outBound(count)).will(returnValue(HCCL_SUCCESS));
    std::shared_ptr<hccl::hcclComm> comm;
    comm.reset(new (std::nothrow) hccl::hcclComm());
    MOCKER(HcomGetCommByGroup).stubs().with(any(), outBound(comm)).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclAllGather).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    MOCKER(HcomGetDeviceType).stubs().with(any()).will(returnValue(DevType::DEV_TYPE_910_95));
    HcomOpInputStruct inputStruct;
    HcclResult result = HcomAllGatherKernel(launchArgs_, &inputStruct);
    EXPECT_EQ(result, HCCL_SUCCESS);
    int addr = 10;
    std::vector<void*> Addrs;
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    result = HcomLaunchAllGatherKernel(&inputStruct, Addrs, Addrs);
    EXPECT_EQ(result, HCCL_SUCCESS);
#define OPEN_BUILD_PROJECT
}
 
TEST_F(HcomAllToAllKernelTest, HcomAllReduceKernel_v2_When_Normal_Expect_ReturnlsHCCL_SUCCESS)
{
#ifdef OPEN_BUILD_PROJECT
#undef OPEN_BUILD_PROJECT
#endif
    // 设置输入和输出形状不一致
    gert::Shape inputShape{2, 2};
    gert::Shape outputShape{3, 3};
    launchArgs_.inputShapes[0] = inputShape;
    launchArgs_.outputShapes[0] = outputShape;
 
    std::string inputAddr = "inputAddr";
    string* pInputAddr = &inputAddr;
    vector<void*> inputAddrs;
    inputAddrs.push_back(pInputAddr);
    launchArgs_.inputAddrs = inputAddrs;
 
    std::string outputAddr = "outputAddr";
    string* pOutputAddr = &outputAddr;
    vector<void*> outputAddrs;
    outputAddrs.push_back(pOutputAddr);
    launchArgs_.outputAddrs = outputAddrs;
 
    HcomOpAttr opAttr;
    opAttr.dataType = HCCL_DATA_TYPE_INT8;
    opAttr.opType = HcomOpType::HCOM_ALL_REDUCE;
    strcpy(opAttr.group, "hccl_world_group");
    launchArgs_.opAttr = opAttr;
 
    std::string stream = "stream";
    string* pStream = &stream;
    launchArgs_.stream = pStream;
 
    launchArgs_.inputNum = 1;
    launchArgs_.outputNum = 1;
 
    uint64_t count = 300 * 1024 * 1024;
    MOCKER(GetCountByShape).stubs().with(any(), any(), outBound(count)).will(returnValue(HCCL_SUCCESS));
    std::shared_ptr<hccl::hcclComm> comm;
    comm.reset(new (std::nothrow) hccl::hcclComm());
    MOCKER(HcomGetCommByGroup).stubs().with(any(), outBound(comm)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hcclComm::GetInCCLbuffer).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hcclComm::GetOutCCLbuffer).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclAllReduce).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcomGetDeviceType).stubs().with(any()).will(returnValue(DevType::DEV_TYPE_910_95));
    HcomOpInputStruct inputStruct;
    HcclResult result = HcomAllReduceKernel(launchArgs_, &inputStruct);
    EXPECT_EQ(result, HCCL_SUCCESS);
    int addr = 10;
    std::vector<void*> Addrs;
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    Addrs.push_back(&addr);
    result = HcomLaunchAllReduceKernel(&inputStruct, Addrs, Addrs);
    EXPECT_EQ(result, HCCL_SUCCESS);
#define OPEN_BUILD_PROJECT
}