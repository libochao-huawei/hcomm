#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>

#define private public
#define protected public
#include "aicpu/aicpu_ts_p2p_channel.h"
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
