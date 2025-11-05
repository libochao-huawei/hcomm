# HCCL FWK 测试框架用户指南

本文档介绍了如何使用和扩展基于 `HcclFwkTest` 的单文件测试框架，主要以操作步骤和代码示例的形式，覆盖构建、运行、常见调试方法，以及如何`添加新的资源创建`和`组合测试用例`。

## 概述

该测试框架用于验证 HCCL 资源的创建与组合（`HcclCommInitClusterInfoConfig`、`HcclAllocThreadRes`、`HcclChannelCreate`、`HcommWriteOnThread ` 等）。默认链接真实 HCCL/ACL 库（`MOCK_HCCL=0`）；未来逐步支持**Mock 模式**（`MOCK_HCCL=1`，可在没有 NPU 环境下本地跑通流程）。

* 核心类：`HcclFwkTest`。
* 主要方法：

  * `init()`：初始化 ACL 与 HCCL 通信域。
  * `setup_channel_pair()` ：分配 buffer、构造 ChannelDesc、调用 `HcclChannelCreate`接口。
  * `do_sample_write()`：在 threadResource 与 channel 可用时，执行一次写作验证。
  * `cleanup()`：释放 buffer、channel、thread，销毁 comm。


## 准备与依赖

* C++14 编译器（g++/clang）。
* 如果运行在 NPU 节点，需要 HCCL/ACL SDK（头文件与库文件）。
* 若使用 ranktable 文件，请准备一个 JSON 格式的 rank table，示例如下。

  示例 `rank_table.json`（可直接作为 `--cluster_info` 或通过环境变量 `RANK_TABLE_FILE` 指定）：

  ```json
  {
    "status":"completed",
    "version":"1.0",
    "server_count":"1",
    "server_list":[
      {
        "server_id":"SERVER_ID_SV1",
        "device":[
          {"device_id":"0","device_ip":"192.168.1.8","rank_id":"0"},
          {"device_id":"1","device_ip":"192.168.1.9","rank_id":"1"}
        ]
      }
    ]
  }
  ```

## 构建

1. 配置环境变量。

   环境变量配置示例如下：

   ```bash
   # 设置 CANN 环境变量，以root用户默认安装路径为例
   source /usr/local/Ascend/ascend-toolkit/set_env.sh
   ```

