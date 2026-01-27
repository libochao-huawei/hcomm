#include <iostream>
#include "gtest/gtest.h"

GTEST_API_ int main(int argc, char **argv)
{
    //testing::GTEST_FLAG(filter) = "CollServiceDeviceModeTest.test_coll_service_device_mode_resume";
    // testing::GTEST_FLAG(filter) = "LiteResTest.AddFabricInfo";
    std::cout << "Start to run all subsystem tests for hccl v2." << std::endl;
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}