#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#define private public
#include "rts_cnt_notify.h"
#undef private

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
};

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