#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <cmath>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>

#include "workflow_pub.h"
#include "topoinfo_struct.h"
#include "hccl_ip_address.h"
#include "dlra_function.h"
#include "network/hccp_tlv.h"
#define private public
#define protected public
#include "hccl_alg.h"
#include "hccl_impl.h"
#include "hccl_aiv.h"
#include "config.h"
#include "externalinput.h"
#include "hccl_communicator.h"
#include "hccl_communicator_attrs.h"
#include "hccl_comm_pub.h"
#include "transport_base_pub.h"
#include "comm_impl.h"
#include "comm_mesh_pub.h"
#include "coll_alg_operator.h"
#include "all_gather_operator.h"
#include "reduce_operator.h"
#include "transport_pub.h"
#include "transport_p2p_pub.h"
#include "hccl_common.h"
#include "broadcast_operator.h"
#include "reduce_scatter_operator.h"
#include "notify_pool.h"
#include "comm_base_pub.h"
#include "task_abort_handler_pub.h"
#include "coll_comm_executor.h"
#include "adapter_rts.h"
#include "heartbeat.h"
#include "acl/acl.h"
#include "hccl.h"
#undef private
#undef protected
#include "remote_notify.h"
#include "profiling_manager.h"
#include "base.h"
#include "adapter_rts_common.h"
#include "hdc_pub.h"
#include "aicpu_hdc_utils.h"
#include "common/aicpu_kfc_def.h"
#include "llt_hccl_stub_mc2.h"
#include "llt_hccl_stub.h"
#include "coll_all_reduce_ring_executor.h"
#include "coll_all_gather_ring_executor.h"
#include "coll_all_gather_mesh_aiv_for_910_93_executor.h"
#include "coll_aligned_all_reduce_double_ring_for_910_93_executor.h"
#include "tbe_vector_reduce.h"
#include "tbe_crack_cleared.h"

using namespace hccl;
using namespace std;
constexpr float BANDWIDTH_HCCS_910B = 18.3f;
constexpr float BANDWIDTH_RDMA_910B = 25.0f * 0.8;
// 常用带宽值   GB/s
constexpr float BANDWIDTH_PCIE_GEN3 = 16.0f * 0.85;
constexpr float BANDWIDTH_PCIE_GEN4 = 32.0f * 0.85;
constexpr float BANDWIDTH_PCIE_GEN5 = 64.0f * 0.85;

class HcclImplTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        MOCKER_CPP(&TbeReduce::TbeVectorReduce::Init)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&TbeReduce::TbeCrackCleard::CrackInit)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&TbeReduce::TbeCrackCleard::Run)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    
        MOCKER_CPP(&TbeReduce::TbeVectorReduce::Run)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    
        MOCKER_CPP(&TbeReduce::TbeVectorReduce::SetGlobalWorkSpace)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        DlRaFunction::GetInstance().DlRaFunctionInit();
        std::cout << "HcclImplTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "HcclImplTest TearDown" << std::endl;
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
        unsetenv("HCCL_ALGO");
    }
    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;

};
HcclDispatcher HcclImplTest::dispatcherPtr = nullptr;
DispatcherPub *HcclImplTest::dispatcher = nullptr;


static void TestConstructParam(HcclCommParams &params, RankTable_t &rankTable)
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
    rankVec[1].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr2(1711319232);
    rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2); // 101.0.168.192
    rankVec[1].serverIdx = 1;
    rankVec[1].serverId = "192.168.0.102";
    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.deviceNum = 2;
    rankTable.serverNum = 2;
}

static void TestConstructParam_1server_4p(HcclCommParams &params, RankTable_t &rankTable)
{
    string commId = "comm ";
    memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
    params.rank = 0;
    params.totalRanks = 4;
    params.isHeterogComm = false;
    params.logicDevId = 0;
    params.commWorkMode = WorkMode::HCCL_MODE_NORMAL;
    params.deviceType = DevType::DEV_TYPE_910_93;

    rankTable.collectiveId = "192.168.0.101-8000-8001";
    vector<RankInfo_t> rankVec(4);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";
    rankVec[1].rankId = 1;
    rankVec[1].deviceInfo.devicePhyId = 1;
    HcclIpAddress ipAddr2(1694542017);
    rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2); // 101.0.168.192
    rankVec[1].serverIdx = 0;
    rankVec[1].serverId = "192.168.0.101";
    rankVec[2].rankId = 2;
    rankVec[2].deviceInfo.devicePhyId = 2;
    HcclIpAddress ipAddr3(1711319232);
    rankVec[2].deviceInfo.deviceIp.push_back(ipAddr3); // 101.0.168.192
    rankVec[2].serverIdx = 0;
    rankVec[2].serverId = "192.168.0.101";
    rankVec[3].rankId = 3;
    rankVec[3].deviceInfo.devicePhyId = 3;
    HcclIpAddress ipAddr4(1711319233);
    rankVec[3].deviceInfo.deviceIp.push_back(ipAddr4); // 101.0.168.192
    rankVec[3].serverIdx = 0;
    rankVec[3].serverId = "192.168.0.101";
    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.rankNum = 4;
    rankTable.deviceNum = 4;
    rankTable.serverNum = 1;
}

TEST_F(HcclImplTest, ut_Is2U2PInfer)
{
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcclResult ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_ALL));

    operation->deviceNumPerAggregation_ = 2;
    operation->serverNum_ = 1;
    operation->deviceType_ = DevType::DEV_TYPE_910B;
    operation->meshAggregationRankSize_ = 2;
    bool bret = operation->Is2U2PInfer();
    EXPECT_EQ(bret, true);

    implBase = nullptr;
    GlobalMockObject::verify();
}

namespace hccl {
bool IsAddressAlign(const void *inputPtr, const void *outputPtr, DevType devType);
bool IsDataTypeSupport(HcclDataType dataType, DevType devType);
bool IsRedOpSupport(HcclReduceOp op, DevType devType);
};

TEST_F(HcclImplTest, ut_IsSupportSDMAReduce)
{
    DevType devType = DevType::DEV_TYPE_COUNT;
    EXPECT_EQ(IsRedOpSupport(HcclReduceOp{}, devType), false);
    EXPECT_EQ(IsDataTypeSupport(HcclDataType{}, devType), false);
    EXPECT_EQ(IsAddressAlign(nullptr, nullptr, devType), false);
    EXPECT_EQ(IsSupportSDMAReduce(nullptr, nullptr, HcclDataType{}, HcclReduceOp{}), false);
}

TEST_F(HcclImplTest, ut_hcclimpl_GetBandWidthPerNPU)
{
    float bandWidth;
    HcclCommParams params;
    RankTable_t rankTable;
    DevType devType = DevType::DEV_TYPE_COUNT;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

     MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcclResult ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    devType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(devType))
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->GetBandWidthPerNPU(0, bandWidth);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(bandWidth, BANDWIDTH_HCCS_910B);

    implBase->userRankSize_ = 8;
    implBase->deviceNumPerAggregation_ = 8;
    ret = implBase->GetBandWidthPerNPU(1, bandWidth);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(bandWidth, BANDWIDTH_RDMA_910B);

    implBase->userRankSize_ = 16;
    implBase->deviceNumPerAggregation_ = 8;
    ret = implBase->GetBandWidthPerNPU(1, bandWidth);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(bandWidth, BANDWIDTH_PCIE_GEN5);

    GlobalMockObject::verify();
    devType = DevType::DEV_TYPE_COUNT;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(devType))
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->GetBandWidthPerNPU(0, bandWidth);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    ret = implBase->GetBandWidthPerNPU(1, bandWidth);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    ret = implBase->GetBandWidthPerNPU(3, bandWidth);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    implBase = nullptr;
    GlobalMockObject::verify();
}
#if 1

