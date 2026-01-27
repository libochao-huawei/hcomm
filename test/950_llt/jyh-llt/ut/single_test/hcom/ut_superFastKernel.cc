#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#define private public
#define protected public
#include "hcom_all_reduce_fusion.h"
#include "hcom_broadcast_fusion.h"
#include "hvd_all_reduce_fusion.h"
#include "hcom_reduce_fusion.h"
#include "hcom_alltoallvc_fusion.h"
#include "hcom_allgather_fusion.h"
#include "hcom_reducescatter_fusion.h"
#include "plugin_manager.h"
#include "hcom_fusion_optimizer.h"
#include "offline_build_config_parse.h"
#include "hcom_graph_optimizer.h"
#include "hvd_graph_optimizer.h"
#include "hcom_op_utils.h"
#undef private
#undef protected
#include "../inc/hccl/base.h"
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
#include "v80_rank_table.h"
#include "hcom_op_utils.h"
#include "hcom_ops_kernel_info_store.h"
#include "external/ge/ge_api_types.h"   // ge对内options
#include "framework/common/ge_types.h"  // ge对外options
#include "graph/ge_local_context.h"
#include "hcom_pub.h"
#include "hcom.h"

#include "graph/debug/ge_attr_define.h"
#include "graph/utils/graph_utils.h"
#include "graph/utils/node_utils.h"
#include "graph/utils/attr_utils.h"
#include "graph/node.h"
#include "graph/op_desc.h"
#include "graph/utils/tensor_utils.h"
#include "graph/ge_tensor.h"

#include "evaluator.h"
#include "model.h"
#include "cluster.h"

#include <iostream>
#include <fstream>
#include "graph/ge_context.h"
#include "workflow_pub.h"
#include "topoinfo_struct.h"
#include "hccl_ip_address.h"
#include "dlra_function.h"
#include "dltdt_function.h"
#include "acl/acl.h"
#include "hccl.h"
#define private public
#define protected public
#include "hccl_impl.h"
#include "config.h"
#include "hccl_communicator.h"
#include "hccl_communicator_attrs.h"
#include "network_manager_pub.h"
#include "transport_base_pub.h"
#include "comm_impl.h"
#include "comm_mesh_pub.h"
#include "dispatcher_pub.h"
#include "externalinput.h"
#include "coll_alg_operator.h"
#include "all_gather_operator.h"
#include "reduce_operator.h"
#include "reduce_scatter_operator.h"
#include "broadcast_operator.h"
#include "coll_comm_executor.h"
#include "comm_base_pub.h"
#include "task_abort_handler_pub.h"
#include "adapter_rts.h"
#include "coll_reduce_scatter_v_executor.h"
#include "coll_all_gather_v_executor.h"
#include "coll_reduce_scatter_v_mesh_opbase_executor.h"
#include "coll_all_gather_v_mesh_executor.h"
#include "coll_all_gather_v_executor.h"
#include "coll_all_gatherv_mesh_aiv_executor.h"
#include "coll_all_gatherv_mesh_aiv_smallcount_executor.h"
#include "coll_all_gather_v_mesh_graph_executor.h"
#include "coll_reduce_scatter_v_aiv_big_count_executor.h"
#include "coll_reduce_scatter_v_mesh_aiv_smallcount_executor.h"
#include "coll_reduce_scatter_v_for_310p_ring_executor.h"
#include "coll_all_gatherv_for_310p_executor.h"
#undef private
#undef protected
#include "tbe_vector_reduce.h"
#include "tbe_crack_cleared.h"
#include "param_check_pub.h"
#include "base.h"
#include "adapter_rts_common.h"
#include "coll_all_reduce_ring_executor.h"
#include "coll_all_gather_ring_executor.h"
#include "coll_aligned_all_reduce_double_ring_for_910_93_executor.h"
#include "all_gather_v_operator.h"
#include "reduce_scatter_v_operator.h"
#include "profiling_manager.h"
#include "hcom_all_reduce_fusion.h"
#include "plugin_manager.h"
#include "hcom_graph_optimizer.h"
#include "auto_tuning_plugin.h"
#include "auto_tuning_hcom_ops_kernel_info_store.h"
#include "auto_tuning_hcom_ops_kernel_builder.h"
#include "hcom_ops_kernel_info_store.h"
#include "ops_kernel_info_store_base.h"
#include "ops_kernel_info_store.h"
#include "hcom_ops_stores.h"
#include "auto_tuning_hcom_all_reduce_fusion.h"
#include "tuning_utils.h"
#include "hccl_ex.h"
#include "op_base.h"
#include "hcom_graph_superkernel.h"
#include "hcom_graph_mc2.h"
#include "external/hcom/hcom_topo_info.h"
#include "hcom_graph_optimizer.h"

