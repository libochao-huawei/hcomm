---
applyTo: "test/ut/**,test/ut/stub/**,test/CMakeLists.txt,test/ut/CMakeLists.txt"
description: "HCOMM 单元测试生成与调试指引（参考 ut_test）"
---

# HCOMM UT 指引

适用于本仓库中的单元测试编写、补测、调试与最小范围构建。

## 0. 快速模式（默认优先）

当目标是“尽快验证新写的少量 UT”时，优先走快速模式：

1. 只改测试代码（`test/ut/**`、`test/ut/stub/**`、必要的测试 CMakeLists）
2. 分析 UT 工程依赖后，临时注释不相关 target
3. 优先使用 `build_ut` + workflow preset（如 `build-ut-test`）执行
4. 先过编译，再扩展用例覆盖

若遇到网络下载卡住（third_party 下载重试），先暂停构建并切到“写用例/静态检查”阶段，网络恢复后再继续。

## 1. 基本原则

- 优先只修改 `test/ut/**` 和 `test/ut/stub/**`
- 不要为了让测试通过而顺手改产品代码，除非用户明确要求
- 写测试前先读同目录或邻近目录已有 UT，保持风格一致
- 优先复用已有 stub / mockcpp 写法，不新造测试框架

## 2. 路径约定

默认把被测源码在 `src/` 下的相对路径映射到 `test/ut/` 下。

例如：

- 被测源码：`src/framework/communicator/impl/xxx.cpp`
- 建议测试目录：`test/ut/framework/communicator/impl/`

若已有更合适的现有测试组，优先并入现有组，而不是重复建组。

## 3. 生成测试前必须做的事

### 3.1 先读这些内容

- 被测接口定义和实现
- 相邻测试文件
- `test/ut/stub/**` 中已有 stub 模式
- 对应目录 `CMakeLists.txt`

### 3.2 先明确三件事

- 被测函数的输入输出和错误码语义
- 依赖项哪些需要 stub / mock
- 主路径、异常路径、边界路径各至少覆盖一个

## 4. 测试代码风格

- 测试名优先体现接口、场景、预期
- 断言优先验证对外可观察行为，不要过度绑定内部实现
- mock / stub 放到 `test/ut/stub/` 或现有公共位置
- 新增 stub 后同步更新对应 CMakeLists.txt

## 5. 构建与调试策略

若用户目标是快速验证单个测试组，可临时缩小 `test/ut/CMakeLists.txt` 的编译范围，但结束前必须恢复。

### 5.1 推荐执行入口（按优先级）

优先级 1：workflow preset

- 已存在 `build_ut` 且可切换 workflow preset 时，优先使用 `build-ut-test`。
- 日志中若出现：`Build files have been written to: .../build_ut`、`Switching to workflow preset: build-ut-test`，可继续沿该路径构建与执行。

优先级 2：脚本入口

- 使用 `bash build.sh --ut`（或 `bash build.sh -u`）执行 UT 构建与测试。

优先级 3：手动 CMake（仅在前两者不可用时）

- 手动配置 `build_ut` 并执行目标 UT。

### 5.2 临时瘦身 target（强烈建议）

修改前先备份：

```bash
cp test/ut/CMakeLists.txt /tmp/hcomm_ut_CMakeLists.txt.bak
cp test/CMakeLists.txt /tmp/hcomm_test_CMakeLists.txt.bak
```

瘦身规则：

缩小范围时遵守：

- 保留 `add_subdirectory(stub)`
- 保留本次目标测试组
- 其他临时注释的项在任务结束前恢复

建议同时在 `test/CMakeLists.txt` 中仅保留本次 UT 所需路径，避免带上无关 legacy UT。

恢复命令（必须执行）：

```bash
cp /tmp/hcomm_ut_CMakeLists.txt.bak test/ut/CMakeLists.txt
cp /tmp/hcomm_test_CMakeLists.txt.bak test/CMakeLists.txt
```

### 5.3 网络阻塞场景处理

当构建卡在 third_party 下载（如 gtest / boost / mockcpp）时：

- 不要反复全量重跑。
- 先中断当前构建，保留日志。
- 先完成以下离线可做工作：
	- 补齐/修正测试用例逻辑
	- 校对 CMake target 与 link 依赖
	- 检查头文件可见性与命名空间
- 网络恢复后，从 `build_ut` 路径续跑。

### 5.4 调试顺序（提高定位速度）

1. 先看第一个编译错误，不要同时改多个方向
2. 先解决符号可见性/头文件路径，再处理断言逻辑
3. 先保证单测可编译，再扩展边界 case
4. 每次改动后最小增量重编译

优先检查：

- 编译错误落在测试文件还是产品代码
- 链接错误是否源于漏加 stub / target_link_libraries
- 运行失败是否源于桩行为不符合真实接口约束

### 5.5 结果输出要求

每轮 UT 结束后，至少说明：

- 本轮执行入口（workflow preset / build.sh --ut / 手动 CMake）
- 是否做过 target 瘦身
- 当前编译状态（通过/失败）
- 当前执行状态（通过/失败/未执行）
- 若失败：首个错误位置与下一步处理计划

## 6. 覆盖重点

至少覆盖：

- 正常路径
- 非法参数/空指针/边界值
- 下层接口失败后的错误传播
- 资源或状态恢复逻辑

若变更涉及公共接口、资源管理、DFX 或并发，必须额外关注失败路径覆盖。

## 7. 不要做的事

- 不要只补 happy path
- 不要把多个无关场景塞进一个测试函数
- 不要通过修改源码语义来迁就测试
- 不要新增与现有风格冲突的测试框架或目录结构
- 不要在网络失败时无限重试全量构建
- 不要临时注释 target 后忘记恢复
