#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>

#include "local_ub_rma_buffer.h"
#include "local_rdma_rma_buffer.h"
#include "local_ipc_rma_buffer.h"
#include "remote_rma_buffer.h"

using namespace Hccl;

class LocalRmaBufferTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "LocalRmaBuffer tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "LocalRmaBuffer tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        MOCKER(HrtIpcSetMemoryName).stubs().with(any(), any(), any(), any());
        MOCKER(HrtDevMemAlignWithPage).stubs().with(any(), any(), any(), any(), any());
        MOCKER(HrtIpcDestroyMemoryName).stubs().with(any());

        std::cout << "A Test case in LocalRmaBuffer SetUP." << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in LocalRmaBuffer TearDown." << std::endl;
    }

    std::shared_ptr<DevBuffer> devBuf = DevBuffer::Create(0x100, 0x100);
};

TEST_F(LocalRmaBufferTest, localubrmabuffer_construct_error)
{
    EXPECT_THROW(LocalUbRmaBuffer localUbRmaBuffer(nullptr, nullptr), NullPtrException);
};

TEST_F(LocalRmaBufferTest, localubrmabuffer_serialize)
{
    u32 token = 1;
    MOCKER(GetUbToken).stubs().will(returnValue(token));
    RdmaHandle rdmaHandle = (void *)0x1000000;
    LocalUbRmaBuffer localUbRmaBuffer(devBuf, rdmaHandle);
    string msg = localUbRmaBuffer.Describe();
    EXPECT_NE(0, msg.length());

    UbRmaBufferExchangeData exchangeData;
    exchangeData.addr = devBuf->GetAddr();
    exchangeData.size = devBuf->GetSize();
    exchangeData.tokenValue = 1;
    exchangeData.tokenId = 0;
    u8 key[HRT_UB_MEM_KEY_MAX_LEN]{};
    memcpy_s(exchangeData.key, HRT_UB_MEM_KEY_MAX_LEN, key, HRT_UB_MEM_KEY_MAX_LEN);
};

TEST_F(LocalRmaBufferTest, getExchangeDto_rdma_test)
{
    u32 token = 1;
    MOCKER(GetUbToken).stubs().will(returnValue(token));
    RdmaHandle rdmaHandle = (void *)0x1000000;
    LocalUbRmaBuffer localUbRmaBuffer(devBuf, rdmaHandle);

    localUbRmaBuffer.GetExchangeDto();
};

TEST_F(LocalRmaBufferTest, Serialize_test)
{
    u32 token = 1;
    MOCKER(GetUbToken).stubs().will(returnValue(token));
    RdmaHandle rdmaHandle = (void *)0x1000000;
    LocalRdmaRmaBuffer localRdmaRmaBuffer(devBuf, rdmaHandle);

    localRdmaRmaBuffer.Describe();
};

TEST_F(LocalRmaBufferTest, getExchangeDto_ipc_test)
{
    u32 token = 1;
    MOCKER(GetUbToken).stubs().will(returnValue(token));
    LocalIpcRmaBuffer localIpcRmaBuffer(devBuf);

    localIpcRmaBuffer.Describe();
    localIpcRmaBuffer.GetExchangeDto();

    u32 pid = 1;
    MOCKER(HrtDeviceGetBareTgid).stubs().will(returnValue(pid));
    localIpcRmaBuffer.Grant(pid);
};

TEST_F(LocalRmaBufferTest, generate_safe_random_number)
{
    MOCKER(HrtGetDevice).stubs().will(returnValue(1));
    MOCKER(HrtGetDevicePhyIdByIndex).stubs().will(returnValue(1));
    MOCKER(HrtRaGetSecRandom).stubs().with(any(), any());
    u32 token = GetUbToken();
}