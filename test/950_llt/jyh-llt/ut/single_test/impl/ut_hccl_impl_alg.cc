#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include <string>
#include "mem_device_pub.h"
#define private public
#define protected public
#include "hccl_impl.h"
#include "hccl_communicator.h"
#include "comm_impl.h"
#include "coll_comm_executor.h"
#include "coll_all_gather_single_rank_executor.h"
#include "coll_reduce_scatter_executor.h"
#include "coll_reduce_scatter_comm_executor.h"
#include "coll_reduce_scatter_ring_executor.h"
#include "coll_reduce_scatter_mesh_executor.h"
#include "coll_reduce_scatter_v_executor.h"
#include "coll_reduce_scatter_v_mesh_opbase_executor.h"
#include "coll_all_gather_v_mesh_executor.h"
#include "coll_all_gather_v_executor.h"
#include "coll_reduce_scatter_v_aiv_big_count_executor.h"
#include "coll_reduce_scatter_v_mesh_aiv_smallcount_executor.h"
#include "coll_all_gatherv_mesh_aiv_executor.h"
#include "coll_all_gatherv_mesh_aiv_smallcount_executor.h"
#include "coll_all_gather_v_executor.h"
#include "coll_reduce_comm_executor.h"
#include "dispatcher_pub.h"
#include "alg_configurator.h"
#include "coll_reduce_scatter_v_for_310p_ring_executor.h"
#include "coll_all_gatherv_for_310p_executor.h"
#undef private
#undef protected
#include "all_gather_v_operator.h"
#include "reduce_scatter_v_operator.h"
#include "coll_all_gather_v_mesh_graph_executor.h"
#include "dlra_function.h"

using namespace hccl;
using namespace std;

class HcclImplAlgTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        DlRaFunction::GetInstance().DlRaFunctionInit();
        std::cout << "HcclImplAlgTest SetUP" << std::endl;
        TestConstructParam(params, rankTable);
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "HcclImplAlgTest TearDown" << std::endl;
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
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
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
        rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.0.168.192
        rankVec[0].serverIdx = 0;
        rankVec[0].serverId = "192.168.0.101";
        rankVec[1].rankId = 1;
        rankVec[1].deviceInfo.devicePhyId = 0;
        HcclIpAddress ipAddr2(1711319232);
        rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2); // 101.0.168.192
        rankVec[1].serverIdx = 1;
        rankVec[1].serverId = "192.168.0.102";
        rankTable.rankList.assign(rankVec.begin(), rankVec.end());
        rankTable.deviceNum = 2;
        rankTable.serverNum = 2;
    }

    static void TestConstructParamForOneServer(HcclCommParams &params, RankTable_t &rankTable)
    {
        string commId = "comm ";
        memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
        params.rank = 0;
        params.totalRanks = 2;
        params.isHeterogComm = false;
        params.logicDevId = 0;
        params.commWorkMode = WorkMode::HCCL_MODE_NORMAL;
        params.deviceType = DevType::DEV_TYPE_910B;

        rankTable.collectiveId = "192.168.0.101-8000-8001";
        vector<RankInfo_t> rankVec(2);
        rankVec[0].rankId = 0;
        rankVec[0].deviceInfo.devicePhyId = 0;
        HcclIpAddress ipAddr1(1694542016);
        rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.0.168.192
        rankVec[0].serverIdx = 0;
        rankVec[0].serverId = "192.168.0.101";
        rankVec[1].rankId = 1;
        rankVec[1].deviceInfo.devicePhyId = 1;
        HcclIpAddress ipAddr2(1711319232);
        rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2); // 101.0.168.192
        rankVec[1].serverIdx = 0;
        rankVec[1].serverId = "192.168.0.101";
        rankTable.rankList.assign(rankVec.begin(), rankVec.end());
        rankTable.deviceNum = 2;
        rankTable.serverNum = 1;
    }
    static HcclCommParams params;
    static RankTable_t rankTable;

    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;

};
HcclDispatcher HcclImplAlgTest::dispatcherPtr = nullptr;
DispatcherPub *HcclImplAlgTest::dispatcher = nullptr;

HcclCommParams HcclImplAlgTest::params;
RankTable_t HcclImplAlgTest::rankTable;
TEST_F(HcclImplAlgTest, ut_ReduceComm)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);

    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    impl->deviceLogicId_ = 0;
    impl->deviceType_ = DevType::DEV_TYPE_910;
    impl->topoType_ = TopoType::TOPO_TYPE_8P_RING;

    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    topoMatcher->topoInfo_.deviceLogicId = 0;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_910;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_8P_RING;
    CollReduceCommExecutor* executor = new CollReduceCommExecutor(impl->dispatcher_, topoMatcher);

    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 4096;
    opParam.DataDes.count = 1024;
    opParam.DataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.reduceType = HCCL_REDUCE_SUM;
    opParam.root = 0;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(opParam.tag, HcclCMDType::HCCL_CMD_REDUCE, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    DeviceMem scratchMem = DeviceMem::alloc(4096);
    resourceResponse.scratchMem = scratchMem;
    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    implBase = nullptr;

    delete executor;
    GlobalMockObject::verify();
}

TEST_F(HcclImplAlgTest, ut_ReduceScatterComm)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;

    impl->deviceLogicId_ = 0;
    impl->deviceType_ = DevType::DEV_TYPE_910;
    impl->topoType_ = TopoType::TOPO_TYPE_8P_RING;

    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB));

    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    topoMatcher->topoInfo_.deviceLogicId = 0;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_910;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_8P_RING;
    CollReduceScatterCommExecutor* executor = new CollReduceScatterCommExecutor(impl->dispatcher_, topoMatcher);

    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 4096;
    opParam.DataDes.count = 1024;
    opParam.DataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.reduceType = HCCL_REDUCE_SUM;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(opParam.tag, HcclCMDType::HCCL_CMD_REDUCE_SCATTER, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    DeviceMem scratchMem = DeviceMem::alloc(4096);
    resourceResponse.scratchMem = scratchMem;
    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    implBase = nullptr;

    delete executor;
    GlobalMockObject::verify();
}

