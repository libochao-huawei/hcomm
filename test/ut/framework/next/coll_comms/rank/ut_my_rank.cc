#include <iostream>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include "hcomm_c_adpt.h"
#include "my_rank.h"

using namespace hccl;

namespace {
HcommResult StubChannelGetUserRemoteMemSuccess(ChannelHandle, CommMem **remoteMem, char ***memTag, uint32_t *memNum)
{
    if (remoteMem != nullptr) {
        *remoteMem = nullptr;
    }
    if (memTag != nullptr) {
        *memTag = nullptr;
    }
    if (memNum != nullptr) {
        *memNum = 0;
    }
    return HCCL_SUCCESS;
}

HcommResult StubChannelGetUserRemoteMemRemoteMemNull(ChannelHandle, CommMem **remoteMem, char ***memTag, uint32_t *memNum)
{
    if (remoteMem != nullptr) {
        *remoteMem = nullptr;
    }
    if (memTag != nullptr) {
        static char memTagData[] = "HcclBuffer";
        static char *memTagList[] = {memTagData};
        *memTag = memTagList;
    }
    if (memNum != nullptr) {
        *memNum = 1;
    }
    return HCCL_SUCCESS;
}

HcommResult StubChannelGetUserRemoteMemMemTagNull(ChannelHandle, CommMem **remoteMem, char ***memTag, uint32_t *memNum)
{
    static CommMem remoteMemData{};
    if (remoteMem != nullptr) {
        *remoteMem = &remoteMemData;
    }
    if (memTag != nullptr) {
        *memTag = nullptr;
    }
    if (memNum != nullptr) {
        *memNum = 1;
    }
    return HCCL_SUCCESS;
}
} // namespace

class MyRankTest : public testing::Test {
protected:
    void TearDown() override
    {
        GlobalMockObject::verify();
    }

    std::unique_ptr<MyRank> CreateMyRank()
    {
        aclrtBinHandle binHandle = nullptr;
        CommConfig config;
        ManagerCallbacks callbacks;
        return std::make_unique<MyRank>(binHandle, 0, config, callbacks);
    }
};

TEST_F(MyRankTest, Ut_When_ChannelGetRemoteMem_Normal_Expect_SUCCESS)
{
    MOCKER_CPP(&HcommChannelGetUserRemoteMem).stubs().will(invoke(StubChannelGetUserRemoteMemSuccess));

    auto myRank = CreateMyRank();
    ChannelHandle channel = 0x12345;
    CommMem *remoteMem = nullptr;
    char **memTag = nullptr;
    uint32_t memNum = 0;

    HcclResult ret = myRank->ChannelGetRemoteMem(channel, &remoteMem, &memTag, &memNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(memNum, 0U);
}

TEST_F(MyRankTest, Ut_When_ChannelGetRemoteMem_InnerReturnsError_Expect_ForwardError)
{
    MOCKER_CPP(&HcommChannelGetUserRemoteMem).stubs().will(returnValue(static_cast<HcommResult>(HCCL_E_INTERNAL)));

    auto myRank = CreateMyRank();
    ChannelHandle channel = 0x12345;
    CommMem *remoteMem = nullptr;
    char **memTag = nullptr;
    uint32_t memNum = 0;

    HcclResult ret = myRank->ChannelGetRemoteMem(channel, &remoteMem, &memTag, &memNum);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(MyRankTest, Ut_When_ChannelGetRemoteMem_RemoteMemNull_Expect_E_PTR)
{
    MOCKER_CPP(&HcommChannelGetUserRemoteMem).stubs().will(invoke(StubChannelGetUserRemoteMemRemoteMemNull));

    auto myRank = CreateMyRank();
    ChannelHandle channel = 0x12345;
    CommMem *remoteMem = nullptr;
    char **memTag = nullptr;
    uint32_t memNum = 0;

    HcclResult ret = myRank->ChannelGetRemoteMem(channel, &remoteMem, &memTag, &memNum);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(MyRankTest, Ut_When_ChannelGetRemoteMem_MemTagNull_Expect_E_PTR)
{
    MOCKER_CPP(&HcommChannelGetUserRemoteMem).stubs().will(invoke(StubChannelGetUserRemoteMemMemTagNull));

    auto myRank = CreateMyRank();
    ChannelHandle channel = 0x12345;
    CommMem *remoteMem = nullptr;
    char **memTag = nullptr;
    uint32_t memNum = 0;

    HcclResult ret = myRank->ChannelGetRemoteMem(channel, &remoteMem, &memTag, &memNum);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(MyRankTest, Ut_When_ChannelGetRemoteMem_ParamRemoteMemNull_Expect_E_PTR)
{
    auto myRank = CreateMyRank();

    ChannelHandle channel = 0x12345;
    char **memTag = nullptr;
    uint32_t memNum = 0;
    HcclResult ret = myRank->ChannelGetRemoteMem(channel, nullptr, &memTag, &memNum);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(MyRankTest, Ut_When_ChannelGetRemoteMem_ParamMemTagNull_Expect_E_PTR)
{
    auto myRank = CreateMyRank();

    ChannelHandle channel = 0x12345;
    CommMem *remoteMem = nullptr;
    uint32_t memNum = 0;
    HcclResult ret = myRank->ChannelGetRemoteMem(channel, &remoteMem, nullptr, &memNum);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(MyRankTest, Ut_When_ChannelGetRemoteMem_ParamMemNumNull_Expect_E_PTR)
{
    auto myRank = CreateMyRank();

    ChannelHandle channel = 0x12345;
    CommMem *remoteMem = nullptr;
    char **memTag = nullptr;
    HcclResult ret = myRank->ChannelGetRemoteMem(channel, &remoteMem, &memTag, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}