2. 编译。
  
   在当前代码仓根目录下执行如下命令：

   ```bash
   # 真实硬件环境执行
   bash build.sh --fwk_test_hlt
   # 模拟环境执行
   bash build.sh --fwk_test_hlt_mock
   ```
   
   其他编译方式可以参考[其他编译方式参考](#其他编译方式参考)。

3. 触发执行。

   ```bash
   # 查看测试用例列表
   ./build/hccl_fwk_test --cluster_info test/hlt/ranktable.json --rank 0 --list

   # 执行指定测试用例（allocthread为例）
   ./build/hccl_fwk_test --cluster_info test/hlt/ranktable.json --rank 0 --test allocthread
   ```

## 运行

* **单个进程运行示例**

  ```bash
  ./hccl_fwk_test --cluster_info '{"server_list":[{"device":[{"device_id":"0","device_ip":"127.0.0.1","rank_id":"0"}]}]}' --rank 0 --peers 2 --size 4096
  ```

  命令行参数（常用）含义如下：

  * `--cluster_info <path_or_json>`：rank table 文件路径或 JSON 字符串。
  * `--rank N`：本进程 rank id（0、1、...）。
  * `--peers P`：要在 `setup_channel_pair` 中创建的 peer/channel 数。
  * `--size B`：单消息/缓冲区大小（bytes）。

  部分参数也可以使用环境变量作为备选，环境变量如下所示：

  * `RANK_TABLE_FILE`：等同参数`--cluster_info`。
  * `RANK_ID`：等同参数`--rank`。

* **多进程运行示例**

  以两进程为例，在每个 rank 的进程上分别运行如下命令（由 launcher/脚本启动）：

  ```bash
  # 进程1
  export RANK_TABLE_FILE=../rank_table.json
  export RANK_ID=0
  ./hccl_fwk_test --cluster_info $RANK_TABLE_FILE --rank 0 --peers 2 --size 4096
  
  # 进程2
  export RANK_TABLE_FILE=../rank_table.json
  export RANK_ID=1
  ./hccl_fwk_test --cluster_info $RANK_TABLE_FILE --rank 1 --peers 2 --size 4096
  ```

## 输出结果

框架会打印关键步骤与状态，例如：

* `[ACL] Initialized`（仅真实环境）
* `[HCCL] Comm initialized`
* `[HCCL] Created N channel(s)`

**判断标准**

* 所有关键步骤无错误返回则代表 `success`。

## 常见问题与调试建议

* **comm init 失败**：检查 `cluster_info`（文件路径或 JSON）是否正确；rank 是否在 rank table 范围内；真实环境下确认 HCCL/ACL 库路径与 ABI 匹配。
* **device 设置失败（ACL）**：确认 `aclInit` 成功并且 `aclrtSetDevice(rank)` 的 device id 在节点上存在。
* **Channel 创建失败**：不同 HCCL SDK 的 `ChannelDesc` 字段可能不同。请根据真实头文件调整 `setup_channel_pair()` 中的填充逻辑（remote address / memHandle / flags）。
* **内存分配失败**：在真实环境优先使用 `aclrtMalloc`，若使用 host fallback，请确保对齐与大小合理。
* **多进程协调问题**：真实测试通常由集群 launcher 启动，确认各进程的 `--cluster_info` / `RANK_ID` 与 rank table 一致。
* **若要调试 mock**：`Mock` 模式会打印模拟行为，方便定位参数解析、流程顺序问题。

## 如何新增测试用例

下面示例说明如何在 `hccl_fwk_test.cpp` 内扩展新的资源测试用例，并将其加入 CLI 调用列表。

### 约定

* 新的测试函数签名：`bool test_your_case(const Config &cfg)`，返回 `true` 表示通过，`false` 表示失败。
* 在 `main()` 中维护一个 `registry`（`vector<pair<string, func>>`），将测试名称映射到函数。
* `--test <name>` 命令行参数可用于只执行单个测试（现有源码可轻松扩展为支持）。

### 模板代码（直接拷入文件）

```cpp
// 在文件顶部或合适位置添加声明
bool test_your_case(const Config &cfg);

// 实现
bool test_your_case(const Config &cfg) {
    std::cout << "=== test_your_case ===\n";
    HcclFwkTest t(cfg);
    if (!t.init()) { std::cerr << "init failed\n"; return false; }

    const int rounds = 100;
    for (int i = 0; i < rounds; ++i) {
        if (!t.setup_channel_pair()) {
            std::cerr << "setup_channel_pair failed at iter " << i << "\n";
            t.cleanup(); // ensure cleanup
            return false;
        }
        // Immediately cleanup channels & buffers (use HcclFwkTest::cleanup)
        t.cleanup();
        // re-init for next iter
        if (!t.init()) { std::cerr << "re-init failed\n"; return false; }
    }
    t.cleanup();
    std::cout << "[PASS] create_destroy_many\n";
    return true;
}
```

### 把测试注册到主流程

在函数定义后注册，用例名称为：your_testcas；用例函数：test_your_testcase：

```cpp
REGISTER_HCCL_TEST(your_testcase, test_your_testcase);
```

### 测试用例使用说明

* 列出可用用例：

  ```bash
  ./hccl_fwk_test --list
  ```

* 运行单个用例：

  ```bash
  ./hccl_fwk_test --cluster_info ./rank_table.json --rank 0 --test comminit
  ```

* 运行全部用例（默认）：

  ```bash
  ./hccl_fwk_test --cluster_info ./rank_table.json --rank 0
  ```

### 更严密的断言（建议）

* 在资源创建后可做更具体的验证（例如：写入 buffer 后读回并比较内容）。
* 若 HCCL 提供异步完成通知/回调，可等待完成再验证。
* 检查返回码、以及（真实库）可能的日志文件 / kernel 输出。

## 推荐的测试场景清单

1. **alloc_thread**：分配不同线程数的 ThreadRes，边界值（0、1、128）。
2. **channel_single**：单通道创建，检查返回 handle 非空。
3. **channel_multi**：批量创建多个 channel（N=1..32）。
4. **combo**：先 alloc thread 再 create channel（组合测试）。
5. **stress**：循环创建/销毁，观察失败率与资源泄露。

## 附录

### 建议

* 如果完成了接口的Mock，**建议先在 Mock 模式下验证流程与 CLI**，再切换到真实 HCCL/ACL 环境。
* **将 ranktable 与多进程 launcher 的启动脚本分离**，用脚本并行启动多个 rank，方便真实场景测试。
* **扩展 registry / CLI** 以便按名字执行单个测试或并行执行测试集合。

### 其他编译方式参考

在hlt/test目录下执行如下命令：

```bash
# 真实硬件环境执行
mkdir -p build && cd build
cmake ../ && cmake --build . && ./hccl_fwk_test --cluster_info ../ranktable.json --rank 0

# 模拟环境执行
cmake -DMOCK_HCCL=1 ../ && cmake --build . && ./hccl_fwk_test --cluster_info ../ranktable.json --rank 0

# 其他运行参考
./hccl_fwk_test --cluster_info '{"server_list":[{"device":[{"device_id":"0","device_ip":"127.0.0.1","rank_id":"0"}]}]}' --rank 0 --peers 2 --size 4096
```
