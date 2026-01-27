#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "cnt_1ton_notify_lite.h"
#include "binary_stream.h"
#include "string_util.h"
#include "log.h"

using namespace Hccl;

class Cnt1toNNotifyLiteTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "Cnt1toNNotifyLiteTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "Cnt1toNNotifyLiteTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in Cnt1toNNotifyLiteTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in Cnt1toNNotifyLiteTest TearDown" << std::endl;
    }
    u32 fakeNotifyId = 1;
    u32 fakeDevPhyId = 2;
};

TEST_F(Cnt1toNNotifyLiteTest, cnt_1ton_notify_lite_give_uniqueId)
{
    BinaryStream binaryStream;
    binaryStream << fakeNotifyId;
    binaryStream << fakeDevPhyId;
    std::vector<char> result;
    binaryStream.Dump(result);

    Cnt1tonNotifyLite lite(result);
    EXPECT_EQ(fakeNotifyId, lite.GetId());
    EXPECT_EQ(fakeDevPhyId, lite.GetDevPhyId());
}