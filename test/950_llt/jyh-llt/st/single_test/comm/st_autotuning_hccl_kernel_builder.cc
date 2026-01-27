#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#define private public
#define protected public
#include "hcom_all_reduce_fusion.h"
#include "plugin_manager.h"
#include "hcom_graph_optimizer.h"
#include "auto_tuning_plugin.h"
#include "auto_tuning_hcom_ops_kernel_info_store.h"
#include "hcom_ops_kernel_info_store.h"
#include "ops_kernel_info_store_base.h"
#include "ops_kernel_info_store.h"
#include "hcom_ops_stores.h"
#include "auto_tuning_hcom_all_reduce_fusion.h"
#include "tuning_utils.h"
#include "auto_tuning_hcom_ops_kernel_builder.h"
#include "hcom_op_utils.h"
#undef private
#undef protected

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "hccl_comm_pub.h"
#include "sal.h"
#include "hccl_impl.h"
#include "llt_hccl_stub_pub.h"
#include "externalinput.h"
#include "config.h"
#include "topoinfo_ranktableParser_pub.h"
#include "external/ge/ge_api_types.h" // ge对内options
#include "framework/common/ge_types.h" // ge对外options
#include "hcom.h"
#include "../inc/hccl/hcom_executor.h"
#include "compute_graph.h"
#include <iostream>
#include <fstream>

using namespace std;
using namespace hccl;

class AutoTuningHcclKernelBuilderTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--AutoTuningHcclKernelBuilderTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {

