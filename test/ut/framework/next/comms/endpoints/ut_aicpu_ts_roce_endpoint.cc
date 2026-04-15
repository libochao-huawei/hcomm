#include <arpa/inet.h>

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include <vector>

#define private public
#include "aicpu_ts_roce_endpoint.h"
#include "reged_mems/aicpu_ts_roce_mem.h"
#undef private

using namespace hcomm;

class AicpuTsRoceEndpointTest : public testing::Test {
protected:
    void TearDown() override { GlobalMockObject::verify(); }
};

TEST_F(AicpuTsRoceEndpointTest, Ut_Init_WhenLocIsHost_Returns_NOT_SUPPORT) {
    EndpointDesc desc{};
    desc.protocol = COMM_PROTOCOL_ROCE;
    desc.loc.locType = ENDPOINT_LOC_TYPE_HOST;
    desc.commAddr.type = COMM_ADDR_TYPE_IP_V4;

    AicpuTsRoceEndpoint ep(desc);
    EXPECT_EQ(ep.Init(), HCCL_E_NOT_SUPPORT);
}

TEST_F(AicpuTsRoceEndpointTest, Ut_AddListenSocketWhiteList_WhenEmpty_Returns_PARA) {
    const std::vector<SocketWlistInfo> empty{};
    EXPECT_EQ(AicpuTsRoceEndpoint::AddListenSocketWhiteList(16666, empty), HCCL_E_PARA);
}

TEST_F(AicpuTsRoceEndpointTest, Ut_AddListenSocketWhiteList_WhenNoListenSocket_Returns_NOT_FOUND) {
    SocketWlistInfo entry{};
    entry.connLimit = 1U;
    const std::vector<SocketWlistInfo> one{ entry };

    constexpr uint32_t kUnusedListenPort = 49151U;
    EXPECT_EQ(AicpuTsRoceEndpoint::AddListenSocketWhiteList(kUnusedListenPort, one), HCCL_E_NOT_FOUND);
}

TEST_F(AicpuTsRoceEndpointTest, Ut_AcceptDataSocket_WhenNoListenSocket_Returns_NOT_FOUND) {
    std::shared_ptr<hccl::HcclSocket> out;
    constexpr uint32_t kUnusedListenPort = 49152U;
    EXPECT_EQ(AicpuTsRoceEndpoint::AcceptDataSocket(kUnusedListenPort, "tag", out, 0), HCCL_E_NOT_FOUND);
    EXPECT_EQ(out.get(), nullptr);
}

TEST_F(AicpuTsRoceEndpointTest, Ut_UnregisterMemory_WhenMemHandleNull_Returns_PTR) {
    EndpointDesc desc{};
    desc.protocol = COMM_PROTOCOL_ROCE;
    desc.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
    desc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    ASSERT_EQ(inet_pton(AF_INET, "10.10.10.10", &desc.commAddr.addr), 1);

    AicpuTsRoceEndpoint ep(desc);
    ep.regedMemMgr_ = std::make_shared<AicpuTsRoceRegedMemMgr>(nullptr);
    EXPECT_EQ(ep.UnregisterMemory(nullptr), HCCL_E_PTR);
}

TEST_F(AicpuTsRoceEndpointTest, Ut_RegisterMemory_WhenMemHandleOutNull_Returns_PTR) {
    EndpointDesc desc{};
    desc.protocol = COMM_PROTOCOL_ROCE;
    desc.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
    desc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    ASSERT_EQ(inet_pton(AF_INET, "10.10.10.11", &desc.commAddr.addr), 1);

    AicpuTsRoceEndpoint ep(desc);
    ep.regedMemMgr_ = std::make_shared<AicpuTsRoceRegedMemMgr>(nullptr);
    HcommMem mem{};
    mem.addr = reinterpret_cast<void *>(0x1000U);
    mem.size = 4096U;
    mem.type = COMM_MEM_TYPE_DEVICE;
    EXPECT_EQ(ep.RegisterMemory(mem, "t", nullptr), HCCL_E_PTR);
}