TEST_F(HcclImplAlgTest, ut_ReduceScatterMeshExecutor)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;

    impl->deviceLogicId_ = 0;
    impl->deviceType_ = DevType::DEV_TYPE_910;
    impl->topoType_ = TopoType::TOPO_TYPE_4P_RING;

    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB));

    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    topoMatcher->topoInfo_.deviceLogicId = 0;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_910;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_4P_RING;
    CollReduceScatterMeshExecutor* executor = new CollReduceScatterMeshExecutor(impl->dispatcher_, topoMatcher);

    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 4096;
    opParam.DataDes.count = 1024;
    opParam.DataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.reduceType = HCCL_REDUCE_SUM;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(AlgTemplateBase::PrepareSliceMeshStreams)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(opParam.tag, HcclCMDType::HCCL_CMD_REDUCE_SCATTER, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    DeviceMem scratchMem = DeviceMem::alloc(4096);
    resourceResponse.scratchMem = scratchMem;
    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    implBase = nullptr;

    delete executor;
    GlobalMockObject::verify();
}

TEST_F(HcclImplAlgTest, ut_ReduceScatterRingExecutor_HD)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;

    impl->deviceLogicId_ = 0;
    impl->deviceType_ = DevType::DEV_TYPE_910;
    impl->topoType_ = TopoType::TOPO_TYPE_4P_RING;

    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB));

    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    topoMatcher->topoInfo_.deviceLogicId = 0;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_910;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_4P_RING;
    CollReduceScatterRingExecutor* executor = new CollReduceScatterRingExecutor(impl->dispatcher_, topoMatcher);

    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 4096;
    opParam.DataDes.count = 1024;
    opParam.DataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.reduceType = HCCL_REDUCE_SUM;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(opParam.tag, HcclCMDType::HCCL_CMD_REDUCE_SCATTER, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    DeviceMem scratchMem = DeviceMem::alloc(4096);
    resourceResponse.scratchMem = scratchMem;
    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    implBase = nullptr;

    delete executor;
    GlobalMockObject::verify();
}

TEST_F(HcclImplAlgTest, ut_ReduceScatterRingExecutor_Ring)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    impl->deviceLogicId_ = 0;
    impl->deviceType_ = DevType::DEV_TYPE_910;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;

    impl->topoType_ = TopoType::TOPO_TYPE_4P_RING;

    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB));

    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    topoMatcher->topoInfo_.deviceLogicId = 0;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_910;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_4P_RING;
    CollReduceScatterRingExecutor* executor = new CollReduceScatterRingExecutor(impl->dispatcher_, topoMatcher);

    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 4096;
    opParam.DataDes.count = 1024;
    opParam.DataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.reduceType = HCCL_REDUCE_SUM;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(opParam.tag, HcclCMDType::HCCL_CMD_REDUCE_SCATTER, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    DeviceMem scratchMem = DeviceMem::alloc(4096);
    resourceResponse.scratchMem = scratchMem;
    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    implBase = nullptr;

    delete executor;
    GlobalMockObject::verify();
}

TEST_F(HcclImplAlgTest, ut_CollMultiRingAllReduceAndMultiRootScatter)
{
    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB));
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096 * 2);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;

    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    impl->deviceLogicId_ = 0;
    impl->deviceType_ = DevType::DEV_TYPE_910;

    std::vector<Slice> dataSegsSlice;   // 数据分成ranksize份，每份的起始偏移和大小
    std::vector<std::vector<Slice>> multRingsSliceZero;

    ret = AlgTemplateBase::PrepareSliceData(inputMem.size() / 4, 4, 8, 0, dataSegsSlice);

    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    topoMatcher->topoInfo_.deviceLogicId = 0;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_910;
    CollReduceScatterExecutor* executor = new CollReduceScatterRingExecutor(impl->dispatcher_, topoMatcher);

    OpParam opParam;
    opParam.tag = tag;
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = count * 4 * 2;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = count * 4;
    opParam.DataDes.count = count;
    opParam.DataDes.dataType = dataType;
    opParam.reduceType = op;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    std::vector<std::vector<u32>> multiRingOrder;
    std::vector<u32> tmpOuter0 = { 0, 1, 2, 6, 5, 4, 7, 3 }; // 环0
    std::vector<u32> tmpOuter1 = { 0, 3, 7, 4, 5, 6, 2, 1 }; // 环1
    std::vector<u32> tmpOuter2 = { 0, 2, 3, 1, 5, 7, 6, 4 }; // 环2
    std::vector<u32> tmpOuter3 = { 0, 4, 6, 7, 5, 1, 3, 2 }; // 环3
    multiRingOrder.push_back(tmpOuter0);
    multiRingOrder.push_back(tmpOuter1);
    multiRingOrder.push_back(tmpOuter2);
    multiRingOrder.push_back(tmpOuter3);

    MOCKER(LocalNotify::Wait)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(LocalNotify::Post)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&CollCommExecutor::GetRingsOrderByTopoType)
    .stubs()
    .will(returnValue(multiRingOrder));
    MOCKER_CPP(&CollCommExecutor::CheckCommSize)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&CollNativeExecutorBase::AddSubStreamToProfiling)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&StreamActiveManager::StreamActive)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    SubCommInfo mockCommInfo {0, 8, std::vector<LINK>()};
    MOCKER_CPP(&CollNativeExecutorBase::GetSubCommInfo)
    .stubs()
    .will(returnValue(mockCommInfo));
    MOCKER_CPP(&CollNativeExecutorBase::GetRankByUserRank)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    resourceRequest.streamNum = 3;
    resourceRequest.notifyNum = 6;

    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(opParam.tag, HcclCMDType::HCCL_CMD_REDUCE_SCATTER, opParam, resourceRequest, resourceResponse);
    std::vector<Stream> streams(3, Stream(StreamType::STREAM_TYPE_ONLINE));
    resourceResponse.slaveStreams = streams;
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    DeviceMem scratchMem = DeviceMem::alloc(count * 4 * 2 + 64);
    resourceResponse.scratchMem = scratchMem;
    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    multRingsSliceZero = executor->ReduceScatterRingSlicePrepare(4, 8, true, outputMem, dataSegsSlice, tag);
    ret = executor->MultiRingMultiRootScatter(tag, inputMem, outputMem, count, dataType, multRingsSliceZero, 0, opParam.stream, 0);
    ret = executor->MultiRingAllReduce(tag, inputMem, outputMem, count, dataType, op, multRingsSliceZero, opParam.stream, 0, 0);
    implBase = nullptr;

    delete executor;
    GlobalMockObject::verify();
}