        std::cout << "\033[36m--AutoTuningHcclKernelBuilderTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
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

TEST_F(AutoTuningHcclKernelBuilderTest, st_AutoTuning_Ops_Kernel_Builder_E2E)
{
    bool bErr = false;
    std::map<std::string,std::string> options;
    options.insert(pair<string,string> (ge::BUILD_MODE,"tuning"));
    options.insert(pair<string,string> (ge::TUNING_PATH,"./"));
    options.insert(pair<string,string> ("ge.jobType","4"));

    ge::Status ge_ret;
    HCCL_INFO("test Initialize.");
    ge_ret = Initialize(options);
    EXPECT_EQ(ge_ret, ge::SUCCESS);

    std::map<string, OpsKernelInfoStorePtr> opKernInfos;
    std::map<string, GraphOptimizerPtr> graphOptimizers;
    GetGraphOptimizerObjs(graphOptimizers);
    GetOpsKernelInfoStores(opKernInfos);
    auto iter1 = graphOptimizers.find(HCCL_GRAPH_OPTIMIZER_NAME);
    if (iter1 == graphOptimizers.end()) {
        bErr = true;
    }
    EXPECT_EQ(bErr, false);
    if (bErr) {
        return;
    }
    auto iter2 = opKernInfos.find(AUTOTUNE_HCCL_OPS_LIB_NAME);
    if (iter2 == opKernInfos.end()) {
        bErr = true;
    }
    EXPECT_EQ(bErr, false);
    if (bErr) {
        return;
    }

    OpsKernelInfoStorePtr opsKernerInfoStorePtr = opKernInfos[AUTOTUNE_HCCL_OPS_LIB_NAME];
    GraphOptimizerPtr graphOptimizerPtr = graphOptimizers[HCCL_GRAPH_OPTIMIZER_NAME];

    HCCL_INFO("test opsKernerInfoStorePtr Initialize.");
    ge_ret = opsKernerInfoStorePtr->Initialize(options);
    EXPECT_EQ(ge_ret, ge::SUCCESS);
    HCCL_INFO("test graphOptimizerPtr Initialize.");
    ge_ret = graphOptimizerPtr->Initialize(options, nullptr);
    EXPECT_EQ(ge_ret, ge::SUCCESS);

    ge::ComputeGraphPtr compute_graph = std::make_shared<ge::ComputeGraph>("test_graph");
    ge::OpDescPtr opDescPtr = nullptr;
    ge::OpDesc op;
    ge::NodePtr node1 = compute_graph->AddNode(opDescPtr);
    ge::NodePtr node2 = compute_graph->AddNode(opDescPtr);
    node1->GetOutDataAnchor(0)->LinkTo(node2->GetInDataAnchor(0));
    node2->GetOutDataAnchor(0)->LinkTo(node1->GetInDataAnchor(0));
    const string type = HCCL_KERNEL_OP_TYPE_ALLREDUCE;
    op.SetType(type);
    ge::AttrUtils::HasAttr(opDescPtr, "DUMMY_SET_TRUE_GROUP");
    ge::AttrUtils::SetStr(opDescPtr, "reduction", "sum");
    ge::AttrUtils::SetStr(opDescPtr, "group", "hccl_world_group");
    ge::AttrUtils::SetInt(opDescPtr, "fusion", 2);
    ge::AttrUtils::SetInt(opDescPtr, "fusion_id", 2);
    ge::AttrUtils::SetStr(opDescPtr, "_batch_label", "batchlabel");

    HCCL_INFO("test OptimizeGraphPrepare.");
    ge_ret = graphOptimizerPtr->OptimizeGraphPrepare(*compute_graph);
    EXPECT_EQ(ge_ret, ge::SUCCESS);
    HCCL_INFO("test OptimizeGraphPrepare.");

    std::vector<GradientDataInfo> recordInfos(2);
    recordInfos[0].dataSize = 1024;
    recordInfos[0].dataType = "float";
    recordInfos[0].graphId = 0;
    recordInfos[0].groupName = "hccl_world_group";
    recordInfos[0].gradientNodeName = "test_gradientNodeName_0";
    recordInfos[0].traceNodeName = "test_traceNodeName_0";
    recordInfos[0].allReduceNodeName = "test_allReduceNodeName_0";
    recordInfos[1].dataSize = 2048;
    recordInfos[1].dataType = "float";
    recordInfos[1].graphId = 0;
    recordInfos[1].groupName = "hccl_world_group";
    recordInfos[1].gradientNodeName = "test_gradientNodeName_1";
    recordInfos[1].traceNodeName = "test_traceNodeName_1";
    recordInfos[1].allReduceNodeName = "test_allReduceNodeName_1";
    MOCKER_CPP(&AutoTuningHcomAllReduceFusion::RecordGradientDataInfo)
    .stubs()
    .with(any(), outBound(recordInfos))
    .will(returnValue(HCCL_SUCCESS));
    ge_ret = graphOptimizerPtr->OptimizeOriginalGraph(*compute_graph);
    ge::AttrUtils::HasAttr(opDescPtr, "DUMMY_SET_FALSE_GROUP"); 
    GlobalMockObject::verify();
    EXPECT_EQ(ge_ret, ge::SUCCESS);
    EXPECT_EQ(recordInfos.size(), 2);
    HCCL_INFO("test OptimizeFusedGraph.");
    u64 streamNumber = 4;
    MOCKER(HcomGetWorkspaceSubStreamNum)
    .stubs()
    .with(any(), outBound(streamNumber))
    .will(returnValue(HCCL_SUCCESS));
    ge_ret = graphOptimizerPtr->OptimizeFusedGraph(*compute_graph);
    EXPECT_EQ(ge_ret, ge::SUCCESS);
    HCCL_INFO("test OptimizeWholeGraph.");
    ge_ret = graphOptimizerPtr->OptimizeWholeGraph(*compute_graph);
    EXPECT_EQ(ge_ret, ge::SUCCESS);
    HCCL_INFO("test GetAttributes.");
    ge::GraphOptimizerAttribute attrs;
    ge_ret = graphOptimizerPtr->GetAttributes(attrs);
    EXPECT_EQ(ge_ret, ge::SUCCESS);
    EXPECT_EQ(attrs.engineName, HCCL_OPS_ENGIN);
    EXPECT_EQ(attrs.scope, ge::UNIT);

    HCCL_INFO("test CheckSupported.");
    std::string reason;
    bool bRet = opsKernerInfoStorePtr->CheckSupported(node1->GetOpDesc(), reason);
    EXPECT_EQ(bRet, true);

    HCCL_INFO("test GetAllOpsKernelInfo.");
    std::map<string, ge::OpInfo> infos;
    opsKernerInfoStorePtr->GetAllOpsKernelInfo(infos);
    EXPECT_EQ(infos.size(), AUTO_TUNING_HCOM_SUPPORTED_OP_TYPE.size());

    AutoTuningHcomOpsKernelBuilder autoTuningHcomKernelBuilder;
    HCCL_INFO("test CalcOpRunningParam.");
    ge_ret = autoTuningHcomKernelBuilder.CalcOpRunningParam(*node1);
    EXPECT_EQ(ge_ret, ge::SUCCESS);
    int64_t streamNum;
    ge::AttrUtils::GetInt(node1->GetOpDesc(), "used_stream_num", streamNum);
    EXPECT_EQ(streamNum, 0);
    std::vector<int64_t> workSpaceBytes = node1->GetOpDesc()->GetWorkspaceBytes();
    EXPECT_EQ(workSpaceBytes[0], 0);

    HCCL_INFO("test GenerateTask.");
    ge::Buffer tempBuffer;
    ge::RunContext runContext_dummy;
    std::vector<domi::TaskDef> taskDefList;
    int64_t streamId = 10000;
    node1->GetOpDesc()->SetStreamId((s64)streamId);
    
    ge_ret = autoTuningHcomKernelBuilder.GenerateTask(*node1, runContext_dummy, taskDefList);
    EXPECT_EQ(ge_ret, ge::SUCCESS);

    u32 result_type = taskDefList[0].type();
    u32 result_stream_id = taskDefList[0].stream_id();
    std::string result_hccl_hccl_type = taskDefList[0].mutable_kernel_hccl()->hccl_type();
    EXPECT_EQ(result_type, RT_MODEL_TASK_HCCL);
    EXPECT_EQ(result_stream_id, streamId);
    EXPECT_EQ(result_hccl_hccl_type, HCCL_KERNEL_OP_TYPE_ALLREDUCE);

    HCCL_INFO("test LoadTask.");
    s8* sendbuf = (s8*)sal_malloc(10 * sizeof(float));
    sal_memset(sendbuf, 10 * sizeof(float), 0, 10 * sizeof(float));
    s8* recv = (s8*)sal_malloc(10 * sizeof(float));
    sal_memset(recv, 10 * sizeof(float), 0, 10 * sizeof(float));
    rtStream_t stream;
    rtError_t rt_ret = rtStreamCreate(&stream, 5);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE); 

