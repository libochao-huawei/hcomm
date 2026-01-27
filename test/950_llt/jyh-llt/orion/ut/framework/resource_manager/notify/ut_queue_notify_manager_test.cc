#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "queue_notify_manager.h"
#include "local_notify.h"
#include "communicator_impl.h"

using namespace Hccl;

class QueueNotifyManagerTest : public testing::Test {
protected:
    static void SetUpTestCase() {
        std::cout << "QueueNotifyManagerTest SetUP" << std::endl;
    }

    static void TearDownTestCase() {
        std::cout << "QueueNotifyManagerTest TearDown" << std::endl;
    }

    virtual void SetUp() {
        MOCKER(HrtGetDeviceType).stubs().will(returnValue(DevType(DevType::DEV_TYPE_910A2)));
        std::cout << "A Test case in QueueNotifyManagerTest SetUP" << std::endl;
    }

    virtual void TearDown () {
        GlobalMockObject::verify();

        std::cout << "A Test case in QueueNotifyManagerTest TearDown" << std::endl;
    }
};

TEST_F(QueueNotifyManagerTest, applyfor_return_ok)
{
    CommunicatorImpl comm;
    QueueNotifyManager queueNotifyManager(comm);
    //Given
    MOCKER(HrtGetDevice)
            .stubs()
            .will(returnValue(1));
    MOCKER(HrtNotifyCreate)
            .stubs()
            .will(returnValue((void*)(0)));
    MOCKER(HrtIpcSetNotifyName)
            .stubs();
    MOCKER(HrtGetNotifyID)
            .stubs()
            .will(returnValue(1));
    MOCKER(HrtNotifyGetAddr)
            .stubs()
            .will(returnValue((u64)0));
    MOCKER(HrtNotifyGetOffset)
            .stubs()
            .will(returnValue(1));
    MOCKER(HrtGetSocVer)
            .stubs();

    QId fakePostQid = 1;
    QId fakeWaitQid = 2;
    u32 fakeCount = 3;

    //When
    queueNotifyManager.ApplyFor(fakePostQid, fakeWaitQid, fakeCount);

    //When
    queueNotifyManager.ApplyFor(fakePostQid, fakeWaitQid, fakeCount); // duplicate apply

}

TEST_F(QueueNotifyManagerTest, release_return_ok)
{
    CommunicatorImpl comm;
    QueueNotifyManager queueNotifyManager(comm);
    //Given
    MOCKER(HrtGetDevice)
            .stubs()
            .will(returnValue(1));
    MOCKER(HrtNotifyCreate)
            .stubs()
            .will(returnValue((void*)(0)));
    MOCKER(HrtIpcSetNotifyName)
            .stubs();
    MOCKER(HrtGetNotifyID)
            .stubs()
            .will(returnValue(1));
    MOCKER(HrtNotifyGetAddr)
            .stubs()
            .will(returnValue((u64)0));
    MOCKER(HrtNotifyGetOffset)
            .stubs()
            .will(returnValue(1));
    MOCKER(HrtGetSocVer)
            .stubs();

    QId fakePostQid = 1;
    QId fakeWaitQid = 2;
    u32 fakeCount = 3;
    queueNotifyManager.ApplyFor(fakePostQid, fakeWaitQid, fakeCount);
    
    //When
    auto result = queueNotifyManager.Release(fakePostQid, fakeWaitQid, fakeCount);

    //Then
    EXPECT_EQ(true, result);
}

TEST_F(QueueNotifyManagerTest, destroy_return_nok)
{
    CommunicatorImpl comm;
    QueueNotifyManager queueNotifyManager(comm);
    //Given
    MOCKER(HrtGetDevice)
            .stubs()
            .will(returnValue(1));
    MOCKER(HrtNotifyCreate)
            .stubs()
            .will(returnValue((void*)(0)));
    MOCKER(HrtIpcSetNotifyName)
            .stubs();
    MOCKER(HrtGetNotifyID)
            .stubs()
            .will(returnValue(1));
    MOCKER(HrtNotifyGetAddr)
            .stubs()
            .will(returnValue((u64)0));
    MOCKER(HrtNotifyGetOffset)
            .stubs()
            .will(returnValue(1));
    MOCKER(HrtGetSocVer)
            .stubs();

    QId fakePostQid = 1;
    QId fakeWaitQid = 2;
    u32 fakeCount = 3;
    queueNotifyManager.ApplyFor(fakePostQid, fakeWaitQid, fakeCount);
    
    QId fakePostQid1 = 1;
    QId fakeWaitQid1 = 2;
    u32 fakeCount1 = 3;
    queueNotifyManager.ApplyFor(fakePostQid1, fakeWaitQid1, fakeCount1);
    
    //When
    auto result = queueNotifyManager.Destroy();

    //Then
    EXPECT_EQ(true, result);
}

TEST_F(QueueNotifyManagerTest, release_test)
{
    CommunicatorImpl comm;
    QueueNotifyManager queueNotifyManager(comm);
    //Given
        MOCKER(HrtGetDevice)
                .stubs()
                .will(returnValue(1));
        MOCKER(HrtNotifyCreate)
                .stubs()
                .will(returnValue((void*)(0)));
        MOCKER(HrtIpcSetNotifyName)
                .stubs();
        MOCKER(HrtGetNotifyID)
                .stubs()
                .will(returnValue(1));
        MOCKER(HrtNotifyGetAddr)
                .stubs()
                .will(returnValue((u64)0));
        MOCKER(HrtNotifyGetOffset)
                .stubs()
                .will(returnValue(1));
        MOCKER(HrtGetSocVer)
                .stubs();
        auto result = queueNotifyManager.Release(999,999,399);
}