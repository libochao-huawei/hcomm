#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <mockcpp/MockObject.h>
#define private public
#include "recover_info.h"
#include "invalid_params_exception.h"
#include "internal_exception.h"
#undef private
 
using namespace Hccl;
 
class RecoverInfoTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "MemTransportManager tests set up." << std::endl;
    }
 
    static void TearDownTestCase()
    {
        std::cout << "MemTransportManager tests tear down." << std::endl;
    }
 
    virtual void SetUp()
    {
        std::cout << "A Test case in MemTransportManager SetUP" << std::endl;
    }
 
    virtual void TearDown()
    {
        std::cout << "A Test case in MemTransportManager TearDown" << std::endl;
        GlobalMockObject::verify();
    }
};
 
TEST_F(RecoverInfoTest, RecoverInfo_describe)
{
    RecoverInfoData recoverInfoData      = RecoverInfoData(0, 0, 0);
    RecoverInfo recoverInfo              = RecoverInfo(recoverInfoData,0);
    recoverInfo.Describe();
}
 
TEST_F(RecoverInfoTest, RecoverInfo_get_unique_id)
{
    RecoverInfoData recoverInfoData      = RecoverInfoData(0, 0, 0);
    RecoverInfo recoverInfo              = RecoverInfo(recoverInfoData,0);
    recoverInfo.GetUniqueId();
}
 
TEST_F(RecoverInfoTest, RecoverInfo_set_crc_value)
{
    RecoverInfoData recoverInfoData      = RecoverInfoData(0, 0, 0);
    RecoverInfo recoverInfo              = RecoverInfo(recoverInfoData,0);
    recoverInfo.SetCrcValue(1);
}
 
TEST_F(RecoverInfoTest, RecoverInfo_check_1)
{
    RecoverInfoData recoverInfoData          = RecoverInfoData(0, 0, 0);
    RecoverInfo recoverInfo                  = RecoverInfo(recoverInfoData,0);
    RecoverInfoData otherRecoverInfoData     = RecoverInfoData{0, 0, 0};
    RecoverInfo otherRecoverInfo             = RecoverInfo(otherRecoverInfoData,1);
    recoverInfo.Check(otherRecoverInfo.GetUniqueId());
}
 
TEST_F(RecoverInfoTest, RecoverInfo_check_2)
{
    RecoverInfoData recoverInfoData          = RecoverInfoData(0, 1, 0);
    RecoverInfo recoverInfo                  = RecoverInfo(recoverInfoData,0);
    RecoverInfoData otherRecoverInfoData     = RecoverInfoData{0, 0, 0};
    RecoverInfo otherRecoverInfo             = RecoverInfo(otherRecoverInfoData,1);
    EXPECT_THROW(recoverInfo.Check(otherRecoverInfo.GetUniqueId()), InvalidParamsException);
}
 
TEST_F(RecoverInfoTest, RecoverInfo_check_3)
{
    RecoverInfoData recoverInfoData          = RecoverInfoData(0, 0, 1);
    RecoverInfo recoverInfo                  = RecoverInfo(recoverInfoData,0);
    RecoverInfoData otherRecoverInfoData     = RecoverInfoData{0, 0, 0};
    RecoverInfo otherRecoverInfo             = RecoverInfo(otherRecoverInfoData,1);
    EXPECT_THROW(recoverInfo.Check(otherRecoverInfo.GetUniqueId()), InvalidParamsException);
}
 
TEST_F(RecoverInfoTest, RecoverInfo_check_4)
{
    RecoverInfoData recoverInfoData          = RecoverInfoData(1, 0, 0);
    RecoverInfo recoverInfo                  = RecoverInfo(recoverInfoData,0);
    RecoverInfoData otherRecoverInfoData     = RecoverInfoData{0, 0, 0};
    RecoverInfo otherRecoverInfo             = RecoverInfo(otherRecoverInfoData,1);
    EXPECT_THROW(recoverInfo.Check(otherRecoverInfo.GetUniqueId()), InvalidParamsException);
}

TEST_F(RecoverInfoTest, RecoverInfo_1)
{
    RecoverInfoData recoverInfoData          = RecoverInfoData(1, 0, 0);
    RecoverInfo recoverInfo                  = RecoverInfo(recoverInfoData,0);
    RecoverInfo(recoverInfo.GetUniqueId());
}