    ge::GETaskInfo task;
    ge::GETaskKernelHcclInfo hcclInfo;
    task.kernelHcclInfo.push_back(hcclInfo); 
    task.kernelHcclInfo[0].count=10;
    task.kernelHcclInfo[0].dataType=HCCL_DATA_TYPE_FP32;
    task.kernelHcclInfo[0].hccl_type=HCCL_KERNEL_OP_TYPE_ALLREDUCE;    
    task.kernelHcclInfo[0].inputDataAddr=sendbuf;
    task.kernelHcclInfo[0].outputDataAddr=recv;
    task.kernelHcclInfo[0].opType=HCCL_REDUCE_SUM;
    task.kernelHcclInfo[0].rootId=0;
    task.kernelHcclInfo[0].hcclQosCfg=INVALID_QOSCFG; 
    task.type = RT_MODEL_TASK_HCCL;
    task.stream = stream;
    AUTO_TUNING_HCCL_KERNEL_INFO_PRIVATE_DEF privateDefBuf = {2, 0, HCCL_DATA_TYPE_INT8}; 
    task.privateDef = (void *)&privateDefBuf.rankSize;
    task.privateDefLen = sizeof(AUTO_TUNING_HCCL_KERNEL_INFO_PRIVATE_DEF); 

    ge_ret = opsKernerInfoStorePtr->LoadTask(task);
    EXPECT_EQ(ge_ret, ge::SUCCESS);

    HCCL_INFO("test opsKernerInfoStorePtr Finalize.");
    ge_ret = opsKernerInfoStorePtr->Finalize();
    EXPECT_EQ(ge_ret, ge::SUCCESS);
    HCCL_INFO("test graphOptimizerPtr Finalize.");
    ge_ret = graphOptimizerPtr->Finalize();
    EXPECT_EQ(ge_ret, ge::SUCCESS);

    HCCL_INFO("test Finalize.");
    ge_ret = Finalize();
    EXPECT_EQ(ge_ret, ge::SUCCESS);

    rt_ret = rtStreamDestroy(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sal_free(sendbuf);
    sal_free(recv);

    std::string summaryPath = "gradient_summary.csv";
    remove(summaryPath.c_str());
}

TEST_F(AutoTuningHcclKernelBuilderTest, stAddFusionMapInFusionJson)
{
    HcclResult ret;
    std::string filePath = "./ut_Ascend910A_gradient_fusion_1.json";
    std::string fusionHash = "3047522271241092779";
    std::string socVersion = "Ascend910A";

    MOCKER(HcomOpUtils::CreateFusionConfigVersion)
    .stubs()
    .with(outBound(socVersion))
    .will(returnValue(HCCL_SUCCESS));

    AutoTuningHcomAllReduceFusion object;
    ret = object.AddFusionMapInFusionJson(fusionHash);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    filePath = "./ut_Ascend910A_gradient_fusion_2.json";
    char file_name_1[] = "./ut_Ascend910A_gradient_fusion_2.json";
    std::ofstream outfile_empty(file_name_1, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile_empty.is_open()) {
        HCCL_INFO("open %s success", file_name_1);
    } else {
        HCCL_ERROR("open %s failed", file_name_1);
    }
    outfile_empty.close();

    MOCKER_CPP(&AutoTuningHcomAllReduceFusion::GetFusionWorkPath)
    .stubs()
    .with(outBound(filePath))
    .will(returnValue(HCCL_SUCCESS));

    //AutoTuningHcomAllReduceFusion object;
    ret = object.AddFusionMapInFusionJson(fusionHash);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
    remove(file_name_1);
}