using namespace std;
using namespace hccl;

namespace hccl {
extern HcclResult GetCountFromOpDesc(
    const ge::OpDescPtr &op, const std::string &sCollectiveType, HcclDataType dataType, u64 &count, u32 rankSize);
}

class SuperFastKernelTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        DlRaFunction::GetInstance().DlRaFunctionInit();
        std::cout << "\033[36m--SuperFastKernelTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {

        std::cout << "\033[36m--SuperFastKernelTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum).stubs().with(any(), outBound(portNum)).will(returnValue(HCCL_SUCCESS));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
};

RankTable_t get_rank_table_rank_nic_device()
{
    RankTable_t rankTable;
    rankTable.deviceNum = 1;

    rankTable.serverNum = 1;
    rankTable.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    rankTable.nicNum = 1;
    rankTable.nicNames.push_back("eth0");
    rankTable.rankNum = 1;

    // rank 信息
    RankInfo_t rank;
    rank.rankId = 0;
    rank.serverIdx = 0;
    rank.serverId = "192.168.1.1";
    rank.deviceInfo.devicePhyId = 0;
    rank.deviceInfo.deviceIp.push_back(HcclIpAddress("172.17.10.1"));
    rankTable.rankList.push_back(rank);

    return rankTable;
}

HcclResult HcomGetRankSizeStub(const char *group, u32 *rankSize)
{
    *rankSize = 1;
    return HCCL_SUCCESS;
}

// TODO: To figure out why the ut_hcom_GenerateGroupHash must be located before 
//       ut_mc2_GetAddrFromDesc to avoid the LLT failed for latter while calling Initialize.
TEST_F(SuperFastKernelTest, ut_hcom_GenerateGroupHash)
{
    std::string group = "group1";
    std::string groupHash;
    GenerateGroupHash(group, groupHash);
    EXPECT_TRUE(!groupHash.empty());
}

