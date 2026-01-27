#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <iostream>
#include <fstream>

#define private public
#include "hcom_launch_kernel.h"
#include "hccl_comm_pub.h"
#include "hccl_communicator.h"
#include "hcom_private.h"
#undef private
#include "hcom_node_converter.h"
#include "hcom_build_graph.h"
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
#include "lowering_global_data.h"
#include "llt_hccl_stub_kernel_run_ctx_faker.h"
#include "ge/ge_allocator.h"
#include "env_config.h"


using namespace std;
using namespace hccl;
namespace hccl {
extern HcclResult GetCountByShape(const gert::Shape &shape, HcclDataType dataType, uint64_t &count);
extern HcclResult HcomAllToAllGetOpAttr(const ge::NodePtr &node, struct HcomOpAttr &opAttr);
}  // namespace hccl
class HcomLoweringTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {

        std::cout << "HcomLoweringTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "HcomLoweringTest TearDown" << std::endl;
    }
    virtual void SetUp()
    {
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

TEST_F(HcomLoweringTest, st_GenerateHcomOpArgs)
{
    ge::NodePtr node;

    // 空node，异常场景
    GenerateHcomOpArgs(node);

    bool bRet;
    std::string tempStr = "test_group";
    bRet = ge::AttrUtils::SetStr(node->GetOpDesc(), "group", tempStr);
    EXPECT_EQ(bRet, true);
    HcclDataType dataType = HCCL_DATA_TYPE_INT8;
    bRet = ge::AttrUtils::SetInt(node->GetOpDesc(), "dataType", dataType);
    EXPECT_EQ(bRet, true);

    // HcomRemoteRead
    node->GetOpDesc()->SetType(HCCL_KERNEL_OP_TYPE_REMOTE_READ);
    GenerateHcomOpArgs(node);

    // HcomRemoteRefRead
    node->GetOpDesc()->SetType(HCCL_KERNEL_OP_TYPE_REMOTE_REF_READ);
    GenerateHcomOpArgs(node);

    // HcomRemoteWrite
    node->GetOpDesc()->SetType(HCCL_KERNEL_OP_TYPE_REMOTE_WRITE);
    GenerateHcomOpArgs(node);

    // HcomRemoteScatterWrite
    node->GetOpDesc()->SetType(HCCL_KERNEL_OP_TYPE_REMOTE_SCATTER_WRITE);
    GenerateHcomOpArgs(node);

    // HcomAllReduce node
    node->GetOpDesc()->SetType(HCCL_KERNEL_OP_TYPE_ALLREDUCE);
    bRet = ge::AttrUtils::SetStr(node->GetOpDesc(), "reduction", "sum");
    EXPECT_EQ(bRet, true);
    GenerateHcomOpArgs(node);

    // HcomAllGather node
    node->GetOpDesc()->SetType(HCCL_KERNEL_OP_TYPE_ALLGATHER);
    bRet = ge::AttrUtils::SetInt(node->GetOpDesc(), "rank_size", 8);
    EXPECT_EQ(bRet, true);
    GenerateHcomOpArgs(node);

    // HcomBroadcast node
    node->GetOpDesc()->SetType(HCCL_KERNEL_OP_TYPE_BROADCAST);
    bRet = ge::AttrUtils::SetInt(node->GetOpDesc(), "root_rank", 0);
    EXPECT_EQ(bRet, true);
    GenerateHcomOpArgs(node);

    // HcomReduceScatter
    node->GetOpDesc()->SetType(HCCL_KERNEL_OP_TYPE_REDUCESCATTER);
    GenerateHcomOpArgs(node);

    // HcomGatherAllToAllV
    node->GetOpDesc()->SetType(HCCL_KERNEL_OP_TYPE_GATHER_ALLTOALLV);
    bRet = ge::AttrUtils::SetInt(node->GetOpDesc(), "addr_length", 8);
    EXPECT_EQ(bRet, true);
    GenerateHcomOpArgs(node);

    // HcomReduceScatterV node
    node->GetOpDesc()->SetType(HCCL_KERNEL_OP_TYPE_REDUCESCATTERV);
    GenerateHcomOpArgs(node);

    // HcomAllGatherV node
    node->GetOpDesc()->SetType(HCCL_KERNEL_OP_TYPE_ALLGATHERV);
    GenerateHcomOpArgs(node);
}

TEST_F(HcomLoweringTest, st_LaunchHcomOpKernel)
{
    gert::bg::ValueHolderPtr valuePtr = nullptr;
	gert::bg::DevMemValueHolderPtr memValuePtr = nullptr;

    HcomLaunchArg launchArg;
    launchArg.opArgs = valuePtr;
    launchArg.stream = valuePtr;

    std::vector<gert::bg::DevMemValueHolderPtr> inputAddrs;
    inputAddrs.push_back(memValuePtr);
    std::vector<gert::bg::DevMemValueHolderPtr> outputAddrs;
    outputAddrs.push_back(memValuePtr);
    std::vector<gert::bg::ValueHolderPtr> inputShapes;
    inputShapes.push_back(valuePtr);
    std::vector<gert::bg::ValueHolderPtr> outputShapes;
    outputShapes.push_back(valuePtr);

    LaunchHcomOpKernel(launchArg, inputAddrs, outputAddrs, inputShapes, outputShapes);
}

TEST_F(HcomLoweringTest, st_node_emptyNode)
{
    ge::NodePtr node;
    gert::bg::ValueHolderPtr valuePtr = nullptr;
	gert::bg::DevMemValueHolderPtr memValuePtr = nullptr;
    gert::LowerInput lowerInput;
    lowerInput.input_addrs.push_back(memValuePtr);
    lowerInput.input_shapes.push_back(valuePtr);
    lowerInput.global_data = nullptr;
    LoweringHcomNode(node, lowerInput);
}

TEST_F(HcomLoweringTest, st_node_broadcast)
{
    ge::ComputeGraph graph("test_graph");
    ge::OpDescPtr op;
    std::string tempStr = "test_group";
    ge::AttrUtils::SetStr(op, "group", tempStr);
    HcclDataType dataType = HCCL_DATA_TYPE_INT8;
    ge::AttrUtils::SetInt(op, "dataType", dataType);
    op->SetType(HCCL_KERNEL_OP_TYPE_BROADCAST);
    ge::AttrUtils::SetInt(op, "root_rank", 0);
    auto bcastNode = graph.AddNode(op);

    gert::LowerInput lowerInput;
    gert::bg::ValueHolderPtr valuePtr;
	gert::bg::DevMemValueHolderPtr memValuePtr;
    lowerInput.input_addrs.push_back(memValuePtr);
    lowerInput.input_shapes.push_back(valuePtr);
    gert::LoweringGlobalData global_data;
    lowerInput.global_data = &global_data;
    LoweringHcomNode(bcastNode, lowerInput);
}

TEST_F(HcomLoweringTest, st_node_send)
{
    ge::ComputeGraph graph("test_graph");
    ge::OpDescPtr op;
    std::string tempStr = "test_group";
    ge::AttrUtils::SetStr(op, "group", tempStr);
    HcclDataType dataType = HCCL_DATA_TYPE_INT8;
    ge::AttrUtils::SetInt(op, "dataType", dataType);
    op->SetType(HCCL_KERNEL_OP_TYPE_SEND);
    ge::AttrUtils::SetInt(op, "dest_rank", 1);
    ge::AttrUtils::SetInt(op, "sr_tag", 111);
    auto sendNode = graph.AddNode(op);

    gert::LowerInput lowerInput;
    gert::bg::ValueHolderPtr valuePtr;
    gert::bg::DevMemValueHolderPtr AddrsPtr;
    lowerInput.input_addrs.push_back(AddrsPtr);
    lowerInput.input_shapes.push_back(valuePtr);
    gert::LoweringGlobalData global_data;
    lowerInput.global_data = &global_data;
    LoweringHcomNode(sendNode, lowerInput);
}

TEST_F(HcomLoweringTest, st_node_receive)
{
    ge::ComputeGraph graph("test_graph");
    ge::OpDescPtr op;
    std::string tempStr = "test_group";
    ge::AttrUtils::SetStr(op, "group", tempStr);
    HcclDataType dataType = HCCL_DATA_TYPE_INT8;
    ge::AttrUtils::SetInt(op, "dataType", dataType);
    op->SetType(HCCL_KERNEL_OP_TYPE_RECEIVE);
    ge::AttrUtils::SetInt(op, "src_rank", 0);
    ge::AttrUtils::SetInt(op, "sr_tag", 111);
    auto receiveNode = graph.AddNode(op);

    gert::LowerInput lowerInput;
    gert::bg::ValueHolderPtr valuePtr;
    gert::bg::DevMemValueHolderPtr AddrsPtr;
    lowerInput.input_addrs.push_back(AddrsPtr);
    lowerInput.input_shapes.push_back(valuePtr);
    gert::LoweringGlobalData global_data;
    lowerInput.global_data = &global_data;
    LoweringRecvNode(receiveNode, lowerInput);
}

TEST_F(HcomLoweringTest, st_node_reduce)
{
    ge::ComputeGraph graph("test_graph");
    ge::OpDescPtr op;
    std::string tempStr = "test_group";
    ge::AttrUtils::SetStr(op, "group", tempStr);
    HcclDataType dataType = HCCL_DATA_TYPE_INT8;
    ge::AttrUtils::SetInt(op, "dataType", dataType);
    op->SetType(HCCL_KERNEL_OP_TYPE_REDUCE);
    ge::AttrUtils::SetInt(op, "root_rank", 0);
    ge::AttrUtils::SetInt(op, "sr_tag", 111);
    ge::AttrUtils::SetStr(op, "reduction", "sum");
    auto reduceNode = graph.AddNode(op);

    gert::LowerInput lowerInput;
    gert::bg::ValueHolderPtr valuePtr;
    gert::bg::DevMemValueHolderPtr AddrsPtr;
    lowerInput.input_addrs.push_back(AddrsPtr);
    lowerInput.input_shapes.push_back(valuePtr);
    gert::LoweringGlobalData global_data;
    lowerInput.global_data = &global_data;
    LoweringRecvNode(reduceNode, lowerInput);
}

TEST_F(HcomLoweringTest, st_node_allreduce)
{
    ge::ComputeGraph graph("test_graph");
    ge::OpDescPtr op;
    std::string tempStr = "test_group";
    ge::AttrUtils::SetStr(op, "group", tempStr);
    HcclDataType dataType = HCCL_DATA_TYPE_INT8;
    ge::AttrUtils::SetInt(op, "dataType", dataType);
    op->SetType(HCCL_KERNEL_OP_TYPE_ALLREDUCE);
    ge::AttrUtils::SetStr(op, "reduction", "sum");
    auto allreduceNode = graph.AddNode(op);

    gert::LowerInput lowerInput;
    gert::bg::ValueHolderPtr valuePtr;
	gert::bg::DevMemValueHolderPtr memValuePtr;
    lowerInput.input_addrs.push_back(memValuePtr);
    lowerInput.input_shapes.push_back(valuePtr);
    gert::LoweringGlobalData global_data;
    lowerInput.global_data = &global_data;
    LoweringHcomNode(allreduceNode, lowerInput);
}

TEST_F(HcomLoweringTest, st_node_reducescatterv)
{
    ge::ComputeGraph graph("test_graph");
    ge::OpDescPtr op;
    std::string tempStr = "test_group";
    ge::AttrUtils::SetStr(op, "group", tempStr);
    HcclDataType dataType = HCCL_DATA_TYPE_INT8;
    ge::AttrUtils::SetInt(op, "dataType", dataType);
    op->SetType(HCCL_KERNEL_OP_TYPE_REDUCESCATTERV);
    ge::AttrUtils::SetStr(op, "reduction", "sum");
    auto reducescattervNode = graph.AddNode(op);
 
    gert::LowerInput lowerInput;
    gert::bg::ValueHolderPtr valuePtr;
	gert::bg::DevMemValueHolderPtr memValuePtr;
    for (int i = 0; i < 5; i++) {
        lowerInput.input_addrs.push_back(memValuePtr);
        lowerInput.input_shapes.push_back(valuePtr);
    }
    gert::LoweringGlobalData global_data;
    lowerInput.global_data = &global_data;
    LoweringAlltoAllNode(reducescattervNode, lowerInput);
}

TEST_F(HcomLoweringTest, st_node_alltoallv)
{
    ge::ComputeGraph graph("test_graph");
    ge::OpDescPtr op;
    std::string tempStr = "test_group";
    ge::AttrUtils::SetStr(op, "group", tempStr);
    HcclDataType dataType = HCCL_DATA_TYPE_INT8;
    ge::AttrUtils::SetInt(op, "dataType", dataType);
    op->SetType(HCCL_KERNEL_OP_TYPE_ALLTOALLV);
    auto alltoallvNode = graph.AddNode(op);

    gert::LowerInput lowerInput;
    gert::bg::ValueHolderPtr valuePtr;
	gert::bg::DevMemValueHolderPtr memValuePtr;
    for (int i = 0; i < 5; i++) {
        lowerInput.input_addrs.push_back(memValuePtr);
        lowerInput.input_shapes.push_back(valuePtr);
    }
    gert::LoweringGlobalData global_data;
    lowerInput.global_data = &global_data;
    LoweringAlltoAllNode(alltoallvNode, lowerInput);
}

TEST_F(HcomLoweringTest, st_node_allgatherv)
{
    ge::ComputeGraph graph("test_graph");
    ge::OpDescPtr op;
    std::string tempStr = "test_group";
    ge::AttrUtils::SetStr(op, "group", tempStr);
    HcclDataType dataType = HCCL_DATA_TYPE_INT8;
    ge::AttrUtils::SetInt(op, "dataType", dataType);
    op->SetType(HCCL_KERNEL_OP_TYPE_ALLGATHERV);
    auto allgathervNode = graph.AddNode(op);

    gert::LowerInput lowerInput;
    gert::bg::ValueHolderPtr valuePtr;
	gert::bg::DevMemValueHolderPtr memValuePtr;
    for (int i = 0; i < 4; i++) {
        lowerInput.input_addrs.push_back(memValuePtr);
        lowerInput.input_shapes.push_back(valuePtr);
    }
    gert::LoweringGlobalData global_data;
    lowerInput.global_data = &global_data;
    LoweringAlltoAllNode(allgathervNode, lowerInput);
}

TEST_F(HcomLoweringTest, st_node_alltoallvc)
{
    ge::ComputeGraph graph("test_graph");
    ge::OpDescPtr op;
    std::string tempStr = "test_group";
    ge::AttrUtils::SetStr(op, "group", tempStr);
    HcclDataType dataType = HCCL_DATA_TYPE_INT8;
    ge::AttrUtils::SetInt(op, "dataType", dataType);
    op->SetType(HCCL_KERNEL_OP_TYPE_ALLTOALLVC);
    auto alltoallvcNode = graph.AddNode(op);

    gert::LowerInput lowerInput;
    gert::bg::ValueHolderPtr valuePtr;
	gert::bg::DevMemValueHolderPtr memValuePtr;
    for (int i = 0; i < 2; i++) {
        lowerInput.input_addrs.push_back(memValuePtr);
        lowerInput.input_shapes.push_back(valuePtr);
    }
    gert::LoweringGlobalData global_data;
    lowerInput.global_data = &global_data;
    LoweringAlltoAllNode(alltoallvcNode, lowerInput);
}

TEST_F(HcomLoweringTest, st_node_gather_alltoallv)
{
    ge::ComputeGraph graph("test_graph");
    ge::OpDescPtr op;
    std::string tempStr = "test_group";
    ge::AttrUtils::SetStr(op, "group", tempStr);
    HcclDataType dataType = HCCL_DATA_TYPE_INT8;
    ge::AttrUtils::SetInt(op, "dataType", dataType);
    op->SetType(HCCL_KERNEL_OP_TYPE_GATHER_ALLTOALLV);
    ge::AttrUtils::SetInt(op, "addr_length", 8);
    auto galltoallvNode = graph.AddNode(op);

    gert::LowerInput lowerInput;
    gert::bg::ValueHolderPtr valuePtr;
	gert::bg::DevMemValueHolderPtr memValuePtr;
    for (int i = 0; i < 4; i++) {
        lowerInput.input_addrs.push_back(memValuePtr);
        lowerInput.input_shapes.push_back(valuePtr);
    }
    gert::LoweringGlobalData global_data;
    lowerInput.global_data = &global_data;
    LoweringAlltoAllNode(galltoallvNode, lowerInput);
}

TEST_F(HcomLoweringTest, st_node_gather_remote)
{
    ge::ComputeGraph graph("test_graph");
    ge::OpDescPtr op;
    std::string tempStr = "test_group";
    ge::AttrUtils::SetStr(op, "group", tempStr);
    HcclDataType dataType = HCCL_DATA_TYPE_INT8;
    ge::AttrUtils::SetInt(op, "dataType", dataType);
    op->SetType(HCCL_KERNEL_OP_TYPE_REMOTE_READ);
    auto remoteNode = graph.AddNode(op);

    gert::LowerInput lowerInput;
    gert::bg::ValueHolderPtr valuePtr;
	gert::bg::DevMemValueHolderPtr memValuePtr;
    for (int i = 0; i < 1; i++) {
        lowerInput.input_addrs.push_back(memValuePtr);
        lowerInput.input_shapes.push_back(valuePtr);
    }
    gert::LoweringGlobalData global_data;
    lowerInput.global_data = &global_data;
    LoweringRemoteNode(remoteNode, lowerInput);
}

TEST_F(HcomLoweringTest, st_hcomLaunchKernel_allGather)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_hcomLaunchKernel_allGather.json";
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
    u32 ret1 = hrtSetDevice(0);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcomLaunchKernel_allGather.json";
    char* rank_ID = "0";

    ret1 = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    AsyncAnyValue any_value;

    HcomOpLaunchArgs launchArgs0;
    HcomOpAttr opAttr;
    opAttr.dataType = HCCL_DATA_TYPE_INT8;
    opAttr.opType = HcomOpType::HCOM_ALL_GATHER;
    strcpy(opAttr.group, "hccl_world_group");
    launchArgs0.opAttr = opAttr;
    any_value.data.pointer = &launchArgs0;

    MOCKER(HcclAllGather)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(GetHcomOpLaunchArgs)
    .stubs()
    .with(any(), outBound(launchArgs0))
    .will(returnValue(HCCL_SUCCESS));
    HcomOpInputStruct inputStruct;
    std::vector<void*> output = {&inputStruct};
    auto kernelHolder = gert::KernelRunContextFaker()
                        .KernelIONum(0,1)
                        .Outputs(output)
                        .Build();
    auto context = kernelHolder.GetContext<gert::KernelContext>();

    PrepareHcomKernel(context);

    HcomDestroy();
    remove(file_name_t);
    GlobalMockObject::verify();
}

