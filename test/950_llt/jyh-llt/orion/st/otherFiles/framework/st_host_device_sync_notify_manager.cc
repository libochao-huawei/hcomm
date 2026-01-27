#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#define private public
#include "host_device_sync_notify_manager.h"
#undef private

using namespace Hccl;

class HostDeviceSyncNotifyManagerTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "HostDeviceSyncNotifyManagerTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "HostDeviceSyncNotifyManagerTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        MOCKER(HrtGetDevice).stubs().will(returnValue(0));
        MOCKER(HrtNotifyCreate).stubs().will(returnValue((void *)(fakeNotifyHandleAddr)));
        MOCKER(HrtGetNotifyID).stubs().will(returnValue(fakeNotifyId));
        MOCKER(HrtGetDevicePhyIdByIndex).stubs().will(returnValue(1));
        std::cout << "A Test case in HostDeviceSyncNotifyManagerTest SetUp" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in HostDeviceSyncNotifyManagerTest TearDown" << std::endl;
    }

private:
    u64 fakeNotifyHandleAddr = 100;
    u32 fakeNotifyId         = 1;
};