TEST_F(HcclImplAlgTest, ut_ReduceScatterVFor910BMesh)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910B;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    impl->deviceLogicId_ = 0;
    impl->deviceType_ = DevType::DEV_TYPE_910B;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_NP_MESH;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    impl->topoType_ = TopoType::TOPO_TYPE_NP_MESH;

    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    topoMatcher->topoInfo_.deviceLogicId = 0;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_910B;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_NP_MESH;
    std::unique_ptr<ReduceScatterVOperator> operation(
        new (std::nothrow) ReduceScatterVOperator(algConfigurator.get(), cclBufferManager, dispatcher, topoMatcher));

    CollReduceScatterVMeshOpbaseExecutor *executor = new CollReduceScatterVMeshOpbaseExecutor(impl->dispatcher_, topoMatcher);

    std::vector<u64> counts {1, 2, 3, 4};
    std::vector<u64> displs {0};
    for (auto i = 1; i < counts.size(); ++i) {
            displs.emplace_back(displs[i-1] + counts[i-1]);
    }

    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 4096;
    opParam.VDataDes.counts = counts.data();
    opParam.VDataDes.displs = displs.data();
    opParam.VDataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.reduceType = HCCL_REDUCE_SUM;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);

    std::string algName = "";
    std::string newTag = opParam.tag;
    ret = operation->SelectAlg(tag, opParam, algName, newTag);
    opParam.tag = newTag;

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    SubCommInfo mockCommInfo {0, 1, std::vector<LINK>()};
    MOCKER_CPP(&CollNativeExecutorBase::GetSubCommInfo)
    .stubs()
    .will(returnValue(mockCommInfo));

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(newTag, HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    DeviceMem scratchMem = DeviceMem::alloc(4096);
    resourceResponse.scratchMem = scratchMem;
    resourceResponse.slaveStreams.resize(1);
    resourceResponse.notifiesMain.resize(1);
    resourceResponse.notifiesAux.resize(1);
    resourceResponse.notifiesDevMain.resize(1);
    resourceResponse.notifiesDevAux.resize(1);
    executor->inCCLbufferSize_ = inputMem.size();

    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    implBase = nullptr;

    delete executor;
    GlobalMockObject::verify();
}

TEST_F(HcclImplAlgTest, ut_ReduceScatterVFor910BMesh_Zero_Count)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910B;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    impl->deviceLogicId_ = 0;
    impl->deviceType_ = DevType::DEV_TYPE_910B;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_NP_MESH;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    impl->topoType_ = TopoType::TOPO_TYPE_NP_MESH;

    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    topoMatcher->topoInfo_.deviceLogicId = 0;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_910B;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_NP_MESH;
    std::unique_ptr<ReduceScatterVOperator> operation(
        new (std::nothrow) ReduceScatterVOperator(algConfigurator.get(), cclBufferManager, dispatcher, topoMatcher));

    CollReduceScatterVMeshOpbaseExecutor *executor = new CollReduceScatterVMeshOpbaseExecutor(impl->dispatcher_, topoMatcher);

    std::vector<u64> counts {0, 2, 3, 4};
    std::vector<u64> displs {0};
    for (auto i = 1; i < counts.size(); ++i) {
            displs.emplace_back(displs[i-1] + counts[i-1]);
    }

    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = nullptr;
    opParam.outputSize = 4096;
    opParam.VDataDes.counts = counts.data();
    opParam.VDataDes.displs = displs.data();
    opParam.VDataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.reduceType = HCCL_REDUCE_SUM;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);

    std::string algName = "";
    std::string newTag = opParam.tag;
    ret = operation->SelectAlg(tag, opParam, algName, newTag);
    opParam.tag = newTag;

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    SubCommInfo mockCommInfo {0, 1, std::vector<LINK>()};
    MOCKER_CPP(&CollNativeExecutorBase::GetSubCommInfo)
    .stubs()
    .will(returnValue(mockCommInfo));

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(newTag, HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    DeviceMem scratchMem = DeviceMem::alloc(4096);
    resourceResponse.scratchMem = scratchMem;
    resourceResponse.slaveStreams.resize(1);
    resourceResponse.notifiesMain.resize(1);
    resourceResponse.notifiesAux.resize(1);
    resourceResponse.notifiesDevMain.resize(1);
    resourceResponse.notifiesDevAux.resize(1);
    executor->inCCLbufferSize_ = inputMem.size();

    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    implBase = nullptr;

    delete executor;
    GlobalMockObject::verify();
}

