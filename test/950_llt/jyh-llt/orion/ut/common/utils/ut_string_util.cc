#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <mockcpp/mokc.h>
#include "string_util.h"

using namespace std;

class StringUtilTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "StringUtilTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "StringUtilTest TearDown" << std::endl;
    }
    virtual void SetUp()
    {
        std::cout << "A Test case in StringUtilTest SetUp" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in StringUtilTest TearDown" << std::endl;
    }
};

TEST_F(StringUtilTest, snprintf_s_throw_test) {
	
	std::string  str = "";
    for(int i = 0; i < 8500; ++i){
        str += "a";
    }
    std::cout << "ut string size is: " << str.size() << std::endl;
    Hccl::StringFormat(str.c_str());
}