TEST_F(HcomLoweringTest, st_hcomLaunchKernel_allgatherv)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_hcomLaunchKernel_allGatherv.json";
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
    u32 ret1 = hrtSetDevice(0);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcomLaunchKernel_allGatherv.json";
    char* rank_ID = "0";

    ret1 = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    HcomOpLaunchArgs launchArgs;
    HcomOpAttr opAttr;
    opAttr.dataType = HCCL_DATA_TYPE_INT8;
    opAttr.opType = HcomOpType::HCOM_ALL_GATHER_V;
    strcpy(opAttr.group, "hccl_world_group");
    launchArgs.opAttr = opAttr;

    std::string stream = "stream";
    string* pStream = &stream;
    launchArgs.stream = pStream;

    launchArgs.inputNum = 4;
    launchArgs.outputNum = 1;

    launchArgs.inputAddrs.resize(4);
    launchArgs.inputAddrs[0] = (char*)malloc(1024);
    memset_s(launchArgs.inputAddrs[0], 1024, 0, 1024);
    launchArgs.inputAddrs[1] = (char*)malloc(1024);
    memset_s(launchArgs.inputAddrs[1], 1024, 0, 1024);
    launchArgs.inputAddrs[2] = (char*)malloc(1024);
    memset_s(launchArgs.inputAddrs[2], 1024, 0, 1024);
    launchArgs.inputAddrs[3] = (char*)malloc(1024);
    memset_s(launchArgs.inputAddrs[3], 1024, 0, 1024);

    gert::Shape inferShape({0});
    vector<gert::Shape> inShape;
    inShape.push_back(inferShape);
    inShape.push_back(inferShape);
    inShape.push_back(inferShape);
    inShape.push_back(inferShape);
    launchArgs.inputShapes = inShape;

    launchArgs.outputAddrs.resize(1);
    launchArgs.outputAddrs[0] = (char*)malloc(1024);
    memset_s(launchArgs.outputAddrs[0], 1024, 0, 1024);

    gert::Shape outshape({0});
    launchArgs.outputShapes.push_back(outshape);

    MOCKER(HcclAllGatherV)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcomOpInputStruct inputStruct;
    HcomAllGatherVKernel(launchArgs, &inputStruct);
    inputStruct.launchArgs = launchArgs;
    HcomLaunchAllGatherVKernel(&inputStruct, launchArgs.inputAddrs, launchArgs.outputAddrs);

    free(launchArgs.inputAddrs[0]);
    free(launchArgs.inputAddrs[1]);
    free(launchArgs.inputAddrs[2]);
    free(launchArgs.inputAddrs[3]);
    free(launchArgs.outputAddrs[0]);
    HcomDestroy();
    remove(file_name_t);
    GlobalMockObject::verify();
}