TEST_F(HcclImplTest, ut_AllocTransport910B)
{
    HcclResult ret = HCCL_SUCCESS;

    TransportType transportType = TransportType::TRANS_TYPE_ROCE;
    MOCKER_CPP(&TransportManager::GetTransportType)
    .stubs()
    .will(returnValue(transportType));

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> communicator(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    communicator->Init(params, rankTable);

    TransportRequest transportReq1;
    transportReq1.isValid = true;
    transportReq1.remoteUserRank = 0;
    transportReq1.remoteUserRank = 1;
    transportReq1.linkType = TransportLinkType::HCCS;
    TransportRequest transportReq2;
    transportReq2.isValid = true;
    transportReq2.remoteUserRank = 0;
    transportReq2.remoteUserRank = 1;
    transportReq2.linkType = TransportLinkType::SIO;

    SingleSubCommTransport singleTrans;
    singleTrans.transportRequests.emplace_back(transportReq1);
    singleTrans.transportRequests.emplace_back(transportReq2);
    singleTrans.links.resize(2,nullptr);
    singleTrans.status.resize(2, TransportStatus::INIT);
    LevelNSubCommTransport levelTrans;
    levelTrans.emplace_back(singleTrans);
    OpCommTransport opTrans;
    opTrans.emplace_back(levelTrans);
    TransportIOMem transMem;

    MOCKER_CPP(&NotifyPool::RegisterOp).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&NotifyPool::UnregisterOp).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtGetDeviceType).stubs().with(outBound(DevType::DEV_TYPE_910B)).will(returnValue(HCCL_SUCCESS));

   // stubs in CreateDestSockets
    MOCKER_CPP(&TransportManager::UpdateIsInterRdma).stubs().with(any(), outBound(false), any()).will(ignoreReturnValue());
    MOCKER_CPP(&TransportManager::MakeRemoteLinkInfo).stubs().will(returnValue(HCCL_SUCCESS));

   // stubs in CreateLink
    MOCKER(hrtErrMSetErrorContextPub).stubs().with(any()).will(ignoreReturnValue());
    MOCKER(hrtSetDevice).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportManager::TransportInit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtResetDevice).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclSocketManager::DestroySockets, void(HcclSocketManager::*)(const std::string&))
    .stubs().with(any()).will(ignoreReturnValue());

    std::string tag = "test";
    ret = communicator->transportManager_->Alloc(tag, transMem, opTrans, true, true);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_AllocTransport910B_When_OOM_Expect_ReturnIsHCCL_E_OOM)
{
    HcclResult ret = HCCL_SUCCESS;

    TransportType transportType = TransportType::TRANS_TYPE_ROCE;
    MOCKER_CPP(&TransportManager::GetTransportType)
    .stubs()
    .will(returnValue(transportType));

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> communicator(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    communicator->Init(params, rankTable);

    TransportRequest transportReq1;
    transportReq1.isValid = true;
    transportReq1.isUsedRdma = true;
    transportReq1.remoteUserRank = 0;
    transportReq1.remoteUserRank = 1;
    transportReq1.linkType = TransportLinkType::HCCS;
    TransportRequest transportReq2;
    transportReq2.isValid = true;
    transportReq1.isUsedRdma = true;
    transportReq2.remoteUserRank = 0;
    transportReq2.remoteUserRank = 1;
    transportReq2.linkType = TransportLinkType::SIO;

    SingleSubCommTransport singleTrans;
    singleTrans.transportRequests.emplace_back(transportReq1);
    singleTrans.transportRequests.emplace_back(transportReq2);
    singleTrans.links.resize(2,nullptr);
    singleTrans.status.resize(2, TransportStatus::INIT);
    LevelNSubCommTransport levelTrans;
    levelTrans.emplace_back(singleTrans);
    OpCommTransport opTrans;
    opTrans.emplace_back(levelTrans);
    TransportIOMem transMem;

    MOCKER_CPP(&NotifyPool::RegisterOp).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&NotifyPool::UnregisterOp).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtGetDeviceType).stubs().with(outBound(DevType::DEV_TYPE_910B)).will(returnValue(HCCL_SUCCESS));

   // stubs in CreateDestSockets
    MOCKER_CPP(&TransportManager::CreateDestSockets).stubs()
        .with(any(), any(), any(), any(), any(), outBound(false), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));

   // stubs in CreateLink
    MOCKER(hrtErrMSetErrorContextPub).stubs().with(any()).will(ignoreReturnValue());
    MOCKER(hrtSetDevice).stubs().with(any()).will(returnValue(HCCL_E_OOM));

    MOCKER(hrtResetDevice).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclSocketManager::DestroySockets, void(HcclSocketManager::*)(const std::string&))
    .stubs().with(any()).will(ignoreReturnValue());

    std::string tag = "test";
    ret = communicator->transportManager_->Alloc(tag, transMem, opTrans, true, true);
    EXPECT_EQ(ret, HCCL_E_OOM);
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_AllocTransport910C)
{
    HcclResult ret = HCCL_SUCCESS;
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> communicator(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    communicator->Init(params, rankTable);

    TransportRequest transportReq1;
    transportReq1.isValid = true;
    transportReq1.remoteUserRank = 0;
    transportReq1.remoteUserRank = 1;
    transportReq1.inputMemType = CCL_INPUT;
    transportReq1.outputMemType = CCL_OUTPUT;
    transportReq1.linkType = TransportLinkType::HCCS;
    TransportRequest transportReq2;
    transportReq2.isValid = true;
    transportReq2.remoteUserRank = 0;
    transportReq2.remoteUserRank = 1;
    transportReq2.inputMemType = CCL_INPUT;
    transportReq2.outputMemType = CCL_OUTPUT;
    transportReq2.linkType = TransportLinkType::SIO;

    SingleSubCommTransport singleTrans;
    singleTrans.transportRequests.emplace_back(transportReq1);
    singleTrans.transportRequests.emplace_back(transportReq2);
    singleTrans.links.resize(2, nullptr);
    singleTrans.status.resize(2, TransportStatus::INIT);
    TransportIOMem transMem;

    MOCKER_CPP(&TransportManager::GetIOMem).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

   // stubs in CreateDestSockets
    MOCKER_CPP(&TransportManager::UpdateIsInterRdma).stubs().with(any(), outBound(false), any()).will(ignoreReturnValue());
    MOCKER_CPP(&TransportManager::MakeRemoteLinkInfo).stubs().will(returnValue(HCCL_SUCCESS));

    // stubs in IsHccsTransport
    MOCKER(hrtGetPairDeviceLinkType).stubs().with(any(), any(), outBound(LinkTypeInServer::HCCS_SW_TYPE)).will(returnValue(HCCL_SUCCESS));
 
    MOCKER(Is310PDevice).stubs().will(returnValue(false));
    MOCKER_CPP(&HcclSocketManager::CreateSingleLinkSocket).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

   // stubs in CreateLink
    MOCKER(hrtErrMSetErrorContextPub).stubs().with(any()).will(ignoreReturnValue());
    MOCKER(hrtSetDevice).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportManager::TransportInit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    
    MOCKER(hrtResetDevice).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportManager::checkSubCommLinkThreadsStatus).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclSocketManager::DestroySockets, void(HcclSocketManager::*)(const std::string&))
    .stubs().with(any()).will(ignoreReturnValue());

    std::string tag = "test";
    ret = communicator->transportManager_->AllocSubCommLinks(tag, transMem, singleTrans, false, false, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_AllocTransport910C_When_OOM_Expect_ReturnIsHCCL_E_OOM)
{
    HcclResult ret = HCCL_SUCCESS;
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> communicator(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    communicator->Init(params, rankTable);

    TransportRequest transportReq1;
    transportReq1.isValid = true;
    transportReq1.remoteUserRank = 0;
    transportReq1.remoteUserRank = 1;
    transportReq1.inputMemType = CCL_INPUT;
    transportReq1.outputMemType = CCL_OUTPUT;
    transportReq1.linkType = TransportLinkType::HCCS;
    TransportRequest transportReq2;
    transportReq2.isValid = true;
    transportReq2.remoteUserRank = 0;
    transportReq2.remoteUserRank = 1;
    transportReq2.inputMemType = CCL_INPUT;
    transportReq2.outputMemType = CCL_OUTPUT;
    transportReq2.linkType = TransportLinkType::SIO;

    SingleSubCommTransport singleTrans;
    singleTrans.transportRequests.emplace_back(transportReq1);
    singleTrans.transportRequests.emplace_back(transportReq2);
    singleTrans.links.resize(2, nullptr);
    singleTrans.status.resize(2, TransportStatus::INIT);
    TransportIOMem transMem;

    MOCKER_CPP(&TransportManager::GetIOMem).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

   // stubs in CreateDestSockets
    MOCKER_CPP(&TransportManager::UpdateIsInterRdma).stubs().with(any(), outBound(false), any()).will(ignoreReturnValue());
    MOCKER_CPP(&TransportManager::MakeRemoteLinkInfo).stubs().will(returnValue(HCCL_SUCCESS));

    // stubs in IsHccsTransport
    MOCKER(hrtGetPairDeviceLinkType).stubs().with(any(), any(), outBound(LinkTypeInServer::HCCS_SW_TYPE)).will(returnValue(HCCL_SUCCESS));
 
    MOCKER(Is310PDevice).stubs().will(returnValue(false));
    MOCKER_CPP(&HcclSocketManager::CreateSingleLinkSocket).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

   // stubs in CreateLink
    MOCKER(hrtErrMSetErrorContextPub).stubs().with(any()).will(ignoreReturnValue());
    MOCKER(hrtSetDevice).stubs().with(any()).will(returnValue(HCCL_E_OOM));
    
    MOCKER(hrtResetDevice).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclSocketManager::DestroySockets, void(HcclSocketManager::*)(const std::string&))
    .stubs().with(any()).will(ignoreReturnValue());

    std::string tag = "test";
    ret = communicator->transportManager_->AllocSubCommLinks(tag, transMem, singleTrans, false, false, 0);
    EXPECT_EQ(ret, HCCL_E_OOM);
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_checkSubCommLinkThreadsStatus_910C)
{
    HcclResult ret = HCCL_SUCCESS;
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> communicator(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    communicator->Init(params, rankTable);

    TransportRequest transportReq1;
    transportReq1.isValid = true;
    transportReq1.remoteUserRank = 0;
    transportReq1.remoteUserRank = 1;
    transportReq1.inputMemType = CCL_INPUT;
    transportReq1.outputMemType = CCL_OUTPUT;
    transportReq1.linkType = TransportLinkType::HCCS;
    TransportRequest transportReq2;
    transportReq2.isValid = true;
    transportReq2.remoteUserRank = 0;
    transportReq2.remoteUserRank = 1;
    transportReq2.inputMemType = CCL_INPUT;
    transportReq2.outputMemType = CCL_OUTPUT;
    transportReq2.linkType = TransportLinkType::SIO;

    SingleSubCommTransport singleTrans;
    singleTrans.transportRequests.emplace_back(transportReq1);
    singleTrans.transportRequests.emplace_back(transportReq2);
    singleTrans.links.resize(2, nullptr);
    singleTrans.status.resize(2, TransportStatus::INIT);

    std::vector<std::pair<u32, u32>> remoteRankMap = {{0, 1}};
    struct SubCommLinkPara subCommLinkPara(singleTrans, remoteRankMap, 0, 1);
    subCommLinkPara.linkResult.resize(2, HCCL_SUCCESS);

    MOCKER_CPP(&NotifyPool::UnregisterOp).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    std::string tag = "test";
    ret = communicator->transportManager_->checkSubCommLinkThreadsStatus(tag, subCommLinkPara, false);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_CheckSubCommLinkThreadsStatus_910C_When_OOM_Expect_ReturnIsHCCL_E_OOM)
{
    HcclResult ret = HCCL_SUCCESS;
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> communicator(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    communicator->Init(params, rankTable);

    TransportRequest transportReq1;
    transportReq1.isValid = true;
    transportReq1.remoteUserRank = 0;
    transportReq1.remoteUserRank = 1;
    transportReq1.inputMemType = CCL_INPUT;
    transportReq1.outputMemType = CCL_OUTPUT;
    transportReq1.linkType = TransportLinkType::HCCS;
    TransportRequest transportReq2;
    transportReq2.isValid = true;
    transportReq2.remoteUserRank = 0;
    transportReq2.remoteUserRank = 1;
    transportReq2.inputMemType = CCL_INPUT;
    transportReq2.outputMemType = CCL_OUTPUT;
    transportReq2.linkType = TransportLinkType::SIO;

    SingleSubCommTransport singleTrans;
    singleTrans.transportRequests.emplace_back(transportReq1);
    singleTrans.transportRequests.emplace_back(transportReq2);
    singleTrans.links.resize(2, nullptr);
    singleTrans.status.resize(2, TransportStatus::INIT);

    std::vector<std::pair<u32, u32>> remoteRankMap = {{0, 1}};
    struct SubCommLinkPara subCommLinkPara(singleTrans, remoteRankMap, 0, 1);
    subCommLinkPara.linkResult.resize(2, HCCL_E_OOM);

    MOCKER_CPP(&NotifyPool::UnregisterOp).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    std::string tag = "test";
    ret = communicator->transportManager_->checkSubCommLinkThreadsStatus(tag, subCommLinkPara, false);
    EXPECT_EQ(ret, HCCL_E_OOM);
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_TransportMgrConstructTransTag)
{
    HcclResult ret = HCCL_SUCCESS;
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> communicator(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    communicator->Init(params, rankTable);

    MOCKER(Is310PDevice).stubs().will(returnValue(false));
    communicator->transportManager_->isHaveCpuRank_ = true;

    std::string tag = "testAlg";
    std::string transTag;
    ret = communicator->transportManager_->ConstructTransTag(tag, transTag, true, 0, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_STREQ(transTag.c_str(), "testAlg_Inter_");

    ret = communicator->transportManager_->ConstructTransTag(tag, transTag, false, 0, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_STREQ(transTag.c_str(), "testAlg_SIO_");

    ret = communicator->transportManager_->ConstructTransTag(tag, transTag, false, 0, true);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_STREQ(transTag.c_str(), "testAlg_Hccs_");

    bool ret1 = communicator->transportManager_->IsHccsTransport(0, TransportLinkType::HCCS);
    EXPECT_EQ(ret1, true);
    ret1 = communicator->transportManager_->IsHccsTransport(0, TransportLinkType::SIO);
    EXPECT_EQ(ret1, false);

    MOCKER(hrtGetPairDeviceLinkType).stubs()
    .with(any(), any(), outBound(LinkTypeInServer::SIO_TYPE))
    .will(returnValue(HCCL_SUCCESS));
    ret1 = communicator->transportManager_->IsHccsTransport(0, TransportLinkType::RESERVED);
    EXPECT_EQ(ret1, false);
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_TransportMgrExceptionHandle)
{
    HcclResult ret = HCCL_SUCCESS;
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> communicator(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    communicator->Init(params, rankTable);

    TransportRequest transportReq1;
    transportReq1.isValid = true;
    transportReq1.remoteUserRank = 0;
    transportReq1.remoteUserRank = 1;
    transportReq1.linkType = TransportLinkType::HCCS;

    SingleSubCommTransport singleTrans;
    singleTrans.transportRequests.emplace_back(transportReq1);
    singleTrans.links.resize(2,nullptr);
    singleTrans.status.resize(2, TransportStatus::INIT);
    LevelNSubCommTransport levelTrans;
    levelTrans.emplace_back(singleTrans);
    OpCommTransport opTrans;
    opTrans.emplace_back(levelTrans);

    MOCKER_CPP(&TransportManager::UpdateIsInterRdma).stubs().with(any(), outBound(false), any()).will(ignoreReturnValue());
    MOCKER_CPP(&TransportManager::MakeRemoteLinkInfo).stubs().will(returnValue(HCCL_SUCCESS));

    // stubs in IsHccsTransport
    MOCKER(hrtGetPairDeviceLinkType).stubs().with(any(), any(), outBound(LinkTypeInServer::HCCS_SW_TYPE)).will(returnValue(HCCL_SUCCESS));

    MOCKER(Is310PDevice).stubs().will(returnValue(false));
    MOCKER_CPP(&HcclSocketManager::AddWhiteList, HcclResult(HcclSocketManager::*)(const std::string&, const HcclNetDevCtx, HcclRankLinkInfo))
    .stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    std::string tag("test");
    ret = communicator->transportManager_->ExceptionHandle(tag, opTrans);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_TransportMgrSetMachinePara)
{
    HcclResult ret = HCCL_SUCCESS;
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> communicator(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    communicator->Init(params, rankTable);

    std::string tag("testSetMachinePara");
    MachineType machineType = MachineType::MACHINE_CLIENT_TYPE;
    std::string serverId("testServer_id");
    std::vector<std::shared_ptr<HcclSocket>> socketList;
    DeviceMem inputMem;
    DeviceMem outputMem;
    DeviceMem expMem;
    MachinePara machinePara;
    RankInfo loaclRankInfo;
    RankInfo remoteRankInfo;
    HcclNetDevCtx netDevCtx = nullptr;

    ret = communicator->transportManager_->SetMachinePara(tag, machineType, serverId, 1, true, LinkMode::LINK_DUPLEX_MODE,
        socketList, inputMem, outputMem, expMem, false, false, false, 1, 0, 1, machinePara, loaclRankInfo, remoteRankInfo, netDevCtx,
        TransportLinkType::RESERVED);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(machinePara.specifyLink, LinkTypeInServer::RESERVED_LINK_TYPE);

    ret = communicator->transportManager_->SetMachinePara(tag, machineType, serverId, 1, true, LinkMode::LINK_DUPLEX_MODE,
        socketList, inputMem, outputMem, expMem, false, false, false, 1, 0, 1, machinePara, loaclRankInfo, remoteRankInfo, netDevCtx,
        TransportLinkType::HCCS);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(machinePara.specifyLink, LinkTypeInServer::HCCS_SW_TYPE);

    ret = communicator->transportManager_->SetMachinePara(tag, machineType, serverId, 1, true, LinkMode::LINK_DUPLEX_MODE,
        socketList, inputMem, outputMem, expMem, false, false, false, 1, 0, 1, machinePara, loaclRankInfo, remoteRankInfo, netDevCtx,
        TransportLinkType::SIO);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(machinePara.specifyLink, LinkTypeInServer::SIO_TYPE);
    GlobalMockObject::verify();
}
#endif

std::vector<RankInfo_t> get_rank_list_1()
{
    std::vector<RankInfo_t> rankList;

    // rank 信息
    RankInfo_t rank;
    rank.serverIdx = 0;
    rank.serverId = "192.168.1.1";
    rank.deviceInfo.devicePhyId = 0;
    rankList.push_back(rank);

    rank.serverIdx = 1;
    rank.serverId = "192.168.1.2";
    rank.deviceInfo.devicePhyId = 0;
    rankList.push_back(rank);

   return rankList;
}

std::vector<RankInfo_t> get_rank_list_2()
{
    std::vector<RankInfo_t> rankList;

    // rank 信息
    RankInfo_t rank;
    rank.serverIdx = 0;
    rank.serverId = "192.168.1.1";
    rank.deviceInfo.devicePhyId = 0;
    rankList.push_back(rank);
    rank.deviceInfo.devicePhyId = 8;
    rankList.push_back(rank);

    rank.serverIdx = 1;
    rank.serverId = "192.168.1.2";
    rank.deviceInfo.devicePhyId = 1;
    rankList.push_back(rank);
    rank.deviceInfo.devicePhyId = 8;
    rankList.push_back(rank);

   return rankList;
}

TEST_F(HcclImplTest, ut_hcclimpl_CalAndSetMeshAggRankSize_1)
{
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcclResult ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    implBase->deviceType_ = DevType::DEV_TYPE_910B;
    implBase->deviceNumPerServer_ = 1;
    implBase->deviceNumPerAggregation_ = 0;

    ret = HCCL_E_PARA;
    std::vector<RankInfo_t> rankList = get_rank_list_1();
    ret = implBase->attrCollector_.TransformRankInfoByServerId(rankList, implBase->servRankInfo_);
    ret = implBase->attrCollector_.CalAndSetMeshAggRankSize();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_CalAndSetMeshAggRankSize_2)
{
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());
   
    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcclResult ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    implBase->deviceType_ = DevType::DEV_TYPE_910B;
    implBase->deviceNumPerServer_ = 2;
    implBase->deviceNumPerAggregation_ = 1;

    ret = HCCL_E_PARA;
    std::vector<RankInfo_t> rankList = get_rank_list_2();
    ret = implBase->attrCollector_.TransformRankInfoByServerId(rankList, implBase->servRankInfo_);
    ret = implBase->attrCollector_.CalAndSetMeshAggRankSize();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    implBase = nullptr;
    GlobalMockObject::verify();
}


TEST_F(HcclImplTest, ut_GetCqeError)
{
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcclResult ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    implBase->deviceLogicId_ = 0;

    HcclResult result;

    implBase->GetCqeError(result);

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_SelectAlgoTypeForReduceScatter)
{
    float delay = 60;
    u64 curSize = 100; // 单位：字节(B)
    float bandWidth = 0.005;
    u32 deviceNumPerAggregation = 8;
    AlgTypeLevel1 algType;
    HcclResult ret;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_REDUCE_SCATTER));

    operation->moduleNum_ = 2;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForReduceScatter(delay, curSize, bandWidth, algType);
    auto iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_RING);

    operation->moduleNum_ = 3;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForReduceScatter(delay, curSize, bandWidth, algType);
    iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_RING);

    operation->moduleNum_ = 4;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForReduceScatter(delay, curSize, bandWidth, algType);
    iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_NHR);

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_SelectAlgoTypeForReduceScatter_15SEVER)
{
    float delay = 60;
    u64 curSize = 21000 * 8; // 单位：字节(B) 0.126MB
    float bandWidth = BANDWIDTH_RDMA_910B * 1024 * 1024 * 1024;
    u32 deviceNumPerAggregation = 8;
    AlgTypeLevel1 algType;
    HcclResult ret;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_REDUCE_SCATTER));

    operation->moduleNum_ = 15;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForReduceScatter(delay, curSize, bandWidth, algType);
    auto iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_NHR);

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_SelectAlgoTypeForReduceScatter_11SEVER_1)
{
    float delay = 60;
    u64 curSize = 21000 * 8; // 单位：字节(B) 0.126MB
    float bandWidth = BANDWIDTH_RDMA_910B * 1024 * 1024 * 1024;
    u32 deviceNumPerAggregation = 8;
    AlgTypeLevel1 algType;
    HcclResult ret;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_REDUCE_SCATTER));

    operation->moduleNum_ = 11;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForReduceScatter(delay, curSize, bandWidth, algType);
    auto iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_NHR);

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_SelectAlgoTypeForReduceScatter_11SEVER_2)
{
    float delay = 60;
    u64 curSize = 3900 * 8; // 单位：字节(B) 0.0312MB
    float bandWidth = BANDWIDTH_RDMA_910B * 1024 * 1024 * 1024;
    u32 deviceNumPerAggregation = 8;
    AlgTypeLevel1 algType;
    HcclResult ret;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_REDUCE_SCATTER));

    operation->moduleNum_ = 11;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForReduceScatter(delay, curSize, bandWidth, algType);
    auto iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_NHR);

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_SelectAlgoTypeForAllGather)
{
    float delay = 60;
    u64 curSize = 100; // 单位：字节(B)
    float bandWidth = 0.005;
    u32 deviceNumPerAggregation = 8;
    AlgTypeLevel1 algType;
    HcclResult ret;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_ALLGATHER));

    operation->moduleNum_ = 2;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForAllGather(delay, curSize, bandWidth, algType);
    auto iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_RING);

    operation->moduleNum_ = 3;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForAllGather(delay, curSize, bandWidth, algType);
    iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_RING);

    operation->moduleNum_ = 4;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForAllGather(delay, curSize, bandWidth, algType);
    iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_NHR);

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_SelectAlgoTypeForAllGather_15SEVER)
{
    float delay = 60;
    u64 curSize = 21000 * 8; // 单位：字节(B) 0.126MB
    float bandWidth = BANDWIDTH_RDMA_910B * 1024 * 1024 * 1024;
    u32 deviceNumPerAggregation = 8;
    AlgTypeLevel1 algType;
    HcclResult ret;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_ALLGATHER));

    operation->moduleNum_ = 15;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForAllGather(delay, curSize, bandWidth, algType);
    auto iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_NHR);

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_SelectAlgoTypeForAllGather_11SEVER_1)
{
    float delay = 60;
    u64 curSize = 21000 * 8; // 单位：字节(B) 0.126MB
    float bandWidth = BANDWIDTH_RDMA_910B * 1024 * 1024 * 1024;
    u32 deviceNumPerAggregation = 8;
    AlgTypeLevel1 algType;
    HcclResult ret;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_ALLGATHER));

    operation->moduleNum_ = 11;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForAllGather(delay, curSize, bandWidth, algType);
    auto iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_NHR);

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_SelectAlgoTypeForAllGather_11SEVER_2)
{
    float delay = 60;
    u64 curSize = 3900 * 8; // 单位：字节(B) 0.0312MB
    float bandWidth = BANDWIDTH_RDMA_910B * 1024 * 1024 * 1024;
    u32 deviceNumPerAggregation = 8;
    AlgTypeLevel1 algType;
    HcclResult ret;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_ALLGATHER));

    operation->moduleNum_ = 11;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForAllGather(delay, curSize, bandWidth, algType);
    auto iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", impl->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_NHR);

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_SelectAlgoTypeForGather)
{
    float delay = 60;
    u64 curSize = 100; // 单位：字节(B)
    float bandWidth = 0.005;
    u32 deviceNumPerAggregation = 8;
    AlgTypeLevel1 algType;
    HcclResult ret;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_GATHER));

    operation->moduleNum_ = 2;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForGather(delay, curSize, bandWidth, algType);
    auto iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_RING);

    operation->moduleNum_ = 3;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForGather(delay, curSize, bandWidth, algType);
    iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_RING);

    operation->moduleNum_ = 4;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForGather(delay, curSize, bandWidth, algType);
    iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_HD);

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_SelectAlgoTypeForAllReduce)
{
    float delay = 60;
    u64 curSize = 100; // 单位：字节(B)
    float bandWidth = 0.005;
    u32 deviceNumPerAggregation = 8;
    AlgTypeLevel1 algType;
    HcclResult ret;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_ALLREDUCE));

    operation->moduleNum_ = 2;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForAllReduce(delay, curSize, bandWidth, algType);
    auto iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_RING);

    operation->moduleNum_ = 3;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForAllReduce(delay, curSize, bandWidth, algType);
    iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_RING);

    operation->moduleNum_ = 4;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForAllReduce(delay, curSize, bandWidth, algType);
    iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_NHR);

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_SelectAlgoTypeForAllReduce_15SEVER)
{
    float delay = 60;
    u64 curSize = 21000000 * 8; // 单位：字节(B) 0.126MB
    float bandWidth = BANDWIDTH_RDMA_910B * 1024 * 1024 * 1024;
    u32 deviceNumPerAggregation = 8;
    AlgTypeLevel1 algType;
    HcclResult ret;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_ALLREDUCE));

    operation->moduleNum_ = 15;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForAllReduce(delay, curSize, bandWidth, algType);
    auto iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_NHR);

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_SelectAlgoTypeForAllReduce_11SEVER_1)
{
    float delay = 60;
    u64 curSize = 21000000 * 8; // 单位：字节(B) 0.126MB
    float bandWidth = BANDWIDTH_RDMA_910B * 1024 * 1024 * 1024;
    u32 deviceNumPerAggregation = 8;
    AlgTypeLevel1 algType;
    HcclResult ret;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_ALLREDUCE));

    operation->moduleNum_ = 11;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForAllReduce(delay, curSize, bandWidth, algType);
    auto iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_NHR);

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_SelectAlgoTypeForAllReduce_11SEVER_2)
{
    float delay = 60;
    u64 curSize = 2000000 * 8; // 单位：字节(B) 0.0312MB
    float bandWidth = BANDWIDTH_RDMA_910B * 1024 * 1024 * 1024;
    u32 deviceNumPerAggregation = 8;
    AlgTypeLevel1 algType;
    HcclResult ret;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_ALLREDUCE));

    operation->moduleNum_ = 11;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForAllReduce(delay, curSize, bandWidth, algType);
    auto iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_NHR);

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_SelectAlgoTypeForBroadcast)
{
    float delay = 60;
    u64 curSize = 100; // 单位：字节(B)
    float bandWidth = 0.005;
    u32 deviceNumPerAggregation = 8;
    AlgTypeLevel1 algType;
    HcclResult ret;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_BROADCAST));

    operation->moduleNum_ = 2;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForBroadcast(delay, curSize, bandWidth, algType);
    auto iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_RING);

    operation->moduleNum_ = 3;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForBroadcast(delay, curSize, bandWidth, algType);
    iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_RING);

    operation->moduleNum_ = 4;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForBroadcast(delay, curSize, bandWidth, algType);
    iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_HD);

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_SelectAlgoTypeForReduce)
{
    float delay = 60;
    u64 curSize = 100; // 单位：字节(B)
    float bandWidth = 0.005;
    u32 deviceNumPerAggregation = 8;
    AlgTypeLevel1 algType;
    HcclResult ret;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_REDUCE));

    operation->moduleNum_ = 2;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForReduce(delay, curSize, bandWidth, algType);
    auto iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_RING);

    operation->moduleNum_ = 3;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForReduce(delay, curSize, bandWidth, algType);
    iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_RING);

    operation->moduleNum_ = 4;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    ret = operation->SelectAlgoTypeForReduce(delay, curSize, bandWidth, algType);
    iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_HD);

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_GetDefaultAlgoLevel1V2_REDUCE_SCATTER)
{
    u64 curCount = 100; // 单位：字节(B)
    u32 deviceNumPerAggregation = 8;
    HcclCMDType hcclCMDType = HcclCMDType::HCCL_CMD_REDUCE_SCATTER;
    HcclDataType dataType = HcclDataType::HCCL_DATA_TYPE_UINT64;
    u32 unitSize = SIZE_TABLE[dataType];
    u64 curSize = curCount*unitSize;
    AlgTypeLevel1 algType;
    HcclResult ret;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_REDUCE_SCATTER));

    operation->moduleNum_ = 2;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    operation->deviceType_ = DevType::DEV_TYPE_910;
    ret = operation->GetDefaultAlgoLevel1V2(hcclCMDType, curSize, curSize, algType);
    auto iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_RING);

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_GetDefaultAlgoLevel1V2_ALLGATHER)
{
    u64 curCount = 100; // 单位：字节(B)
    u32 deviceNumPerAggregation = 8;
    HcclCMDType hcclCMDType = HcclCMDType::HCCL_CMD_ALLGATHER;
    HcclDataType dataType = HcclDataType::HCCL_DATA_TYPE_UINT64;
    u32 unitSize = SIZE_TABLE[dataType];
    u64 curSize = curCount*unitSize;
    AlgTypeLevel1 algType;
    HcclResult ret;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_ALLGATHER));

    operation->moduleNum_ = 2;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    operation->deviceType_ = DevType::DEV_TYPE_910;
    ret = operation->GetDefaultAlgoLevel1V2(hcclCMDType, curSize, curSize, algType);
    auto iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_RING);

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_GetDefaultAlgoLevel1V2_ALLREDUCE)
{
    u64 curCount = 100; // 单位：字节(B)
    u32 deviceNumPerAggregation = 8;
    HcclCMDType hcclCMDType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    HcclDataType dataType = HcclDataType::HCCL_DATA_TYPE_UINT64;
    u32 unitSize = SIZE_TABLE[dataType];
    u64 curSize = curCount*unitSize;
    AlgTypeLevel1 algType;
    HcclResult ret;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_ALLREDUCE));

    operation->moduleNum_ = 2;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    operation->deviceType_ = DevType::DEV_TYPE_910;
    ret = operation->GetDefaultAlgoLevel1V2(hcclCMDType, curSize, curSize, algType);
    auto iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algType, AlgTypeLevel1::ALG_LEVEL1_RING);

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_GetDefaultAlgoLevel1V2_OTHER_TYPE)
{
    u64 curCount = 100; // 单位：字节(B)
    u32 deviceNumPerAggregation = 8;
    HcclCMDType hcclCMDType = HcclCMDType::HCCL_CMD_MAX;
    HcclDataType dataType = HcclDataType::HCCL_DATA_TYPE_UINT64;
    u32 unitSize = SIZE_TABLE[dataType];
    u64 curSize = curCount*unitSize;
    AlgTypeLevel1 algType;
    HcclResult ret;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_MAX));

    operation->moduleNum_ = 2;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    operation->deviceType_ = DevType::DEV_TYPE_910;
    ret = operation->GetDefaultAlgoLevel1V2(hcclCMDType, curSize, curSize, algType);
    auto iter = HCCL_ALGO_LEVEL1_NAME_MAP.find(algType);
    if (iter == HCCL_ALGO_LEVEL1_NAME_MAP.end()) {
        HCCL_ERROR("level1: algType[%u] is invalid.", algType);
        ret = HCCL_E_INTERNAL;
    }
    HCCL_RUN_INFO(
        "hccl algorithm: there are %u server in level1, using %s algo.", operation->moduleNum_, iter->second.c_str());
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_AutoSelectAlgTypeLevel1)
{
    u64 curCount = 100; // 单位：字节(B)
    u32 deviceNumPerAggregation = 8;
    HcclCMDType hcclCMDType = HcclCMDType::HCCL_CMD_REDUCE_SCATTER;
    HcclDataType dataType = HcclDataType::HCCL_DATA_TYPE_UINT64;
    u32 unitSize = SIZE_TABLE[dataType];
    u64 curSize = curCount*unitSize;
    std::string algTypeLevel1Tag;
    HcclResult ret;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_REDUCE_SCATTER));

    operation->moduleNum_ = 2;
    operation->deviceNumPerAggregation_ = deviceNumPerAggregation;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    operation->deviceType_ = DevType::DEV_TYPE_910;
    operation->isAlgoLevel1Default_ = true;
    ret = operation->AutoSelectAlgTypeLevel1(hcclCMDType, curSize, curSize, algTypeLevel1Tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(algTypeLevel1Tag, "ALG_LEVEL1_RING");

    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_AiCpuSetCommResource)
{
    void *commInputPtr = nullptr;
    u64 commInputSize;
    void *commOutputPtr = nullptr;
    u64 commOutputSize = 0;
    HcclResult ret;
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    std::string tag = "test";
    CommInfo tmpComm;
    std::vector<RankInfo> paraVector;
    u32 rankSize = 8;
    u32 curRankId = 0;
    u64 commBufferSize = 20;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcclImpl *impl = implBase->implAlg_->pimpl_.get();

    ret = implBase->GetInCCLbuffer(commInputPtr, commInputSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = implBase->GetOutCCLbuffer(commOutputPtr, commOutputSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem expMem = implBase->cclBufferManager_.GetCommExpBuffer();

    implBase->notifyPool_.reset(new (std::nothrow) NotifyPool());
    ret = implBase->notifyPool_->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    IntraExchanger exchanger{};
    tmpComm.commLevel0.resize(4);
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    for (int i = 0; i < 4; i++) {
        tmpComm.commLevel0[i].reset(new (std::nothrow) CommRing(tag, 0, 8, curRankId, rankSize,
            TopoType::TOPO_TYPE_8P_RING, implBase->dispatcher_, implBase->notifyPool_, netDevCtxMap, exchanger, paraVector,
            inputMem, outputMem, false, nullptr, 0));
    }
    EXPECT_EQ(tmpComm.commLevel0[0]->transportInfo_.size(), rankSize);
    tmpComm.commLevel0[0]->transportInfo_.clear();
    std::chrono::milliseconds kdefaultTimeout = std::chrono::seconds(120);
    MachinePara linkPara;
    std::shared_ptr<Transport> link(new Transport(new (std::nothrow) TransportBase(
        nullptr, nullptr, linkPara, kdefaultTimeout)));

    ret = implBase->notifyPool_->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::shared_ptr<LocalIpcNotify> localNotify = nullptr;
    std::shared_ptr<RemoteNotify> remoteNotify = nullptr;

    RemoteRankInfo info(0, -1);
    SalGetBareTgid(reinterpret_cast<u32*>(&info.remotePid));
    ret = implBase->notifyPool_->Alloc(tag, info, localNotify, NotifyLoadType::DEVICE_NOTIFY);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::vector<u8> data(NOTIFY_INFO_LENGTH, 0);
    ret = localNotify->Serialize(data);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remoteNotify.reset(new (std::nothrow) RemoteNotify());

    ret = remoteNotify->Init(data);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = remoteNotify->Open();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    link->pimpl_->remoteSendDoneDeviceNotify_ = remoteNotify;
    link->pimpl_->localSendDoneDeviceNotify_ = localNotify;
    link->pimpl_->remoteSendReadyDeviceNotify_ = remoteNotify;
    link->pimpl_->localSendReadyDeviceNotify_ = localNotify;
    for (u32 ringIndex = 0; ringIndex < rankSize; ringIndex++) {
        tmpComm.commLevel0[0]->transportInfo_.push_back(link);
    }
    implBase->tagCommInfo_.insert(std::pair<std::string, CommInfo>(tag, std::move(tmpComm)));

    Level1StreamInfo tmpInnerStreamInfo;
    tmpInnerStreamInfo.ringNum = rankSize;
    tmpInnerStreamInfo.ringDeviceSignal.resize(rankSize - 1);
    tmpInnerStreamInfo.ringDeviceSignalAux.resize(rankSize - 1);
    tmpInnerStreamInfo.ringDeviceStreams.resize(rankSize);

    for (u32 ringIndex = 0; ringIndex < tmpInnerStreamInfo.ringNum; ringIndex++) {
        tmpInnerStreamInfo.ringDeviceStreams[ringIndex] = Stream(StreamType::STREAM_TYPE_DEVICE);
        if (ringIndex != tmpInnerStreamInfo.ringNum - 1) {
            tmpInnerStreamInfo.ringDeviceSignal[ringIndex] = localNotify;
            tmpInnerStreamInfo.ringDeviceSignalAux[ringIndex] = localNotify;
        }
    }

    MOCKER_CPP(&Transport::GetRemoteMem, HcclResult(Transport::*)(hccl::UserMemType, void**))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&Transport::GetRemoteMem, HcclResult(Transport::*)(std::vector<void*>*))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    // 配置profiling开关
    auto &profilingManager = hccl::ProfilingManager::Instance();
    profilingManager.StartAddtionInfoSubscribe();

    rtStream_t aiCpuStream;
    Stream stream(aiCpuStream);
    implBase->tagStreamInfo_.insert(std::pair<std::string, Level1StreamInfo>(tag, std::move(tmpInnerStreamInfo)));
    ret = implBase->SetCommResource(commBufferSize, commInputPtr, commOutputPtr, expMem.ptr(),
        implBase->tagCommInfo_[tag].commLevel0[0].get(), implBase->tagStreamInfo_[tag], stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 profConfigL0 = 0x84000985;
    profilingManager.StopSubscribe(profConfigL0);

    HCCL_INFO("check if HcclCombinOpParam is match with aicpu struct HccCommResParamTask");
    HcclCombinOpParam *combinOparaPtr = reinterpret_cast<HcclCombinOpParam *>(implBase->combinOparaMem_->ptr());
    HcclCombinOpParam &combinOpara = *combinOparaPtr;
    EXPECT_EQ(implBase->combinOparaMem_->size(), 8928); // 请确认HccCommResParamTask同步修改
    EXPECT_EQ(combinOpara.rankId, curRankId);
    EXPECT_EQ(combinOpara.signalInfo.aicpuNotify.rankId, curRankId);
    EXPECT_EQ(combinOpara.rankNum, rankSize);
    EXPECT_EQ(combinOpara.winSize, commBufferSize);

    for(int i = 0; i < rankSize; i++) {
        EXPECT_EQ(combinOpara.signalInfo.noIpcEvents[i].resId, 0);
        EXPECT_EQ(combinOpara.signalInfo.ipcNotifys[i].rankId, i);
        EXPECT_EQ(combinOpara.signalInfo.ipcNotifys[i + rankSize].rankId, i);
        EXPECT_EQ(combinOpara.signalInfo.ipcNotifys[i + rankSize * 2].rankId, i);
        EXPECT_EQ(combinOpara.signalInfo.ipcNotifys[i + rankSize * 3].rankId, i);
        EXPECT_EQ(combinOpara.signalInfo.noIpcNotifys[i].rankId, i);
        if (i == curRankId) {
            EXPECT_EQ(combinOpara.signalInfo.ipcNotifys[i].resId, INVALID_U64);
            EXPECT_EQ(combinOpara.signalInfo.ipcNotifys[i + rankSize].resId, INVALID_U64);
            EXPECT_EQ(combinOpara.signalInfo.ipcNotifys[i + rankSize * 2].resId, INVALID_U64);
            EXPECT_EQ(combinOpara.signalInfo.ipcNotifys[i + rankSize * 3].resId, INVALID_U64);
        } else {
            EXPECT_EQ(combinOpara.signalInfo.ipcNotifys[i].tsId, 3);
            EXPECT_EQ(combinOpara.signalInfo.ipcNotifys[i + rankSize].tsId, 3);
            EXPECT_EQ(combinOpara.signalInfo.ipcNotifys[i + rankSize * 2].tsId, 3);
            EXPECT_EQ(combinOpara.signalInfo.ipcNotifys[i + rankSize * 3].tsId, 3);
            EXPECT_EQ(combinOpara.signalInfo.noIpcNotifys[i].tsId, 3);
            EXPECT_EQ(combinOpara.signalInfo.noIpcNotifys[i + rankSize].tsId, 3);
        }

        EXPECT_EQ(combinOpara.streamInfo[i].sqIds, 122);
    }

    implBase = nullptr;
    GlobalMockObject::verify();

}

HcclResult stub_hrtRaGetInterfaceVersionX(unsigned int phyId, unsigned int interfaceOpcode,
                                         unsigned int* interfaceVersion)
{
    *interfaceVersion = 1;
    return HCCL_SUCCESS;
}

TEST_F(HcclImplTest, ut_hcclimpl_AiCpuSetCommResource_MultiServer)
{
    u32 ifnumVersion = 3;
    MOCKER(hrtRaGetInterfaceVersion)
    .stubs()
    .with(any(), any(), outBoundP(&ifnumVersion))
    .will(returnValue(HCCL_SUCCESS));

    void *commInputPtr = nullptr;
    u64 commInputSize;
    void *commOutputPtr = nullptr;
    u64 commOutputSize = 0;
    HcclResult ret;
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    std::string tag = "test";
    CommInfo tmpComm;
    std::vector<RankInfo> paraVector;
    u32 rankSize = 8;
    u32 curRankId = 0;
    u64 commBufferSize = 20;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
	MOCKER(hrtRaGetSingleSocketVnicIpInfo)
	.stubs()
	.with(any())
	.will(invoke(stub_hrtRaGetSingleSocketVnicIpInfo));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcclImpl *impl = implBase->implAlg_->pimpl_.get();
    ret = implBase->GetInCCLbuffer(commInputPtr, commInputSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = implBase->GetOutCCLbuffer(commOutputPtr, commOutputSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem expMem = implBase->cclBufferManager_.GetCommExpBuffer();
    implBase->notifyPool_.reset(new (std::nothrow) NotifyPool());
    ret = implBase->notifyPool_->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    IntraExchanger exchanger{};
    tmpComm.commLevel0.resize(4);
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    for (int i = 0; i < 4; i++) {
        tmpComm.commLevel0[i].reset(new (std::nothrow) CommRing(tag, 0, 8, curRankId, rankSize,
            TopoType::TOPO_TYPE_8P_RING, implBase->dispatcher_, implBase->notifyPool_, netDevCtxMap, exchanger, paraVector,
            inputMem, outputMem, false, nullptr, 0));
    }
    EXPECT_EQ(tmpComm.commLevel0[0]->transportInfo_.size(), rankSize);
    tmpComm.commLevel0[0]->transportInfo_.clear();
    std::chrono::milliseconds kdefaultTimeout = std::chrono::seconds(120);
    MachinePara linkPara;
    std::shared_ptr<Transport> link(new Transport(new (std::nothrow) TransportBase(
        nullptr, nullptr, linkPara, kdefaultTimeout)));
    ret = implBase->notifyPool_->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::shared_ptr<LocalIpcNotify> localNotify = nullptr;
    std::shared_ptr<RemoteNotify> remoteNotify = nullptr;
    RemoteRankInfo info(0, -1);
    SalGetBareTgid(reinterpret_cast<u32*>(&info.remotePid));
    ret = implBase->notifyPool_->Alloc(tag, info, localNotify, NotifyLoadType::DEVICE_NOTIFY);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::vector<u8> data(NOTIFY_INFO_LENGTH, 0);
    ret = localNotify->Serialize(data);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remoteNotify.reset(new (std::nothrow) RemoteNotify());

    ret = remoteNotify->Init(data);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = remoteNotify->Open();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    link->pimpl_->remoteSendDoneDeviceNotify_ = remoteNotify;
    link->pimpl_->localSendDoneDeviceNotify_ = localNotify;
    link->pimpl_->remoteSendReadyDeviceNotify_ = remoteNotify;
    link->pimpl_->localSendReadyDeviceNotify_ = localNotify;
    for (u32 ringIndex = 0; ringIndex < rankSize; ringIndex++) {
        tmpComm.commLevel0[0]->transportInfo_.push_back(link);
    }
    implBase->tagCommInfo_.insert(std::pair<std::string, CommInfo>(tag, std::move(tmpComm)));

    Level1StreamInfo tmpInnerStreamInfo;
    tmpInnerStreamInfo.ringNum = rankSize;
    tmpInnerStreamInfo.ringDeviceSignal.resize(rankSize - 1);
    tmpInnerStreamInfo.ringDeviceSignalAux.resize(rankSize - 1);
    tmpInnerStreamInfo.ringDeviceStreams.resize(rankSize);

    for (u32 ringIndex = 0; ringIndex < tmpInnerStreamInfo.ringNum; ringIndex++) {
        tmpInnerStreamInfo.ringDeviceStreams[ringIndex] = Stream(StreamType::STREAM_TYPE_DEVICE);
        if (ringIndex != tmpInnerStreamInfo.ringNum - 1) {
            tmpInnerStreamInfo.ringDeviceSignal[ringIndex] = localNotify;
            tmpInnerStreamInfo.ringDeviceSignalAux[ringIndex] = localNotify;
        }
    }

    MOCKER_CPP(&Transport::GetRemoteMem, HcclResult(Transport::*)(hccl::UserMemType, void**))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&Transport::GetRemoteMem, HcclResult(Transport::*)(std::vector<void*>*))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&Transport::GetRemoteMemKey, HcclResult(Transport::*)(hccl::UserMemType, uint32_t *))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&Transport::GetLocalMemDetails)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    std::vector<HcclQpInfoV2> qpInfos(1);
    MOCKER_CPP(&Transport::GetAiQpInfo).stubs().with(outBound(qpInfos)).will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&Transport::GetChipId)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    machinePara.localDeviceId = 0;
    std::chrono::milliseconds timeout;
 
    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("test", 
        nullptr, remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machinePara.sockets.push_back(newSocket);

    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    TransportBase transportBase(dispatcher, notifyPool, machinePara, timeout);
    MOCKER_CPP_VIRTUAL(transportBase, &TransportBase::GetRemoteMemSize)
        .stubs()
        .with(any(), outBound(commBufferSize))
        .will(returnValue(HCCL_SUCCESS));
    // 配置profiling开关
    auto &profilingManager = hccl::ProfilingManager::Instance();
    profilingManager.StartAddtionInfoSubscribe();

    rtStream_t aiCpuStream;
    Stream stream(aiCpuStream);
    implBase->tagStreamInfo_.insert(std::pair<std::string, Level1StreamInfo>(tag, std::move(tmpInnerStreamInfo)));
    implBase->isA2MC2MultiServer_ = true;
    ret = implBase->SetCommResource(commBufferSize, commInputPtr, commOutputPtr, expMem.ptr(),
        implBase->tagCommInfo_[tag].commLevel0[0].get(), implBase->tagStreamInfo_[tag], stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 profConfigL0 = 0x84000985;
    profilingManager.StopSubscribe(profConfigL0);

    HCCL_INFO("check if HcclCombinOpParam is match with aicpu struct HccCommResParamTask");
    HcclCombinOpParam *combinOparaPtr = reinterpret_cast<HcclCombinOpParam *>(implBase->combinOparaMem_->ptr());
    HcclCombinOpParam &combinOpara = *combinOparaPtr;
    EXPECT_EQ(implBase->combinOparaMem_->size(), 8928); // 请确认HccCommResParamTask同步修改
    EXPECT_EQ(combinOpara.rankId, curRankId);
    EXPECT_EQ(combinOpara.signalInfo.aicpuNotify.rankId, curRankId);
    EXPECT_EQ(combinOpara.rankNum, rankSize);
    EXPECT_EQ(combinOpara.winSize, commBufferSize);

    EXPECT_EQ(implBase->transDevIbverbsDataMem_->size() / sizeof(TransportDeviceNormalData), rankSize);
    TransportDeviceNormalData *transDevIbverbsData =
        reinterpret_cast<TransportDeviceNormalData *>(implBase->transDevIbverbsDataMem_->ptr());
    for (u32 i = 0; i < rankSize; i++) {
            if (i != curRankId) {
                void* bufferIn = nullptr;
                void* bufferOut = nullptr;
                uint32_t remoteInMemKey = 0;
                uint32_t remoteOutMemKey = 0;
                EXPECT_EQ(transDevIbverbsData[i].remoteInputMem.addr, reinterpret_cast<uint64_t>(commInputPtr));
                EXPECT_EQ(transDevIbverbsData[i].remoteInputMem.size, commBufferSize);
                EXPECT_EQ(transDevIbverbsData[i].remoteOutputMem.addr, reinterpret_cast<uint64_t>(commOutputPtr));
                EXPECT_EQ(transDevIbverbsData[i].remoteOutputMem.size, commBufferSize);
            } else {
                // 本rank的信息
                EXPECT_EQ(transDevIbverbsData[i].localInputMem.addr, reinterpret_cast<uint64_t>(commInputPtr));
                EXPECT_EQ(transDevIbverbsData[i].localInputMem.size, commBufferSize);
                EXPECT_EQ(transDevIbverbsData[i].localOutputMem.addr, reinterpret_cast<uint64_t>(commOutputPtr));
                EXPECT_EQ(transDevIbverbsData[i].localOutputMem.size, commBufferSize);
            }
    }
    implBase = nullptr;
    GlobalMockObject::verify();

}

TEST_F(HcclImplTest, ut_hcclImpl_get_double_ring_topo_type_1)
{
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcclImpl *impl = implBase->implAlg_->pimpl_.get();
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    TopoType topoType;
    AlgType algType;
    algType.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_NP_DOUBLE_RING;
    algType.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    DevType chipType = DevType::DEV_TYPE_910_93;
    ret = algConfigurator->GetTopoTypeByAlgType(algType, chipType, topoType);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(topoType, TopoType::TOPO_TYPE_NP_DOUBLE_RING);
    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclImpl_get_double_ring_topo_type_2)
{
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcclImpl *impl = implBase->implAlg_->pimpl_.get();
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    TopoType topoType;
    AlgType algType;
    algType.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_NP_DOUBLE_RING;
    algType.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_HD;
    DevType chipType = DevType::DEV_TYPE_910_93;
    ret = algConfigurator->GetTopoTypeByAlgType(algType, chipType, topoType);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(topoType, TopoType::TOPO_TYPE_NP_DOUBLE_RING);
    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclImpl_is_double_ring_topo_pattern_1)
{
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclResult tmpRet = implBase->Init(params, rankTable);
    EXPECT_EQ(tmpRet, HCCL_SUCCESS);

    hcclImpl *impl = implBase->implAlg_->pimpl_.get();
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    algConfigurator->topoAttr_.deviceNumPerAggregation = 8;
    algConfigurator->topoAttr_.pairLinkCounter.clear();
    algConfigurator->topoAttr_.pairLinkCounter[static_cast<u32>(LinkTypeInServer::HCCS_SW_TYPE)] = 48;
    algConfigurator->topoAttr_.pairLinkCounter[static_cast<u32>(LinkTypeInServer::SIO_TYPE)] = 8;

    bool ret = algConfigurator->IsHCCSSWNumEqualToTwiceSIONum();
    EXPECT_EQ(ret, true);
    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclImpl_is_double_ring_topo_pattern_2)
{
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclResult tmpRet = implBase->Init(params, rankTable);
    EXPECT_EQ(tmpRet, HCCL_SUCCESS);

    hcclImpl *impl = implBase->implAlg_->pimpl_.get();
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    algConfigurator->topoAttr_.deviceNumPerAggregation = 8;
    algConfigurator->topoAttr_.pairLinkCounter.clear();
    algConfigurator->topoAttr_.pairLinkCounter[static_cast<u32>(LinkTypeInServer::HCCS_SW_TYPE)] = 0;
    algConfigurator->topoAttr_.pairLinkCounter[static_cast<u32>(LinkTypeInServer::SIO_TYPE)] = 8;

    bool ret = algConfigurator->IsHCCSSWNumEqualToTwiceSIONum();
    EXPECT_EQ(ret, false);
    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclImpl_is_double_ring_topo_pattern_3)
{
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclResult tmpRet = implBase->Init(params, rankTable);
    EXPECT_EQ(tmpRet, HCCL_SUCCESS);

    hcclImpl *impl = implBase->implAlg_->pimpl_.get();
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    algConfigurator->topoAttr_.deviceNumPerAggregation = 8;
    algConfigurator->topoAttr_.pairLinkCounter.clear();
    algConfigurator->topoAttr_.pairLinkCounter[static_cast<u32>(LinkTypeInServer::HCCS_SW_TYPE)] = 48;
    algConfigurator->topoAttr_.pairLinkCounter[static_cast<u32>(LinkTypeInServer::SIO_TYPE)] = 0;

    bool ret = algConfigurator->IsHCCSSWNumEqualToTwiceSIONum();
    EXPECT_EQ(ret, false);
    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclImpl_run_double_ring_executer_by_all_gather)
{
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    implBase->implAlg_->algConfigurator_->topoType_ = TopoType::TOPO_TYPE_NP_DOUBLE_RING;
    implBase->implAlg_->topoAttr_.deviceType = DevType::DEV_TYPE_910_93;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);
    HcomCollOpInfo opInfo;
    opInfo.inputAddr = inputMem.ptr();
    opInfo.outputAddr = outputMem.ptr();
    opInfo.count = count;
    opInfo.dataType = dataType;
    opInfo.reduceOp = op;

    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_NP_DOUBLE_RING;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_910_93;
    std::unique_ptr<AllGatherOperator> operation(new (std::nothrow) AllGatherOperator(algConfigurator.get(), cclBufferManager, dispatcher, topoMatcher));
    operation->topoType_ = TopoType::TOPO_TYPE_NP_DOUBLE_RING;

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

    std::string algName = "";
    std::string newTag = "";
    ret = operation->SelectAlg(tag, opParam, algName, newTag);
    EXPECT_TRUE(algName == "AlignedAllGatherDoubleRingFor91093Executor");
    AlgResourceRequest resourceRequest;
    ret = operation->CalcResRequest(algName, opParam, resourceRequest);

    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclImpl_run_double_ring_executer_by_all_reduce)
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
    TestConstructParam_1server_4p(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910_93;

    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Heartbeat::RegisterRanks)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Heartbeat::UnRegisterRanks)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&AlgConfigurator::IsHCCSSWNumEqualToTwiceSIONum)
    .stubs()
    .will(returnValue(true));

    ret = implBase->AtomicInitSet();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->InitCCLbuffer(4096, 4096);
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    impl->topoAttr_.deviceLogicId = 0;
    impl->topoAttr_.devicePhyId = 0;
    impl->topoType_ = TopoType::TOPO_TYPE_NP_DOUBLE_RING;

    algConfigurator->topoAttr_.deviceLogicId = 0;
    algConfigurator->topoAttr_.devicePhyId = 0;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLREDUCE].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_NP_DOUBLE_RING;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLREDUCE].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;

    algConfigurator->topoType_ = TopoType::TOPO_TYPE_NP_DOUBLE_RING;

    const std::vector<std::vector<u32>> tmpRingNics = {
        { 0, 1, 2, 3, 4, 5, 6, 7 },
        { 0, 1, 2, 3, 4, 5, 6, 7 }
    };
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&CollCommExecutor::MultiRingReduceScatter)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&CollCommExecutor::MultiRingAllGather)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    std::printf("[ut_CollAllReduceRingFor91093Executor_Ring]");
    ret = implBase->AllReduce(tag, inputMem.ptr(), outputMem.ptr(), count, dataType, op, stream.ptr());
    implBase = nullptr;

    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclImpl_run_fast_double_ring_for_910_93_executer_by_all_reduce)
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
    setenv("HCCL_ALGO", "level0:ring", 1);
    ret = InitEnvVarParam();
    TestConstructParam_1server_4p(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910_93;

    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&Heartbeat::RegisterRanks)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Heartbeat::UnRegisterRanks)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 

    MOCKER_CPP(&AlgConfigurator::IsHCCSSWNumEqualToTwiceSIONum)
    .stubs()
    .will(returnValue(true));

    ret = implBase->AtomicInitSet();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->InitCCLbuffer(4096, 4096);
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    impl->topoAttr_.deviceLogicId = 0;
    impl->topoAttr_.devicePhyId = 0;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLREDUCE].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_NP_DOUBLE_RING;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLREDUCE].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    impl->topoType_ = TopoType::TOPO_TYPE_NP_DOUBLE_RING;
    algConfigurator->topoType_ = TopoType::TOPO_TYPE_NP_DOUBLE_RING;

    const std::vector<std::vector<u32>> tmpRingNics = {
        { 0, 1, 2, 3, 4, 5, 6, 7 },
        { 0, 1, 2, 3, 4, 5, 6, 7 }
    };

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&CollCommExecutor::MultiRingReduceScatter)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&CollCommExecutor::MultiRingAllGather)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    std::printf("[ut_CollAllReduceRingFor91093Executor_Ring]");
    ret = implBase->AllReduce(tag, inputMem.ptr(), outputMem.ptr(), count, dataType, op, stream.ptr());
    implBase = nullptr;

    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_SendAndReceiveFor310P3)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "worldBatchSendRecv_test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_310P3;
    rankTable.serverNum = 2;

    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    implBase->AtomicInitSet();
    EXPECT_EQ(ret, HCCL_SUCCESS);


    MOCKER_CPP(&HcclCommunicator::ExecOp)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    rtStream_t stream;
    rtError_t rt_ret = rtStreamCreate(&stream, 0);
    implBase->Send(tag, inputMem.ptr(), count, dataType, 1, stream);
    implBase->Receive(tag, outputMem.ptr(), count, dataType, 1, stream);
    rtStreamDestroy(stream);
    implBase = nullptr;
    GlobalMockObject::verify();
}

