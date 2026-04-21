#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>

#include "next/comms/endpoint_pairs/channels/aicpu/aicpu_ts_uboe_channel.h"

#define private public
using namespace hcomm;

class AicpuTsUboeChannelTest : public testing::Test {
protected:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() override {}
    void TearDown() override { GlobalMockObject::verify(); }
};

TEST_F(AicpuTsUboeChannelTest, Ut_Clean_WithoutInit_Returns_SUCCESS) {
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsUboeChannel ch(ep, desc);

    auto ret = ch.Clean();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(AicpuTsUboeChannelTest, Ut_Init_MockedHelpers_Returns_SUCCESS) {
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsUboeChannel ch(ep, desc);

    // Mock private helper methods used by Init
    MOCKER_CPP(&AicpuTsUboeChannel::ParseInputParam, HcclResult(AicpuTsUboeChannel::*)())
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&AicpuTsUboeChannel::BuildSocket, HcclResult(AicpuTsUboeChannel::*)())
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&AicpuTsUboeChannel::BuildNotify, HcclResult(AicpuTsUboeChannel::*)())
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&AicpuTsUboeChannel::BuildBuffer, HcclResult(AicpuTsUboeChannel::*)(std::vector<std::shared_ptr<Hccl::Buffer>>&))
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    auto ret = ch.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(AicpuTsUboeChannelTest, Ut_GetStatus_WhenSocketNotReady_Returns_INIT) {
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsUboeChannel ch(ep, desc);

    // Ensure initial channelStatus is INIT
    EXPECT_EQ(ch.channelStatus, ChannelStatus::INIT);

    // Mock IsSocketReady to simulate socket not ready
    MOCKER_CPP(&AicpuTsUboeChannel::IsSocketReady, bool(AicpuTsUboeChannel::*)())
        .stubs()
        .with(any())
        .will(returnValue(false));

    auto status = ch.GetStatus();
    EXPECT_EQ(status, ChannelStatus::INIT);
}
