#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "host_device_sync_notify_manager.h"

using namespace Hccl;

class HostDeviceSyncNotifyManagerTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "HostDeviceSyncNotifyManagerTest SetUP" << std::endl;
        u32 fakeDevPhyId = 1;
        u64 fakeNotifyHandleAddr = 100;
        u32 fakeNotifyId = 1;
        u64 fakeOffset = 200;
        char fakeName[65] = "testRtsNotify";
        MOCKER(HrtGetDevice).stubs().will(returnValue(0));
        MOCKER(HrtNotifyCreate).stubs().will(returnValue((void *)(fakeNotifyHandleAddr)));
        MOCKER(HrtNotifyCreateWithFlag).stubs().will(returnValue((void *)(fakeNotifyHandleAddr)));
        MOCKER(HrtGetNotifyID).stubs().will(returnValue(fakeNotifyId));
        MOCKER(HrtGetDevicePhyIdByIndex).stubs().will(returnValue(fakeDevPhyId));
        MOCKER(HrtIpcSetNotifyName).stubs().with(any(), outBoundP(fakeName, sizeof(fakeName)), any());
        MOCKER(HrtNotifyGetOffset).stubs().will(returnValue(fakeOffset));
        MOCKER(HrtGetDeviceType).stubs().will(returnValue(DevType(DevType::DEV_TYPE_910_95)));
    }

    static void TearDownTestCase()
    {
        std::cout << "HostDeviceSyncNotifyManagerTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in HostDeviceSyncNotifyManagerTest SetUp" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in HostDeviceSyncNotifyManagerTest TearDown" << std::endl;
    }
};

TEST_F(HostDeviceSyncNotifyManagerTest, Ut_GetAiCpuCntNotifys_When_Repeat_Expect_OK)
{
    HostDeviceSyncNotifyManager manager = HostDeviceSyncNotifyManager();
    u8 cnt = 10;
    void* aicpuNotify[10];
    manager.GetMc2AiCpuNotifys(cnt, aicpuNotify);
    EXPECT_EQ(10, manager.mc2AicpuNotifys_.size());
    cnt = 8;
    manager.GetMc2AiCpuNotifys(cnt, aicpuNotify);
    EXPECT_EQ(10, manager.mc2AicpuNotifys_.size());
}