struct CustomDeleterForTransport
{
    void operator()(Transport* p) const
    {
        ;
    }
};

TEST_F(HcclImplTest, ut_hcclimpl_SendCommFor310P3)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "worldBatchSendRecv_test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_310P3;
    rankTable.serverNum = 2;

    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    implBase->AtomicInitSet();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    std::unique_ptr<CommInfo> commInfo = nullptr;
    commInfo.reset(new (std::nothrow) CommInfo);
    impl->tagCommInfo_.insert(std::pair<std::string, CommInfo>(tag, std::move(*commInfo)));

    impl->tagCommInfo_[tag].commP2P.resize(1);
    std::string rootInfo = "test_collective";
    IntraExchanger exchanger{};
    std::vector<RankInfo> para_vector(1);
    TopoType topoFlag = TopoType::TOPO_TYPE_COMMON;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    impl->tagCommInfo_[tag].commP2P[0].reset(new (std::nothrow) CommP2P(rootInfo, 0, 1, 0, 1, topoFlag, dispatcher, nullptr, netDevCtxMap, exchanger, para_vector, DeviceMem(), DeviceMem(), true, nullptr, 0));
    impl->tagCommInfo_[tag].commP2P[0]->rankMap_.push_back(0);
    impl->tagCommInfo_[tag].commP2P[0]->rankMap_.push_back(0);

    MOCKER_CPP(&CommBase::GetTransportByRank)
    .stubs()
    .with(any())
    .will(returnValue(std::shared_ptr<Transport>((Transport*)0x01, CustomDeleterForTransport())));

    MOCKER_CPP(&SendReceive::SendPrepare)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&SendReceive::RegisterProfiler)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&SendReceive::SendRunAsync)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    Stream stream(StreamType::STREAM_TYPE_ONLINE);
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;
    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_ReceiveCommFor310P3)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "worldBatchSendRecv_test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_310P3;
    rankTable.serverNum = 2;

    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    implBase->AtomicInitSet();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    std::unique_ptr<CommInfo> commInfo = nullptr;
    commInfo.reset(new (std::nothrow) CommInfo);
    HCCL_INFO("tag[%s]", tag.c_str());
    HCCL_INFO("commInfo[%p]", commInfo.get());
    HCCL_INFO("tagCommInfo_.size[%u]",impl-> tagCommInfo_.size());
    impl->tagCommInfo_.insert(std::pair<std::string, CommInfo>(tag, std::move(*commInfo)));

    impl->tagCommInfo_[tag].commP2P.resize(1);
    std::string rootInfo = "test_collective";
    IntraExchanger exchanger{};
    std::vector<RankInfo> para_vector(1);
    TopoType topoFlag = TopoType::TOPO_TYPE_COMMON;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    impl->tagCommInfo_[tag].commP2P[0].reset(new (std::nothrow) CommP2P(rootInfo, 0, 1, 0, 1, topoFlag, dispatcher, nullptr, netDevCtxMap, exchanger, para_vector, DeviceMem(), DeviceMem(), true, nullptr, 0));
    impl->tagCommInfo_[tag].commP2P[0]->rankMap_.push_back(0);
    impl->tagCommInfo_[tag].commP2P[0]->rankMap_.push_back(0);

    MOCKER_CPP(&CommBase::GetTransportByRank)
    .stubs()
    .with(any())
    .will(returnValue(std::shared_ptr<Transport>((Transport*)0x01, CustomDeleterForTransport())));

    MOCKER_CPP(&SendReceive::ReceivePrepare)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&SendReceive::RegisterProfiler)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&SendReceive::ReceiveRunAsync)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    Stream stream(StreamType::STREAM_TYPE_ONLINE);
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;
    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hrtRaGetSocketVnicIpInfos)
{
    HcclResult ret =  HCCL_SUCCESS;
    u32 phy_id = 2;
    vector<u32> ids;
    std::vector<HcclIpAddress> vnicIp;
    vnicIp.push_back(HcclIpAddress("1.0.0.0"));
    vnicIp.push_back(HcclIpAddress("2.0.0.0"));
    DeviceIdType deviceIdType =  DeviceIdType::DEVICE_ID_TYPE_PHY_ID;
    IdType idType = static_cast<IdType>(deviceIdType);
    ret = hrtRaGetSocketVnicIpInfos(phy_id, idType, ids, vnicIp);
    EXPECT_EQ(ret, HCCL_E_PARA);
    ids.push_back(1);
    ids.push_back(2);
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hrtRaGetSocketVnicIpInfos(phy_id, idType, ids, vnicIp);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcclImplTest, ut_H2DTlvInit)
{
    HcclResult ret =  HCCL_SUCCESS;
    struct TlvInitInfo  init_info;
    uint32_t buffer_size = 0;
    void *tlv_handle = nullptr;

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = H2DTlvInit(&init_info, &buffer_size, &tlv_handle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DlRaFunction::GetInstance().dlH2DTlvInit = nullptr;
    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .will(returnValue(HCCL_E_NOT_SUPPORT));
    ret = H2DTlvInit(&init_info, &buffer_size, &tlv_handle);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}

TEST_F(HcclImplTest, ut_H2DTlvRequest)
{
    HcclResult ret =  HCCL_SUCCESS;
    struct TlvMsg send_msg;
    struct TlvMsg recv_msg;
    void *tlv_handle = nullptr;
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = H2DTlvRequest(tlv_handle,0, &send_msg, &send_msg);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DlRaFunction::GetInstance().dlH2DTlvRequest = nullptr;
    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .will(returnValue(HCCL_E_NOT_SUPPORT));
    ret = H2DTlvRequest(tlv_handle,0, &send_msg, &send_msg);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}

TEST_F(HcclImplTest, ut_H2DTlvDeinit)
{
    HcclResult ret =  HCCL_SUCCESS;
    void *tlv_handle = nullptr;

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(10))
    .will(returnValue(HCCL_E_NOT_SUPPORT));
    ret = H2DTlvDeinit(&tlv_handle);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}