TEST_F(HcomLoweringTest, st_hcomLaunchKernel_broadcast)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_hcomLaunchKernel_broadcast.json";
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
    u32 ret1 = hrtSetDevice(0);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcomLaunchKernel_broadcast.json";
    char* rank_ID = "0";

    ret1 = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    HcomOpLaunchArgs launchArgs;
    HcomOpAttr opAttr;
    opAttr.dataType = HCCL_DATA_TYPE_INT8;
    opAttr.opType = HcomOpType::HCOM_BROADCAST;
    strcpy(opAttr.group, "hccl_world_group");
    opAttr.op.broadcast.root = 0;
    launchArgs.opAttr = opAttr;

    std::string stream = "stream";
    string* pStream = &stream;
    launchArgs.stream = pStream;

    launchArgs.inputNum = 1;
    launchArgs.outputNum = 1;

    gert::Shape inferShape;
    vector<gert::Shape> shape;
    shape.push_back(inferShape);
    launchArgs.inputShapes = shape;
    launchArgs.outputShapes = shape;

    std::string inputAddr = "inputAddr";
    string* pInputAddr = &inputAddr;
    vector<void*> inputAddrs;
    inputAddrs.push_back(pInputAddr);
    launchArgs.inputAddrs = inputAddrs;

    std::string outputAddr = "outputAddr";
    string* pOutputAddr = &outputAddr;
    vector<void*> outputAddrs;
    outputAddrs.push_back(pOutputAddr);
    launchArgs.outputAddrs = outputAddrs;

    MOCKER(GetHcomOpLaunchArgs)
    .stubs()
    .with(any(), outBound(launchArgs))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HcclBroadcast)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcomOpInputStruct inputStruct;
    std::vector<void*> output = {&inputStruct};
    auto kernelHolder = gert::KernelRunContextFaker()
                        .KernelIONum(0,1)
                        .Outputs(output)
                        .Build();
    auto context = kernelHolder.GetContext<gert::KernelContext>();

    PrepareHcomKernel(context);
    HcomDestroy();
    remove(file_name_t);
    GlobalMockObject::verify();
}

