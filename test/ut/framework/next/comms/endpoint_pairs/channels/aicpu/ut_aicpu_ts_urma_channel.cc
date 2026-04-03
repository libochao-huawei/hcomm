#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>

#include "next/comms/endpoint_pairs/channels/aicpu/aicpu_ts_urma_channel.h"

#define private public
using namespace hcomm;

class AicpuTsUrmaChannelTest : public testing::Test {
protected:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() override {}
    void TearDown() override { GlobalMockObject::verify(); }
};

TEST_F(AicpuTsUrmaChannelTest, Ut_Clean_WithoutInit_Returns_SUCCESS) {
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsUrmaChannel ch(ep, desc);

    auto ret = ch.Clean();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(AicpuTsUrmaChannelTest, Ut_Resume_MockedBuilds_Returns_SUCCESS) {
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsUrmaChannel ch(ep, desc);

    // Mock private helper methods BuildConnection and BuildUbMemTransport
    MOCKER_CPP(&AicpuTsUrmaChannel::BuildConnection, HcclResult(AicpuTsUrmaChannel::*)())
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&AicpuTsUrmaChannel::BuildUbMemTransport, HcclResult(AicpuTsUrmaChannel::*)())
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    auto ret = ch.Resume();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