TEST_F(HcclImplAlgTest, ut_ReduceScatterVFor310PRing)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);
 
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_310P3;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());
 
    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
 
    impl->deviceLogicId_ = 0;
    impl->deviceType_ = DevType::DEV_TYPE_310P3;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    impl->topoType_ = TopoType::TOPO_TYPE_4P_RING;
 
    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));
 
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    topoMatcher->topoInfo_.deviceLogicId = 0;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_310P3;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_4P_RING;
    std::unique_ptr<ReduceScatterVOperator> operation(
        new (std::nothrow) ReduceScatterVOperator(algConfigurator.get(), cclBufferManager, dispatcher, topoMatcher));
 
    CollReduceScatterVFor310PRingExecutor *executor = new CollReduceScatterVFor310PRingExecutor(impl->dispatcher_, topoMatcher);
 
    std::vector<u64> counts {1, 2, 3, 4};
    std::vector<u64> displs {0};
    for (auto i = 1; i < counts.size(); ++i) {
            displs.emplace_back(displs[i-1] + counts[i-1]);
    }
 
    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 4096;
    opParam.VDataDes.counts = counts.data();
    opParam.VDataDes.displs = displs.data();
    opParam.VDataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.reduceType = HCCL_REDUCE_SUM;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);
 
    std::string algName = "";
    std::string newTag = opParam.tag;
    ret = operation->SelectAlg(tag, opParam, algName, newTag);
    opParam.tag = newTag;
 
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    SubCommInfo mockCommInfo {0, 1, std::vector<LINK>()};
    MOCKER_CPP(&CollNativeExecutorBase::GetSubCommInfo)
    .stubs()
    .will(returnValue(mockCommInfo));
 
    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(newTag, HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    DeviceMem scratchMem = DeviceMem::alloc(4096);
    resourceResponse.scratchMem = scratchMem;
    resourceResponse.slaveStreams.resize(1);
    resourceResponse.notifiesMain.resize(1);
    resourceResponse.notifiesAux.resize(1);
    resourceResponse.notifiesDevMain.resize(1);
    resourceResponse.notifiesDevAux.resize(1);
    executor->inCCLbufferSize_ = inputMem.size();
 
    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    implBase = nullptr;
 
    delete executor;
    GlobalMockObject::verify();
}

TEST_F(HcclImplAlgTest, ut_ReduceScatterVFor310PRingNosupportInlineReduce)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);
 
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_310P3;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());
 
    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
 
    impl->deviceLogicId_ = 0;
    impl->deviceType_ = DevType::DEV_TYPE_310P3;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    impl->topoType_ = TopoType::TOPO_TYPE_4P_RING;
 
    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));
 
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    topoMatcher->topoInfo_.deviceLogicId = 0;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_310P3;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_4P_RING;
    std::unique_ptr<ReduceScatterVOperator> operation(
        new (std::nothrow) ReduceScatterVOperator(algConfigurator.get(), cclBufferManager, dispatcher, topoMatcher));
 
    CollReduceScatterVFor310PRingExecutor *executor = new CollReduceScatterVFor310PRingExecutor(impl->dispatcher_, topoMatcher);
 
    std::vector<u64> counts {1, 2, 3, 4};
    std::vector<u64> displs {0};
    for (auto i = 1; i < counts.size(); ++i) {
            displs.emplace_back(displs[i-1] + counts[i-1]);
    }
 
    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 4096;
    opParam.VDataDes.counts = counts.data();
    opParam.VDataDes.displs = displs.data();
    opParam.VDataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.reduceType = HCCL_REDUCE_MAX;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);
 
    std::string algName = "";
    std::string newTag = opParam.tag;
    ret = operation->SelectAlg(tag, opParam, algName, newTag);
    opParam.tag = newTag;
 
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    SubCommInfo mockCommInfo {0, 1, std::vector<LINK>()};
    MOCKER_CPP(&CollNativeExecutorBase::GetSubCommInfo)
    .stubs()
    .will(returnValue(mockCommInfo));
 
    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(newTag, HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    DeviceMem scratchMem = DeviceMem::alloc(4096);
    resourceResponse.scratchMem = scratchMem;
    resourceResponse.slaveStreams.resize(1);
    resourceResponse.notifiesMain.resize(1);
    resourceResponse.notifiesAux.resize(1);
    resourceResponse.notifiesDevMain.resize(1);
    resourceResponse.notifiesDevAux.resize(1);
    executor->inCCLbufferSize_ = inputMem.size();
 
    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
 
    implBase = nullptr;
 
    delete executor;
    GlobalMockObject::verify();
}

TEST_F(HcclImplAlgTest, ut_AllGatherVFor310PExecutor_Ring)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);
 
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_310P3;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());
 
    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
 
    impl->deviceLogicId_ = 0;
    impl->deviceType_ = DevType::DEV_TYPE_310P3;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    impl->topoType_ = TopoType::TOPO_TYPE_4P_RING;
 
    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));
 
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    topoMatcher->topoInfo_.deviceLogicId = 0;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_310P3;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_4P_RING;
    std::unique_ptr<AllGatherVOperator> operation(
        new (std::nothrow) AllGatherVOperator(algConfigurator.get(), cclBufferManager, dispatcher, topoMatcher));
 
 
    CollAllGatherVFor310PExecutor *executor = new CollAllGatherVFor310PExecutor(impl->dispatcher_, topoMatcher);
 
    std::vector<u64> counts {1, 2, 3, 4};
    std::vector<u64> displs {0};
    for (auto i = 1; i < counts.size(); ++i) {
            displs.emplace_back(displs[i-1] + counts[i-1]);
    }
 
    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 4096;
    opParam.VDataDes.counts = counts.data();
    opParam.VDataDes.displs = displs.data();
    opParam.VDataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);
 
    std::string algName = "";
    std::string newTag = opParam.tag;
    ret = operation->SelectAlg(tag, opParam, algName, newTag);
    opParam.tag = newTag;
 
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    SubCommInfo mockCommInfo {0, 1, std::vector<LINK>()};
    MOCKER_CPP(&CollNativeExecutorBase::GetSubCommInfo)
    .stubs()
    .will(returnValue(mockCommInfo));
 
    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(newTag, HcclCMDType::HCCL_CMD_ALLGATHER_V, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    DeviceMem scratchMem = DeviceMem::alloc(4096);
    resourceResponse.scratchMem = scratchMem;
    resourceResponse.slaveStreams.resize(1);
    resourceResponse.notifiesMain.resize(1);
    resourceResponse.notifiesAux.resize(1);
    resourceResponse.notifiesDevMain.resize(1);
    resourceResponse.notifiesDevAux.resize(1);
 
    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    implBase = nullptr;
 
    delete executor;
    GlobalMockObject::verify();
}