TEST_F(HcclImplTest, ut_hccl_Communicator_SetRetryEnable_Inter_Super_Pod_true)
{
    HcclResult ret;
    setenv("HCCL_OP_RETRY_ENABLE", "L0:0, L1:0, L2:1", 1);
    ret = ParseRetryEnable();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclCommunicator impl;
    std::vector<RankInfo> ranks;

    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.0.0.10";
    tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    tmp_para_0.superPodId = "0";

    RankInfo tmp_para_1;

    tmp_para_1.userRank = 1;
    tmp_para_1.devicePhyId = 1;
    tmp_para_1.deviceType = DevType::DEV_TYPE_910;
    tmp_para_1.serverId = "10.0.1.10";
    tmp_para_1.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    tmp_para_1.superPodId = "1";

    ranks.push_back(tmp_para_0);
    ranks.push_back(tmp_para_1);

    impl.serverId_ = "10.0.0.10";
    impl.attrCollector_.serverId_ = "10.0.0.10";
    std::vector<RankInfo_t> rankListNew;
    ret = impl.attrCollector_.TransformRankList(ranks, rankListNew);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = impl.attrCollector_.SetServerNum(rankListNew);
    impl.serverNum_ = impl.attrCollector_.GetServerNum();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = GetSuperPodNum(rankListNew, impl.superPodNum_);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    impl.deviceNumPerAggregation_ = 8;
    HcclIpAddress serverIp("172.17.10.1");
    HcclIpAddress localIp("172.17.10.1");
    SetRetryEnable(DevType::DEV_TYPE_910_93, impl.superPodNum_,
        impl.serverNum_, impl.deviceNumPerAggregation_,
        impl.isDiffDeviceType_, impl.GetAivModeConfig(), serverIp, localIp, impl.retryEnable_);
    std::cout << "superPodNum_ is " << unsigned(impl.superPodNum_) << std::endl;
    std::cout << "serverNum_ is " << unsigned(impl.serverNum_) << std::endl;
    std::cout << "deviceNumPerAggregation_ is " << unsigned(impl.deviceNumPerAggregation_) << std::endl;
    if (impl.retryEnable_) {
        std::cout << "retryEnable_ is true" << std::endl;
    } else {
        std::cout << "retryEnable_ is false" << std::endl;
    }
    EXPECT_EQ(impl.retryEnable_, true);
    unsetenv("HCCL_OP_RETRY_ENABLE");
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hccl_Communicator_SetRetryEnable_Inter_2Sever_true)
{
    HcclResult ret;
    setenv("HCCL_OP_RETRY_ENABLE", "L0:0, L1:1, L2:0", 1);
    ret = ParseRetryEnable();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclCommunicator impl;
    std::vector<RankInfo> ranks;

    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.0.0.10";
    tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    tmp_para_0.superPodId = "0";

    RankInfo tmp_para_1;

    tmp_para_1.userRank = 1;
    tmp_para_1.devicePhyId = 1;
    tmp_para_1.deviceType = DevType::DEV_TYPE_910;
    tmp_para_1.serverId = "10.0.1.10";
    tmp_para_1.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    tmp_para_1.superPodId = "0";

    ranks.push_back(tmp_para_0);
    ranks.push_back(tmp_para_1);

    impl.serverId_ = "10.0.0.10";
    impl.attrCollector_.serverId_ = "10.0.0.10";
    std::vector<RankInfo_t> rankListNew;
    ret = impl.attrCollector_.TransformRankList(ranks, rankListNew);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = impl.attrCollector_.SetServerNum(rankListNew);
    impl.serverNum_ = impl.attrCollector_.GetServerNum();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = GetSuperPodNum(rankListNew, impl.superPodNum_);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    impl.deviceNumPerAggregation_ = 8;
    HcclIpAddress serverIp("172.17.10.1");
    HcclIpAddress localIp("172.17.10.1");
    SetRetryEnable(DevType::DEV_TYPE_910_93, impl.superPodNum_, impl.serverNum_, impl.deviceNumPerAggregation_,
        impl.isDiffDeviceType_, impl.GetAivModeConfig(), serverIp, localIp, impl.retryEnable_);
    std::cout << "superPodNum_ is " << unsigned(impl.superPodNum_) << std::endl;
    std::cout << "serverNum_ is " << unsigned(impl.serverNum_) << std::endl;
    std::cout << "deviceNumPerAggregation_ is " << unsigned(impl.deviceNumPerAggregation_) << std::endl;
    if (impl.retryEnable_) {
        std::cout << "retryEnable_ is true" << std::endl;
    } else {
        std::cout << "retryEnable_ is false" << std::endl;
    }
    EXPECT_EQ(impl.retryEnable_, true);
    unsetenv("HCCL_OP_RETRY_ENABLE");
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hccl_Communicator_SetRetryEnable_Inter_1Sever_true)
{
    HcclResult ret;
    setenv("HCCL_OP_RETRY_ENABLE", "L0:0, L1:1, L2:0", 1);
    ret = ParseRetryEnable();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclCommunicator impl;
    std::vector<RankInfo> ranks;

    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.0.0.10";
    tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    tmp_para_0.superPodId = "0";

    ranks.push_back(tmp_para_0);

    impl.serverId_ = "10.0.0.10";

    std::vector<RankInfo_t> rankListNew;
    ret = impl.attrCollector_.TransformRankList(ranks, rankListNew);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = impl.attrCollector_.SetServerNum(rankListNew);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = GetSuperPodNum(rankListNew, impl.superPodNum_);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    impl.deviceNumPerAggregation_ = 8;
    HcclIpAddress serverIp("172.17.10.1");
    HcclIpAddress localIp("172.17.10.1");
    SetRetryEnable(DevType::DEV_TYPE_910_93, impl.superPodNum_, impl.serverNum_, impl.deviceNumPerAggregation_,
        impl.isDiffDeviceType_, impl.GetAivModeConfig(), serverIp, localIp, impl.retryEnable_);
    std::cout << "superPodNum_ is " << unsigned(impl.superPodNum_) << std::endl;
    std::cout << "serverNum_ is " << unsigned(impl.serverNum_) << std::endl;
    std::cout << "deviceNumPerAggregation_ is " << unsigned(impl.deviceNumPerAggregation_) << std::endl;
    if (impl.retryEnable_) {
        std::cout << "retryEnable_ is true" << std::endl;
    } else {
        std::cout << "retryEnable_ is false" << std::endl;
    }
    EXPECT_EQ(impl.retryEnable_, false);
    unsetenv("HCCL_OP_RETRY_ENABLE");
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hccl_Communicator_SetRetryEnable_Intra_Sever_true)
{
    HcclResult ret;
    setenv("HCCL_OP_RETRY_ENABLE", "L0:1, L1:0, L2:0", 1);
    ret = ParseRetryEnable();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclCommunicator impl;
    std::vector<RankInfo> ranks;

    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.0.0.10";
    tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    tmp_para_0.superPodId = "0";

    ranks.push_back(tmp_para_0);

    impl.serverId_ = "10.0.0.10";
    impl.attrCollector_.serverId_ = "10.0.0.10";

    std::vector<RankInfo_t> rankListNew;
    ret = impl.attrCollector_.TransformRankList(ranks, rankListNew);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = impl.attrCollector_.SetServerNum(rankListNew);
    impl.serverNum_ = impl.attrCollector_.GetServerNum();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = GetSuperPodNum(rankListNew, impl.superPodNum_);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    impl.deviceNumPerAggregation_ = 8;
    HcclIpAddress serverIp("172.17.10.1");
    HcclIpAddress localIp("172.17.10.1");
    SetRetryEnable(DevType::DEV_TYPE_910_93, impl.superPodNum_, impl.serverNum_, impl.deviceNumPerAggregation_,
        impl.isDiffDeviceType_, impl.GetAivModeConfig(), serverIp, localIp, impl.retryEnable_);
    std::cout << "superPodNum_ is " << unsigned(impl.superPodNum_) << std::endl;
    std::cout << "serverNum_ is " << unsigned(impl.serverNum_) << std::endl;
    std::cout << "deviceNumPerAggregation_ is " << unsigned(impl.deviceNumPerAggregation_) << std::endl;
    if (impl.retryEnable_) {
        std::cout << "retryEnable_ is true" << std::endl;
    } else {
        std::cout << "retryEnable_ is false" << std::endl;
    }
    EXPECT_EQ(impl.retryEnable_, false);
    unsetenv("HCCL_OP_RETRY_ENABLE");
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hccl_Communicator_SetRetryEnable_All_False)
{
    HcclResult ret;
    setenv("HCCL_OP_RETRY_ENABLE", "L0:0, L1:0, L2:0", 1);
    ret = ParseRetryEnable();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclCommunicator impl;
    std::vector<RankInfo> ranks;

    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.0.0.10";
    tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    tmp_para_0.superPodId = "0";

    RankInfo tmp_para_1;

    tmp_para_1.userRank = 1;
    tmp_para_1.devicePhyId = 1;
    tmp_para_1.deviceType = DevType::DEV_TYPE_910;
    tmp_para_1.serverId = "10.0.1.10";
    tmp_para_1.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    tmp_para_1.superPodId = "1";

    ranks.push_back(tmp_para_0);
    ranks.push_back(tmp_para_1);

    impl.serverId_ = "10.0.0.10";

    std::vector<RankInfo_t> rankListNew;
    ret = impl.attrCollector_.TransformRankList(ranks, rankListNew);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = impl.attrCollector_.SetServerNum(rankListNew);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = GetSuperPodNum(rankListNew, impl.superPodNum_);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    impl.deviceNumPerAggregation_ = 8;
    HcclIpAddress serverIp("172.17.10.1");
    HcclIpAddress localIp("172.17.10.1");
    SetRetryEnable(DevType::DEV_TYPE_910_93, impl.superPodNum_, impl.serverNum_, impl.deviceNumPerAggregation_,
        impl.isDiffDeviceType_, impl.GetAivModeConfig(), serverIp, localIp, impl.retryEnable_);
    std::cout << "superPodNum_ is " << unsigned(impl.superPodNum_) << std::endl;
    std::cout << "serverNum_ is " << unsigned(impl.serverNum_) << std::endl;
    std::cout << "deviceNumPerAggregation_ is " << unsigned(impl.deviceNumPerAggregation_) << std::endl;
    if (impl.retryEnable_) {
        std::cout << "retryEnable_ is true" << std::endl;
    } else {
        std::cout << "retryEnable_ is false" << std::endl;
    }
    EXPECT_EQ(impl.retryEnable_, false);
    unsetenv("HCCL_OP_RETRY_ENABLE");
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hccl_Communicator_SetRetryEnable_deviceNumPerAggregation_1)
{
    HcclResult ret;
    setenv("HCCL_OP_RETRY_ENABLE", "L0:1, L1:0, L2:0", 1);
    ret = ParseRetryEnable();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclCommunicator impl;
    std::vector<RankInfo> ranks;

    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.0.0.10";
    tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    tmp_para_0.superPodId = "0";

    RankInfo tmp_para_1;

    tmp_para_1.userRank = 1;
    tmp_para_1.devicePhyId = 1;
    tmp_para_1.deviceType = DevType::DEV_TYPE_910;
    tmp_para_1.serverId = "10.0.1.10";
    tmp_para_1.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    tmp_para_1.superPodId = "1";

    ranks.push_back(tmp_para_0);
    ranks.push_back(tmp_para_1);

    impl.serverId_ = "10.0.0.10";

    std::vector<RankInfo_t> rankListNew;
    ret = impl.attrCollector_.TransformRankList(ranks, rankListNew);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = impl.attrCollector_.SetServerNum(rankListNew);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = GetSuperPodNum(rankListNew, impl.superPodNum_);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    impl.deviceNumPerAggregation_ = 1;
    HcclIpAddress serverIp("172.17.10.1");
    HcclIpAddress localIp("172.17.10.1");
    SetRetryEnable(DevType::DEV_TYPE_910_93, impl.superPodNum_, impl.serverNum_, impl.deviceNumPerAggregation_,
        impl.isDiffDeviceType_, impl.GetAivModeConfig(), serverIp, localIp, impl.retryEnable_);
    std::cout << "superPodNum_ is " << unsigned(impl.superPodNum_) << std::endl;
    std::cout << "serverNum_ is " << unsigned(impl.serverNum_) << std::endl;
    std::cout << "deviceNumPerAggregation_ is " << unsigned(impl.deviceNumPerAggregation_) << std::endl;
    if (impl.retryEnable_) {
        std::cout << "retryEnable_ is true" << std::endl;
    } else {
        std::cout << "retryEnable_ is false" << std::endl;
    }
    EXPECT_EQ(impl.retryEnable_, false);
    unsetenv("HCCL_OP_RETRY_ENABLE");
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hccl_Communicator_SetRetryEnable_illegal_config)
{
    HcclResult ret;
    setenv("HCCL_OP_RETRY_ENABLE", " ", 1);
    ret = ParseRetryEnable();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    setenv("HCCL_OP_RETRY_ENABLE", "L0,1", 1);
    ret = ParseRetryEnable();
    EXPECT_EQ(ret, HCCL_E_PARA);

    setenv("HCCL_OP_RETRY_ENABLE", "L3:1", 1);
    ret = ParseRetryEnable();
    EXPECT_EQ(ret, HCCL_E_PARA);

    setenv("HCCL_OP_RETRY_ENABLE", "L2:1, L2:1", 1);
    ret = ParseRetryEnable();
    EXPECT_EQ(ret, HCCL_E_PARA);

    setenv("HCCL_OP_RETRY_ENABLE", "L2:0, L1:0, L0:0", 1);
    ret = ParseRetryEnable();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    unsetenv("HCCL_OP_RETRY_ENABLE");
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_transport_base_stop_test)
{
    HcclResult ret =  HCCL_SUCCESS;
    DispatcherPub *dispatcher;
    const std::unique_ptr<NotifyPool> notifyPool;
    std::chrono::milliseconds timeout;
    MachinePara machinePara;
    hccl::TransportBase transportBase(dispatcher, notifyPool, machinePara, timeout);
    ret = transportBase.Stop();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcclImplTest, ut_transport_base_Resume_test)
{
    HcclResult ret =  HCCL_SUCCESS;
    DispatcherPub *dispatcher;
    const std::unique_ptr<NotifyPool> notifyPool;
    std::chrono::milliseconds timeout;
    MachinePara machinePara;
    hccl::TransportBase transportBase(dispatcher, notifyPool, machinePara, timeout);
    ret = transportBase.Resume();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcclImplTest, ut_migrate_link_to_stop_or_Resume)
{
    HcclResult ret = HCCL_SUCCESS;
    HcclCommunicator hcclCommunicator;

    std::shared_ptr<Transport> link = std::make_shared<Transport>((TransportBase *)nullptr);

    MOCKER_CPP(&Transport::Stop)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&Transport::Resume)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    ret = hcclCommunicator.MigrateLinkToStopOrResume(link, true);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcclCommunicator.MigrateLinkToStopOrResume(link, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_transport_base_fence_test)
{
    HcclResult ret =  HCCL_SUCCESS;
    DispatcherPub *dispatcher;
    const std::unique_ptr<NotifyPool> notifyPool;
    std::chrono::milliseconds timeout;
    MachinePara machinePara;
    std::shared_ptr<Transport> link_base(new Transport(new (std::nothrow) TransportBase(
        dispatcher, notifyPool, machinePara, timeout)));
    ret = link_base->Fence();
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    Stream stream(StreamType::STREAM_TYPE_ONLINE);
    ret = StreamSync(dispatcherPtr, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcclImplTest, ut_migrate_link_vector_to_stop_or_Resume_test)
{
    HcclResult ret = HCCL_SUCCESS;
    HcclCommunicator hcclCommunicator;
    TransportType type = TransportType::TRANS_TYPE_P2P;
    TransportPara para;
    const std::unique_ptr<NotifyPool> notifyPool;
    MachinePara machinePara;
    const TransportDeviceP2pData &transDevP2pData = TransportDeviceP2pData();
    auto ptr = std::make_shared<Transport>(type, para, dispatcherPtr, notifyPool, machinePara, transDevP2pData);
    const std::vector<LINK> links = {ptr};
    MOCKER_CPP(&HcclCommunicator::MigrateLinkToStopOrResume)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    ret = hcclCommunicator.MigrateLinkVectorToStopOrResume(links, true);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hcclCommunicator.MigrateLinkVectorToStopOrResume(links, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_traverse_link_vector_test)
{
    HcclResult ret = HCCL_SUCCESS;
    HcclCommunicator hcclCommunicator;
    std::vector<std::shared_ptr<Transport> > links;
    std::vector<std::unique_ptr<CommBase> > commBaseVector;

    MOCKER_CPP(&HcclCommunicator::MigrateLinkVectorToStopOrResume)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    ret = hcclCommunicator.TraverseLinkVector(commBaseVector, true);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hcclCommunicator.TraverseLinkVector(commBaseVector, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hccl_communicator_stop_test)
{
    HcclResult ret = HCCL_SUCCESS;
    auto notifypool = std::make_unique<NotifyPool>();
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap = {};
    std::vector<RankInfo> paraVector = {};
    std::string str = "test";
    IntraExchanger exchanger;
    DeviceMem inputMem;
    DeviceMem outputMem;
    auto commBase = std::make_unique<CommBase>(str, 0, 0, 0, 0, paraVector, TopoType::TOPO_TYPE_COMMON, nullptr, std::move(notifypool), netDevCtxMap, exchanger,inputMem, outputMem, true);
    HcclCommunicator hcclCommunicator;
    std::string tag = "test";
    CommInfo tmpComm;
    tmpComm.commLevel1.push_back(std::move(commBase));
    tmpComm.commLevel0.push_back(std::move(commBase));
    tmpComm.commLevel2.push_back(std::move(commBase));
    tmpComm.commP2P.push_back(std::move(commBase));
    tmpComm.commIntraServer = std::move(commBase);
    hcclCommunicator.tagCommInfo_.insert(std::pair<std::string, CommInfo>(tag, std::move(tmpComm)));

    MOCKER_CPP(&HcclCommunicator::MigrateLinkVectorToStopOrResume)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::TraverseLinkVector)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    ret = hcclCommunicator.Stop();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hccl_communicator_Resume_test)
{
    HcclResult ret = HCCL_SUCCESS;
    auto notifypool = std::make_unique<NotifyPool>();
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap = {};
    std::vector<RankInfo> paraVector = {};
    std::string str = "test";
    IntraExchanger exchanger;
    DeviceMem inputMem;
    DeviceMem outputMem;
    auto commBase = std::make_unique<CommBase>(str, 0, 0, 0, 0, paraVector, TopoType::TOPO_TYPE_COMMON, nullptr, std::move(notifypool), netDevCtxMap, exchanger, inputMem, outputMem, true);
    HcclCommunicator hcclCommunicator;
    std::string tag = "test";
    CommInfo tmpComm;
    tmpComm.commLevel1.push_back(std::move(commBase));
    tmpComm.commLevel0.push_back(std::move(commBase));
    tmpComm.commLevel2.push_back(std::move(commBase));
    tmpComm.commP2P.push_back(std::move(commBase));
    tmpComm.commIntraServer = std::move(commBase);
    hcclCommunicator.tagCommInfo_.insert(std::pair<std::string, CommInfo>(tag, std::move(tmpComm)));
    MOCKER_CPP(&HcclCommunicator::MigrateLinkVectorToStopOrResume)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::TraverseLinkVector)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtResourceClean)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&OpRetryManager::SetRetryStateToWaitResume)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&OpRetryManager::ExitWaitResumeState)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = hcclCommunicator.Resume();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_regist_tast_abort_handler_test)
{
    hcclComm hcclcomm;
    hcclcomm.communicator_ = std::make_unique<HcclCommunicator>();
    auto ret = hcclcomm.RegistTaskAbortHandler();
    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(HcclImplTest, ut_un_regist_tast_abort_handler_test)
{

    hcclComm hcclcomm;
    hcclcomm.communicator_ = std::make_unique<HcclCommunicator>();
    MOCKER(hrtTaskAbortHandleCallback)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    auto ret = hcclcomm.UnRegistTaskAbortHandler();
    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(HcclImplTest, ut_hccl_comm_suspend_test)
{
    hcclComm hcclcomm;
    hcclcomm.communicator_ = std::make_unique<HcclCommunicator>();
    MOCKER(hrtTaskAbortHandleCallback)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclCommunicator::Suspend, HcclResult(HcclCommunicator:: *)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    auto ret = hcclcomm.Suspend();
    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(HcclImplTest, ut_TraverseSingleSubCommTransport_first_test)
{
    TransportType type = TransportType::TRANS_TYPE_P2P;
    TransportPara para;
    const std::unique_ptr<NotifyPool> notifyPool;
    MachinePara machinePara;
    const TransportDeviceP2pData &transDevP2pData = TransportDeviceP2pData();
    HcclCommunicator hcclCommunicator;
    SingleSubCommTransport singleSubCommTransport;
    TransportRequest req;
    singleSubCommTransport.transportRequests.push_back(req);
    singleSubCommTransport.transportRequests[0].isValid = false;
    auto ptr = std::make_shared<Transport>(type, para, dispatcherPtr, notifyPool, machinePara, transDevP2pData);
    singleSubCommTransport.links.push_back(ptr);
    
    MOCKER_CPP(&Transport::Stop)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&Transport::Resume)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    auto ret = hcclCommunicator.TraverseSingleSubCommTransport(singleSubCommTransport, true);
    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(HcclImplTest, ut_TraverseSingleSubCommTransport_second_test)
{
    TransportType type = TransportType::TRANS_TYPE_P2P;
    TransportPara para;
    const std::unique_ptr<NotifyPool> notifyPool;
    MachinePara machinePara;
    const TransportDeviceP2pData &transDevP2pData = TransportDeviceP2pData();
    HcclCommunicator hcclCommunicator;
    SingleSubCommTransport singleSubCommTransport;
    TransportRequest req;
    singleSubCommTransport.transportRequests.push_back(req);
    singleSubCommTransport.transportRequests[0].isValid = false;
    auto ptr = std::make_shared<Transport>(type, para, dispatcherPtr, notifyPool, machinePara, transDevP2pData);
    singleSubCommTransport.links.push_back(ptr);
    
    MOCKER_CPP(&Transport::Stop)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&Transport::Resume)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    singleSubCommTransport.transportRequests[0].isValid = true;
    auto ret = hcclCommunicator.TraverseSingleSubCommTransport(singleSubCommTransport, true);
    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(HcclImplTest, ut_TraverseSingleSubCommTransport_third_test)
{
    TransportType type = TransportType::TRANS_TYPE_P2P;
    TransportPara para;
    const std::unique_ptr<NotifyPool> notifyPool;
    MachinePara machinePara;
    const TransportDeviceP2pData &transDevP2pData = TransportDeviceP2pData();
    HcclCommunicator hcclCommunicator;
    SingleSubCommTransport singleSubCommTransport;
    TransportRequest req;
    singleSubCommTransport.transportRequests.push_back(req);
    singleSubCommTransport.transportRequests[0].isValid = false;
    auto ptr = std::make_shared<Transport>(type, para, dispatcherPtr, notifyPool, machinePara, transDevP2pData);
    singleSubCommTransport.links.push_back(ptr);
    
    MOCKER_CPP(&Transport::Stop)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&Transport::Resume)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    singleSubCommTransport.transportRequests[0].isValid = true;
    auto ret = hcclCommunicator.TraverseSingleSubCommTransport(singleSubCommTransport, false);
    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(HcclImplTest, ut_TraverseLevelNSubCommTransport_test)
{
    LevelNSubCommTransport lev;
    lev.resize(1);  
    MOCKER_CPP(&HcclCommunicator::TraverseSingleSubCommTransport)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    HcclCommunicator hcclCommunicator;

    auto ret = hcclCommunicator.TraverseLevelNSubCommTransport(lev, true);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    ret = hcclCommunicator.TraverseLevelNSubCommTransport(lev, false);
    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(HcclImplTest, ut_TraverseOpCommTransport_test)
{
    OpCommTransport op;
    op.resize(1);
    HcclCommunicator hcclCommunicator;

    MOCKER_CPP(&HcclCommunicator::TraverseLevelNSubCommTransport)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    auto ret = hcclCommunicator.TraverseOpCommTransport(op, true);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    ret = hcclCommunicator.TraverseOpCommTransport(op, false);
    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(HcclImplTest, ut_TraverseAlgResourceResponse_test)
{
    HcclCommunicator hcclCommunicator;
    AlgResourceResponse alg;
    hcclCommunicator.resMap_.insert(std::pair<std::string, AlgResourceResponse>(std::make_pair("test", alg)));
    MOCKER_CPP(&HcclCommunicator::TraverseOpCommTransport)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    auto ret = hcclCommunicator.TraverseAlgResourceResponse(true);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    ret = hcclCommunicator.TraverseAlgResourceResponse(false);
    EXPECT_EQ(HCCL_SUCCESS, ret);
}


HcclResult GetCmd2(hccl::HDCommunicate*This,unsigned int offset, unsigned int length, unsigned char* value){
    KfcExecStatus* op = reinterpret_cast<KfcExecStatus*> (value);
    op->execStatus.kfcStatus = KfcStatus::kRuning;
    return HCCL_SUCCESS;
}

HcclResult GetCmd3(hccl::HDCommunicate*This,unsigned int offset, unsigned int length, unsigned char* value){
    KfcExecStatus* op = reinterpret_cast<KfcExecStatus*> (value);
    op->execStatus.kfcStatus = KfcStatus::kStoplaunch;
    return HCCL_SUCCESS;
}

HcclResult GetCmd4(hccl::HDCommunicate*This,unsigned int offset, unsigned int length, unsigned char* value){
    KfcExecStatus* op = reinterpret_cast<KfcExecStatus*> (value);
    op->execStatus.kfcStatus = KfcStatus::kError;
    return HCCL_SUCCESS;
}



HcclResult GetCmd1(hccl::HDCommunicate*This,unsigned int offset, unsigned int length, unsigned char* value){
    KfcExecStatus* op = reinterpret_cast<KfcExecStatus*> (value);
    op->execStatus.kfcStatus = KfcStatus::kEnd;
    return HCCL_SUCCESS;
}


TEST_F(HcclImplTest,ut_stopExec_not_in){
    
    HcclCommunicator hcclCommunicator;
    auto ret = hcclCommunicator.StopExec();
    EXPECT_EQ(HCCL_SUCCESS, ret);
    GlobalMockObject::verify();
    
    hcclCommunicator.SetAicpuCommEngine(true);
    hcclCommunicator.SetMC2EnvFlag();
    MOCKER_CPP(&HDCommunicate::Get)
    .stubs()
    .will(invoke(GetCmd1));
    ret = hcclCommunicator.StopExec();
    EXPECT_EQ(HCCL_SUCCESS, ret);
    hcclCommunicator.isAicpuCommEngine_ = false; //保证析构的时候不会调用到相关命令
    hcclCommunicator.isNsRecovery_ = false;
    GlobalMockObject::verify();

}

TEST_F(HcclImplTest,ut_Clean_not_in){
    
    HcclCommunicator hcclCommunicator;
    auto ret = hcclCommunicator.Clean();
    EXPECT_EQ(HCCL_SUCCESS, ret);
    GlobalMockObject::verify();

    hcclCommunicator.SetAicpuCommEngine(true);
    hcclCommunicator.SetMC2EnvFlag();
    MOCKER_CPP(&HDCommunicate::Get)
    .stubs()
    .will(invoke(GetCmd2));
    ret = hcclCommunicator.Clean();
    EXPECT_EQ(HCCL_SUCCESS, ret);
    hcclCommunicator.isAicpuCommEngine_ = false; //保证析构的时候不会调用到相关命令
    hcclCommunicator.isNsRecovery_ = false;
    GlobalMockObject::verify();

}

// 自适应算法选择，pipeline编排算法context数量超过ffts子图规格
TEST_F(HcclImplTest, ut_hcclimpl_AutoSelectAlgTypeLevel1_OutOfFfts)
{
    u64 curCount = 8UL * 1024UL * 1024UL * 1024UL; // 单位：字节(B)
    HcclDataType dataType = HcclDataType::HCCL_DATA_TYPE_UINT64;
    u32 unitSize = SIZE_TABLE[dataType];
    u64 curSize = curCount*unitSize;
    std::string algTypeLevel1Tag;
    HcclResult ret;

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;

    std::unique_ptr<CollAlgOperator> operation(new (std::nothrow)
        CollAlgOperator(algConfigurator.get(),  cclBufferManager, dispatcher, topoMatcher, HcclCMDType::HCCL_CMD_REDUCE_SCATTER));

    operation->deviceNumPerAggregation_ = 8;
    operation->userRankSize_ = operation->moduleNum_ * operation->deviceNumPerAggregation_;
    operation->deviceType_ = DevType::DEV_TYPE_910B;
    operation->isAlgoLevel1Default_ = true;
    operation->algType_.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_NP_MESH;
    operation->algType_.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;

    u32 contextNum;
    {   // ReduceScatter算子，>=1524机之后超过FFTS子图规格
        operation->moduleNum_ = 1523;
        contextNum = operation->CalcContextNumForPipeline(HcclCMDType::HCCL_CMD_REDUCE_SCATTER);
        ret = operation->AutoSelectAlgTypeLevel1(HcclCMDType::HCCL_CMD_REDUCE_SCATTER, curSize, curSize, algTypeLevel1Tag, true, true, false);
        EXPECT_EQ(contextNum > HCCL_FFTS_CAPACITY, false);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        EXPECT_EQ(algTypeLevel1Tag, "ALG_LEVEL1_PIPELINE");

        operation->moduleNum_ = 1524;
        u32 contextNum = operation->CalcContextNumForPipeline(HcclCMDType::HCCL_CMD_REDUCE_SCATTER);
        ret = operation->AutoSelectAlgTypeLevel1(HcclCMDType::HCCL_CMD_REDUCE_SCATTER, curSize, curSize, algTypeLevel1Tag, true, true, false);
        EXPECT_EQ(contextNum > HCCL_FFTS_CAPACITY, true);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        EXPECT_EQ(algTypeLevel1Tag, "ALG_LEVEL1_NHR");
    }

    {   // AllGather算子，>=1524机之后超过FFTS子图规格
        operation->moduleNum_ = 1523;
        contextNum = operation->CalcContextNumForPipeline(HcclCMDType::HCCL_CMD_ALLGATHER);
        ret = operation->AutoSelectAlgTypeLevel1(HcclCMDType::HCCL_CMD_ALLGATHER, curSize, curSize, algTypeLevel1Tag, true, true, false);
        EXPECT_EQ(contextNum > HCCL_FFTS_CAPACITY, false);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        EXPECT_EQ(algTypeLevel1Tag, "ALG_LEVEL1_PIPELINE");

        operation->moduleNum_ = 1524;
        contextNum = operation->CalcContextNumForPipeline(HcclCMDType::HCCL_CMD_ALLGATHER);
        ret = operation->AutoSelectAlgTypeLevel1(HcclCMDType::HCCL_CMD_ALLGATHER, curSize, curSize, algTypeLevel1Tag, true, true, false);
        EXPECT_EQ(contextNum > HCCL_FFTS_CAPACITY, true);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        EXPECT_EQ(algTypeLevel1Tag, "ALG_LEVEL1_NHR");
    }

    {   // AllReduce算子，>=762机之后超过FFTS子图规格
        operation->moduleNum_ = 761;
        contextNum = operation->CalcContextNumForPipeline(HcclCMDType::HCCL_CMD_ALLREDUCE);
        ret = operation->AutoSelectAlgTypeLevel1(HcclCMDType::HCCL_CMD_ALLREDUCE, curSize, curSize, algTypeLevel1Tag, true, true, false);
        EXPECT_EQ(contextNum > HCCL_FFTS_CAPACITY, false);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        EXPECT_EQ(algTypeLevel1Tag, "ALG_LEVEL1_PIPELINE");

        operation->moduleNum_ = 762;
        contextNum = operation->CalcContextNumForPipeline(HcclCMDType::HCCL_CMD_ALLREDUCE);
        ret = operation->AutoSelectAlgTypeLevel1(HcclCMDType::HCCL_CMD_ALLREDUCE, curSize, curSize, algTypeLevel1Tag, true, true, false);
        EXPECT_EQ(contextNum > HCCL_FFTS_CAPACITY, true);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        EXPECT_EQ(algTypeLevel1Tag, "ALG_LEVEL1_NHR");
    }

    {   // Alltoall算子，>=1490机之后超过FFTS子图规格
        operation->moduleNum_ = 1489;
        contextNum = operation->CalcContextNumForPipeline(HcclCMDType::HCCL_CMD_ALLTOALL);
        EXPECT_EQ(contextNum > HCCL_FFTS_CAPACITY, false);

        operation->moduleNum_ = 1490;
        contextNum = operation->CalcContextNumForPipeline(HcclCMDType::HCCL_CMD_ALLTOALL);
        EXPECT_EQ(contextNum > HCCL_FFTS_CAPACITY, true);
    }

    implBase = nullptr;
    GlobalMockObject::verify();
}

static void TestConstructParam_1server_8p(HcclCommParams &params, RankTable_t &rankTable)
{
    string commId = "comm ";
    memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
    params.rank = 0;
    params.totalRanks = 8;
    params.isHeterogComm = false;
    params.logicDevId = 0;
    params.commWorkMode = WorkMode::HCCL_MODE_NORMAL;
    params.deviceType = DevType::DEV_TYPE_910;

    rankTable.collectiveId = "192.168.0.101-8000-8001";
    vector<RankInfo_t> rankVec(8);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";

    rankVec[1].rankId = 1;
    rankVec[1].deviceInfo.devicePhyId = 1;
    HcclIpAddress ipAddr2(1694542017);
    rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2); // 101.0.168.192
    rankVec[1].serverIdx = 0;
    rankVec[1].serverId = "192.168.0.101";

    rankVec[2].rankId = 2;
    rankVec[2].deviceInfo.devicePhyId = 2;
    HcclIpAddress ipAddr3(1711319232);
    rankVec[2].deviceInfo.deviceIp.push_back(ipAddr3); // 101.0.168.192
    rankVec[2].serverIdx = 0;
    rankVec[2].serverId = "192.168.0.101";

    rankVec[3].rankId = 3;
    rankVec[3].deviceInfo.devicePhyId = 3;
    HcclIpAddress ipAddr4(1711319233);
    rankVec[3].deviceInfo.deviceIp.push_back(ipAddr4); // 101.0.168.192
    rankVec[3].serverIdx = 0;
    rankVec[3].serverId = "192.168.0.101";

    rankVec[4].rankId = 4;
    rankVec[4].deviceInfo.devicePhyId = 4;
    HcclIpAddress ipAddr5(1711319234);
    rankVec[4].deviceInfo.deviceIp.push_back(ipAddr5); // 101.0.168.192
    rankVec[4].serverIdx = 0;
    rankVec[4].serverId = "192.168.0.101";

    rankVec[5].rankId = 5;
    rankVec[5].deviceInfo.devicePhyId = 5;
    HcclIpAddress ipAddr6(1711319235);
    rankVec[5].deviceInfo.deviceIp.push_back(ipAddr6); // 101.0.168.192
    rankVec[5].serverIdx = 0;
    rankVec[5].serverId = "192.168.0.101";

    rankVec[6].rankId = 6;
    rankVec[6].deviceInfo.devicePhyId = 6;
    HcclIpAddress ipAddr7(1711319236);
    rankVec[6].deviceInfo.deviceIp.push_back(ipAddr7); // 101.0.168.192
    rankVec[6].serverIdx = 0;
    rankVec[6].serverId = "192.168.0.101";

    rankVec[7].rankId = 7;
    rankVec[7].deviceInfo.devicePhyId = 7;
    HcclIpAddress ipAddr8(1711319237);
    rankVec[7].deviceInfo.deviceIp.push_back(ipAddr8); // 101.0.168.192
    rankVec[7].serverIdx = 0;
    rankVec[7].serverId = "192.168.0.101";

    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.rankNum = 8;
    rankTable.deviceNum = 8;
    rankTable.serverNum = 1;
}

static void TestConstructParam_1server_8p_910_93(HcclCommParams &params, RankTable_t &rankTable)
{
    string commId = "comm ";
    memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
    params.rank = 0;
    params.totalRanks = 8;
    params.isHeterogComm = false;
    params.logicDevId = 0;
    params.commWorkMode = WorkMode::HCCL_MODE_NORMAL;
    params.deviceType = DevType::DEV_TYPE_910_93;

    rankTable.collectiveId = "192.168.0.101-8000-8001";
    vector<RankInfo_t> rankVec(8);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";

    rankVec[1].rankId = 1;
    rankVec[1].deviceInfo.devicePhyId = 1;
    HcclIpAddress ipAddr2(1694542017);
    rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2); // 101.0.168.192
    rankVec[1].serverIdx = 0;
    rankVec[1].serverId = "192.168.0.101";

    rankVec[2].rankId = 2;
    rankVec[2].deviceInfo.devicePhyId = 2;
    HcclIpAddress ipAddr3(1711319232);
    rankVec[2].deviceInfo.deviceIp.push_back(ipAddr3); // 101.0.168.192
    rankVec[2].serverIdx = 0;
    rankVec[2].serverId = "192.168.0.101";

    rankVec[3].rankId = 3;
    rankVec[3].deviceInfo.devicePhyId = 3;
    HcclIpAddress ipAddr4(1711319233);
    rankVec[3].deviceInfo.deviceIp.push_back(ipAddr4); // 101.0.168.192
    rankVec[3].serverIdx = 0;
    rankVec[3].serverId = "192.168.0.101";

    rankVec[4].rankId = 4;
    rankVec[4].deviceInfo.devicePhyId = 4;
    HcclIpAddress ipAddr5(1711319234);
    rankVec[4].deviceInfo.deviceIp.push_back(ipAddr5); // 101.0.168.192
    rankVec[4].serverIdx = 0;
    rankVec[4].serverId = "192.168.0.101";

    rankVec[5].rankId = 5;
    rankVec[5].deviceInfo.devicePhyId = 5;
    HcclIpAddress ipAddr6(1711319235);
    rankVec[5].deviceInfo.deviceIp.push_back(ipAddr6); // 101.0.168.192
    rankVec[5].serverIdx = 0;
    rankVec[5].serverId = "192.168.0.101";

    rankVec[6].rankId = 6;
    rankVec[6].deviceInfo.devicePhyId = 6;
    HcclIpAddress ipAddr7(1711319236);
    rankVec[6].deviceInfo.deviceIp.push_back(ipAddr7); // 101.0.168.192
    rankVec[6].serverIdx = 0;
    rankVec[6].serverId = "192.168.0.101";

    rankVec[7].rankId = 7;
    rankVec[7].deviceInfo.devicePhyId = 7;
    HcclIpAddress ipAddr8(1711319237);
    rankVec[7].deviceInfo.deviceIp.push_back(ipAddr8); // 101.0.168.192
    rankVec[7].serverIdx = 0;
    rankVec[7].serverId = "192.168.0.101";

    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.rankNum = 8;
    rankTable.deviceNum = 8;
    rankTable.serverNum = 1;
}

TEST_F(HcclImplTest, ut_hcclImpl_run_ring_executer_by_all_reduce_stars)
{
    HcclResult ret = HCCL_SUCCESS;
    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));
    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&StreamActiveManager::StreamActive)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(GetExternalInputHcclEnableFfts)
    .stubs()
    .will(returnValue(false));
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam_1server_8p(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    ret = implBase->AtomicInitSet();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->InitCCLbuffer(4096, 4096);
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    impl->topoAttr_.deviceLogicId = 0;
    impl->topoAttr_.devicePhyId = 0;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLREDUCE].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLREDUCE].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;

    impl->topoType_ = TopoType::TOPO_TYPE_8P_RING;
    algConfigurator->topoType_ = TopoType::TOPO_TYPE_8P_RING;
    CollAllReduceRingExecutor* executor = new CollAllReduceRingExecutor(impl->dispatcher_, topoMatcher);
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
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
    const std::vector<std::vector<u32>> tmpRingNics = {
        { 0, 1, 2, 3, 4, 5, 6, 7 },
        { 0, 1, 2, 3, 4, 5, 6, 7 }
    };

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(opParam.tag, HcclCMDType::HCCL_CMD_ALLREDUCE, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    resourceResponse.slaveStreams.resize(3);
    resourceResponse.notifiesMain.resize(3);
    resourceResponse.notifiesAux.resize(3);
    resourceResponse.notifiesDevMain.resize(3);
    resourceResponse.notifiesDevAux.resize(3);

    resourceResponse.threadManage.resize(3);
    for (u32 ringIndex = 0; ringIndex < 3; ringIndex ++) {
        resourceResponse.threadManage[ringIndex].reset(new (std::nothrow) ThreadManage(ringIndex, ringIndex, impl->dispatcher_));
    }
    ret = executor->Orchestrate(opParam, resourceResponse);
    delete executor;

    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclImpl_run_ring_executer_by_all_gather_stars)
{
    HcclResult ret = HCCL_SUCCESS;
    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));
    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&StreamActiveManager::StreamActive)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(GetExternalInputHcclEnableFfts)
    .stubs()
    .will(returnValue(false));
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam_1server_8p(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    ret = implBase->AtomicInitSet();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->InitCCLbuffer(4096, 4096);
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    impl->topoAttr_.deviceLogicId = 0;
    impl->topoAttr_.devicePhyId = 0;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLREDUCE].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLREDUCE].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    impl->topoType_ = TopoType::TOPO_TYPE_8P_RING;
    algConfigurator->topoType_ = TopoType::TOPO_TYPE_8P_RING;
    CollAllGatherRingExecutor* executor = new CollAllGatherRingExecutor(impl->dispatcher_, topoMatcher);
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 4096;
    opParam.DataDes.count = 1024;
    opParam.DataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);
    const std::vector<std::vector<u32>> tmpRingNics = {
        { 0, 1, 2, 3, 4, 5, 6, 7 },
        { 0, 1, 2, 3, 4, 5, 6, 7 }
    };
    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(opParam.tag, HcclCMDType::HCCL_CMD_ALLGATHER, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    resourceResponse.slaveStreams.resize(3);
    resourceResponse.notifiesMain.resize(3);
    resourceResponse.notifiesAux.resize(3);
    resourceResponse.notifiesDevMain.resize(3);
    resourceResponse.notifiesDevAux.resize(3);

    resourceResponse.threadManage.resize(3);
    for (u32 ringIndex = 0; ringIndex < 3; ringIndex ++) {
        resourceResponse.threadManage[ringIndex].reset(new (std::nothrow) ThreadManage(ringIndex, ringIndex, impl->dispatcher_));
    }
    ret = executor->Orchestrate(opParam, resourceResponse);
    delete executor;

    GlobalMockObject::verify();
}
#if 1
TEST_F(HcclImplTest, ut_hcclImpl_run_double_ring_executer_by_all_reduce_stars_2)
{
    HcclResult ret = HCCL_SUCCESS;
    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));
    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&StreamActiveManager::StreamActive)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(GetExternalInputHcclEnableFfts)
    .stubs()
    .will(returnValue(false));

    MOCKER_CPP(&ThreadManage::WaitDone)
    .stubs()
    .with(any());

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam_1server_8p_910_93(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910_93;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    ret = implBase->AtomicInitSet();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->InitCCLbuffer(4096, 4096);
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    impl->topoAttr_.deviceLogicId = 0;
    impl->topoAttr_.devicePhyId = 0;
    algConfigurator->topoType_ = TopoType::TOPO_TYPE_NP_DOUBLE_RING;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLREDUCE].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_NP_DOUBLE_RING;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLREDUCE].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    impl->topoType_ = TopoType::TOPO_TYPE_NP_DOUBLE_RING;
    algConfigurator->topoType_ = TopoType::TOPO_TYPE_NP_DOUBLE_RING;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_NP_DOUBLE_RING;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_910_93;

    CollAllReduceRingFor91093Executor* executor = new CollAllReduceRingFor91093Executor(impl->dispatcher_, topoMatcher);
    executor->algType_.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_NP_DOUBLE_RING;
    executor->algType_.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
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
    const std::vector<std::vector<u32>> tmpRingNics = {
        { 0, 1, 2, 3, 4, 5, 6, 7 },
        { 0, 1, 2, 3, 4, 5, 6, 7 }
    };

    std::printf("[st_CollAllReduceRingFor91093Executor_Ring]");

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(opParam.tag, HcclCMDType::HCCL_CMD_ALLREDUCE, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    resourceResponse.slaveStreams.resize(3);
    resourceResponse.notifiesMain.resize(3);
    resourceResponse.notifiesAux.resize(3);
    resourceResponse.notifiesDevMain.resize(3);
    resourceResponse.notifiesDevAux.resize(3);
    resourceResponse.opTransportResponse.resize(COMM_LEVEL_RESERVED);
    resourceResponse.opTransportResponse[COMM_LEVEL0].resize(2);
    resourceResponse.opTransportResponse[COMM_LEVEL0][0].links.resize(8);
    resourceResponse.opTransportResponse[COMM_LEVEL0][1].links.resize(8);

    //暂时注释  resourceResponse.threadManage.resize(3);
    //暂时注释  for (u32 ringIndex = 0; ringIndex < 3; ringIndex ++) {
    //暂时注释      resourceResponse.threadManage[ringIndex].reset(new (std::nothrow) ThreadManage(ringIndex, ringIndex, impl->dispatcher_));
    //暂时注释  }
    //暂时注释 ret = executor->Orchestrate(opParam, resourceResponse);
    delete executor;
    GlobalMockObject::verify();
}
#endif

