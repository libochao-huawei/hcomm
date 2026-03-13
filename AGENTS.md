# AGENTS.md - Agentic Coding Guidelines for HCOMM

HCOMM (Huawei Communication) is a communication foundation library for Ascend devices using CMake as build system.

## Build Commands

### Basic Build
```bash
bash build.sh --pkg              # Build host package
bash build.sh --pkg --full       # Build host + device
bash build.sh --pkg -j8          # With thread count
```

### Unit Tests (UT)
```bash
bash build.sh --ut               # Build and run UT
ENABLE_UT=on cmake -B build -DCMAKE_BUILD_TYPE=Debug  # Build UT only
cmake --build build -j$(nproc)
```

### Single Test Execution
```bash
find build/test -type f -executable
./build/test/ut/framework/communicator/communicator_utest
./build/test/ut/framework/communicator/communicator_utest --gtest_filter="*AllReduce*"
```

### Other Options
```bash
bash build.sh --pkg --asan        # AddressSanitizer
bash build.sh --pkg --cov         # Coverage
bash build.sh --aicpu            # Device kernel only
bash build.sh --pkg -p /path/to/cann  # Specify CANN path
```

## Code Style

### Copyright Header
```cpp
/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * ...
 */
```

### File Naming
- `.h` public headers, `.hh` internal, `.cc` source files
- Test files: `ut_<ModuleName>_test.cc`
- Lowercase with underscores: `hccl_comm.h`, `ccl_buffer_manager.cc`

### Naming Conventions
- **Classes**: PascalCase with `Hccl`/`Ccl` prefix
  ```cpp
  class CCLBufferManager;
  class HcclCommunicator;
  ```
- **Functions**: PascalCase
  ```cpp
  HcclResult CreateCCLbuffer(u64 size, DeviceMem &buffer);
  ```
- **Variables**: camelCase, type prefix for members
  ```cpp
  u64 count = 0;
  s8 *sendBuf = nullptr;
  DeviceMem buffer_;  // member variable
  ```
- **Constants**: kCamelCase or UPPER_SNAKE_CASE
- **Enums**: PascalCase values

### Type Abbreviations
- `u8`, `u16`, `u32`, `u64` - unsigned integers
- `s8`, `s16`, `s32`, `s64` - signed integers
- `bool` - boolean

### Include Order
1. Corresponding header
2. Project headers (`"base/err_msg.h"`, `"log.h"`)
3. External headers (`<string>`, `<stdio.h>`)
4. System headers

### Error Handling
- Return `HcclResult` (enum class)
- Use macros:
  ```cpp
  CHK_PRT_RET(condition, log_message, return_value);
  CHK_RET(function_call);
  HCCL_ERROR("message");
  HCCL_INFO("message");
  ```

### Namespace & Logging
- Use `namespace hccl { }`
- Log format: `[ClassName][FunctionName]message`
- Levels: HCCL_ERROR, HCCL_WARN, HCCL_INFO, HCCL_DEBUG, HCCL_TRACE

### Testing Conventions
```cpp
#include "hccl_api_base_test.h"

class HcclAllReduceTest : public BaseInit {
public:
    void SetUp() override { ... }
    void TearDown() override { ... }
};

TEST_F(HcclAllReduceTest, Ut_TestName_Condition_Expect_Result)
{
    EXPECT_EQ(ret, expectedValue);
}
```
- Macros: `UT_USE_1SERVER_1RANK_AS_DEFAULT`, `UT_COMM_CREATE_DEFAULT(comm)`, `UT_STREAM_CREATE_DEFAULT(stream)`

### API Design
- Use `extern "C"` for C API headers
- Document with Doxygen-style comments

## References
- [CANN Community Coding Standards](https://gitcode.com/cann/community/tree/master/contributor/coding-standards)
- [Build Documentation](./docs/build.md)
