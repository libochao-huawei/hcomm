#include <iostream>
#include "gtest/gtest.h"

GTEST_API_ int main(int argc, char **argv)
{
    // testing::GTEST_FLAG(filter) = "CollServiceAiCpuImplTest.*";
    
    std::cout << "Start to run all unit tests for hccl v2." << std::endl;
    setenv("LD_LIBRARY_PATH", "./llt/ace/comop/hccl/stub/workspace/fwkacllib/lib64", 1);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}