#include "gtest/gtest.h"

#define private public
#include "next/comms/endpoint_pairs/channels/ccu/ccu_urma_channel.h"
#undef private

using namespace hcomm;

class CcuUrmaChannelTest : public testing::Test {
protected:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CcuUrmaChannelTest, Ut_Clean_When_ImplIsNull_Expect_HCCL_E_PTR) {
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    CcuUrmaChannel ch(ep, desc);

    auto ret = ch.Clean();
    EXPECT_EQ(ret, HcclResult::HCCL_E_PTR);
}

// Minimal test-only connection deriving from CcuConnection to avoid heavy Init()
class TestCcuConnection : public CcuConnection {
public:
    TestCcuConnection(const CommAddr &locAddr, const CommAddr &rmtAddr,
        const CcuChannelInfo &channelInfo, const std::vector<CcuJetty *> &ccuJettys)
        : CcuConnection(locAddr, rmtAddr, channelInfo, ccuJettys) {}
    // Do not call Init(); use default base behavior for Clean()
};

TEST_F(CcuUrmaChannelTest, Ut_Clean_When_ImplIsPresent_Expect_HCCL_SUCCESS) {
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    CcuUrmaChannel ch(ep, desc);

    // Prepare minimal safe objects to construct a CcuTransport without heavy Init
    CommAddr locAddr{};
    CommAddr rmtAddr{};
    CcuChannelInfo channelInfo{};
    std::vector<CcuJetty *> jettys{};

    // Create a test connection (does not call Init)
    std::unique_ptr<CcuConnection> conn(new TestCcuConnection(locAddr, rmtAddr, channelInfo, jettys));

    // Fake socket pointer (not dereferenced by Clean())
    Hccl::Socket *fakeSocket = reinterpret_cast<Hccl::Socket *>(0x1);

    // Prepare a simple buffer info
    CcuTransport::CclBufferInfo bufInfo(0x1000, 0x100, 1, 1);

    // Construct transport instance (constructor does not call Init)
    std::unique_ptr<CcuTransport> transport(new CcuTransport(fakeSocket, std::move(conn), bufInfo));

    // Inject into channel (we used #define private public when included)
    ch.impl_ = std::move(transport);

    auto ret = ch.Clean();
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(CcuUrmaChannelTest, Ut_Resume_When_Called_Expect_HCCL_SUCCESS) {
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    CcuUrmaChannel ch(ep, desc);

    auto ret = ch.Resume();
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}
