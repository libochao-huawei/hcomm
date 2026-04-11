#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>

#include "next/comms/endpoint_pairs/channels/aicpu/aicpu_ts_roce_channel.h"

using namespace hcomm;

class AicpuTsRoceChannelTest : public testing::Test {
protected:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() override {}
    void TearDown() override { GlobalMockObject::verify(); }
};

TEST_F(AicpuTsRoceChannelTest, Ut_Clean_WithoutInit_Returns_SUCCESS) {
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsRoceChannel ch(ep, desc);

    auto ret = ch.Clean();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(AicpuTsRoceChannelTest, Ut_Resume_WithoutInit_Returns_SUCCESS) {
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsRoceChannel ch(ep, desc);

    auto ret = ch.Resume();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(AicpuTsRoceChannelTest, Ut_GetChannelKind_Returns_AICPU_TS_ROCE) {
    HcommChannelDesc desc{};
    AicpuTsRoceChannel ch(reinterpret_cast<EndpointHandle>(0x1), desc);
    EXPECT_EQ(ch.GetChannelKind(), HcommChannelKind::AICPU_TS_ROCE);
}

TEST_F(AicpuTsRoceChannelTest, Ut_GetStatus_WithoutInit_Returns_INIT) {
    HcommChannelDesc desc{};
    AicpuTsRoceChannel ch(reinterpret_cast<EndpointHandle>(0x1), desc);
    EXPECT_EQ(ch.GetStatus(), ChannelStatus::INIT);
}

TEST_F(AicpuTsRoceChannelTest, Ut_GetNotifyNum_WhenNullOut_Returns_PTR) {
    HcommChannelDesc desc{};
    AicpuTsRoceChannel ch(reinterpret_cast<EndpointHandle>(0x1), desc);
    EXPECT_EQ(ch.GetNotifyNum(nullptr), HCCL_E_PTR);
}

TEST_F(AicpuTsRoceChannelTest, Ut_GetUserRemoteMem_Returns_NOT_SUPPORT) {
    HcommChannelDesc desc{};
    AicpuTsRoceChannel ch(reinterpret_cast<EndpointHandle>(0x1), desc);
    CommMem *remoteMem = nullptr;
    char **memTag = nullptr;
    uint32_t memNum = 0;
    EXPECT_EQ(ch.GetUserRemoteMem(&remoteMem, &memTag, &memNum), HCCL_E_NOT_SUPPORT);
}

TEST_F(AicpuTsRoceChannelTest, Ut_Serialize_WithoutInit_Returns_INTERNAL) {
    HcommChannelDesc desc{};
    AicpuTsRoceChannel ch(reinterpret_cast<EndpointHandle>(0x1), desc);
    std::shared_ptr<hccl::DeviceMem> out;
    EXPECT_EQ(ch.Serialize(out), HCCL_E_INTERNAL);
    EXPECT_EQ(out.get(), nullptr);
}
