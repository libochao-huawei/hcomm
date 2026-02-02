extern "C" {
#include "ut_dispatch.h"
#include "tc_hccp.h"
}

#include "gtest/gtest.h"
#include <stdio.h>


using namespace std;

class Hccp : public testing::Test
{
protected:
   static void SetUpTestCase()
    {
        std::cout << "\033[36m--RoCE Hccp SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--RoCE Hccp TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
    }
    virtual void TearDown()
    {
    }
};

TEST_M(Hccp, tc_normal);
TEST_M(Hccp, tc_abnormal);
