#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include "../../../framework/communicator/hccl_api_base_test.h"
#include "../../stub/llt_hccl_stub_rank_graph.h"

using namespace hccl;

class HcclCommHostTest : public testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override { GlobalMockObject::verify(); }
};

// Resume when communicator is V2 -> should call CollComm::Resume
TEST_F(HcclCommHostTest, Ut_ResumeWhenIsCommunicatorV2ExpectSuccess)
{
    MOCKER(hrtGetDeviceType)
        .stubs()
        .with(outBound(DevType::DEV_TYPE_950))
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(IsSupportHCCLV2)
        .stubs()
        .will(returnValue(true));
    setenv("HCCL_INDEPENDENT_OP","1",1);

    void* commV2 = (void*)0x2000;
    RankGraphStub rankGraphStub;
    std::shared_ptr<Hccl::RankGraph> rankGraphV2 = rankGraphStub.Create2PGraph();
    u32 rank = 1;
    HcclMem cclBuffer{};
    cclBuffer.size = 1;
    cclBuffer.type = HcclMemType::HCCL_MEM_TYPE_HOST;
    cclBuffer.addr = (void*)0x1000;
    char commName[ROOTINFO_INDENTIFIER_MAX_LENGTH] = {};

    std::shared_ptr<hccl::hcclComm> hcclCommPtr = std::make_shared<hccl::hcclComm>(1, 1, commName);

    // Mock CollComm::Init and GetHDCommunicate so InitCollComm succeeds and collComm_ is created
    MOCKER_CPP(&CollComm::Init)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&CollComm::GetHDCommunicate)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    // Stub CollComm::Resume to return success
    MOCKER_CPP(&CollComm::Resume)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    HcclCommConfig config{};
    unsetenv("HCCL_DFS_CONFIG");
    HcclResult ret = hcclCommPtr->InitCollComm(commV2, rankGraphV2.get(), rank, cclBuffer, commName, &config);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // Set devType to V2 and call Resume
    hcclCommPtr->devType_ = DevType::DEV_TYPE_950;
    ret = hcclCommPtr->Resume();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcclCommHostTest, Ut_ResumeWhenIsCommunicatorV2AndCollResumeFailsExpectError)
{
    MOCKER(hrtGetDeviceType)
        .stubs()
        .with(outBound(DevType::DEV_TYPE_950))
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(IsSupportHCCLV2)
        .stubs()
        .will(returnValue(true));
    setenv("HCCL_INDEPENDENT_OP","1",1);

    void* commV2 = (void*)0x2000;
    RankGraphStub rankGraphStub;
    std::shared_ptr<Hccl::RankGraph> rankGraphV2 = rankGraphStub.Create2PGraph();
    u32 rank = 1;
    HcclMem cclBuffer{};
    cclBuffer.size = 1;
    cclBuffer.type = HcclMemType::HCCL_MEM_TYPE_HOST;
    cclBuffer.addr = (void*)0x1000;
    char commName[ROOTINFO_INDENTIFIER_MAX_LENGTH] = {};

    std::shared_ptr<hccl::hcclComm> hcclCommPtr = std::make_shared<hccl::hcclComm>(1, 1, commName);

    MOCKER_CPP(&CollComm::Init)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&CollComm::GetHDCommunicate)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    // Fail resume
    MOCKER_CPP(&CollComm::Resume)
        .stubs()
        .will(returnValue(HCCL_E_INTERNAL));

    HcclCommConfig config{};
    unsetenv("HCCL_DFS_CONFIG");
    HcclResult ret = hcclCommPtr->InitCollComm(commV2, rankGraphV2.get(), rank, cclBuffer, commName, &config);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcclCommPtr->devType_ = DevType::DEV_TYPE_950;
    ret = hcclCommPtr->Resume();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(HcclCommHostTest, Ut_GetCommStatusWhenIsCommunicatorV2ExpectCollStatus)
{
    MOCKER(hrtGetDeviceType)
        .stubs()
        .with(outBound(DevType::DEV_TYPE_950))
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(IsSupportHCCLV2)
        .stubs()
        .will(returnValue(true));
    setenv("HCCL_INDEPENDENT_OP","1",1);

    void* commV2 = (void*)0x2000;
    RankGraphStub rankGraphStub;
    std::shared_ptr<Hccl::RankGraph> rankGraphV2 = rankGraphStub.Create2PGraph();
    u32 rank = 1;
    HcclMem cclBuffer{};
    cclBuffer.size = 1;
    cclBuffer.type = HcclMemType::HCCL_MEM_TYPE_HOST;
    cclBuffer.addr = (void*)0x1000;
    char commName[ROOTINFO_INDENTIFIER_MAX_LENGTH] = {};

    std::shared_ptr<hccl::hcclComm> hcclCommPtr = std::make_shared<hccl::hcclComm>(1, 1, commName);

    MOCKER_CPP(&CollComm::Init)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&CollComm::GetHDCommunicate)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    // stub GetCommStatus to return SUSPENDING
    MOCKER_CPP(&CollComm::GetCommStatus)
        .stubs()
        .will(returnValue(HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING));

    HcclCommConfig config{};
    unsetenv("HCCL_DFS_CONFIG");
    HcclResult ret = hcclCommPtr->InitCollComm(commV2, rankGraphV2.get(), rank, cclBuffer, commName, &config);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcclCommPtr->devType_ = DevType::DEV_TYPE_950;
    HcclCommStatus status = HcclCommStatus::HCCL_COMM_STATUS_INVALID;
    ret = hcclCommPtr->GetCommStatus(status);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(status, HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING);
}

TEST_F(HcclCommHostTest, Ut_GetCommStatusWhenIsCommunicatorV1ExpectReady)
{
    std::shared_ptr<hccl::hcclComm> hcclCommPtr = std::make_shared<hccl::hcclComm>();
    hcclCommPtr->devType_ = DevType::DEV_TYPE_910_93;

    HcclCommStatus status = HcclCommStatus::HCCL_COMM_STATUS_INVALID;
    HcclResult ret = hcclCommPtr->GetCommStatus(status);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(status, HcclCommStatus::HCCL_COMM_STATUS_READY);
}