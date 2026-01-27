#include "gtest/gtest.h"

GTEST_API_ int main(int argc, char **argv) {
    // testcase调试代码，只跑特定的用例
    // testing::GTEST_FLAG(filter) = "CcuRevampParallelTest.ccu_mem_conflict_for_single_async_node";
    testing::InitGoogleTest(&argc, argv);
    // 用来控制各个LLT任务的topoPath.json的名字，不能和其他任务的名字一样，可能会冲突
    setenv("TOPO_PATH_NAME", "function_ut_testcase", 1);
    return RUN_ALL_TESTS();
}