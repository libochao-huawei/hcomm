# hccl llt

## 概述

HCCL Low Level Test（LLT）是HCCL（华为集合通信库）中的一个测试层，主要用于对HCCL的不同组件进行测试和验证，确保其功能的正确性和性能的稳定性。LLT涵盖了HCCL的算法层、框架层、平台层、对外接口层多个部分，通过全面的测试用例，保障HCCL在各种场景下的可靠性和高效性。

## 目录结构说明

```shell
test/
├── CMakeLists.txt
├── README.md
├── llt # 原有测试用例，待分解
├── algorithm # 算法层测试用例
│   ├── CMakeLists.txt
│   ├── README.md
│   ├── executor_testcase_generalization
│   ├── testcase
│   ├── executor_reduce_testcase_generalization
│   └── executor_alltoall_A3_pipeline_testcase
├── framework # 框架层测试用例
│   ├── CMakeLists.txt
│   ├── README.md
│   ├── single_test
│   └── mc2_test
├── platform # 平台层测试用例
│   ├── CMakeLists.txt
│   └── README.md
├── hccl_api_multi_thread_test # 对外接口API多线程测试用例
│   ├── CMakeLists.txt
│   └── README.md
├── hccl_api_single_thread_test # 对外接口API单线程测试用例
│   ├── CMakeLists.txt
│   └── README.md
└── stub # 桩函数
    ├── CMakeLists.txt
    ├── README.md
    ├── framework_stub
    ├── algorithm_stub
    └── platform_stub
```
## 编译命令

- 注意执行编译命令之前，要做好([环境准备](../README.md#环境准备))工作。
- 在hccl代码仓根目录执行下面的命令。

```shell
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
测试命令生成的可执行程序，生成到了根目录下面build/test的文件夹中。
可以通过修改本目录下CMakeLists.txt文件中的这两行命令来控制生成位置。
```shell
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${HCCL_OPEN_CODE_ROOT}/build/test)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${HCCL_OPEN_CODE_ROOT}/build/test)
```

## 如何加入新的测试用例

测试用例是基于gtest框架进行编写的，之前可以先了解下gtest测试框架([gtest documentation](https://google.github.io/googletest/))。

## 如何指定运行特定的测试用例

如果想要单独运行某些测试用例，可以直接参考gtest文档中的([Running a Subset of the Tests](https://google.github.io/googletest/advanced.html#running-a-subset-of-the-tests))。
因为在运行build.sh时已经设置好了环境变量并生成了可执行文件，因此在未修改代码时，下次运行可以直接执行build/test文件夹下对应的可执行文件。以hccl_utest_api_single_thread为例：
```shell
    # 这会在build/test目录下生成hccl_utest_api_single_thread文件，并执行一次hccl_utest_api_single_thread的测试用例
    bash build.sh --nlohmann_path /home/nlohmann_json/include --hccl_utest_api_single_thread

    # hccl_utest_api_single_thread依赖桩函数下的二进制文件，因此需要将路径加入环境变量中，其他测试用例暂不需要
    # 如果想要执行特定用例，需按照gtest的方法，例如只执行HcclCommInitRootInfoTest的测试用例
    export LD_LIBRARY_PATH="test/stub/framework_stub/workspace/fwkacllib/lib64:${LD_LIBRARY_PATH}" && ./build/test/hccl_utest_api_single_thread --gtest_filter=HcclCommInitRootInfoTest.*
```