TEST_F(HcclImplTest, ut_hcclImpl_all_gather_asym)
{
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;
    implBase->implAlg_->algConfigurator_->topoType_ = TopoType::TOPO_TYPE_NP_DOUBLE_RING;
    implBase->implAlg_->topoAttr_.deviceType = DevType::DEV_TYPE_910_93;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    Stream stream(StreamType::STREAM_TYPE_ONLINE);
    HcomCollOpInfo opInfo;
    opInfo.inputAddr = inputMem.ptr();
    opInfo.outputAddr = outputMem.ptr();
    opInfo.count = count;
    opInfo.dataType = dataType;
    opInfo.reduceOp = op;

    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CCLBufferManager &cclBufferManager = implBase->implAlg_->cclBufferManager_;
    const HcclDispatcher dispatcher = implBase->implAlg_->dispatcher_;
    topoMatcher->topoInfo_.topoType = TopoType::TOPO_TYPE_NP_DOUBLE_RING;
    topoMatcher->topoInfo_.deviceType = DevType::DEV_TYPE_910_93;
    algConfigurator->topoAttr_.multiModuleDiffDeviceNumMode = true;

    std::unique_ptr<AllGatherOperator> operation(new (std::nothrow) AllGatherOperator(algConfigurator.get(), cclBufferManager, dispatcher, topoMatcher));
    operation->topoType_ = TopoType::TOPO_TYPE_NP_DOUBLE_RING;

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

    std::string algName = "";
    std::string newTag = "";
    ret = operation->SelectAlg(tag, opParam, algName, newTag);
    EXPECT_TRUE(algName == "AllGatherComm");
    AlgResourceRequest resourceRequest;
    ret = operation->CalcResRequest(algName, opParam, resourceRequest);

    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, invalid_valid__reserve_release_IpcMemory)
{
    MOCKER_CPP(&HcclCommunicator::SetMemoryRange).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::ActivateCommMemory).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::DeactivateCommMemory).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::UnsetMemoryRange).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::InitZeroCopyMemoryAgent).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::DeinitZeroCopyMemoryAgent).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
 
    hcclComm hcclcomm;
    hcclcomm.communicator_ = std::make_unique<HcclCommunicator>();
    char dummyReserveMem[10];
    void *vir_ptr = dummyReserveMem;
    auto devID = 0;
    auto reserve_mem = 10;
    EXPECT_EQ(
        HCCL_SUCCESS, HcclCommSetMemoryRange(reinterpret_cast<HcclComm>(&hcclcomm), vir_ptr, reserve_mem, 0, 0));
    int dummyHandle = 0;
    aclrtDrvMemHandle mem_handle = &dummyHandle;
    EXPECT_EQ(HCCL_SUCCESS, HcclCommActivateCommMemory(reinterpret_cast<HcclComm>(&hcclcomm), vir_ptr, reserve_mem, 0, mem_handle, 0));
    EXPECT_EQ(HCCL_SUCCESS, HcclCommDeactivateCommMemory(reinterpret_cast<HcclComm>(&hcclcomm), vir_ptr));
    EXPECT_EQ(HCCL_SUCCESS, HcclCommUnsetMemoryRange(reinterpret_cast<HcclComm>(&hcclcomm), vir_ptr));
}

