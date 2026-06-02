#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>

#define private public
#define protected public
#include "next/comms/endpoint_pairs/channels/aicpu/aicpu_ts_p2p_channel.h"
#undef private
#undef protected
using namespace hcomm;

class AicpuTsP2pChannelTest : public testing::Test {
protected:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() override {}
    void TearDown() override { GlobalMockObject::verify(); }
};

// Makebufs: LocalIpcRmaBuffer* handle → Buffer 字段 verifies memType/memTag/addr/size
TEST_F(AicpuTsP2pChannelTest, Ut_Makebufs_When_LocalIpcHandle_Expect_BufferFieldsFromRmaBuffer)
{
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsP2pChannel ch(ep, desc);

    auto rawBuffer = std::make_shared<Hccl::Buffer>(0x44400, 0x200, HCCL_MEM_TYPE_DEVICE, "p2p_user");
    auto localRmaBuffer = std::make_shared<Hccl::LocalIpcRmaBuffer>(rawBuffer);
    HcommMemHandle memHandles[1] = { reinterpret_cast<HcommMemHandle>(localRmaBuffer.get()) };

    std::vector<std::shared_ptr<Hccl::Buffer>> bufs;
    ASSERT_EQ(ch.Makebufs(memHandles, 1, bufs), HCCL_SUCCESS);
    ASSERT_EQ(bufs.size(), 1U);
    EXPECT_EQ(bufs[0]->GetAddr(), localRmaBuffer->GetAddr());
    EXPECT_EQ(bufs[0]->GetSize(), localRmaBuffer->GetSize());
    EXPECT_EQ(bufs[0]->GetMemType(), rawBuffer->GetMemType());
    EXPECT_EQ(bufs[0]->GetMemTag(), rawBuffer->GetMemTag());
}

// Makebufs: 多个 handle 同时传入
TEST_F(AicpuTsP2pChannelTest, Ut_Makebufs_When_MultipleHandles_Expect_AllConverted)
{
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsP2pChannel ch(ep, desc);

    auto raw1 = std::make_shared<Hccl::Buffer>(0x1000, 0x100, HCCL_MEM_TYPE_DEVICE, "tag1");
    auto raw2 = std::make_shared<Hccl::Buffer>(0x2000, 0x200, HCCL_MEM_TYPE_HOST, "tag2");
    auto loc1 = std::make_shared<Hccl::LocalIpcRmaBuffer>(raw1);
    auto loc2 = std::make_shared<Hccl::LocalIpcRmaBuffer>(raw2);
    HcommMemHandle memHandles[2] = {
        reinterpret_cast<HcommMemHandle>(loc1.get()),
        reinterpret_cast<HcommMemHandle>(loc2.get())
    };

    std::vector<std::shared_ptr<Hccl::Buffer>> bufs;
    ASSERT_EQ(ch.Makebufs(memHandles, 2, bufs), HCCL_SUCCESS);
    ASSERT_EQ(bufs.size(), 2U);

    EXPECT_EQ(bufs[0]->GetMemTag(), "tag1");
    EXPECT_EQ(bufs[0]->GetMemType(), HCCL_MEM_TYPE_DEVICE);
    EXPECT_EQ(bufs[1]->GetMemTag(), "tag2");
    EXPECT_EQ(bufs[1]->GetMemType(), HCCL_MEM_TYPE_HOST);
}

// Makebufs: nullptr handle → CHK_PTR_NULL 触发返回错误
TEST_F(AicpuTsP2pChannelTest, Ut_Makebufs_When_NullHandle_Expect_Error)
{
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsP2pChannel ch(ep, desc);

    HcommMemHandle memHandles[1] = { nullptr };
    std::vector<std::shared_ptr<Hccl::Buffer>> bufs;
    EXPECT_NE(ch.Makebufs(memHandles, 1, bufs), HCCL_SUCCESS);
}

// Makebufs: memHandleNum == 0 → 空 bufs 返回
TEST_F(AicpuTsP2pChannelTest, Ut_Makebufs_When_ZeroHandles_Expect_EmptyBufs)
{
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsP2pChannel ch(ep, desc);

    std::vector<std::shared_ptr<Hccl::Buffer>> bufs;
    ASSERT_EQ(ch.Makebufs(nullptr, 0, bufs), HCCL_SUCCESS);
    EXPECT_TRUE(bufs.empty());
}
