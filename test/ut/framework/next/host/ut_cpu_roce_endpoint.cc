#include "gtest/gtest.h"
#include "hcomm_c_adpt.h"
#include "ip_address.h"

class CpuRoceEndpointTest : public testing::Test {};

TEST_F(CpuRoceEndpointTest, Ut_When_Endpoint_LocType_Invalid_Expect_Return_HCCL_E_PARA)
{
    EndpointDesc endpointDesc{};
    endpointDesc.protocol = COMM_PROTOCOL_ROCE;
    endpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    endpointDesc.commAddr.addr = Hccl::IpAddress("1.0.0.0").GetBinaryAddress().addr;
    endpointDesc.loc.locType = ENDPOINT_LOC_TYPE_RESERVED;

    EndpointHandle endpointHandle = nullptr;
    HcommResult ret = HcommEndpointCreate(&endpointDesc, &endpointHandle);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(CpuRoceEndpointTest, Ut_When_RoceAndDeviceLocType_Expect_Return_HCCL_E_PARA)
{
    EndpointDesc endpointDesc{};
    endpointDesc.protocol = COMM_PROTOCOL_ROCE;
    endpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    endpointDesc.commAddr.addr = Hccl::IpAddress("1.0.0.0").GetBinaryAddress().addr;
    endpointDesc.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;

    EndpointHandle endpointHandle = nullptr;
    HcommResult ret = HcommEndpointCreate(&endpointDesc, &endpointHandle);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(CpuRoceEndpointTest, ut_HcommEndpointGet_When_EndpointNotFound_Expect_ReturnHCCL_E_PARA)
{
    EndpointHandle handle = reinterpret_cast<EndpointHandle>(0x12345678);
    void *endpoint = nullptr;
    HcommResult ret = HcommEndpointGet(handle, &endpoint);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(CpuRoceEndpointTest, ut_HcommEndpointDestroy_When_EndpointNull_Expect_ReturnHCCL_E_PTR)
{
    HcommResult ret = HcommEndpointDestroy(nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(CpuRoceEndpointTest, ut_HcommEndpointDestroy_When_EndpointNotFound_Expect_ReturnHCCL_E_INTERNAL)
{
    EndpointHandle handle = reinterpret_cast<EndpointHandle>(0x12345678);
    HcommResult ret = HcommEndpointDestroy(handle);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(CpuRoceEndpointTest, ut_HcommMemReg_When_MemIsNull_Expect_ReturnHCCL_E_PTR)
{
    HcommMemHandle memHandle;
    HcommResult ret = HcommMemReg(nullptr, "tag", nullptr, &memHandle);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(CpuRoceEndpointTest, ut_HcommMemReg_When_MemHandleIsNull_Expect_ReturnHCCL_E_PTR)
{
    HcommMem mem{};
    mem.type = COMM_MEM_TYPE_DEVICE;
    mem.addr = reinterpret_cast<void *>(0x12345678);
    mem.size = 16;
    HcommResult ret = HcommMemReg(nullptr, "tag", &mem, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(CpuRoceEndpointTest, ut_HcommMemUnreg_When_EndpointIsNull_Expect_ReturnHCCL_E_PTR)
{
    HcommMemHandle memHandle = reinterpret_cast<HcommMemHandle>(0x12345678);
    HcommResult ret = HcommMemUnreg(nullptr, memHandle);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(CpuRoceEndpointTest, ut_HcommMemExport_When_EndpointIsNull_Expect_ReturnHCCL_E_PTR)
{
    void *memDesc = nullptr;
    uint32_t memDescLen = 0;
    HcommResult ret = HcommMemExport(nullptr, reinterpret_cast<HcommMemHandle>(0x12345678), &memDesc, &memDescLen);
    EXPECT_EQ(ret, HCCL_E_PTR);
}