TEST_F(HcclImplTest, ut_clean_ccl_buffer)
{
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcclResult ret;
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = implBase->CreateCommCCLbuffer();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = implBase->cclBufferManager_.CleanCCLbuffer();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcclImplTest,ut_GetAicpuTaskException)
{
    MOCKER_CPP(&HDCommunicate::Get)
    .stubs()
    .will(returnValue(HCCL_E_PARA));
    HcclCommunicator hcclCommunicator;
    hcclCommunicator.kfcStatusTransferD2H_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, 20));

    hcclCommunicator.GetAicpuTaskException();
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest,ut_AivResume)
{
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    HcclResult ret;
    std::unique_ptr<HcclCommunicator> hcclCommunicator(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = hcclCommunicator->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hcclCommunicator->cclBufferManager_.CreateCommAIVbuffer(false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER(GetExternalInputHcclAivMode)
    .stubs()
    .will(returnValue(true));

    hcclCommunicator->AivResume();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcclCommunicator->cclBufferManager_.ReleaseCommAIVbuffer();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_AiCpuSetCommResource_AIVRoce)
{
    u32 ifnumVersion = 3;
    MOCKER(hrtRaGetInterfaceVersion)
    .stubs()
    .with(any(), any(), outBoundP(&ifnumVersion))
    .will(returnValue(HCCL_SUCCESS));
 
    void *commInputPtr = nullptr;
    u64 commInputSize;
    void *commOutputPtr = nullptr;
    u64 commOutputSize = 0;
    HcclResult ret;
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    std::string tag = "test";
    CommInfo tmpComm;
    std::vector<RankInfo> paraVector;
    u32 rankSize = 8;
    u32 curRankId = 0;
    u64 commBufferSize = 20;
 
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());
 
    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hcclImpl *impl = implBase->implAlg_->pimpl_.get();
 
    ret = implBase->GetInCCLbuffer(commInputPtr, commInputSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = implBase->GetOutCCLbuffer(commOutputPtr, commOutputSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem expMem = implBase->cclBufferManager_.GetCommExpBuffer();
 
    implBase->notifyPool_.reset(new (std::nothrow) NotifyPool());
    ret = implBase->notifyPool_->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    IntraExchanger exchanger{};
    tmpComm.commLevel0.resize(4);
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    for (int i = 0; i < 4; i++) {
        tmpComm.commLevel0[i].reset(new (std::nothrow) CommMesh(tag, 0, 4, curRankId, rankSize,
            TopoType::TOPO_TYPE_8P_MESH, implBase->dispatcher_, implBase->notifyPool_, netDevCtxMap, exchanger,
            paraVector, inputMem, outputMem, true, nullptr, 0, ""));
    } 
    EXPECT_EQ(tmpComm.commLevel0[0]->transportInfo_.size(), rankSize);
    tmpComm.commLevel0[0]->transportInfo_.clear();
    std::chrono::milliseconds kdefaultTimeout = std::chrono::seconds(120);
    /*创建link*/
    MachinePara machine_para;
 
    machine_para.localDeviceId = 0;
    machine_para.deviceLogicId = 0;
    machine_para.nicDeploy == NICDeployment::NIC_DEPLOYMENT_DEVICE;
    machine_para.localIpAddr = HcclIpAddress("192.168.0.23");
    std::shared_ptr<Transport> link = nullptr;
    TransportPara para = {};
 
    link.reset(new Transport(TransportType::TRANS_TYPE_IBV_EXP, para, dispatcher, implBase->notifyPool_, machine_para));
    link->Init();
    ret = implBase->notifyPool_->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::shared_ptr<LocalIpcNotify> localNotify = nullptr;
    std::shared_ptr<RemoteNotify> remoteNotify = nullptr;
 
    RemoteRankInfo info(0, -1);
    SalGetBareTgid(reinterpret_cast<u32*>(&info.remotePid));
    ret = implBase->notifyPool_->Alloc(tag, info, localNotify, NotifyLoadType::DEVICE_NOTIFY);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    std::vector<u8> data(NOTIFY_INFO_LENGTH, 0);
    ret = localNotify->Serialize(data);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remoteNotify.reset(new (std::nothrow) RemoteNotify());
 
    ret = remoteNotify->Init(data);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = remoteNotify->Open();
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    link->pimpl_->remoteSendDoneDeviceNotify_ = remoteNotify;
    link->pimpl_->localSendDoneDeviceNotify_ = localNotify;
    link->pimpl_->remoteSendReadyDeviceNotify_ = remoteNotify;
    link->pimpl_->localSendReadyDeviceNotify_ = localNotify;
    for (u32 ringIndex = 0; ringIndex < rankSize; ringIndex++) {
        tmpComm.commLevel0[0]->transportInfo_.push_back(link);
        tmpComm.commLevel0[0]->transportType_.push_back(TransportType::TRANS_TYPE_IBV_EXP);
    }
    implBase->tagCommInfo_.insert(std::pair<std::string, CommInfo>(tag, std::move(tmpComm)));
 
    Level1StreamInfo tmpInnerStreamInfo;
    tmpInnerStreamInfo.ringNum = rankSize;
    tmpInnerStreamInfo.ringDeviceSignal.resize(rankSize - 1);
    tmpInnerStreamInfo.ringDeviceSignalAux.resize(rankSize - 1);
    tmpInnerStreamInfo.ringDeviceStreams.resize(rankSize);
 
    for (u32 ringIndex = 0; ringIndex < tmpInnerStreamInfo.ringNum; ringIndex++) {
        tmpInnerStreamInfo.ringDeviceStreams[ringIndex] = Stream(StreamType::STREAM_TYPE_DEVICE);
        if (ringIndex != tmpInnerStreamInfo.ringNum - 1) {
            tmpInnerStreamInfo.ringDeviceSignal[ringIndex] = localNotify;
            tmpInnerStreamInfo.ringDeviceSignalAux[ringIndex] = localNotify;
        }
    }
 
    MOCKER_CPP(&Transport::GetRemoteMem, HcclResult(Transport::*)(hccl::UserMemType, void**))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
 
    MOCKER_CPP(&Transport::GetRemoteMem, HcclResult(Transport::*)(std::vector<void*>*))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
  
    MOCKER_CPP(&Transport::GetRemoteMemKey, HcclResult(Transport::*)(hccl::UserMemType, uint32_t *))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&Transport::GetLocalMemDetails)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    std::vector<HcclQpInfoV2> qpInfos(1);
    MOCKER_CPP(&Transport::GetAiQpInfo).stubs().with(outBound(qpInfos)).will(returnValue(HCCL_SUCCESS));
 
 
    MOCKER_CPP(&Transport::GetChipId)
    .stubs()  
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    // 配置profiling开关
    auto &profilingManager = hccl::ProfilingManager::Instance();
    profilingManager.StartAddtionInfoSubscribe();
 
    // MachinePara machinePara;
    machine_para.localDeviceId = 0;
    std::chrono::milliseconds timeout;
 
    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(
        new (std::nothrow) HcclSocket("test", nullptr, remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machine_para.sockets.push_back(newSocket);
 
    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    TransportBase transportBase(dispatcher, notifyPool, machine_para, timeout);
    MOCKER_CPP_VIRTUAL(transportBase, &TransportBase::GetRemoteMemSize)
        .stubs()
        .with(any(), outBound(commBufferSize))
        .will(returnValue(HCCL_SUCCESS));
 
    rtStream_t aiCpuStream;
    Stream stream(aiCpuStream);
    implBase->isA2MC2MultiServer_ = true;
    implBase->tagStreamInfo_.insert(std::pair<std::string, Level1StreamInfo>(tag, std::move(tmpInnerStreamInfo)));
    ret = implBase->GenAiRMAInfo(implBase->tagCommInfo_[tag].commLevel0[0].get());
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    MOCKER_CPP(&HcclCommunicator::GetQosCfg)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER(hrtMemAsyncCopyByQos)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    
    ret = implBase->H2DAiRMAInfo(tag, aiCpuStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclCombinOpParam *combinOparaPtr = reinterpret_cast<HcclCombinOpParam *>(implBase->combinOparaMem_->ptr());
    EXPECT_NE(combinOparaPtr->aiRMAInfo, nullptr);
 
    u64 profConfigL0 = 0x84000985;
    profilingManager.StopSubscribe(profConfigL0);
 
    HCCL_INFO("check if HcclCombinOpParam is match with aicpu struct HccCommResParamTask");
 
 
    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclImpl_run_a3_aiv_coressnode_opbase)
{
    HcclResult ret = HCCL_SUCCESS;
    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));
    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&StreamActiveManager::StreamActive)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(GetExternalInputHcclEnableFfts)
    .stubs()
    .will(returnValue(false));
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam_1server_8p(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    ret = implBase->AtomicInitSet();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->InitCCLbuffer(4096, 4096);
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    impl->topoAttr_.deviceLogicId = 0;
    impl->topoAttr_.devicePhyId = 0;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLGATHER].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLGATHER].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;

    impl->topoType_ = TopoType::TOPO_TYPE_8P_RING;
    algConfigurator->topoType_ = TopoType::TOPO_TYPE_8P_RING;
    CollAllGatherMeshAivFor91093Executor* executor = new CollAllGatherMeshAivFor91093Executor(impl->dispatcher_, topoMatcher);
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
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
    const std::vector<std::vector<u32>> tmpRingNics = {
        { 0, 1, 2, 3, 4, 5, 6, 7 },
        { 0, 1, 2, 3, 4, 5, 6, 7 }
    };
    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    resourceRequest.aivBufferRequest = 3U;
    implBase->AllocAlgResource(opParam.tag, HcclCMDType::HCCL_CMD_ALLGATHER, opParam, resourceRequest, resourceResponse);
    delete executor;

    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclImpl_run_a3_aiv_coressnode_offload)
{
    HcclResult ret = HCCL_SUCCESS;
    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB));
    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&StreamActiveManager::StreamActive)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(GetExternalInputHcclEnableFfts)
    .stubs()
    .will(returnValue(false));
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam_1server_8p(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    ret = implBase->AtomicInitSet();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->InitCCLbuffer(4096, 4096);
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    impl->topoAttr_.deviceLogicId = 0;
    impl->topoAttr_.devicePhyId = 0;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLGATHER].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLGATHER].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;

    impl->topoType_ = TopoType::TOPO_TYPE_8P_RING;
    algConfigurator->topoType_ = TopoType::TOPO_TYPE_8P_RING;
    CollAllGatherMeshAivFor91093Executor* executor = new CollAllGatherMeshAivFor91093Executor(impl->dispatcher_, topoMatcher);
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
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
    const std::vector<std::vector<u32>> tmpRingNics = {
        { 0, 1, 2, 3, 4, 5, 6, 7 },
        { 0, 1, 2, 3, 4, 5, 6, 7 }
    };

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    resourceRequest.aivBufferRequest = 3U;
    implBase->AllocAlgResource(opParam.tag, HcclCMDType::HCCL_CMD_ALLGATHER, opParam, resourceRequest, resourceResponse);
    delete executor;

    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_update_zerocopy)
{
    HcclResult ret = HCCL_SUCCESS;
    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));
    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&StreamActiveManager::StreamActive)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&OpBaseStreamManager::AllocSlaves)
    .stubs()
    .will(returnValue(std::vector<Stream>{Stream(StreamType::STREAM_TYPE_ONLINE)}));

    MOCKER(GetExternalInputHcclEnableFfts)
    .stubs()
    .will(returnValue(false));
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam_1server_8p(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910_93;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    ret = implBase->AtomicInitSet();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->InitCCLbuffer(4096, 4096);
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    HcclIpAddress serverIp("172.17.10.1");
    HcclIpAddress localIp("172.17.10.1");
    SetRetryEnable(DevType::DEV_TYPE_910_93, 1, 1, 2, true, implBase->GetAivModeConfig(), serverIp, localIp, implBase->retryEnable_);
    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    impl->topoAttr_.deviceLogicId = 0;
    impl->topoAttr_.devicePhyId = 0;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLREDUCE].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLREDUCE].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    impl->topoType_ = TopoType::TOPO_TYPE_8P_RING;
    algConfigurator->topoType_ = TopoType::TOPO_TYPE_8P_RING;
    CollAllGatherRingExecutor* executor = new CollAllGatherRingExecutor(impl->dispatcher_, topoMatcher);
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 64 * 1024 * 1024;
    opParam.DataDes.count = 1024;
    opParam.DataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);
    opParam.aicpuUnfoldMode = true;
    opParam.isZeroCopy = true;
    const std::vector<std::vector<u32>> tmpRingNics = {
        { 0, 1, 2, 3, 4, 5, 6, 7 },
        { 0, 1, 2, 3, 4, 5, 6, 7 }
    };

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->AllocAlgResource(opParam.tag, HcclCMDType::HCCL_CMD_ALLGATHER, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    resourceResponse.slaveStreams.resize(3);
    resourceResponse.notifiesMain.resize(3);
    resourceResponse.notifiesAux.resize(3);
    resourceResponse.notifiesDevMain.resize(3);
    resourceResponse.notifiesDevAux.resize(3);

    resourceResponse.threadManage.resize(3);
    for (u32 ringIndex = 0; ringIndex < 3; ringIndex ++) {
        resourceResponse.threadManage[ringIndex].reset(new (std::nothrow) ThreadManage(ringIndex, ringIndex, impl->dispatcher_));
    }

    for (auto &singleSubCommTransport : resourceResponse.opTransportResponse[COMM_LEVEL0]) {
        for (u64 i = 0; i < singleSubCommTransport.links.size(); ++i) {
            singleSubCommTransport.transportRequests[i].isValid = true;
            MachinePara linkPara;
            std::chrono::milliseconds kdefaultTimeout = std::chrono::seconds(120);
            std::shared_ptr<Transport> link(new Transport(new (std::nothrow) TransportBase(
                    nullptr, nullptr, linkPara, kdefaultTimeout)));
            singleSubCommTransport.links[i] = link;
        }
    }

    implBase->UpdateZeroCopy(opParam, resourceResponse);

    delete executor;

    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_prepare_zerocopy_algname)
{
    HcclResult ret = HCCL_SUCCESS;
    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));
    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&StreamActiveManager::StreamActive)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(GetExternalInputHcclEnableFfts)
    .stubs()
    .will(returnValue(false));
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam_1server_8p(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910_93;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    ret = implBase->AtomicInitSet();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->InitCCLbuffer(4096, 4096);
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    HcclIpAddress serverIp("172.17.10.1");
    HcclIpAddress localIp("172.17.10.1");
    SetRetryEnable(DevType::DEV_TYPE_910, 1, 1, 2, true, implBase->GetAivModeConfig(), serverIp, localIp, implBase->retryEnable_);
    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLREDUCE].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLREDUCE].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    algConfigurator->topoType_ = TopoType::TOPO_TYPE_8P_RING;
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 64 * 1024 * 1024;
    opParam.DataDes.count = 1024;
    opParam.DataDes.dataType = HCCL_DATA_TYPE_FP32;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);
    opParam.aicpuUnfoldMode = true;
    const std::vector<std::vector<u32>> tmpRingNics = {
        { 0, 1, 2, 3, 4, 5, 6, 7 },
        { 0, 1, 2, 3, 4, 5, 6, 7 }
    };

    bool isSupportZeroCopy = implBase->IsSupportZeroCopy(opParam);
    AlgDesc algDesc;
    algDesc.isZeroCopy = true;
    implBase->PrepareZeroCopy("algName", algDesc, opParam);

    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_prepare_zerocopy_op)
{
    HcclResult ret = HCCL_SUCCESS;
    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));
    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&StreamActiveManager::StreamActive)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(GetExternalInputHcclEnableFfts)
    .stubs()
    .will(returnValue(false));
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam_1server_8p(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910_93;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    ret = implBase->AtomicInitSet();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->InitCCLbuffer(4096, 4096);
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    std::shared_ptr<AlgConfigurator> algConfigurator = implBase->implAlg_->algConfigurator_;

    HcclIpAddress serverIp("172.17.10.1");
    HcclIpAddress localIp("172.17.10.1");
    SetRetryEnable(DevType::DEV_TYPE_910, 1, 1, 2, true, implBase->GetAivModeConfig(), serverIp, localIp, implBase->retryEnable_);
    MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    impl->topoAttr_.deviceLogicId = 0;
    impl->topoAttr_.devicePhyId = 0;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLREDUCE].algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    algConfigurator->algType_[HcclCMDType::HCCL_CMD_ALLREDUCE].algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    impl->topoType_ = TopoType::TOPO_TYPE_8P_RING;
    algConfigurator->topoType_ = TopoType::TOPO_TYPE_8P_RING;
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    OpParam opParam;
    opParam.tag = "test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 64 * 1024 * 1024;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 64 * 1024 * 1024;
    opParam.DataDes.count = 1024;
    opParam.DataDes.dataType = HCCL_DATA_TYPE_INT64;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);
    opParam.aicpuUnfoldMode = true;
    const std::vector<std::vector<u32>> tmpRingNics = {
        { 0, 1, 2, 3, 4, 5, 6, 7 },
        { 0, 1, 2, 3, 4, 5, 6, 7 }
    };

    bool isSupportZeroCopy = implBase->IsSupportZeroCopy(opParam);
    AlgDesc algDesc;
    algDesc.isZeroCopy = false;
    implBase->PrepareZeroCopy("algName", algDesc, opParam);

    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_AllocTransportZeroCopy)
{
    HcclResult ret = HCCL_SUCCESS;

    TransportType transportType = TransportType::TRANS_TYPE_ROCE;
    MOCKER_CPP(&TransportManager::GetTransportType)
    .stubs()
    .will(returnValue(transportType));

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910_93;
    std::unique_ptr<HcclCommunicator> communicator(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    communicator->Init(params, rankTable);

    TransportRequest transportReq1;
    transportReq1.isValid = true;
    transportReq1.remoteUserRank = 0;
    transportReq1.remoteUserRank = 1;
    transportReq1.linkType = TransportLinkType::HCCS;
    transportReq1.inputMemType = TransportMemType::CCL_INPUT;
    transportReq1.outputMemType = TransportMemType::CCL_OUTPUT;
    TransportRequest transportReq2;
    transportReq2.isValid = true;
    transportReq2.remoteUserRank = 0;
    transportReq2.remoteUserRank = 1;
    transportReq2.linkType = TransportLinkType::SIO;
    transportReq2.inputMemType = TransportMemType::CCL_INPUT;
    transportReq2.outputMemType = TransportMemType::CCL_OUTPUT;

    SingleSubCommTransport singleTrans;
    singleTrans.transportRequests.emplace_back(transportReq1);
    singleTrans.transportRequests.emplace_back(transportReq2);
    singleTrans.links.resize(2,nullptr);
    singleTrans.status.resize(2, TransportStatus::INIT);
    LevelNSubCommTransport levelTrans;
    levelTrans.emplace_back(singleTrans);
    OpCommTransport opTrans;
    opTrans.emplace_back(levelTrans);
    TransportIOMem transMem;

    MOCKER_CPP(&NotifyPool::RegisterOp).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&NotifyPool::UnregisterOp).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtGetDeviceType).stubs().with(outBound(DevType::DEV_TYPE_910_93)).will(returnValue(HCCL_SUCCESS));

   // stubs in CreateDestSockets
    MOCKER_CPP(&TransportManager::UpdateIsInterRdma).stubs().with(any(), outBound(false), any()).will(ignoreReturnValue());
    MOCKER_CPP(&TransportManager::MakeRemoteLinkInfo).stubs().will(returnValue(HCCL_SUCCESS));

   // stubs in CreateLink
    MOCKER(hrtErrMSetErrorContextPub).stubs().with(any()).will(ignoreReturnValue());
    MOCKER(hrtSetDevice).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportManager::TransportInit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtResetDevice).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclSocketManager::DestroySockets, void(HcclSocketManager::*)(const std::string&))
    .stubs().with(any()).will(ignoreReturnValue());

    std::string tag = "test";
    ret = communicator->transportManager_->Alloc(tag, transMem, opTrans, true, true, true, HcclCMDType::HCCL_CMD_ALLGATHER);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_zerocopy_alloc_slave_empty)
{
    HcclResult ret = HCCL_SUCCESS;
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.totalRanks = 1;
    params.deviceType = DevType::DEV_TYPE_910;
    rankTable.rankList.assign(rankTable.rankList.begin(), rankTable.rankList.begin() + 1);
    rankTable.deviceNum = 1;
    rankTable.serverNum = 1;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB));
    MOCKER_CPP(&HcclCommunicator::IsForceAicpuOpBaseMode)
    .stubs()
    .will(returnValue(false));
    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&OpBaseStreamManager::AllocSlaves)
    .stubs()
    .will(returnValue(std::vector<Stream>()));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CollAllGatherRingExecutor* executor = new CollAllGatherRingExecutor(impl->dispatcher_, topoMatcher);
    executor->workflowMode_ = HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB;
    executor->SetAlgType(AlgType());

    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    OpParam opParam;
    opParam.tag = "test_test_test";
    opParam.inputPtr = inputMem.ptr();
    opParam.inputSize = 4096;
    opParam.outputPtr = outputMem.ptr();
    opParam.outputSize = 4096;
    opParam.DataDes.count = 1024;
    opParam.DataDes.dataType = HCCL_DATA_TYPE_INT64;
    opParam.reduceType = HCCL_REDUCE_SUM;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);
    opParam.aicpuUnfoldMode = true;
    opParam.isZeroCopy = true;

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    resourceRequest.scratchMemSize = 4096;
    resourceRequest.isInGraphCaptureZeroCopy = true;
    resourceRequest.streamNum = 2;
    implBase->AllocAlgResource(opParam.tag, HcclCMDType::HCCL_CMD_ALLGATHER, opParam, resourceRequest, resourceResponse);

    delete executor;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_Mc2CreateAndLaunchContext_multi_server)
{
    HcclResult ret = HCCL_SUCCESS;
    std::unique_ptr<HcclCommunicator> hcclCommunicator(new (std::nothrow) HcclCommunicator());

    ret = InitEnvVarParam();

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.totalRanks = 1;
    params.deviceType = DevType::DEV_TYPE_910;
    rankTable.rankList.assign(rankTable.rankList.begin(), rankTable.rankList.begin() + 1);
    rankTable.deviceNum = 1;
    rankTable.serverNum = 1;

    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB));
    MOCKER_CPP(&HcclCommunicator::IsForceAicpuOpBaseMode)
    .stubs()
    .will(returnValue(false));
    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&OpBaseStreamManager::AllocSlaves)
    .stubs()
    .will(returnValue(std::vector<Stream>()));

    ret = hcclCommunicator->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcclCommunicator->identifier_ = "1";
    hcclCommunicator->isA2MC2MultiServer_ = true;
    constexpr u32 h2dBufferSize = sizeof(KfcExecControl);
    constexpr u32 d2hBufferSize = sizeof(KfcExecStatus);
    hcclCommunicator->kfcControlTransferH2D_.reset(new (std::nothrow)
                                                       hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    hcclCommunicator->kfcStatusTransferD2H_.reset(new (std::nothrow)
                                                      hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));

    rtStream_t stream = (rtStream_t)NULL;
    bool isOpbaseMode = false;
    void* commContext = nullptr;
    std::string tag = "tag";

    MOCKER_CPP(&HcclCommunicator::GetQosCfg)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclCommunicator::InitWorkSpace)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    u32 ifnumVersion = 3;
    MOCKER(hrtRaGetInterfaceVersion)
    .stubs()
    .with(any(), any(), outBoundP(&ifnumVersion))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtMemSyncCopy)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(aclrtLaunchKernelWithConfig).stubs().with(any()).will(returnValue(ACL_SUCCESS));
    MOCKER(hrtMemAsyncCopyByQos)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    int tmp = 0;
    HostMem hostMem = HostMem::create(&tmp, sizeof(tmp));
    hcclCommunicator->transDevIbverbsDataMem_ = std::make_shared<HostMem>(hostMem);
    hcclCommunicator->combinedCapabilityMem_ = std::make_shared<HostMem>(hostMem);
    ret = hcclCommunicator->Mc2CreateAndLaunchContext(stream, isOpbaseMode, &commContext, tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcclCommunicator->kfcControlTransferH2D_ = nullptr;
    hcclCommunicator->kfcStatusTransferD2H_ = nullptr;

    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_GetRemoteUserMemResource_When_Link_Normal_Expect_ReturnSuccess)
{
    HcclResult ret = HCCL_SUCCESS;
    std::unique_ptr<HcclCommunicator> hcclCommunicator(new (std::nothrow) HcclCommunicator());
    // 构建window resource
    OpCommTransport userMemTransport;
    LevelNSubCommTransport levelNTransport;
    SingleSubCommTransport singleTransport;
    // transport
    std::vector<TransportRequest> transportRequests;
    TransportRequest transportRequest;
    transportRequest.isValid = true;
    transportRequests.push_back(transportRequest);
    singleTransport.transportRequests = transportRequests;
    // links
    std::vector<LINK> links;
    MachinePara machinePara;
    machinePara.remoteWorldRank = 0;
    std::chrono::milliseconds kdefaultTimeout = std::chrono::seconds(120);
    DeviceMem window = DeviceMem::alloc(128);
    auto transportP2p = new (std::nothrow) TransportP2p(nullptr, nullptr, machinePara, kdefaultTimeout);
    transportP2p->remoteInputPtr_ = window.ptr();
    transportP2p->remoteOutputPtr_ = window.ptr();
    transportP2p->remoteInputSize_ = 128;
    transportP2p->remoteOutputSize_ = 128;
    std::shared_ptr<Transport> link(new Transport(transportP2p));
    links.push_back(link);
    singleTransport.links = links;
    levelNTransport.push_back(singleTransport);
    userMemTransport.push_back(levelNTransport);
    hcclCommunicator->userMemTransport_ = userMemTransport;
    // exec
    ret = hcclCommunicator->GetRemoteUserMemResource();
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    window.free();
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_hcclimpl_AiCpuSetCommResource_EnvTest)
{
    u32 ifnumVersion = 3;
    MOCKER(hrtRaGetInterfaceVersion)
    .stubs()
    .with(any(), any(), outBoundP(&ifnumVersion))
    .will(returnValue(HCCL_SUCCESS));
 
    void *commInputPtr = nullptr;
    u64 commInputSize;
    void *commOutputPtr = nullptr;
    u64 commOutputSize = 0;
    HcclResult ret;
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    std::string tag = "test_MC2MultiServer";
    CommInfo tmpComm;
    std::vector<RankInfo> paraVector;
    u32 rankSize = 8;
    u32 curRankId = 0;
    u64 commBufferSize = 20;
 
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());
 
    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hcclImpl *impl = implBase->implAlg_->pimpl_.get();
 
    ret = implBase->GetInCCLbuffer(commInputPtr, commInputSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = implBase->GetOutCCLbuffer(commOutputPtr, commOutputSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem expMem = implBase->cclBufferManager_.GetCommExpBuffer();
 
    implBase->notifyPool_.reset(new (std::nothrow) NotifyPool());
    ret = implBase->notifyPool_->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    IntraExchanger exchanger{};
    tmpComm.commLevel0.resize(4);
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    for (int i = 0; i < 4; i++) {
        tmpComm.commLevel0[i].reset(new (std::nothrow) CommMesh(tag, 0, 4, curRankId, rankSize,
            TopoType::TOPO_TYPE_8P_MESH, implBase->dispatcher_, implBase->notifyPool_, netDevCtxMap, exchanger,
            paraVector, inputMem, outputMem, true, nullptr, 0, ""));
    } 
    EXPECT_EQ(tmpComm.commLevel0[0]->transportInfo_.size(), rankSize);
    tmpComm.commLevel0[0]->transportInfo_.clear();
    std::chrono::milliseconds kdefaultTimeout = std::chrono::seconds(120);
    /*创建link*/
    MachinePara machine_para;
 
    machine_para.localDeviceId = 0;
    machine_para.deviceLogicId = 0;
    machine_para.nicDeploy == NICDeployment::NIC_DEPLOYMENT_DEVICE;
    machine_para.localIpAddr = HcclIpAddress("192.168.0.23");
    std::shared_ptr<Transport> link = nullptr;
    TransportPara para = {};
 
    link.reset(new Transport(TransportType::TRANS_TYPE_IBV_EXP, para, dispatcher, implBase->notifyPool_, machine_para));
    link->Init();
    ret = implBase->notifyPool_->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::shared_ptr<LocalIpcNotify> localNotify = nullptr;
    std::shared_ptr<RemoteNotify> remoteNotify = nullptr;
 
    RemoteRankInfo info(0, -1);
    SalGetBareTgid(reinterpret_cast<u32*>(&info.remotePid));
    ret = implBase->notifyPool_->Alloc(tag, info, localNotify, NotifyLoadType::DEVICE_NOTIFY);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    std::vector<u8> data(NOTIFY_INFO_LENGTH, 0);
    ret = localNotify->Serialize(data);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remoteNotify.reset(new (std::nothrow) RemoteNotify());
 
    ret = remoteNotify->Init(data);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = remoteNotify->Open();
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    link->pimpl_->remoteSendDoneDeviceNotify_ = remoteNotify;
    link->pimpl_->localSendDoneDeviceNotify_ = localNotify;
    link->pimpl_->remoteSendReadyDeviceNotify_ = remoteNotify;
    link->pimpl_->localSendReadyDeviceNotify_ = localNotify;
    for (u32 ringIndex = 0; ringIndex < rankSize; ringIndex++) {
        tmpComm.commLevel0[0]->transportInfo_.push_back(link);
        tmpComm.commLevel0[0]->transportType_.push_back(TransportType::TRANS_TYPE_IBV_EXP);
    }
    implBase->tagCommInfo_.insert(std::pair<std::string, CommInfo>(tag, std::move(tmpComm)));
 
    Level1StreamInfo tmpInnerStreamInfo;
    tmpInnerStreamInfo.ringNum = rankSize;
    tmpInnerStreamInfo.ringDeviceSignal.resize(rankSize - 1);
    tmpInnerStreamInfo.ringDeviceSignalAux.resize(rankSize - 1);
    tmpInnerStreamInfo.ringDeviceStreams.resize(rankSize);
 
    for (u32 ringIndex = 0; ringIndex < tmpInnerStreamInfo.ringNum; ringIndex++) {
        tmpInnerStreamInfo.ringDeviceStreams[ringIndex] = Stream(StreamType::STREAM_TYPE_DEVICE);
        if (ringIndex != tmpInnerStreamInfo.ringNum - 1) {
            tmpInnerStreamInfo.ringDeviceSignal[ringIndex] = localNotify;
            tmpInnerStreamInfo.ringDeviceSignalAux[ringIndex] = localNotify;
        }
    }
 
    MOCKER_CPP(&Transport::GetRemoteMem, HcclResult(Transport::*)(hccl::UserMemType, void**))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
 
    MOCKER_CPP(&Transport::GetRemoteMem, HcclResult(Transport::*)(std::vector<void*>*))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
  
    MOCKER_CPP(&Transport::GetRemoteMemKey, HcclResult(Transport::*)(hccl::UserMemType, uint32_t *))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&Transport::GetLocalMemDetails)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    std::vector<HcclQpInfoV2> qpInfos(1);
    MOCKER_CPP(&Transport::GetAiQpInfo).stubs().with(outBound(qpInfos)).will(returnValue(HCCL_SUCCESS));
 
 
    MOCKER_CPP(&Transport::GetChipId)
    .stubs()  
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    // 配置profiling开关
    auto &profilingManager = hccl::ProfilingManager::Instance();
    profilingManager.StartAddtionInfoSubscribe();
 
    // MachinePara machinePara;
    machine_para.localDeviceId = 0;
    std::chrono::milliseconds timeout;
 
    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(
        new (std::nothrow) HcclSocket("test", nullptr, remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machine_para.sockets.push_back(newSocket);
 
    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    TransportBase transportBase(dispatcher, notifyPool, machine_para, timeout);
    MOCKER_CPP_VIRTUAL(transportBase, &TransportBase::GetRemoteMemSize)
        .stubs()
        .with(any(), outBound(commBufferSize))
        .will(returnValue(HCCL_SUCCESS));
 
    rtStream_t aiCpuStream;
    Stream stream(aiCpuStream);
    implBase->isA2MC2MultiServer_ = true;
    implBase->tagStreamInfo_.insert(std::pair<std::string, Level1StreamInfo>(tag, std::move(tmpInnerStreamInfo)));

    u32 intraRoceSwitch = 0;
    MOCKER(GetExternalInputIntraRoceSwitch)
        .stubs()
        .will(returnValue(intraRoceSwitch));

    MOCKER_CPP(&HcclCommunicator::InitNic)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclCommunicator::CreateCommAndStreamRes)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclCommunicator::Mc2CreateAndLaunchContext)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    ret = implBase->CreateCommResource(tag, aiCpuStream, true, nullptr, "BatchWrite=level1:hierarchy");
    EXPECT_EQ(ret, HCCL_SUCCESS);
  
    implBase = nullptr;
    GlobalMockObject::verify();
}