TEST_F(HcclImplAlgTest, ut_AllGatherVFor910BExecutor_Mesh)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParamForOneServer(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910B;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    impl->deviceLogicId_ = 0;
    impl->deviceType_ = DevType::DEV_TYPE_910B;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_4P_MESH;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    impl->topoType_ = TopoType::TOPO_TYPE_8P_MESH;

    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    topoMatcher->topoInfo_.deviceLogicId = 0;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_910B;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_8P_MESH;
    std::unique_ptr<AllGatherVOperator> operation(
        new (std::nothrow) AllGatherVOperator(algConfigurator.get(), cclBufferManager, dispatcher, topoMatcher));


    CollAllGatherVMeshExecutor *executor = new CollAllGatherVMeshExecutor(impl->dispatcher_, topoMatcher);

    std::vector<u64> counts {1, 2, 3, 4};
    std::vector<u64> displs {0};
    for (auto i = 1; i < counts.size(); ++i) {
            displs.emplace_back(displs[i-1] + counts[i-1]);
    }

    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 4096;
    opParam.VDataDes.counts = counts.data();
    opParam.VDataDes.displs = displs.data();
    opParam.VDataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);

    std::string algName = "";
    std::string newTag = opParam.tag;
    ret = operation->SelectAlg(tag, opParam, algName, newTag);
    opParam.tag = newTag;

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    SubCommInfo mockCommInfo {0, 1, std::vector<LINK>()};
    MOCKER_CPP(&CollNativeExecutorBase::GetSubCommInfo)
    .stubs()
    .will(returnValue(mockCommInfo));

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(newTag, HcclCMDType::HCCL_CMD_ALLGATHER_V, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    DeviceMem scratchMem = DeviceMem::alloc(4096);
    resourceResponse.scratchMem = scratchMem;
    resourceResponse.slaveStreams.resize(1);
    resourceResponse.notifiesMain.resize(1);
    resourceResponse.notifiesAux.resize(1);
    resourceResponse.notifiesDevMain.resize(1);
    resourceResponse.notifiesDevAux.resize(1);

    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    implBase = nullptr;

    delete executor;
    GlobalMockObject::verify();
}

TEST_F(HcclImplAlgTest, ut_AllGatherVFor910BExecutor_Mesh_Zero_Count)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParamForOneServer(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910B;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    impl->deviceLogicId_ = 0;
    impl->deviceType_ = DevType::DEV_TYPE_910B;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_4P_MESH;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    impl->topoType_ = TopoType::TOPO_TYPE_8P_MESH;

    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    topoMatcher->topoInfo_.deviceLogicId = 0;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_910B;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_8P_MESH;
    std::unique_ptr<AllGatherVOperator> operation(
        new (std::nothrow) AllGatherVOperator(algConfigurator.get(), cclBufferManager, dispatcher, topoMatcher));


    CollAllGatherVMeshExecutor *executor = new CollAllGatherVMeshExecutor(impl->dispatcher_, topoMatcher);

    std::vector<u64> counts {0, 2, 3, 4};
    std::vector<u64> displs {0};
    for (auto i = 1; i < counts.size(); ++i) {
            displs.emplace_back(displs[i-1] + counts[i-1]);
    }

    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = nullptr;
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 4096;
    opParam.VDataDes.counts = counts.data();
    opParam.VDataDes.displs = displs.data();
    opParam.VDataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);

    std::string algName = "";
    std::string newTag = opParam.tag;
    ret = operation->SelectAlg(tag, opParam, algName, newTag);
    opParam.tag = newTag;

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    SubCommInfo mockCommInfo {0, 1, std::vector<LINK>()};
    MOCKER_CPP(&CollNativeExecutorBase::GetSubCommInfo)
    .stubs()
    .will(returnValue(mockCommInfo));

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(newTag, HcclCMDType::HCCL_CMD_ALLGATHER_V, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    DeviceMem scratchMem = DeviceMem::alloc(4096);
    resourceResponse.scratchMem = scratchMem;
    resourceResponse.slaveStreams.resize(1);
    resourceResponse.notifiesMain.resize(1);
    resourceResponse.notifiesAux.resize(1);
    resourceResponse.notifiesDevMain.resize(1);
    resourceResponse.notifiesDevAux.resize(1);

    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    implBase = nullptr;

    delete executor;
    GlobalMockObject::verify();
}