TEST_F(HcomLoweringTest, st_GetHcomOpLaunchArgs_allreduce)
{
    HcomOpLaunchArgs launchArgs;
    HcomOpAttr opAttr;
    std::string stream = "stream";
    opAttr.opType = HcomOpType::HCOM_ALL_REDUCE;
    uint64_t input_num = 1;
    uint64_t output_num = 1;

    gert::StorageShape inStorageShape{};
    gert::StorageShape outStorageShape{};

    std::vector<void*> input = {&opAttr, &stream,
                        reinterpret_cast<void*>(input_num), reinterpret_cast<void*>(output_num),
                        &inStorageShape, &outStorageShape};

    auto kernelHolder = gert::KernelRunContextFaker()
                        .KernelIONum(6,0)
                        .Inputs(input)
                        .Build();
    auto context = kernelHolder.GetContext<gert::KernelContext>();

    GetHcomOpLaunchArgs(context, launchArgs);

    GlobalMockObject::verify();
}

TEST_F(HcomLoweringTest, st_GetHcomOpLaunchArgs_allgather)
{
    HcomOpLaunchArgs launchArgs;
    HcomOpAttr opAttr;
    std::string stream = "stream";
    opAttr.opType = HcomOpType::HCOM_ALL_GATHER;
    uint64_t input_num = 1;
    uint64_t output_num = 1;

    gert::StorageShape inStorageShape{};
    gert::StorageShape outStorageShape{};

    gert::TensorData input_tensor;
    gert::TensorData output_tensor;

    std::vector<void*> input = {&opAttr, &stream, 
                        reinterpret_cast<void*>(input_num), reinterpret_cast<void*>(output_num),
                        &input_tensor, &output_tensor,
                        &inStorageShape, &outStorageShape};

    auto kernelHolder = gert::KernelRunContextFaker()
                        .KernelIONum(8,0)
                        .Inputs(input)
                        .Build();
    auto context = kernelHolder.GetContext<gert::KernelContext>();

    GetHcomOpLaunchArgs(context, launchArgs);

    GlobalMockObject::verify();
}

TEST_F(HcomLoweringTest, st_hcomLaunchKernel_send)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_hcomLaunchKernel_send.json";
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
    u32 ret1 = hrtSetDevice(0);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcomLaunchKernel_send.json";
    char* rank_ID = "0";

    ret1 = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    HcomOpLaunchArgs launchArgs;
    HcomOpAttr opAttr;
    opAttr.dataType = HCCL_DATA_TYPE_INT8;
    opAttr.opType = HcomOpType::HCOM_SEND;
    strcpy(opAttr.group, "hccl_world_group");
    opAttr.op.send.destRank = 1;
    opAttr.op.send.srTag = 111;
    launchArgs.opAttr = opAttr;

    std::string stream = "stream";
    string* pStream = &stream;
    launchArgs.stream = pStream;

    launchArgs.inputNum = 1;
    launchArgs.outputNum = 1;

    gert::Shape inferShape;
    vector<gert::Shape> shape;
    shape.push_back(inferShape);
    launchArgs.inputShapes = shape;
    launchArgs.outputShapes = shape;

    std::string inputAddr = "inputAddr";
    string* pInputAddr = &inputAddr;
    vector<void*> inputAddrs;
    inputAddrs.push_back(pInputAddr);
    launchArgs.inputAddrs = inputAddrs;

    std::string outputAddr = "outputAddr";
    string* pOutputAddr = &outputAddr;
    vector<void*> outputAddrs;
    outputAddrs.push_back(pOutputAddr);
    launchArgs.outputAddrs = outputAddrs;

    MOCKER(GetHcomOpLaunchArgs)
    .stubs()
    .with(any(), outBound(launchArgs))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HcomGetWorldRankFromGroupRank)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hcclStreamSynchronize)
    .stubs()
    .with(any(), any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HcclSend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcomOpInputStruct inputStruct;
    std::vector<void*> output = {&inputStruct};
    auto kernelHolder = gert::KernelRunContextFaker()
                        .KernelIONum(0,1)
                        .Outputs(output)
                        .Build();
    auto context = kernelHolder.GetContext<gert::KernelContext>();

    PrepareHcomKernel(context);
    HcomDestroy();
    remove(file_name_t);
    GlobalMockObject::verify();
}

HcclResult hrtMemSyncCopy_stub(void *dst, uint64_t destMax, const void *src, uint64_t count, HcclRtMemcpyKind kind)
{
    s64 *recvShape = static_cast<s64*>(dst);
    recvShape[0] = 1;
    recvShape[1] = 1;
    return HCCL_SUCCESS;
}

TEST_F(HcomLoweringTest, st_hcomLaunchKernel_recv)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_hcomLaunchKernel_recv.json";
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
    u32 ret1 = hrtSetDevice(0);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcomLaunchKernel_recv.json";
    char* rank_ID = "0";

    ret1 = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    HcomOpLaunchArgs launchOpArgs;
    HcomOpAttr opAttr;
    opAttr.dataType = HCCL_DATA_TYPE_INT8;
    opAttr.opType = HcomOpType::HCOM_RECEIVE;
    strcpy(opAttr.group, "hccl_world_group");
    opAttr.op.send.destRank = 0;
    opAttr.op.send.srTag = 111;
    launchOpArgs.opAttr = opAttr;

    std::string stream = "stream";
    string* pStream = &stream;
    launchOpArgs.stream = pStream;

    MOCKER(hcclStreamSynchronize)
    .stubs()
    .with(any(),any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HcclRecv)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtMemSyncCopy)
    .stubs()
    .will(invoke(hrtMemSyncCopy_stub));


    gert::StorageShape inStorageShape{};
    gert::StorageShape* pinStorageShape = &inStorageShape;
    gert::AllocatorFaker *allocator = new gert::AllocatorFaker();
    ge::DataType out_datatype = ge::DT_INT8;

    auto context_holder =gert::KernelRunContextFaker()
        .KernelIONum(4, 2)
        .Inputs({&opAttr, &stream, allocator, reinterpret_cast<void *>(out_datatype)})
        .Outputs({&inStorageShape, &inStorageShape})
        .Build();

    auto recvContext = context_holder.GetContext<gert::KernelContext>();

    EXPECT_EQ(BuildHcclOutputShapeOutputs(nullptr, recvContext), ge::GRAPH_SUCCESS);
    LaunchRecvKernel(recvContext);

    delete allocator;
    HcomDestroy();
    remove(file_name_t);
    GlobalMockObject::verify();
}