TEST_F(HcclImplTest, ut_HcclCommunicator_AicpuUnfold_and_AllReduceAicpuUnfold)
{
    setenv("HCCL_OP_EXPANSION_MODE", "AI_CPU", 1);
    HcclResult ret = HCCL_SUCCESS;
    std::string tag = "test";
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclRtStream stream = NULL;
    HcclReduceOp op = HCCL_REDUCE_SUM;
    HcclCMDType cmdType =  HcclCMDType::HCCL_CMD_ALLREDUCE;
    s32 device_id = 0;
    // 申请device memory
    void* mem_dev_input = nullptr;
    rtError_t rt_ret = rtMalloc(&mem_dev_input, 1024, RT_MEMORY_DEFAULT, HCCL);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    void* mem_dev_output = nullptr;
    rt_ret = rtMalloc(&mem_dev_output, 1024, RT_MEMORY_DEFAULT, HCCL);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910B;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclCommunicator::InitPara)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclCommunicator::InitOneSidedService)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER_CPP(&HcclCommunicator::IsExistCommRes)
    .stubs()
    .with(any())
    .will(returnValue(true));
    
    MOCKER_CPP(&HcclCommunicator::AicpuKfcTilingDataLaunch)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->AicpuUnfold("tag_test", mem_dev_input, mem_dev_output, count, dataType, HCCL_REDUCE_SUM, stream, cmdType);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = implBase->AllReduceAicpuUnfold("tag_test", mem_dev_input, mem_dev_output, count, dataType, HCCL_REDUCE_SUM, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_OP_EXPANSION_MODE");
    rt_ret = aclrtFree(mem_dev_input);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    rt_ret = aclrtFree(mem_dev_output);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    // 销毁资源

    GlobalMockObject::verify();
}