---
applyTo: "**/*.{h,hpp,c,cc,cpp,cxx,inc}"
description: "华为 C/C++ 编程与安全编程规范（HCOMM 适配版）"
---

# C/C++ 编码规范

适用于本仓库所有 C/C++ 源文件。基线为《华为 C/C++ 语言编程规范》与《华为 C/C++ 安全编程规范》，下面列出在 HCOMM 中**必须遵守**的要点。

## 0. 语言标准

- 本仓库（含 HCCL 仓）当前**最高支持 C++14**，C++17 及以上特性**禁止使用**。
- 常见禁用项（C++17+）：`std::optional` / `std::variant` / `std::any` / `std::string_view` / `std::filesystem` / `std::byte` / `if constexpr` / 结构化绑定 / 内联变量 / 折叠表达式 / `[[nodiscard]]` 等属性。
- 替代写法：
  - `std::optional<T>` → 自定义 `struct { T value; bool valid; }` 或 `std::pair<T, bool>`；
  - `std::variant` → `union` + 显式 tag 字段 / 多态接口 / 函数指针表（ops 表）；
  - `std::string_view` → `const std::string&` 或 `const char*` + length；
  - `if constexpr` → SFINAE 或运行期判断；
  - 结构化绑定 → `std::tie` 或显式 `.first / .second`。
- C 文件遵循 **C99**；不使用 C11/C17 引入的关键字（`_Generic` 等）。

## 1. 命名

| 实体 | 风格 | 示例 |
|---|---|---|
| 类 / 结构体 / 枚举类型 | PascalCase | `CcuKernel`, `CcuTaskParam` |
| 函数 / 方法 | PascalCase | `GeneTaskParam`, `TransRepSequenceToMicrocode` |
| 成员变量 | camelCase + 末尾下划线 | `channels_`, `instrInfo_` |
| 局部变量 / 形参 | camelCase | `taskCount`, `repSeq` |
| 常量 / 宏 | UPPER_SNAKE_CASE | `MAX_CHANNELS`, `HCOMM_OK` |
| 文件名 | snake_case | `ccu_kernel.cc`, `ccu_rep_context_v1.h` |
| 命名空间 | 全小写 | `hcomm`, `hccl` |

- 头文件保护使用 `#ifndef HCOMM_<PATH>_<FILE>_H_` 形式，与既有头文件保持一致。

## 2. 代码风格

- 缩进 **2 空格**，禁用 Tab。
- 行长 **≤120 字符**。
- 花括号 **K&R**：`if (...) {` / `} else {` / 函数定义左花括号另起一行。
- `if`、`for`、`while`、`switch` 后**必须**有空格；单行体也要带花括号。
- 指针/引用符号靠类型：`int* p`、`const T& ref`。
- `using namespace` 不得出现在头文件全局作用域。
- `#include` 顺序：对应头文件 → C 系统头 → C++ 系统头 → 第三方 → 本项目，组间空行。

## 3. 注释（Doxygen）

公开接口（头文件中导出符号）必须有 Doxygen 注释。

```cpp
/// @brief 生成可执行任务参数
/// @param[out] taskArgs  输出任务参数数组
/// @param[out] taskCount 输出任务数量
/// @return 0 成功；负值表示错误码
/// @note   线程安全：是
int GeneTaskParam(std::vector<CcuTaskParam>& taskArgs, int& taskCount);
```

- 实现内部用 `//`；避免重复函数签名的废话注释。
- TODO 必须带责任人或 issue 编号：`// TODO(name): ...`。

## 4. 安全编程（强制）

### 4.1 内存与资源
- 优先 `std::unique_ptr` / `std::shared_ptr` / `std::vector` / `std::string`；禁止裸 `new/delete` 配对管理生命周期。
- 持有资源的类必须遵循 **Rule of Five**（或显式 `= delete` / `= default`）。
- 析构函数不得抛异常。

### 4.2 指针与边界
- 解引用前检查空指针；外部传入指针在函数入口处校验。
- 数组/容器访问：可疑下标用 `at()` 或前置范围检查；裸数组用 `sizeof(arr)/sizeof(arr[0])` 之前先确认是数组而非指针。
- 禁止使用 `strcpy / strcat / sprintf / gets`，改用 `snprintf` 或安全版本（`memcpy_s`、`strcpy_s`，若可用）。

### 4.3 整数与类型
- 涉及 size 的算术使用无符号或显式宽度类型（`size_t`、`uint32_t`），避免有符号溢出 UB。
- 缩窄转换用 `static_cast` 显式表达，禁用 C 风格强转。
- 指针与整数互转一律 `reinterpret_cast`，并加注释说明用途。

### 4.4 错误处理
- 系统调用 / 跨层 API 的返回值**必须**检查；忽略需用 `(void)` 显式标注并写明原因。
- 错误码统一遵循模块既有定义（如 `HCCL_SUCCESS`、`HCOMM_E_*`），不要发明新错误码体系。
- 不要用异常做正常控制流；device 侧禁用异常。

### 4.5 并发
- 共享数据访问需明确同步策略（锁 / atomic / 无锁队列），并在注释中说明。
- 锁的获取顺序在文档中固化，避免死锁。

## 5. 头文件与依赖

- 头文件最小包含（前置声明优先），减少编译耦合。
- 不在头文件中定义非 `inline` / 非模板函数。
- 公共头放在 `pkg_inc/`；内部头放在 `src/.../` 同目录，不外泄实现细节。
- 不要 `#include` 第三方源码目录（`openssl-*-src/`、`third_party/`）下的内部头。

## 6. Device / Host 差异

- device 翻译单元不得依赖 STL 中需要异常或动态分配的容器（除非项目已有约定的 device-safe 封装）。
- 跨边界的数据结构必须 POD 布局，并在两侧显式 `static_assert(sizeof(...))` / `std::is_trivially_copyable_v<...>`。
- 内存所有权穿越 device/host 时，注释中标明分配者与释放者。

## 7. 测试

- 修改公共行为需补/改单元测试（`test/` 目录）。
- 测试命名与被测函数同名 + 场景：`GeneTaskParam_EmptyInput_ReturnsError`。
- 不要在测试里 `sleep`；用同步原语等待。

## 8. 提交前本地校验

```bash
# clang-tidy 仅检查变更
HCOMM_CLANG_TIDY_DISABLE=1 HCOMM_CLANG_TIDY_SCOPE=changed bash scripts/clang_tidy_precommit.sh

# 自动修复
HCOMM_CLANG_TIDY_DISABLE=1 HCOMM_CLANG_TIDY_SCOPE=changed HCOMM_CLANG_TIDY_FIX=1 bash scripts/clang_tidy_precommit.sh
```