TEST_F(SuperFastKernelTest, ut_mc2_GetAddrFromDesc)
{
    MOCKER(GetExternalInputHcclEnableEntryLog)
    .stubs()
    .with(any())
    .will(returnValue(true));
    
    std::map<std::string, std::string> options;
    options.insert(pair<string, string>(ge::TUNING_PATH, "./"));
    options.insert(pair<string, string>("ge.jobType", "4"));

    ge::Status ge_ret;
    HCCL_INFO("test Initialize.");
    ge_ret = Initialize(options);
    EXPECT_EQ(ge_ret, ge::SUCCESS);

    std::string group = "group1";
    ge::OpDescPtr nodeGroup = std::make_shared<ge::OpDesc>();
    EXPECT_EQ(ge::AttrUtils::SetStr(nodeGroup, "group", group), true);
    u64 count;
    std::string sCollectiveType = HCCL_KERNEL_OP_TYPE_RECEIVE;
    std::vector<void *> contexts;
    u32 rankSize = 8;
    nodeGroup->SetType(HCCL_KERNEL_OP_TYPE_BROADCAST);
    MOCKER(HcomGetRankSize).stubs().with(any(), outBound(&rankSize)).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcomSetWorkspaceResource).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcomOpUtils::GetCountFromOpDescSuperkernel).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcomOpUtils::GetAivCoreLimit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcomGetAlgExecParam).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(HcomCreateSuperkernelResource(nodeGroup, contexts), ge::GRAPH_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(SuperFastKernelTest, ut_mc2_HcomCreateSuperkernelResource)
{
    DeviceMem context = DeviceMem::alloc(1024 * 1024);
    void *contextptr = context.ptr();
    MOCKER(HcclAllocComResourceByTiling)
        .stubs()
        .with(any(), any(), any(), outBoundP(&contextptr, sizeof(contextptr)))
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtStreamGetMode).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclCreateComResourceByComm)
        .stubs()
        .with(any(), any(), any(), outBoundP(&contextptr, sizeof(contextptr)))
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&ge::HcomTopoInfo::TryGetGroupTopoInfo).stubs().with(any()).will(returnValue(true));
    MOCKER_CPP(&hcclComm::GetAicpuOpStreamNotify).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&ge::HcomTopoInfo::SetGroupTopoInfo).stubs().with(any()).will(returnValue(ge::GRAPH_SUCCESS));
    std::map<std::string, std::string> options;
    options.insert(pair<string, string>(ge::TUNING_PATH, "./"));
    options.insert(pair<string, string>("ge.jobType", "4"));

    ge::Status ge_ret;
    HCCL_INFO("test Initialize.");
    ge_ret = Initialize(options);
    EXPECT_EQ(ge_ret, ge::SUCCESS);

    std::string group = "group1";
    const ge::OpDescPtr nodeGroup = std::make_shared<ge::OpDesc>();
    std::vector<void *> contexts;
    nodeGroup->SetType("MatmulAllReduce");
    rtStream_t stream;
    EXPECT_EQ(rtStreamCreate(&stream, 0), RT_ERROR_NONE);

    shared_ptr<std::vector<void *>> rt_resource_list = std::make_shared<std::vector<void *>>();
    rt_resource_list->push_back(stream);
    nodeGroup->SetExtAttr("_rt_resource_list", rt_resource_list);
    u64 count;
    std::string sCollectiveType = HCCL_KERNEL_OP_TYPE_RECEIVE;

    std::shared_ptr<hccl::hcclComm> comm;
    comm.reset(new (std::nothrow) hccl::hcclComm());
    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));

    MOCKER(HcomGetCommByGroup).stubs().with(any(), outBound(comm)).will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(HcomCreateSuperkernelResource(nodeGroup, contexts), ge::GRAPH_FAILED);
    rtStreamDestroy(stream);
}

TEST_F(SuperFastKernelTest, ut_hcom_HcomSelectAlg)
{
    MOCKER(HcomGetCommByGroup).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hcclComm::HcclSelectAlg).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hcclComm::HcclCalcBlockDim).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hcclComm::HcclGetAlgExecParam).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    std::string group = "group1";
    std::string groupHash;
    GenerateGroupHash(group, groupHash);
    bool isaiv;

    std::string algName = "ReduceScatterMeshAivExecutor";
    EXPECT_EQ(HcomSelectAlg(0, group.c_str(),
                  1024 * 1024,
                  HcclDataType::HCCL_DATA_TYPE_INT64,
                  HcclReduceOp::HCCL_REDUCE_SUM,
                  HcclCMDType::HCCL_CMD_ALLGATHER,
                  48,
                  isaiv,
                  algName),
        HCCL_SUCCESS);
    EXPECT_EQ(HcomSelectAlg(1, group.c_str(),
                  1024 * 1024,
                  HcclDataType::HCCL_DATA_TYPE_INT64,
                  HcclReduceOp::HCCL_REDUCE_SUM,
                  HcclCMDType::HCCL_CMD_ALLGATHER,
                  48, 
                  isaiv,
                  algName),
        HCCL_SUCCESS);
    u32 blockDim;
    EXPECT_EQ(HcomCalcBlockDim(group.c_str(),
                  HcclCMDType::HCCL_CMD_ALLGATHER,
                  1024 * 1024,
                  HcclDataType::HCCL_DATA_TYPE_INT64,
                  48,
                  algName,
                  blockDim),
        HCCL_SUCCESS);
    std::string tag;
    AivSuperKernelArgs aivKernelArgs;
    void *aivKernelArgs_Ptr = &aivKernelArgs;
    u64 len = 0;
    u32 aivCoreLimit = 8;
    DeviceMem output = DeviceMem::alloc(1024 * 1024);
    DeviceMem input = DeviceMem::alloc(1024 * 1024);
    EXPECT_EQ(HcomGetAlgExecParam(tag,
                group.c_str(),
                  1024 * 1024,
                  input.ptr(),
                  output.ptr(),
                  HcclCMDType::HCCL_CMD_ALLGATHER,
                  true,
                  HcclDataType::HCCL_DATA_TYPE_BFP16,
                  HcclReduceOp::HCCL_REDUCE_SUM,
                  aivKernelArgs_Ptr,
                  len,
                  aivCoreLimit),
        HCCL_SUCCESS);
}

