#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#define private public
#include "rts_1ton_cnt_notify.h"
#undef private

using namespace Hccl;

class Rts1ToNCntNotifyTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "Rts1ToNCntNotifyTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "Rts1ToNCntNotifyTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in Rts1ToNCntNotifyTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in Rts1ToNCntNotifyTest TearDown" << std::endl;
    }
    u64 fakeNotifyHandleAddr = 100;
    u32 fakeNotifyId = 1;
    u32 fakeDevPhyId = 2;
};

TEST_F(Rts1ToNCntNotifyTest, rts1toncntnotify_getuniqueid_test)
{
    MOCKER(HrtGetDeviceType).stubs().will(returnValue(DevType::DEV_TYPE_910A2));
    MOCKER(HrtGetDevice).stubs().will(returnValue(0));
    MOCKER(HrtGetDevicePhyIdByIndex).stubs().will(returnValue(1));
    MOCKER(HrtCntNotifyCreate).stubs().will(returnValue((void *)(fakeNotifyHandleAddr)));
    MOCKER(HrtGetCntNotifyId).stubs().will(returnValue(fakeNotifyId));

    Rts1ToNCntNotify rts1ToNCntNotify;
    rts1ToNCntNotify.id = fakeNotifyId;
    rts1ToNCntNotify.devPhyId = fakeDevPhyId;

    BinaryStream binaryStream;
    binaryStream << rts1ToNCntNotify.id;
    binaryStream << rts1ToNCntNotify.devPhyId;
    std::vector<char> res;
    binaryStream.Dump(res);

    EXPECT_EQ(rts1ToNCntNotify.GetUniqueId(), res);
}