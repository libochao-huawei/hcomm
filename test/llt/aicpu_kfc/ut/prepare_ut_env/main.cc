
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
        //std::cout << "CleanEnv SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        //std::cout << "CleanEnv TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        //std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        //std::cout << "A Test TearDown" << std::endl;
    }
};

TEST_F(UtPrepareEnv, aicpu_kfc_prepare_ut_env)
{
    printf("prepare aicpu_kfc ut llt env start\n");
    // 忽略文件不存在，无权限等异常，不检查返回值
    system("find /dev/shm/ -name 'hccl*' | xargs -i rm {}");
    printf("prepare aicpu_kfc ut llt env finished\n");
    return;
}