TEST_F(SuperFastKernelTest, ut_GetAivCoreLimit)
{
    ge::ComputeGraph graph("test_graph");
    ge::OpDescPtr nodeGroup = nullptr;
    std::string group = HCCL_WORLD_GROUP;
    std::string vectorcoreNum = "1";
    EXPECT_EQ(ge::AttrUtils::SetStr(nodeGroup, "_op_vectorcore_num", vectorcoreNum), true);
    std::string sCollectiveType = HCCL_KERNEL_OP_TYPE_REDUCESCATTER;
    u32 aivCoreLimit;
    HcomOpUtils::GetAivCoreLimit(nodeGroup, sCollectiveType, aivCoreLimit);
    EXPECT_EQ(ge::AttrUtils::SetStr(nodeGroup, "vectorcore", group), true);
    ge::OpDescPtr nodeGroup1 = nullptr;
    EXPECT_EQ(ge::AttrUtils::SetStr(nodeGroup1, "group", group), true);
    HcomOpUtils::GetAivCoreLimit(nodeGroup1, sCollectiveType, aivCoreLimit);

    u64 count;
    sCollectiveType = HCCL_KERNEL_OP_TYPE_RECEIVE;
    HcclResult ret =
        HcomOpUtils::GetCountFromOpDescSuperkernel(nodeGroup, sCollectiveType, HcclDataType::HCCL_DATA_TYPE_INT64, count, 1);
    EXPECT_EQ(ret, HcclResult::HCCL_E_PARA);
    sCollectiveType = HCCL_KERNEL_OP_TYPE_REDUCESCATTER;
    ret =HcomOpUtils::GetCountFromOpDescSuperkernel(nodeGroup, sCollectiveType, HcclDataType::HCCL_DATA_TYPE_INT64, count, 1);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
    sCollectiveType = HCCL_KERNEL_OP_TYPE_ALLGATHER;
    ret = HcomOpUtils::GetCountFromOpDescSuperkernel(nodeGroup, sCollectiveType, HcclDataType::HCCL_DATA_TYPE_INT64, count, 1);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
    sCollectiveType = HCCL_KERNEL_OP_TYPE_ALLREDUCE;
    ret = HcomOpUtils::GetCountFromOpDescSuperkernel(nodeGroup, sCollectiveType, HcclDataType::HCCL_DATA_TYPE_INT64, count, 1);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
    sCollectiveType = HCCL_KERNEL_OP_TYPE_BROADCAST;
    ret = HcomOpUtils::GetCountFromOpDescSuperkernel(nodeGroup, sCollectiveType, HcclDataType::HCCL_DATA_TYPE_INT64, count, 1);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(SuperFastKernelTest, hcclComm_HcclSelectAlg)
{
    // 初始化通信域
    HcclCommParams comm_params;
    comm_params.rank = 0;
    comm_params.totalRanks = 1;
    comm_params.logicDevId = 0;
    comm_params.deviceType = DevType::DEV_TYPE_910;
    HcclResult ret = hcclComm::GetUniqueId(&comm_params.id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER_CPP(&HcclCommunicator::AllocAlgResource).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&CollAlgOperator::GetAivExecParam).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AlltoAllOperator::SetExcutorExtraInfo).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AlltoAllOperator::GetAlltoAllvcSendRecvInfo).stubs().will(returnValue(HCCL_SUCCESS));

    hcclComm comm;
    RankTable_t rankTable = get_rank_table_rank_nic_device();
    CommConfig commConfig("hccl_world_group");
    ret = comm.init(comm_params, commConfig, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    bool isAiv;
    std::string algName = "ReduceScatterMeshAivExecutor";

    std::string tag = "hcclCommTest";
    DeviceMem output = DeviceMem::alloc(1024 * 1024);
    DeviceMem input = DeviceMem::alloc(1024 * 1024);
    AivSuperKernelArgs aivKernelArgs;
    void *aivKernelArgs_Ptr = &aivKernelArgs;
    u64 len = 0;
    u32 aivCoreLimit = 8;
    EXPECT_EQ(comm.HcclSelectAlg(HcclCMDType::HCCL_CMD_ALLREDUCE,
                  1024 * 1024,
                  HcclDataType::HCCL_DATA_TYPE_INT64,
                  HcclReduceOp::HCCL_REDUCE_SUM,
                  48,
                  isAiv,
                  algName),
        HCCL_SUCCESS);
    u32 blockDim;
    EXPECT_EQ(comm.HcclCalcBlockDim(
                  HcclCMDType::HCCL_CMD_ALLREDUCE, 1024 * 1024, HcclDataType::HCCL_DATA_TYPE_INT64, 48, algName, blockDim),
        HCCL_SUCCESS);

    EXPECT_EQ(comm.HcclGetAlgExecParam(tag,
                  1024 * 1024,
                  input.ptr(),
                  output.ptr(),
                  HcclCMDType::HCCL_CMD_ALLREDUCE,
                  true,
                  HcclDataType::HCCL_DATA_TYPE_INT64,
                  HcclReduceOp::HCCL_REDUCE_SUM,
                  aivKernelArgs_Ptr,
                  len,
                  aivCoreLimit),
        HCCL_SUCCESS);

    algName = "ReduceScatterMeshAivExecutor";
    EXPECT_EQ(comm.HcclSelectAlg(HcclCMDType::HCCL_CMD_ALLTOALL,
                  1024 * 1024,
                  HcclDataType::HCCL_DATA_TYPE_INT64,
                  HcclReduceOp::HCCL_REDUCE_SUM,
                  48,
                  isAiv,
                  algName),
        HCCL_SUCCESS);
    EXPECT_EQ(comm.HcclCalcBlockDim(
                  HcclCMDType::HCCL_CMD_ALLTOALL, 1024 * 1024, HcclDataType::HCCL_DATA_TYPE_INT64, 48, algName, blockDim),
        HCCL_SUCCESS);
    EXPECT_EQ(comm.HcclGetAlgExecParam(tag,
                  1024 * 1024,
                  input.ptr(),
                  output.ptr(),
                  HcclCMDType::HCCL_CMD_ALLTOALL,
                  true,
                  HcclDataType::HCCL_DATA_TYPE_INT64,
                  HcclReduceOp::HCCL_REDUCE_SUM,
                  aivKernelArgs_Ptr,
                  len,
                  aivCoreLimit),
        HCCL_SUCCESS);

    isAiv = false;
    comm_params.deviceType = DevType::DEV_TYPE_910B;
    algName = "ReduceScatterMeshAivExecutor";
    hcclComm comm3;
    ret = comm3.init(comm_params, commConfig, rankTable);
    EXPECT_EQ(comm3.HcclSelectAlg(HcclCMDType::HCCL_CMD_REDUCE_SCATTER,
                  1024 * 1024,
                  HcclDataType::HCCL_DATA_TYPE_INT8,
                  HcclReduceOp::HCCL_REDUCE_SUM,
                  48,
                  isAiv,
                  algName),
        HCCL_SUCCESS);
    EXPECT_EQ(isAiv, false);

    GlobalMockObject::verify();
}
struct OpInfo {
    ge::NodePtr nodePtr;
    std::pair<std::string, std::string> opName;
    std::vector<std::pair<std::string, int64_t>> attrInt;
    std::vector<std::pair<std::string, std::string>> attrStr;
};
HcclResult CreateNodePtr(OpInfo &opInfo, ge::ComputeGraph &graph)
{
    ge::OpDescPtr opDescPtr = std::make_shared<ge::OpDesc>(opInfo.opName.first.c_str(), opInfo.opName.second.c_str());
    EXPECT_NE(opDescPtr, nullptr);
    opDescPtr->SetName(opInfo.opName.first.c_str());

    for (auto &it : opInfo.attrInt) {
        bool bErr = ge::AttrUtils::SetInt(opDescPtr, it.first.c_str(), it.second);
        CHK_PRT_RET(!bErr,
            HCCL_ERROR("node[%s] set attr: %s failed", opInfo.opName.first.c_str(), it.first.c_str()),
            HCCL_E_INTERNAL);
    }
    for (auto &it : opInfo.attrStr) {
        bool bErr = ge::AttrUtils::SetStr(opDescPtr, it.first.c_str(), it.second.c_str());
        CHK_PRT_RET(!bErr,
            HCCL_ERROR("node[%s] set attr: %s failed", opInfo.opName.first.c_str(), it.first.c_str()),
            HCCL_E_INTERNAL);
    }

    opInfo.nodePtr = graph.AddNode(opDescPtr);
    CHK_PRT_RET((opInfo.nodePtr == nullptr),
        HCCL_ERROR("[Create]node[%s] failed", opInfo.opName.first.c_str()),
        HCCL_E_INTERNAL);
    return HCCL_SUCCESS;
}
static void TestConstructParam(HcclCommParams &params, RankTable_t &rankTable)
{
    string commId = "comm ";
    memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
    params.rank = 0;
    params.totalRanks = 2;
    params.isHeterogComm = false;
    params.logicDevId = 0;
    params.commWorkMode = WorkMode::HCCL_MODE_NORMAL;
    params.deviceType = DevType::DEV_TYPE_910;

    rankTable.collectiveId = "192.168.0.101-8000-8001";
    vector<RankInfo_t> rankVec(2);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1);  // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";
    rankVec[1].rankId = 1;
    rankVec[1].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr2(1711319232);
    rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2);  // 101.0.168.192
    rankVec[1].serverIdx = 1;
    rankVec[1].serverId = "192.168.0.102";
    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.deviceNum = 2;
    rankTable.serverNum = 2;
}
TEST_F(SuperFastKernelTest, ut_SetSuperKernelScopeAttr)
{
    std::shared_ptr<hccl::hcclComm> comm;
    comm.reset(new (std::nothrow) hccl::hcclComm());
    HcclResult ret = HCCL_SUCCESS;
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.totalRanks = 1;
    params.deviceType = DevType::DEV_TYPE_910;
    rankTable.rankList.assign(rankTable.rankList.begin(), rankTable.rankList.begin() + 1);
    rankTable.deviceNum = 1;
    rankTable.serverNum = 1;
    MOCKER_CPP(&HcclCommunicator::InitRaResource).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    CommConfig commConfig("hccl_world_group");
    comm->communicator_.reset(new (std::nothrow) HcclCommunicator(commConfig));
    ret = comm->communicator_->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcomGetCommByGroup).stubs().with(any(), outBound(comm)).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcomGetRankSize).stubs().will(invoke(HcomGetRankSizeStub));
    // HcomSelectAlg(group.c_str(), count, dataType, HcclReduceOp::HCCL_REDUCE_SUM, opType, ifAiv, algName)
    bool ifAiv = true;
    std::string algName = "AllGatherMeshAivExecutor";
    MOCKER_CPP(&hcclComm::HcclSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), outBound(ifAiv), outBound(algName))
        .will(returnValue(HCCL_SUCCESS));
    ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("test_graph");
    u32 allgatherNum = 2;
    for (u32 i = 0; i < allgatherNum; i++) {
        // allgather
        OpInfo allgatherOpInfo;
        allgatherOpInfo.opName = std::make_pair("AllGather_" + std::to_string(i), HCCL_KERNEL_OP_TYPE_ALLGATHER);
        allgatherOpInfo.attrInt.push_back(std::make_pair("rank_size", 3));
        allgatherOpInfo.attrInt.push_back(std::make_pair("fusion", 2));
        allgatherOpInfo.attrInt.push_back(std::make_pair("fusion_id", 1));
        allgatherOpInfo.attrStr.push_back(std::make_pair("group", HCCL_WORLD_GROUP));
        auto ret = CreateNodePtr(allgatherOpInfo, *graph);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ge::AttrUtils::HasAttr(allgatherOpInfo.nodePtr->GetOpDesc(), "DUMMY_SET_TRUE_GROUP");
        allgatherOpInfo.nodePtr->GetOpDesc()->SetType(HCCL_KERNEL_OP_TYPE_ALLGATHER);
        std::string superKernelScope = "hccl_aiv";
        EXPECT_EQ(
            ge::AttrUtils::SetStr(allgatherOpInfo.nodePtr->GetOpDesc(), "_super_kernel_scope", superKernelScope), true);
    }
    HcomGraphOptimizer graphOptimizer;
    HcclResult ge_ret = graphOptimizer.SetSuperKernelScopeAttr(*graph);
    EXPECT_EQ(ge_ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(SuperFastKernelTest, ut_SetSuperKernelScopeAttr_notsupport)
{
    std::shared_ptr<hccl::hcclComm> comm;
    comm.reset(new (std::nothrow) hccl::hcclComm());
    HcclResult ret = HCCL_SUCCESS;
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.totalRanks = 1;
    params.deviceType = DevType::DEV_TYPE_910_93;
    rankTable.rankList.assign(rankTable.rankList.begin(), rankTable.rankList.begin() + 1);
    rankTable.deviceNum = 1;
    rankTable.serverNum = 1;
    MOCKER(HcomGetRankSize).stubs().will(invoke(HcomGetRankSizeStub));
    MOCKER_CPP(&HcclCommunicator::InitRaResource).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    CommConfig commConfig("hccl_world_group");
    comm->communicator_.reset(new (std::nothrow) HcclCommunicator(commConfig));
    ret = comm->communicator_->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcomGetCommByGroup).stubs().with(any(), outBound(comm)).will(returnValue(HCCL_SUCCESS));
    // HcomSelectAlg(group.c_str(), count, dataType, HcclReduceOp::HCCL_REDUCE_SUM, opType, ifAiv, algName)
    bool ifAiv = true;
    std::string algName = "AllGatherMeshAivExecutor";
    MOCKER_CPP(&hcclComm::HcclSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), outBound(ifAiv), outBound(algName))
        .will(returnValue(HCCL_SUCCESS));
    ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("test_graph");
    u32 allgatherNum = 2;
    for (u32 i = 0; i < allgatherNum; i++) {
        // allgather
        OpInfo allgatherOpInfo;
        allgatherOpInfo.opName = std::make_pair("AllGather_" + std::to_string(i), HCCL_KERNEL_OP_TYPE_ALLTOALLV);
        allgatherOpInfo.attrInt.push_back(std::make_pair("rank_size", 3));
        allgatherOpInfo.attrInt.push_back(std::make_pair("fusion", 2));
        allgatherOpInfo.attrInt.push_back(std::make_pair("fusion_id", 1));
        allgatherOpInfo.attrStr.push_back(std::make_pair("group", HCCL_WORLD_GROUP));
        auto ret = CreateNodePtr(allgatherOpInfo, *graph);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ge::AttrUtils::HasAttr(allgatherOpInfo.nodePtr->GetOpDesc(), "DUMMY_SET_TRUE_GROUP");
        allgatherOpInfo.nodePtr->GetOpDesc()->SetType(HCCL_KERNEL_OP_TYPE_ALLTOALLV);
        std::string superKernelScope = "hccl_aiv";
        EXPECT_EQ(
            ge::AttrUtils::SetStr(allgatherOpInfo.nodePtr->GetOpDesc(), "_super_kernel_scope", superKernelScope), true);
    }
    HcomGraphOptimizer graphOptimizer;
    HcclResult ge_ret = graphOptimizer.SetSuperKernelScopeAttr(*graph);
    EXPECT_EQ(ge_ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

static void TestConstructParam2(HcclCommParams &params, RankTable_t &rankTable)
{
    string commId = "comm ";
    memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
    params.rank = 0;
    params.totalRanks = 2;
    params.isHeterogComm = false;
    params.logicDevId = 0;
    params.commWorkMode = WorkMode::HCCL_MODE_NORMAL;
    params.deviceType = DevType::DEV_TYPE_910;

    rankTable.collectiveId = "192.168.0.101-8000-8001";
    vector<RankInfo_t> rankVec(2);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1);  // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";
    rankVec[1].rankId = 1;
    rankVec[1].deviceInfo.devicePhyId = 1;
    HcclIpAddress ipAddr2(1711319232);
    rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2);  // 101.0.168.192
    rankVec[1].serverIdx = 0;
    rankVec[1].serverId = "192.168.0.101";
    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.deviceNum = 2;
    rankTable.serverNum = 1;
}