TEST_F(HcomLoweringTest, st_hcomLaunchKernel_reduce)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_hcomLaunchKernel_reduce.json";
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
    u32 ret1 = hrtSetDevice(0);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcomLaunchKernel_reduce.json";
    char* rank_ID = "0";

    ret1 = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    HcomOpLaunchArgs launchArgs;
    HcomOpAttr opAttr;
    opAttr.dataType = HCCL_DATA_TYPE_INT8;
    opAttr.opType = HcomOpType::HCOM_REDUCE;
    strcpy(opAttr.group, "hccl_world_group");
    opAttr.op.reduce.root = 0;
    opAttr.op.reduce.reduction = HCCL_REDUCE_SUM;
    launchArgs.opAttr = opAttr;

    std::string stream = "stream";
    string* pStream = &stream;
    launchArgs.stream = pStream;

    launchArgs.inputNum = 1;
    launchArgs.outputNum = 1;

    gert::Shape inferShape;
    vector<gert::Shape> shape;
    shape.push_back(inferShape);
    launchArgs.inputShapes = shape;
    launchArgs.outputShapes = shape;

    std::string inputAddr = "inputAddr";
    string* pInputAddr = &inputAddr;
    vector<void*> inputAddrs;
    inputAddrs.push_back(pInputAddr);
    launchArgs.inputAddrs = inputAddrs;

    std::string outputAddr = "outputAddr";
    string* pOutputAddr = &outputAddr;
    vector<void*> outputAddrs;
    outputAddrs.push_back(pOutputAddr);
    launchArgs.outputAddrs = outputAddrs;

    MOCKER(GetHcomOpLaunchArgs)
    .stubs()
    .with(any(), outBound(launchArgs))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HcclReduce)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcomOpInputStruct inputStruct;
    std::vector<void*> output = {&inputStruct};
    auto kernelHolder = gert::KernelRunContextFaker()
                        .KernelIONum(0,1)
                        .Outputs(output)
                        .Build();
    auto context = kernelHolder.GetContext<gert::KernelContext>();

    PrepareHcomKernel(context);
    HcomDestroy();
    remove(file_name_t);
    GlobalMockObject::verify();
}

TEST_F(HcomLoweringTest, st_hcomLaunchKernel_alltoallvc)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_hcomLaunchKernel_alltoallvc.json";
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
    u32 ret1 = hrtSetDevice(0);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcomLaunchKernel_alltoallvc.json";
    char* rank_ID = "0";

    ret1 = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    HcomOpLaunchArgs launchArgs;
    HcomOpAttr opAttr;
    opAttr.dataType = HCCL_DATA_TYPE_INT8;
    opAttr.opType = HcomOpType::HCOM_ALL_TO_ALL_VC;
    strcpy(opAttr.group, "hccl_world_group");
    opAttr.op.broadcast.root = 0;
    launchArgs.opAttr = opAttr;

    std::string stream = "stream";
    string* pStream = &stream;
    launchArgs.stream = pStream;

    launchArgs.inputNum = 1;
    launchArgs.outputNum = 1;

    gert::Shape inferShape;
    vector<gert::Shape> inShape;
    inShape.push_back(inferShape);
    inShape.push_back(inferShape);

    launchArgs.inputShapes = inShape;
    vector<gert::Shape> shape;
    shape.push_back(inferShape);
    launchArgs.outputShapes = shape;

    std::string inputAddr = "inputAddr";
    string* pInputAddr = &inputAddr;
    vector<void*> inputAddrs;
    inputAddrs.push_back(pInputAddr);
    inputAddrs.push_back(pInputAddr);

    launchArgs.inputAddrs = inputAddrs;

    std::string outputAddr = "outputAddr";
    string* pOutputAddr = &outputAddr;
    vector<void*> outputAddrs;
    outputAddrs.push_back(pOutputAddr);
    launchArgs.outputAddrs = outputAddrs;

    MOCKER(GetHcomOpLaunchArgs)
    .stubs()
    .with(any(), outBound(launchArgs))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HcclAlltoAllVC)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcomOpInputStruct inputStruct;
    std::vector<void*> output = {&inputStruct};
    auto kernelHolder = gert::KernelRunContextFaker()
                        .KernelIONum(0,1)
                        .Outputs(output)
                        .Build();
    auto context = kernelHolder.GetContext<gert::KernelContext>();

    PrepareHcomKernel(context);
    HcomDestroy();
    remove(file_name_t);
    GlobalMockObject::verify();
}

TEST_F(HcomLoweringTest, st_hcomLaunchKernel_gather_alltoallv)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_hcomLaunchKernel_gather_alltoallv.json";
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
    u32 ret1 = hrtSetDevice(0);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcomLaunchKernel_gather_alltoallv.json";
    char* rank_ID = "0";

    ret1 = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    HcomOpLaunchArgs launchArgs;
    HcomOpAttr opAttr;
    opAttr.dataType = HCCL_DATA_TYPE_INT8;
    opAttr.opType = HcomOpType::HCOM_GATHER_ALL_TO_ALL_V;
    strcpy(opAttr.group, "hccl_world_group");
    opAttr.op.gatheralltoallv.addrLength = 8;
    launchArgs.opAttr = opAttr;

    std::string stream = "stream";
    string* pStream = &stream;
    launchArgs.stream = pStream;

    launchArgs.inputNum = 1;
    launchArgs.outputNum = 1;

    gert::Shape inferShape;
    vector<gert::Shape> shape;
    shape.push_back(inferShape);
    launchArgs.inputShapes = shape;
    launchArgs.outputShapes = shape;

    std::string inputAddr = "inputAddr";
    string* pInputAddr = &inputAddr;
    vector<void*> inputAddrs;
    inputAddrs.push_back(pInputAddr);
    inputAddrs.push_back(pInputAddr);
    inputAddrs.push_back(pInputAddr);
    inputAddrs.push_back(pInputAddr);
    launchArgs.inputAddrs = inputAddrs;

    std::string outputAddr = "outputAddr";
    string* pOutputAddr = &outputAddr;
    vector<void*> outputAddrs;
    outputAddrs.push_back(pOutputAddr);
    outputAddrs.push_back(pOutputAddr);
    launchArgs.outputAddrs = outputAddrs;

    MOCKER(GetHcomOpLaunchArgs)
    .stubs()
    .with(any(), outBound(launchArgs))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HcclGatherAlltoAllV).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    HcomOpInputStruct inputStruct;
    std::vector<void*> output = {&inputStruct};
    auto kernelHolder = gert::KernelRunContextFaker()
                        .KernelIONum(0,1)
                        .Outputs(output)
                        .Build();
    auto context = kernelHolder.GetContext<gert::KernelContext>();

    PrepareHcomKernel(context);
    HcomDestroy();
    remove(file_name_t);
    GlobalMockObject::verify();
}

TEST_F(HcomLoweringTest, st_hcomLaunchKernel_reducescatter)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_hcomLaunchKernel_reducescatter.json";
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
    u32 ret1 = hrtSetDevice(0);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcomLaunchKernel_reducescatter.json";
    char* rank_ID = "0";

    ret1 = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    AsyncAnyValue any_value;

    HcomOpLaunchArgs launchArgs0;
    HcomOpAttr opAttr;
    opAttr.dataType = HCCL_DATA_TYPE_INT8;
    opAttr.opType = HcomOpType::HCOM_REDUCE_SCATTER;
    strcpy(opAttr.group, "hccl_world_group");
    launchArgs0.opAttr = opAttr;
    any_value.data.pointer = &launchArgs0;

    MOCKER(GetHcomOpLaunchArgs)
    .stubs()
    .with(any(), outBound(launchArgs0))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HcclReduceScatter)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcomOpInputStruct inputStruct;
    std::vector<void*> output = {&inputStruct};
    auto kernelHolder = gert::KernelRunContextFaker()
                        .KernelIONum(0,1)
                        .Outputs(output)
                        .Build();
    auto context = kernelHolder.GetContext<gert::KernelContext>();

    PrepareHcomKernel(context);
    HcomDestroy();
    remove(file_name_t);
    GlobalMockObject::verify();
}

