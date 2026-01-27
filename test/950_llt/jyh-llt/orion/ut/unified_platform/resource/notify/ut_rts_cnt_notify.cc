#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "rts_cnt_notify.h"

using namespace Hccl;

class RtsCntNotifyTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "RtsCntNotifyTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "RtsCntNotifyTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in RtsCntNotifyTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in RtsCntNotifyTest TearDown" << std::endl;
    }
    u64 fakeNotifyHandleAddr = 100;
    u32 fakeNotifyId = 1;
    u32 fakeDevPhyId = 2;
    u64 fakeOffset = 200;
    u64 fakeAddress = 300;
    u32 fakePid = 100;
    char fakeName[65] = "testRtsNotify";
};

TEST_F(RtsCntNotifyTest, rtscntnotify_construct_ok)
{
    // Given
    MOCKER(HrtGetDeviceType).stubs().will(returnValue(DevType(DevType::DEV_TYPE_910A2)));
    MOCKER(HrtGetDevice).stubs().will(returnValue(0));
    MOCKER(HrtCntNotifyCreate).stubs().will(returnValue((void *)(fakeNotifyHandleAddr)));
    MOCKER(HrtGetCntNotifyId).stubs().will(returnValue(fakeNotifyId));

    // When
    RtsCntNotify rtsCntNotify;
    std::string des = rtsCntNotify.Describe();
    EXPECT_NE(0, des.size());
    // Then
    EXPECT_EQ(fakeNotifyId, rtsCntNotify.GetId());
}

TEST_F(RtsCntNotifyTest, rtscntnotify_postbits_submit_test)
{
    // Given
    MOCKER(HrtGetDeviceType).stubs().will(returnValue(DevType::DEV_TYPE_910A2));
    MOCKER(HrtGetDevice).stubs().will(returnValue(0));
    MOCKER(HrtGetDevicePhyIdByIndex).stubs().will(returnValue(1));
    MOCKER(HrtCntNotifyCreate).stubs().will(returnValue((void *)(fakeNotifyHandleAddr)));
    MOCKER(HrtGetCntNotifyId).stubs().will(returnValue(fakeNotifyId));

    // When
    RtsCntNotify rtsCntNotify;

    // Then
    u32 bitVal = 1;
    Stream stream;
    rtsCntNotify.PostBits(bitVal, stream);
}

TEST_F(RtsCntNotifyTest, rtscntnotify_waitvalue_submit_test)
{
    // Given
    MOCKER(HrtGetDeviceType).stubs().will(returnValue(DevType::DEV_TYPE_910A2));
    MOCKER(HrtGetDevice).stubs().will(returnValue(0));
    MOCKER(HrtGetDevicePhyIdByIndex).stubs().will(returnValue(1));
    MOCKER(HrtCntNotifyCreate).stubs().will(returnValue((void *)(fakeNotifyHandleAddr)));
    MOCKER(HrtGetCntNotifyId).stubs().will(returnValue(fakeNotifyId));

    // When
    RtsCntNotify rtsCntNotify;

    // Then
    u32 value = 1;
    Stream stream;
    rtsCntNotify.WaitValue(value, 100, stream);
}

TEST_F(RtsCntNotifyTest, rtscntnotify_getuniqueid_test)
{
    MOCKER(HrtGetDeviceType).stubs().will(returnValue(DevType::DEV_TYPE_910A2));
    MOCKER(HrtGetDevice).stubs().will(returnValue(0));
    MOCKER(HrtGetDevicePhyIdByIndex).stubs().will(returnValue(1));
    MOCKER(HrtCntNotifyCreate).stubs().will(returnValue((void *)(fakeNotifyHandleAddr)));
    MOCKER(HrtGetCntNotifyId).stubs().will(returnValue(fakeNotifyId));

    RtsCntNotify rtsCntNotify;
    rtsCntNotify.id = fakeNotifyId;
    rtsCntNotify.devPhyId = fakeDevPhyId;

    BinaryStream binaryStream;
    binaryStream << rtsCntNotify.id;
    binaryStream << rtsCntNotify.devPhyId;
    std::vector<char> res;
    binaryStream.Dump(res);

    EXPECT_EQ(rtsCntNotify.GetUniqueId(), res);
}