TEST_F(SuperFastKernelTest, ut_SetSuperKernelScopeAttr_notsupport3)
{
    std::shared_ptr<hccl::hcclComm> comm;
    comm.reset(new (std::nothrow) hccl::hcclComm());
    HcclResult ret = HCCL_SUCCESS;
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam2(params, rankTable);
    params.totalRanks = 2;
    params.deviceType = DevType::DEV_TYPE_910_93;
    rankTable.deviceNum = 2;
    rankTable.serverNum = 1;
    MOCKER_CPP(&HcclCommunicator::InitRaResource).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    CommConfig commConfig("hccl_world_group");
    comm->communicator_.reset(new (std::nothrow) HcclCommunicator(commConfig));
    ret = comm->communicator_->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcomGetCommByGroup).stubs().with(any(), outBound(comm)).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcomGetRankSize).stubs().will(invoke(HcomGetRankSizeStub));
    // HcomSelectAlg(group.c_str(), count, dataType, HcclReduceOp::HCCL_REDUCE_SUM, opType, ifAiv, algName)
    bool ifAiv = true;
    std::string algName = "AllGatherMeshAivExecutor";
    MOCKER_CPP(&hcclComm::HcclSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), outBound(ifAiv), outBound(algName))
        .will(returnValue(HCCL_SUCCESS));
    ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("test_graph");
    u32 allgatherNum = 2;
    for (u32 i = 0; i < allgatherNum; i++) {
        // allgather
        OpInfo allgatherOpInfo;
        allgatherOpInfo.opName = std::make_pair("AllGather_" + std::to_string(i), HCCL_KERNEL_OP_TYPE_ALLGATHER);
        allgatherOpInfo.attrInt.push_back(std::make_pair("rank_size", 3));
        allgatherOpInfo.attrInt.push_back(std::make_pair("fusion", 2));
        allgatherOpInfo.attrInt.push_back(std::make_pair("fusion_id", 1));
        allgatherOpInfo.attrStr.push_back(std::make_pair("group", HCCL_WORLD_GROUP));
        auto ret = CreateNodePtr(allgatherOpInfo, *graph);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ge::AttrUtils::HasAttr(allgatherOpInfo.nodePtr->GetOpDesc(), "DUMMY_SET_TRUE_GROUP");
        allgatherOpInfo.nodePtr->GetOpDesc()->SetType(HCCL_KERNEL_OP_TYPE_ALLGATHER);
        std::string superKernelScope = "hccl_aiv";
        EXPECT_EQ(
            ge::AttrUtils::SetStr(allgatherOpInfo.nodePtr->GetOpDesc(), "_super_kernel_scope", superKernelScope), true);

        std::string vectorcoreNum = "3";
        EXPECT_EQ(ge::AttrUtils::SetStr(allgatherOpInfo.nodePtr->GetOpDesc(), "_op_vectorcore_num", vectorcoreNum), true);
    }
    HcomGraphOptimizer graphOptimizer;
    HcclResult ge_ret = graphOptimizer.SetSuperKernelScopeAttr(*graph);
    EXPECT_EQ(ge_ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

// 覆盖率
TEST_F(SuperFastKernelTest, ut_GetAivCoreLimit1)
{
    const ge::OpDescPtr nodeGroup = std::make_shared<ge::OpDesc>();
    std::string sCollectiveType = HCCL_KERNEL_OP_TYPE_SEND;
    u64 count = 0;
    GetCountFromOpDesc(nodeGroup, sCollectiveType, HcclDataType::HCCL_DATA_TYPE_INT64, count, 1);
}