TEST_F(HcomLoweringTest, st_hcomLaunchKernel_reducescatterv)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_hcomLaunchKernel_reducescatterv.json";
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
    u32 ret1 = hrtSetDevice(0);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcomLaunchKernel_reducescatterv.json";
    char* rank_ID = "0";

    ret1 = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    HcomOpLaunchArgs launchArgs;
    HcomOpAttr opAttr;
    opAttr.dataType = HCCL_DATA_TYPE_INT8;
    opAttr.opType = HcomOpType::HCOM_REDUCE_SCATTER_V;
    opAttr.op.reducescatterv.reduction = HCCL_REDUCE_SUM;
    strcpy(opAttr.group, "hccl_world_group");
    launchArgs.opAttr = opAttr;

    std::string stream = "stream";
    string* pStream = &stream;
    launchArgs.stream = pStream;

    launchArgs.inputNum = 4;
    launchArgs.outputNum = 1;

    launchArgs.inputAddrs.resize(4);
    launchArgs.inputAddrs[0] = (char*)malloc(1024);
    memset_s(launchArgs.inputAddrs[0], 1024, 0, 1024);
    launchArgs.inputAddrs[1] = (char*)malloc(1024);
    memset_s(launchArgs.inputAddrs[1], 1024, 0, 1024);
    launchArgs.inputAddrs[2] = (char*)malloc(1024);
    memset_s(launchArgs.inputAddrs[2], 1024, 0, 1024);
    launchArgs.inputAddrs[3] = (char*)malloc(1024);
    memset_s(launchArgs.inputAddrs[3], 1024, 0, 1024);

    gert::Shape inferShape({2,2});
    vector<gert::Shape> inShape;
    inShape.push_back(inferShape);
    inShape.push_back(inferShape);
    inShape.push_back(inferShape);
    inShape.push_back(inferShape);
    launchArgs.inputShapes = inShape;

    launchArgs.outputAddrs.resize(1);
    launchArgs.outputAddrs[0] = (char*)malloc(1024);
    memset_s(launchArgs.outputAddrs[0], 1024, 0, 1024);

    gert::Shape outshape({0});
    launchArgs.outputShapes.push_back(outshape);

    MOCKER(HcclReduceScatterV)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcomOpInputStruct inputStruct;
    HcomReduceScatterVKernel(launchArgs, &inputStruct);
    inputStruct.launchArgs = launchArgs;
    HcomLaunchReduceScatterVKernel(&inputStruct, launchArgs.inputAddrs, launchArgs.outputAddrs);

    free(launchArgs.inputAddrs[0]);
    free(launchArgs.inputAddrs[1]);
    free(launchArgs.inputAddrs[2]);
    free(launchArgs.inputAddrs[3]);
    free(launchArgs.outputAddrs[0]);
    HcomDestroy();
    remove(file_name_t);
    GlobalMockObject::verify();
}
#if 1
TEST_F(HcomLoweringTest, st_hcomLaunchKernel_alltoallv)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_hcomLaunchKernel_alltoallv.json";
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
    u32 ret1 = hrtSetDevice(0);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcomLaunchKernel_alltoallv.json";
    char* rank_ID = "0";

    ret1 = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    HcomOpLaunchArgs launchArgs;
    HcomOpAttr opAttr;
    opAttr.dataType = HCCL_DATA_TYPE_INT8;
    opAttr.opType = HcomOpType::HCOM_ALL_TO_ALL_V;
    strcpy(opAttr.group, "hccl_world_group");
    opAttr.op.broadcast.root = 0;
    launchArgs.opAttr = opAttr;

    std::string stream = "stream";
    string* pStream = &stream;
    launchArgs.stream = pStream;

    launchArgs.inputNum = 1;
    launchArgs.outputNum = 1;

    gert::Shape inferShape;
    vector<gert::Shape> inShape;
    inShape.push_back(inferShape);
    inShape.push_back(inferShape);
    inShape.push_back(inferShape);
    inShape.push_back(inferShape);
    inShape.push_back(inferShape);
    launchArgs.inputShapes = inShape;
    vector<gert::Shape> shape;
    shape.push_back(inferShape);
    launchArgs.outputShapes = shape;

    std::string inputAddr = "inputAddr";
    string* pInputAddr = &inputAddr;
    vector<void*> inputAddrs;
    inputAddrs.push_back(pInputAddr);
    inputAddrs.push_back(pInputAddr);
    inputAddrs.push_back(pInputAddr);
    inputAddrs.push_back(pInputAddr);
    inputAddrs.push_back(pInputAddr);
    launchArgs.inputAddrs = inputAddrs;

    std::string outputAddr = "outputAddr";
    string* pOutputAddr = &outputAddr;
    vector<void*> outputAddrs;
    outputAddrs.push_back(pOutputAddr);
    launchArgs.outputAddrs = outputAddrs;

    MOCKER(GetHcomOpLaunchArgs)
    .stubs()
    .with(any(), outBound(launchArgs))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HcclAlltoAllV)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcomOpInputStruct inputStruct;
    std::vector<void*> output = {&inputStruct};
    auto kernelHolder = gert::KernelRunContextFaker()
                        .KernelIONum(0,1)
                        .Outputs(output)
                        .Build();
    auto context = kernelHolder.GetContext<gert::KernelContext>();

    PrepareHcomKernel(context);
    HcomDestroy();
    remove(file_name_t);
    GlobalMockObject::verify();
}
#endif

TEST_F(HcomLoweringTest, st_hcomLaunchKernel_allgather_new)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_hcomLaunchKernel_allgather_new.json";
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
    u32 ret1 = hrtSetDevice(0);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcomLaunchKernel_allgather_new.json";
    char* rank_ID = "0";

    ret1 = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    HcomOpLaunchArgs launchArgs;
    HcomOpAttr opAttr;
    opAttr.dataType = HCCL_DATA_TYPE_INT8;
    opAttr.opType = HcomOpType::HCOM_ALL_GATHER;
    strcpy(opAttr.group, "hccl_world_group");
    opAttr.op.broadcast.root = 0;
    launchArgs.opAttr = opAttr;

    std::string stream = "stream";
    string* pStream = &stream;
    launchArgs.stream = pStream;

    launchArgs.inputNum = 1;
    launchArgs.outputNum = 1;

    gert::Shape inferShape;
    vector<gert::Shape> shape;
    shape.push_back(inferShape);
    launchArgs.inputShapes = shape;
    launchArgs.outputShapes = shape;

    std::string inputAddr = "inputAddr";
    string* pInputAddr = &inputAddr;
    vector<void*> inputAddrs;
    inputAddrs.push_back(pInputAddr);
    launchArgs.inputAddrs = inputAddrs;

    std::string outputAddr = "outputAddr";
    string* pOutputAddr = &outputAddr;
    vector<void*> outputAddrs;
    outputAddrs.push_back(pOutputAddr);
    launchArgs.outputAddrs = outputAddrs;

    MOCKER(GetHcomOpLaunchArgs)
    .stubs()
    .with(any(), outBound(launchArgs))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HcclAllGather)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcomOpInputStruct inputStruct;
    std::vector<void*> output = {&inputStruct};
    auto kernelHolder = gert::KernelRunContextFaker()
                        .KernelIONum(0,1)
                        .Outputs(output)
                        .Build();
    auto context = kernelHolder.GetContext<gert::KernelContext>();

    PrepareHcomKernel(context);
    HcomDestroy();
    remove(file_name_t);
    GlobalMockObject::verify();
}