TEST_F(HcclImplAlgTest, ut_AllGatherV_offload_For910B_Executor_Mesh_v1)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParamForOneServer(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910B;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    impl->deviceLogicId_ = 0;
    impl->deviceType_ = DevType::DEV_TYPE_910B;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_4P_MESH;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    impl->topoType_ = TopoType::TOPO_TYPE_4P_MESH;

    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB));

    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    topoMatcher->topoInfo_.deviceLogicId = 0;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_910B;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_4P_MESH;
    std::unique_ptr<AllGatherVOperator> operation(
        new (std::nothrow) AllGatherVOperator(algConfigurator.get(), cclBufferManager, dispatcher, topoMatcher));

    CollAllGatherVMeshGraphExecutor *executor = new CollAllGatherVMeshGraphExecutor(impl->dispatcher_, topoMatcher);

    std::vector<u64> counts {1, 2, 3, 4};
    std::vector<u64> displs {0};
    for (auto i = 1; i < counts.size(); ++i) {
            displs.emplace_back(displs[i-1] + counts[i-1]);
    }

    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 4096;
    opParam.VDataDes.counts = counts.data();
    opParam.VDataDes.displs = displs.data();
    opParam.VDataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.stream = Stream(StreamType::STREAM_TYPE_OFFLINE);

    std::string algName = "";
    std::string newTag = opParam.tag;
    ret = operation->SelectAlg(tag, opParam, algName, newTag);
    opParam.tag = newTag;

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    SubCommInfo mockCommInfo {0, 1, std::vector<LINK>()};
    MOCKER_CPP(&CollNativeExecutorBase::GetSubCommInfo)
    .stubs()
    .will(returnValue(mockCommInfo));

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(newTag, HcclCMDType::HCCL_CMD_ALLGATHER_V, opParam, resourceRequest, resourceResponse);
    resourceResponse.paramInputMem = inputMem;
    resourceResponse.paramOutputMem = outputMem;
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    DeviceMem scratchMem = DeviceMem::alloc(4096);
    resourceResponse.scratchMem = scratchMem;

    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    implBase = nullptr;

    delete executor;
    GlobalMockObject::verify();
}

TEST_F(HcclImplAlgTest, ut_AllGatherV_offload_For910B_Executor_Mesh_v2)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910B;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    impl->deviceLogicId_ = 0;
    impl->deviceType_ = DevType::DEV_TYPE_910B;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_4P_MESH;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    impl->topoType_ = TopoType::TOPO_TYPE_4P_MESH;

    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB));

    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    topoMatcher->topoInfo_.deviceLogicId = 0;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_910B;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_4P_MESH;
    std::unique_ptr<AllGatherVOperator> operation(
        new (std::nothrow) AllGatherVOperator(algConfigurator.get(), cclBufferManager, dispatcher, topoMatcher));

    CollAllGatherVMeshGraphExecutor *executor = new CollAllGatherVMeshGraphExecutor(impl->dispatcher_, topoMatcher);

    std::vector<u64> counts {1, 2, 3, 4};
    std::vector<u64> displs {0};
    for (auto i = 1; i < counts.size(); ++i) {
            displs.emplace_back(displs[i-1] + counts[i-1]);
    }

    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 4096;
    opParam.VDataDes.counts = counts.data();
    opParam.VDataDes.displs = displs.data();
    opParam.VDataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.stream = Stream(StreamType::STREAM_TYPE_OFFLINE);

    std::string algName = "";
    std::string newTag = opParam.tag;
    ret = operation->SelectAlg(tag, opParam, algName, newTag);
    opParam.tag = newTag;
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase = nullptr;
    delete executor;
    GlobalMockObject::verify();
}

TEST_F(HcclImplAlgTest, ut_AllGatherVAiv)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParamForOneServer(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910B;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    impl->deviceLogicId_ = 0;
    impl->deviceType_ = DevType::DEV_TYPE_910B;

    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB));

    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    topoMatcher->topoInfo_.deviceLogicId = 0;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_910B;
    std::unique_ptr<AllGatherVOperator> operation(
        new (std::nothrow) AllGatherVOperator(algConfigurator.get(), cclBufferManager, dispatcher, topoMatcher));
    std::vector<u64> counts {1, 2, 3, 4};
    std::vector<u64> displs {0};
    for (auto i = 1; i < counts.size(); ++i) {
            displs.emplace_back(displs[i-1] + counts[i-1]);
    }
    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 4096;
    opParam.VDataDes.counts = counts.data();
    opParam.VDataDes.displs = displs.data();
    opParam.VDataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);

    std::string algName = "";
    std::string newTag = opParam.tag;
    ret = operation->SelectAlg(tag, opParam, algName, newTag);

    implBase = nullptr;

    unsetenv("HCCL_OP_EXPANSION_MODE");
    GlobalMockObject::verify();
}
TEST_F(HcclImplAlgTest, ut_AllGatherVAivBigCount)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910B;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    impl->deviceLogicId_ = 0;
    impl->deviceType_ = DevType::DEV_TYPE_910B; 
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLGATHER_V].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_2P_MESH;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLGATHER_V].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    impl->topoType_ = TopoType::TOPO_TYPE_2P_MESH;

    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    topoMatcher->topoInfo_.deviceLogicId = 0;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_910B;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_2P_MESH;
    std::unique_ptr<AllGatherVOperator> operation(
        new (std::nothrow) AllGatherVOperator(algConfigurator.get(), cclBufferManager, dispatcher, topoMatcher));

    AllGatherVMeshAivExecutor *executor =
        new AllGatherVMeshAivExecutor(dispatcher, topoMatcher);

    std::vector<u64> counts {4194304, 2, 3, 4};
    std::vector<u64> displs {0};
    for (auto i = 1; i < counts.size(); ++i) {
            displs.emplace_back(displs[i-1] + counts[i-1]);
    }

    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 4096;
    opParam.VDataDes.counts = counts.data();
    opParam.VDataDes.displs = displs.data();
    opParam.VDataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.reduceType = HCCL_REDUCE_SUM;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);

    MOCKER(GetExternalInputHcclAivMode)
    .stubs()
    .will(returnValue(true));

    MOCKER(IsSupportAIVCopy)
    .stubs()
    .will(returnValue(true));

    std::string algName = "";
    std::string newTag = opParam.tag;
    operation->topoMatcher_->externalEnable_.deterministic = 0;
    operation->isSingleMeshAggregation_ = true;
    ret = operation->SelectAlg(tag, opParam, algName, newTag);
    opParam.tag = newTag;

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    SubCommInfo mockCommInfo {0, 1, std::vector<LINK>()};
    MOCKER_CPP(&CollNativeExecutorBase::GetSubCommInfo)
    .stubs()
    .will(returnValue(mockCommInfo));

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(newTag, HcclCMDType::HCCL_CMD_ALLGATHER_V, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    DeviceMem scratchMem = DeviceMem::alloc(4096);
    resourceResponse.scratchMem = scratchMem;
    resourceResponse.slaveStreams.resize(1);
    resourceResponse.notifiesMain.resize(1);
    resourceResponse.notifiesAux.resize(1);
    resourceResponse.notifiesDevMain.resize(1);
    resourceResponse.notifiesDevAux.resize(1);
    executor->inCCLbufferSize_ = inputMem.size();

    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    implBase->profilerManager_->TaskAivProfilerHandle(nullptr, 10);

    implBase = nullptr;
    delete executor;
    GlobalMockObject::verify();
}

