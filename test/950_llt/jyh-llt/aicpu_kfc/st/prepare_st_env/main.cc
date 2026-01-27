
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "gtest/gtest.h"

GTEST_API_ int main(int argc, char **argv) 
{
  printf("Running main() from gtest_main.cc\n");
  //testing::GTEST_FLAG(filter) = "HcomKernelInfoTest.st_LoadTask_comm";
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

using namespace std;
class StPrepareEnv : public testing::Test
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

TEST_F(StPrepareEnv, hccl_prepare_st_env)
{
    printf("prepare hccl st llt env start\n");
    // 忽略文件不存在，无权限等异常，不检查返回值
    system("find /dev/shm/ -name 'hccl*' | xargs -i rm {}");    
    printf("prepare hccl st llt env finished\n");
    return;
}