TEST_F(HcomLoweringTest, st_makeSureInput)
{
    ge::NodePtr node;
    gert::LowerInput lower_input;

    std::vector<gert::bg::ValueHolderPtr> input_shapes;
    gert::bg::ValueHolderPtr inputShape;
    input_shapes.push_back(inputShape);
    input_shapes.push_back(inputShape);
    input_shapes.push_back(inputShape);
    input_shapes.push_back(inputShape);
    input_shapes.push_back(inputShape);
    std::vector<gert::bg::DevMemValueHolderPtr> input_addrs;
	gert::bg::DevMemValueHolderPtr inputAddrs;
    input_addrs.push_back(inputAddrs);
    input_addrs.push_back(inputAddrs);
    input_addrs.push_back(inputAddrs);
    input_addrs.push_back(inputAddrs);
    input_addrs.push_back(inputAddrs);
    lower_input.input_addrs = input_addrs;
    lower_input.input_shapes = input_shapes;
    gert::LoweringGlobalData global_data;
    lower_input.global_data = &global_data;

    MakeSureCommAlltoAllInput(node, lower_input);
}

TEST_F(HcomLoweringTest, st_node_remote_ref_read)
{
    ge::ComputeGraph graph("test_graph");
    ge::OpDescPtr op;
    std::string tempStr = "test_group";
    ge::AttrUtils::SetStr(op, "group", tempStr);
    HcclDataType dataType = HCCL_DATA_TYPE_INT8;
    ge::AttrUtils::SetInt(op, "dataType", dataType);
    op->SetType(HCCL_KERNEL_OP_TYPE_REMOTE_REF_READ);
    auto remoteNode = graph.AddNode(op);

    gert::LowerInput lowerInput;
    gert::bg::ValueHolderPtr valuePtr;
	gert::bg::DevMemValueHolderPtr memValuePtr;
    for (int i = 0; i < 3; i++) {
        lowerInput.input_addrs.push_back(memValuePtr);
        lowerInput.input_shapes.push_back(valuePtr);
    }
    gert::LoweringGlobalData global_data;
    lowerInput.global_data = &global_data;
    LoweringRemoteNode(remoteNode, lowerInput);
}

TEST_F(HcomLoweringTest, st_extract_remote_read_addrinfos)
{
    HcclResult ret;
    HcomOpLaunchArgs launchArgs;
    launchArgs.inputNum = 1;
    launchArgs.inputAddrs.resize(1);
    launchArgs.outputAddrs.resize(1);
    launchArgs.inputAddrs[0] = (char*)malloc(1024);
    memset_s(launchArgs.inputAddrs[0], 1024, 0, 1024);
    launchArgs.outputAddrs[0] = (char*)malloc(1024);
    memset_s(launchArgs.outputAddrs[0], 1024, 0, 1024);
    vector<HcomRemoteAccessAddrInfo> addrInfos;
    gert::Shape inshape;
    inshape.AppendDim(2);
    inshape.AppendDim(2);
    launchArgs.inputShapes.push_back(inshape);
    ret = ExtractRemoteReadAddrInfos(launchArgs, addrInfos);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    free(launchArgs.inputAddrs[0]);
    free(launchArgs.outputAddrs[0]);
}


TEST_F(HcomLoweringTest, st_extract_remote_read_offset_addrinfos)
{
    HcclResult ret;
    HcomOpLaunchArgs launchArgs;
    launchArgs.inputNum = 3;
    launchArgs.inputAddrs.resize(3);
    launchArgs.inputAddrs[0] = (char*)malloc(1024);
    memset_s(launchArgs.inputAddrs[0], 1024, 0, 1024);
    launchArgs.inputAddrs[1] = (char*)malloc(1024);
    memset_s(launchArgs.inputAddrs[1], 1024, 0, 1024);
    launchArgs.inputAddrs[2] = (char*)malloc(1024);
    memset_s(launchArgs.inputAddrs[2], 1024, 0, 1024);
    vector<HcomRemoteAccessAddrInfo> addrInfos;
    gert::Shape inshape;
    inshape.AppendDim(2);
    inshape.AppendDim(2);
    launchArgs.inputShapes.push_back(inshape);
    ret = ExtractRemoteAddrInfosWithOffset(launchArgs, addrInfos);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    free(launchArgs.inputAddrs[0]);
    free(launchArgs.inputAddrs[1]);
    free(launchArgs.inputAddrs[2]);
}


TEST_F(HcomLoweringTest, st_extract_remote_scatter_write_addrinfos)
{
    HcclResult ret;
    HcomOpLaunchArgs launchArgs;
    launchArgs.inputNum = 3;
    launchArgs.inputAddrs.resize(3);
    launchArgs.inputAddrs[0] = (char*)malloc(1024);
    memset_s(launchArgs.inputAddrs[0], 1024, 0, 1024);
    launchArgs.inputAddrs[1] = (char*)malloc(1024);
    memset_s(launchArgs.inputAddrs[1], 1024, 0, 1024);
    launchArgs.inputAddrs[2] = (char*)malloc(1024);
    memset_s(launchArgs.inputAddrs[2], 1024, 0, 1024);
    vector<HcomRemoteAccessAddrInfo> addrInfos;
    gert::Shape inshape;
    inshape.AppendDim(2);
    inshape.AppendDim(2);
    launchArgs.inputShapes.push_back(inshape);
    ret = ExtractRemoteScatterWriteAddrInfos(launchArgs, addrInfos);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    launchArgs.inputNum = 2;
    ret = ExtractRemoteScatterWriteAddrInfos(launchArgs, addrInfos);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    launchArgs.inputNum = 1;
    ret = ExtractRemoteScatterWriteAddrInfos(launchArgs, addrInfos);
    EXPECT_EQ(ret, HCCL_E_PARA);

    free(launchArgs.inputAddrs[0]);
    free(launchArgs.inputAddrs[1]);
    free(launchArgs.inputAddrs[2]);
}

TEST_F(HcomLoweringTest, st_hcomLaunchKernel_allreduce_sess)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_hcomLaunchKernel_allreduce_sess.json";
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
    u32 ret1 = hrtSetDevice(0);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcomLaunchKernel_allreduce_sess.json";
    char* rank_ID = "0";

    ret1 = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    HcomOpLaunchArgs launchArgs;
    HcomOpAttr opAttr;
    memset_s((void *)&opAttr, sizeof(HcomOpAttr), 0, sizeof(HcomOpAttr));
    opAttr.dataType = HCCL_DATA_TYPE_INT8;
    opAttr.opType = HcomOpType::HCOM_ALL_REDUCE;
    strcpy(opAttr.group, "hccl_world_group");
    launchArgs.opAttr = opAttr;

    std::string stream = "stream";
    string* pStream = &stream;
    launchArgs.stream = pStream;

    launchArgs.inputNum = 1;
    launchArgs.outputNum = 1;

    gert::Shape inferShape;
    vector<gert::Shape> shape;
    shape.push_back(inferShape);
    launchArgs.inputShapes = shape;
    launchArgs.outputShapes = shape;

    std::string inputAddr = "inputAddr";
    string* pInputAddr = &inputAddr;
    vector<void*> inputAddrs;
    inputAddrs.push_back(pInputAddr);
    launchArgs.inputAddrs = inputAddrs;

    std::string outputAddr = "outputAddr";
    string* pOutputAddr = &outputAddr;
    vector<void*> outputAddrs;
    outputAddrs.push_back(pOutputAddr);
    launchArgs.outputAddrs = outputAddrs;

    MOCKER(GetHcomOpLaunchArgs)
    .stubs()
    .with(any(), outBound(launchArgs))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HcclAllReduce)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcomOpInputStruct inputStruct;
    std::vector<void*> output = {&inputStruct};
    auto kernelHolder = gert::KernelRunContextFaker()
                        .KernelIONum(0,1)
                        .Outputs(output)
                        .Build();
    auto context = kernelHolder.GetContext<gert::KernelContext>();

    PrepareHcomKernel(context);
    HcomDestroy();
    remove(file_name_t);
    GlobalMockObject::verify();
}

