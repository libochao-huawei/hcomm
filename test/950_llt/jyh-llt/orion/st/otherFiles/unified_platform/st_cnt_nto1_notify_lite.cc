#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "cnt_nto1_notify_lite.h"
#include "binary_stream.h"
#include "string_util.h"
#include "log.h"

using namespace Hccl;

class CntNto1NotifyLiteTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "CntNto1NotifyLiteTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "CntNto1NotifyLiteTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in CntNto1NotifyLiteTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in CntNto1NotifyLiteTest TearDown" << std::endl;
    }
    u32 fakeNotifyId = 1;
    u32 fakeDevPhyId = 2;
};

TEST_F(CntNto1NotifyLiteTest, cnt_nto1_notify_lite_give_uniqueId)
{
    BinaryStream binaryStream;
    binaryStream << fakeNotifyId;
    binaryStream << fakeDevPhyId;
    std::vector<char> result;
    binaryStream.Dump(result);

    CntNto1NotifyLite lite(result);
    EXPECT_EQ(fakeNotifyId, lite.GetId());
    EXPECT_EQ(fakeDevPhyId, lite.GetDevPhyId());
}