TEST_F(HcclImplAlgTest, ut_AllGatherVAivSmallCount)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);
 
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910B;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());
 
    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
 
    impl->deviceLogicId_ = 0;
    impl->deviceType_ = DevType::DEV_TYPE_910B;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLGATHER_V].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_2P_MESH;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLGATHER_V].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    impl->topoType_ = TopoType::TOPO_TYPE_2P_MESH;
 
    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));
 
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    topoMatcher->topoInfo_.deviceLogicId = 0;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_910B;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_2P_MESH;
    std::unique_ptr<AllGatherVOperator> operation(
        new (std::nothrow) AllGatherVOperator(algConfigurator.get(), cclBufferManager, dispatcher, topoMatcher));
 
    CollAllGatherVMeshAivSmallCountExecutor *executor =
        new CollAllGatherVMeshAivSmallCountExecutor(dispatcher, topoMatcher);
 
    std::vector<u64> counts {1, 2, 3, 4};
    std::vector<u64> displs {0};
    for (auto i = 1; i < counts.size(); ++i) {
            displs.emplace_back(displs[i-1] + counts[i-1]);
    }
 
    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 4096;
    opParam.VDataDes.counts = counts.data();
    opParam.VDataDes.displs = displs.data();
    opParam.VDataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.reduceType = HCCL_REDUCE_SUM;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);
 
    MOCKER(GetExternalInputHcclAivMode)
    .stubs()
    .will(returnValue(true));
 
    MOCKER(IsSupportAIVCopy)
    .stubs()
    .will(returnValue(true));
 
    std::string algName = "";
    std::string newTag = opParam.tag;
    operation->topoMatcher_->externalEnable_.deterministic = 0;
    operation->isSingleMeshAggregation_ = true;
    ret = operation->SelectAlg(tag, opParam, algName, newTag);
    opParam.tag = newTag;
 
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    SubCommInfo mockCommInfo {0, 1, std::vector<LINK>()};
    MOCKER_CPP(&CollNativeExecutorBase::GetSubCommInfo)
    .stubs()
    .will(returnValue(mockCommInfo));
 
    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(newTag, HcclCMDType::HCCL_CMD_ALLGATHER_V, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    DeviceMem scratchMem = DeviceMem::alloc(4096);
    resourceResponse.scratchMem = scratchMem;
    resourceResponse.slaveStreams.resize(1);
    resourceResponse.notifiesMain.resize(1);
    resourceResponse.notifiesAux.resize(1);
    resourceResponse.notifiesDevMain.resize(1);
    resourceResponse.notifiesDevAux.resize(1);
    executor->inCCLbufferSize_ = inputMem.size();
 
    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    implBase = nullptr;
 
    delete executor;
    GlobalMockObject::verify();
}

TEST_F(HcclImplAlgTest, ut_AllGatherVAivSmallCount_AIV_TASK)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);
 
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910B;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());
 
    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
 
    impl->deviceLogicId_ = 0;
    impl->deviceType_ = DevType::DEV_TYPE_910B;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLGATHER_V].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_2P_MESH;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLGATHER_V].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    impl->topoType_ = TopoType::TOPO_TYPE_2P_MESH;
 
    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));
 
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    topoMatcher->topoInfo_.deviceLogicId = 0;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_910B;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_2P_MESH;
    std::unique_ptr<AllGatherVOperator> operation(
        new (std::nothrow) AllGatherVOperator(algConfigurator.get(), cclBufferManager, dispatcher, topoMatcher));
 
    CollAllGatherVMeshAivSmallCountExecutor *executor =
        new CollAllGatherVMeshAivSmallCountExecutor(dispatcher, topoMatcher);
 
    std::vector<u64> counts {1, 2, 3, 4};
    std::vector<u64> displs {0};
    for (auto i = 1; i < counts.size(); ++i) {
            displs.emplace_back(displs[i-1] + counts[i-1]);
    }
 
    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 4096;
    opParam.VDataDes.counts = counts.data();
    opParam.VDataDes.displs = displs.data();
    opParam.VDataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.reduceType = HCCL_REDUCE_SUM;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);
 
    MOCKER(GetExternalInputHcclAivMode)
    .stubs()
    .will(returnValue(true));
 
    MOCKER(IsSupportAIVCopy)
    .stubs()
    .will(returnValue(true));
 
    std::string algName = "";
    std::string newTag = opParam.tag;
    operation->topoMatcher_->externalEnable_.deterministic = 0;
    operation->isSingleMeshAggregation_ = true;
    ret = operation->SelectAlg(tag, opParam, algName, newTag);
    opParam.tag = newTag;
 
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    SubCommInfo mockCommInfo {0, 1, std::vector<LINK>()};
    MOCKER_CPP(&CollNativeExecutorBase::GetSubCommInfo)
    .stubs()
    .will(returnValue(mockCommInfo));
 
    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(newTag, HcclCMDType::HCCL_CMD_ALLGATHER_V, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    DeviceMem scratchMem = DeviceMem::alloc(4096);
    resourceResponse.scratchMem = scratchMem;
    resourceResponse.slaveStreams.resize(1);
    resourceResponse.notifiesMain.resize(1);
    resourceResponse.notifiesAux.resize(1);
    resourceResponse.notifiesDevMain.resize(1);
    resourceResponse.notifiesDevAux.resize(1);
    executor->inCCLbufferSize_ = inputMem.size();
    executor->workflowMode_ == HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE;
 
    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    implBase = nullptr;
 
    delete executor;
    GlobalMockObject::verify();
}