TEST_F(HcomLoweringTest, st_hcomLaunchKernel_allreduce_large_sess)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_hcomLaunchKernel_allreduce_large_sess.json";
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
    u32 ret1 = hrtSetDevice(0);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcomLaunchKernel_allreduce_large_sess.json";
    char* rank_ID = "0";

    ret1 = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    HcomOpLaunchArgs launchArgs;
    HcomOpAttr opAttr;
    opAttr.dataType = HCCL_DATA_TYPE_INT8;
    opAttr.opType = HcomOpType::HCOM_ALL_REDUCE;
    strcpy(opAttr.group, "hccl_world_group");
    launchArgs.opAttr = opAttr;

    std::string stream = "stream";
    string* pStream = &stream;
    launchArgs.stream = pStream;

    launchArgs.inputNum = 1;
    launchArgs.outputNum = 1;

    gert::Shape inferShape;
    vector<gert::Shape> shape;
    shape.push_back(inferShape);
    launchArgs.inputShapes = shape;
    launchArgs.outputShapes = shape;

    std::string inputAddr = "inputAddr";
    string* pInputAddr = &inputAddr;
    vector<void*> inputAddrs;
    inputAddrs.push_back(pInputAddr);
    launchArgs.inputAddrs = inputAddrs;

    std::string outputAddr = "outputAddr";
    string* pOutputAddr = &outputAddr;
    vector<void*> outputAddrs;
    outputAddrs.push_back(pOutputAddr);
    launchArgs.outputAddrs = outputAddrs;

    MOCKER(GetHcomOpLaunchArgs)
    .stubs()
    .with(any(), outBound(launchArgs))
    .will(returnValue(HCCL_SUCCESS));

    uint64_t count = 300*1024*1024;
    MOCKER(GetCountByShape)
    .stubs()
    .with(any(), any(), outBound(count))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HcclAllReduce)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcomOpInputStruct inputStruct;
    std::vector<void*> output = {&inputStruct};
    auto kernelHolder = gert::KernelRunContextFaker()
                        .KernelIONum(0,1)
                        .Outputs(output)
                        .Build();
    auto context = kernelHolder.GetContext<gert::KernelContext>();

    PrepareHcomKernel(context);
    HcomDestroy();
    remove(file_name_t);
    GlobalMockObject::verify();
}

TEST_F(HcomLoweringTest, st_hcomLaunchKernel_allreduce_multi_input_sess)
{
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./st_hcomLaunchKernel_allreduce_multi_input_sess.json";
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
    u32 ret1 = hrtSetDevice(0);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    char* rank_table_file = "./st_hcomLaunchKernel_allreduce_multi_input_sess.json";
    char* rank_ID = "0";

    ret1 = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    HcomOpLaunchArgs launchArgs;
    HcomOpAttr opAttr;
    memset_s((void *)&opAttr, sizeof(HcomOpAttr), 0, sizeof(HcomOpAttr));
    opAttr.dataType = HCCL_DATA_TYPE_INT8;
    opAttr.opType = HcomOpType::HCOM_ALL_REDUCE;
    strcpy(opAttr.group, "hccl_world_group");
    launchArgs.opAttr = opAttr;

    std::string stream = "stream";
    string* pStream = &stream;
    launchArgs.stream = pStream;

    launchArgs.inputNum = 2;
    launchArgs.outputNum = 2;

    gert::Shape inferShape;
    vector<gert::Shape> shape;
    shape.push_back(inferShape);
    launchArgs.inputShapes = shape;
    launchArgs.outputShapes = shape;

    std::string inputAddr = "inputAddr";
    string* pInputAddr = &inputAddr;
    vector<void*> inputAddrs;
    inputAddrs.push_back(pInputAddr);
    inputAddrs.push_back(pInputAddr);
    launchArgs.inputAddrs = inputAddrs;

    std::string outputAddr = "outputAddr";
    string* pOutputAddr = &outputAddr;
    vector<void*> outputAddrs;
    outputAddrs.push_back(pOutputAddr);
    outputAddrs.push_back(pOutputAddr);
    launchArgs.outputAddrs = outputAddrs;

    MOCKER(GetHcomOpLaunchArgs)
    .stubs()
    .with(any(), outBound(launchArgs))
    .will(returnValue(HCCL_SUCCESS));

    uint64_t count = 1024;
    MOCKER(GetCountByShape)
    .stubs()
    .with(any(), any(), outBound(count))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HcclAllReduce)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcomOpInputStruct inputStruct;
    std::vector<void*> output = {&inputStruct};
    auto kernelHolder = gert::KernelRunContextFaker()
                        .KernelIONum(0,1)
                        .Outputs(output)
                        .Build();
    auto context = kernelHolder.GetContext<gert::KernelContext>();

    PrepareHcomKernel(context);
    HcomDestroy();
    remove(file_name_t);
    GlobalMockObject::verify();
}

TEST_F(HcomLoweringTest, ut_HcomAllToAllGetOpAttr_test)
{
    ge::NodePtr node;
    HcomOpAttr opAttr;
    EXPECT_EQ(HcomAllToAllGetOpAttr(node, opAttr), HCCL_SUCCESS);
}

TEST_F(HcomLoweringTest, ut_hcomLaunchKernel_allGatherv2_When_Normal_Expect_ReturnlsHCCL_SUCCESS)
{
#ifdef OPEN_BUILD_PROJECT
#undef OPEN_BUILD_PROJECT
#endif
    nlohmann::json rank_table = rank_table_910_1server_1rank;
    char file_name_t[] = "./ut_hcomLaunchKernel_allGather.json";
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
    u32 ret1 = hrtSetDevice(0);
    EXPECT_EQ(ret1, HCCL_SUCCESS);

    char* rank_table_file = "./ut_hcomLaunchKernel_allGather.json";
    char* rank_ID = "0";

    ret1 = HcomInitByFile(rank_table_file, rank_ID);

    AsyncAnyValue any_value;

    HcomOpLaunchArgs launchArgs0;
    HcomOpAttr opAttr;
    opAttr.dataType = HCCL_DATA_TYPE_INT8;
    opAttr.opType = HcomOpType::HCOM_ALL_GATHER;
    strcpy(opAttr.group, "hccl_world_group");
    launchArgs0.opAttr = opAttr;
    any_value.data.pointer = &launchArgs0;

    MOCKER(GetHcomOpLaunchArgs)
    .stubs()
    .with(any(), outBound(launchArgs0))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HcclAllGather).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcomGetDeviceType).stubs().with(any()).will(returnValue(DevType::DEV_TYPE_910_95));

    MOCKER(HcomAllGatherKernel).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    HcomOpInputStruct inputStruct;
    std::vector<void*> output = {&inputStruct};
    auto kernelHolder = gert::KernelRunContextFaker()
                        .KernelIONum(0,1)
                        .Outputs(output)
                        .Build();
    auto context = kernelHolder.GetContext<gert::KernelContext>();

    PrepareHcomKernel(context);
    HcomDestroy();
    remove(file_name_t);
    GlobalMockObject::verify();
#define OPEN_BUILD_PROJECT
}