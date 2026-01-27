#include <stdio.h>
#include <errno.h>
#include "gtest/gtest.h"

GTEST_API_ int main(int argc, char **argv)
{
  printf("Running main() from gtest_main.cc\n");
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

using namespace std;
class UtPrepareEnv : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
    }
    static void TearDownTestCase()
    {
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
    }
    virtual void TearDown()
    {
    }
};

TEST_F(UtPrepareEnv, hccl_insight_package)
{
    printf("hccl_insight binary package start\n");
    printf("hccl_insight binary package finished\n");
    return;
}