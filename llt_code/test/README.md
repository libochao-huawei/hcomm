# HCCL LLT

## 概述

HCCL LLT（Low Level Test）是 HCCL 的测试框架，旨在系统化验证 HCCL 各层级组件的功能完整性与性能稳定性。LLT 涵盖了 HCCL 的算法层、框架层、平台层、对外接口层多个部分，通过全面的测试用例，保障 HCCL 在各种业务场景下的可靠性和高效性。

## 目录结构

```bash
test/
├── CMakeLists.txt
├── README.md
├── llt                             # 原有测试用例，待分解
├── algorithm                       # 算法层测试用例
├── framework                       # 框架层测试用例
├── platform                        # 平台层测试用例
├── hccl_api_multi_thread_test      # 对外接口API多线程测试用例
├── hccl_api_single_thread_test     # 对外接口API单线程测试用例
└── stub                            # 桩函数
```

## 编译与运行

> 注意：执行编译命令之前，请参见[“环境准备”](../docs/build.md#环境准备)准备好相关环境。

在仓库根目录下执行如下命令：

```bash
# 运行所有测试用例
bash build.sh --nlohmann_path /home/nlohmann_json/include --test

# 运行open_hccl_test测试用例
bash build.sh --nlohmann_path /home/nlohmann_json/include --open_hccl_test

# 运行executor_hccl_test测试用例
bash build.sh --nlohmann_path /home/nlohmann_json/include --executor_hccl_test

# 运行executor_reduce_hccl_test测试用例
bash build.sh --nlohmann_path /home/nlohmann_json/include --executor_reduce_hccl_test

# 运行executor_pipeline_hccl_test测试用例
bash build.sh --nlohmann_path /home/nlohmann_json/include --executor_pipeline_hccl_test

# 运行hccl_utest_api_single_thread测试用例
bash build.sh --nlohmann_path /home/nlohmann_json/include --hccl_utest_api_single_thread
```

## 可执行文件位置

所有可执行文件默认输出至 `build/test` 目录，可通过修改 `CMakeLists.txt` 调整路径：

```cmake
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${HCCL_OPEN_CODE_ROOT}/build/test)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${HCCL_OPEN_CODE_ROOT}/build/test)
```

## 如何编写测试用例

> HCCL LLT 测试用例采用 Google Test 框架实现，详细编写规范请参阅 [Google Test 用户指南][1]。

[1]: https://google.github.io/googletest/

1. 根据测试对象选择对应目录，如 `test/algorithm` 或 `test/framework`
2. 基于 Google Test 创建新测试类
3. 更新对应目录的 `CMakeLists.txt`，添加测试入口

测试代码示例：

```c++
#include "gtest/gtest.h"

TEST(MyTestClass, MyTestCase) {
    // 实现断言逻辑
    EXPECT_EQ(actual_value, expected_value);
}
```

## 如何运行特定的测试用例

若想要单独运行某些测试用例，可参考 [Google Test 用户指南][2]，执行用例时添加 `--gtest_filter` 参数即可。

[2]: https://google.github.io/googletest/advanced.html#running-a-subset-of-the-tests

以 `hccl_utest_api_single_thread` 为例：

```bash
# hccl_utest_api_single_thread 依赖桩函数下的二进制文件，因此需要将路径加入环境变量中，其他测试用例暂不需要
export LD_LIBRARY_PATH="test/stub/framework_stub/workspace/fwkacllib/lib64:${LD_LIBRARY_PATH}"

# 只执行 HcclCommInitRootInfoTest 测试类的测试用例
./build/test/hccl_utest_api_single_thread --gtest_filter=HcclCommInitRootInfoTest.*
```
