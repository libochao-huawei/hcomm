#include <arpa/inet.h>

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>

#define private public
#include "next/comms/endpoint_pairs/channels/aicpu/aicpu_ts_roce_channel.h"
#undef private

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

TEST_F(AicpuTsRoceChannelTest, Ut_Init_When_EndpointHandleNull_Returns_E_PTR) {
    HcommChannelDesc desc{};
    AicpuTsRoceChannel ch(static_cast<EndpointHandle>(nullptr), desc);
    EXPECT_EQ(ch.Init(), HCCL_E_PTR);
}

TEST_F(AicpuTsRoceChannelTest, Ut_GetRemoteMem_When_RemoteMemOutNull_Returns_E_PTR) {
    HcommChannelDesc desc{};
    AicpuTsRoceChannel ch(reinterpret_cast<EndpointHandle>(0x1), desc);
    uint32_t n = 0;
    EXPECT_EQ(ch.GetRemoteMem(nullptr, &n, nullptr), HCCL_E_PTR);
}

TEST_F(AicpuTsRoceChannelTest, Ut_GetRemoteMem_When_MemNumOutNull_Returns_E_PTR) {
    HcommChannelDesc desc{};
    AicpuTsRoceChannel ch(reinterpret_cast<EndpointHandle>(0x1), desc);
    HcclMem *p = nullptr;
    EXPECT_EQ(ch.GetRemoteMem(&p, nullptr, nullptr), HCCL_E_PTR);
}

TEST_F(AicpuTsRoceChannelTest, Ut_GetRemoteMem_When_NotInited_Returns_E_PTR) {
    HcommChannelDesc desc{};
    AicpuTsRoceChannel ch(reinterpret_cast<EndpointHandle>(0x1), desc);
    HcclMem *remote = nullptr;
    uint32_t memNum = 0;
    EXPECT_EQ(ch.GetRemoteMem(&remote, &memNum, nullptr), HCCL_E_PTR);
}

TEST_F(AicpuTsRoceChannelTest, Ut_BuildSocketTagName_When_ValidIpv4_Returns_SUCCESS) {
    HcommChannelDesc desc{};
    AicpuTsRoceChannel ch(reinterpret_cast<EndpointHandle>(0x1), desc);
    ch.localEp_.protocol = COMM_PROTOCOL_ROCE;
    ch.localEp_.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
    ch.localEp_.loc.device.devPhyId = 0U;
    ch.localEp_.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    ASSERT_EQ(inet_pton(AF_INET, "192.168.10.1", &ch.localEp_.commAddr.addr), 1);
    ch.remoteEp_.protocol = COMM_PROTOCOL_ROCE;
    ch.remoteEp_.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
    ch.remoteEp_.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    ASSERT_EQ(inet_pton(AF_INET, "192.168.10.2", &ch.remoteEp_.commAddr.addr), 1);
    ch.isLocalIpClient_ = true;
    ch.channelDesc_.port = 16666U;
    ch.channelDesc_.common.index = 7U;
    std::string tag;
    EXPECT_EQ(ch.BuildSocketTagName(tag), HCCL_SUCCESS);
    EXPECT_FALSE(tag.empty());
}