TEST_F(HcclImplAlgTest, ut_ReduceScatterVAivSmallCount)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910B;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    impl->deviceLogicId_ = 0;
    impl->deviceType_ = DevType::DEV_TYPE_910B;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_2P_MESH;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    impl->topoType_ = TopoType::TOPO_TYPE_2P_MESH;

    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    topoMatcher->topoInfo_.deviceLogicId = 0;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_910B;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_2P_MESH;
    std::unique_ptr<ReduceScatterVOperator> operation(
        new (std::nothrow) ReduceScatterVOperator(algConfigurator.get(), cclBufferManager, dispatcher, topoMatcher));

    CollReduceScatterVMeshAivSmallCountExecutor *executor =
        new CollReduceScatterVMeshAivSmallCountExecutor(dispatcher, topoMatcher);

    std::vector<u64> counts {1, 2, 3, 4};
    std::vector<u64> displs {0};
    for (auto i = 1; i < counts.size(); ++i) {
            displs.emplace_back(displs[i-1] + counts[i-1]);
    }

    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 4096;
    opParam.VDataDes.counts = counts.data();
    opParam.VDataDes.displs = displs.data();
    opParam.VDataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.reduceType = HCCL_REDUCE_SUM;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);

    MOCKER(GetExternalInputHcclAivMode)
    .stubs()
    .will(returnValue(true));

    MOCKER(IsSupportAIVReduce)
    .stubs()
    .will(returnValue(true));

    std::string algName = "";
    std::string newTag = opParam.tag;
    operation->topoMatcher_->externalEnable_.deterministic = 0;
    operation->isSingleMeshAggregation_ = true;
    ret = operation->SelectAlg(tag, opParam, algName, newTag);
    opParam.tag = newTag;

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    SubCommInfo mockCommInfo {0, 1, std::vector<LINK>()};
    MOCKER_CPP(&CollNativeExecutorBase::GetSubCommInfo)
    .stubs()
    .will(returnValue(mockCommInfo));

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(newTag, HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V, opParam,
        resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    DeviceMem scratchMem = DeviceMem::alloc(4096);
    resourceResponse.scratchMem = scratchMem;
    resourceResponse.slaveStreams.resize(1);
    resourceResponse.notifiesMain.resize(1);
    resourceResponse.notifiesAux.resize(1);
    resourceResponse.notifiesDevMain.resize(1);
    resourceResponse.notifiesDevAux.resize(1);
    executor->inCCLbufferSize_ = inputMem.size();

    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    implBase = nullptr;

    delete executor;
    GlobalMockObject::verify();
}
TEST_F(HcclImplAlgTest, ut_ReduceScatterVAivBigCount)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910B;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    impl->deviceLogicId_ = 0;
    impl->deviceType_ = DevType::DEV_TYPE_910B;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_2P_MESH;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    impl->topoType_ = TopoType::TOPO_TYPE_2P_MESH;

    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    topoMatcher->topoInfo_.deviceLogicId = 0;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_910B;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_2P_MESH;
    std::unique_ptr<ReduceScatterVOperator> operation(
        new (std::nothrow) ReduceScatterVOperator(algConfigurator.get(), cclBufferManager, dispatcher, topoMatcher));

    CollReduceScatterVAIVBigCountExecutor *executor =
        new CollReduceScatterVAIVBigCountExecutor(dispatcher, topoMatcher);

    std::vector<u64> counts {4194304, 8388608, 12582912, 16777216};
    std::vector<u64> displs {0};
    for (auto i = 1; i < counts.size(); ++i) {
            displs.emplace_back(displs[i-1] + counts[i-1]);
    }

    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 4096;
    opParam.VDataDes.counts = counts.data();
    opParam.VDataDes.displs = displs.data();
    opParam.VDataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.reduceType = HCCL_REDUCE_SUM;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);

    MOCKER(GetExternalInputHcclAivMode)
    .stubs()
    .will(returnValue(true));

    MOCKER(IsSupportAIVReduce)
    .stubs()
    .will(returnValue(true));

    std::string algName = "";
    std::string newTag = opParam.tag;
    operation->topoMatcher_->externalEnable_.deterministic = 0;
    operation->isSingleMeshAggregation_ = true;
    ret = operation->SelectAlg(tag, opParam, algName, newTag);
    opParam.tag = newTag;

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    SubCommInfo mockCommInfo {0, 1, std::vector<LINK>()};
    MOCKER_CPP(&CollNativeExecutorBase::GetSubCommInfo)
    .stubs()
    .will(returnValue(mockCommInfo));

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(newTag, HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    DeviceMem scratchMem = DeviceMem::alloc(4096);
    resourceResponse.scratchMem = scratchMem;
    resourceResponse.slaveStreams.resize(1);
    resourceResponse.notifiesMain.resize(1);
    resourceResponse.notifiesAux.resize(1);
    resourceResponse.notifiesDevMain.resize(1);
    resourceResponse.notifiesDevAux.resize(1);
    executor->inCCLbufferSize_ = inputMem.size();

    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    implBase = nullptr;

    delete executor;
    GlobalMockObject::verify();
}