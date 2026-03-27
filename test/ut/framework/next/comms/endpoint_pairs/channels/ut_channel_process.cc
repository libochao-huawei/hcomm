#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>

#include "next/comms/endpoint_pairs/channels/channel_process.h"

#define private public
using namespace hcomm;

class ChannelProcessTest : public testing::Test {
protected:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() override {}
    void TearDown() override { GlobalMockObject::verify(); }
};

TEST_F(ChannelProcessTest, Ut_ChannelClean_NullList_Returns_E_PARA) {
    // Passing null pointer should return parameter error
    auto ret = ChannelProcess::ChannelClean(nullptr, 1);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(ChannelProcessTest, Ut_ChannelResumeConcurrency_ZeroChannels_Returns_SUCCESS) {
    // Zero channel count should be a no-op and return success
    ChannelHandle dummyList[1] = {0};
    auto ret = ChannelProcess::ChannelResumeConcurrency(dummyList, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(ChannelProcessTest, Ut_ChannelResume_When_ResumeConcurrencyFails_ReturnsError) {
    ChannelHandle list[1] = { (ChannelHandle)0x1 };
    // Mock ChannelResumeConcurrency to return internal error
    MOCKER_CPP(&ChannelProcess::ChannelResumeConcurrency, HcclResult(const ChannelHandle*, uint32_t))
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_E_INTERNAL));

    auto ret = ChannelProcess::ChannelResume(list, 1);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(ChannelProcessTest, Ut_ChannelResume_NullList_Returns_E_PARA) {
    auto ret = ChannelProcess::ChannelResume(nullptr, 1);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(ChannelProcessTest, Ut_ChannelResume_When_GetStatusReturnsTimeout_Returns_Timeout) {
    ChannelHandle list[1] = { (ChannelHandle)0x1 };

    // Mock ChannelResumeConcurrency to succeed
    MOCKER_CPP(&ChannelProcess::ChannelResumeConcurrency, HcclResult(const ChannelHandle*, uint32_t))
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_SUCCESS));

    // Mock ChannelGetStatus to return timeout error
    MOCKER_CPP(&ChannelProcess::ChannelGetStatus, HcclResult(const ChannelHandle*, uint32_t, int32_t*))
        .stubs()
        .with(any(), any(), any())
        .will(returnValue(HCCL_E_TIMEOUT));

    auto ret = ChannelProcess::ChannelResume(list, 1);
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);
}

TEST_F(ChannelProcessTest, Ut_ChannelUpdateKernelLaunch_NullDeviceHandles_Returns_E_PARA) {
    ChannelHandle* deviceHandles = nullptr;
    ChannelHandle hostHandlesArr[1] = { (ChannelHandle)0x1 };
    // listNum > 0 but deviceHandles is null -> expect parameter error
    auto ret = ChannelProcess::ChannelUpdateKernelLaunch(deviceHandles, hostHandlesArr, 1, "test", nullptr);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(ChannelProcessTest, Ut_ChannelUpdateKernelLaunch_NullHostHandles_Returns_E_PARA) {
    ChannelHandle deviceHandlesArr[1] = { (ChannelHandle)0x1 };
    ChannelHandle* hostHandles = nullptr;
    // listNum > 0 but hostHandles is null -> expect parameter error
    auto ret = ChannelProcess::ChannelUpdateKernelLaunch(deviceHandlesArr, hostHandles, 1, "test", nullptr);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(ChannelProcessTest, Ut_ChannelUpdateKernelLaunch_ZeroList_NoOp_Returns_SUCCESS) {
    ChannelHandle deviceHandlesArr[1] = { (ChannelHandle)0x1 };
    ChannelHandle hostHandlesArr[1] = { (ChannelHandle)0x1 };
    // listNum == 0 should be a no-op and return success
    auto ret = ChannelProcess::ChannelUpdateKernelLaunch(deviceHandlesArr, hostHandlesArr, 0, "test", nullptr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
