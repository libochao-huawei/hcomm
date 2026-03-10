# HCCL UT 单元测试用例规范

## 1. 测试代码质量要求

1. 可读性

- 每个测试用例不超过 50 行
- 复杂逻辑提取为辅助方法
- 使用有意义的变量名和常量

2. 可维护性

- 避免硬编码的魔法数字
- 测试数据与测试逻辑分离
- 使用统一的错误处理

3. 性能要求

- 单元测试执行时间 < 1 秒/用例
- 避免不必要的资源分配
- 及时释放资源

4. 覆盖率目标

- 行覆盖率 ≥ 80%
- 函数覆盖率 ≥ 85%

## 2. 测试组织规范

### 2.1 测试目录结构

```
test/
├── ut
|   ├── common
|   ├── framework
|   |   ├── communicator
|   |   └── op_base_api
|   ├── platform
|   |   ├── hcom
|   |   └── resource
|   ├── stub
|   └── depends
├── st/algorithm
|   ├── testcase
|   └── utils
└── CMakeLists.txt
```

### 2.2 大型测试的拆分

1. 按功能模块拆分：同一功能模块的测试集中在一个目录

2. 按测试类型拆分：

- 正常流程测试
- 异常流程测试
- 边界条件测试
- 性能测试

3. 按测试复杂度拆分：

- 基础功能测试（BasicTest）
- 集成功能测试（IntegrationTest）
- 性能测试（ProfTest）

## 3. 命名规则

### 3.1 测试文件命名规范

原则：

- 文件名能清晰反映被测对象和测试类型
- 测试用例名能清晰反映测试内容

1. 公开接口测试

测试 HCCL 对外提供的公开接口，如 `HcclAllReduce()`，建议一个接口对应一个测试文件，文件名中包含完整接口名称，不改变大小写，格式如下：

```
ut_被测接口名_API_test.cc

// 示例
ut_HcclAllReduce_API_test.cc
```

2. 文件级别测试

测试 C++ 文件的函数功能，如测试 `hccl_communicator_host.cc` 文件，建议一个源文件对应一个测试文件，文件名中包含文件名称，不改变大小写，格式如下：

```
ut_文件名.cc

// 示例
ut_hccl_communicator_host.cc
```

3. 参数化测试

测试 HCCL 接口的参数，如 `HcclAllReduce()`，建议一个接口对应一个测试文件，文件名中包含完整接口名称，不改变大小写，格式如下：

```
ut_被测接口_Param_test.cc

// 示例
ut_HcclAllReduce_Param_test.cc
```

4. 性能测试

测试 HCCL 功能模块的性能表现，建议一个模块对应一个测试文件，文件名中包含模块名称，不改变大小写，格式如下：

```
ut_性能测试场景_Prof_test.cc

// 示例
ut_launch_kernel_Prof_test.cc
```

5. 多线程测试

测试 HCCL 多线程场景下的功能模块，建议一个模块对应一个测试文件，文件名中包含模块名称，不改变大小写，格式如下：

```
ut_多线程测试场景_MultiThread_test.cc

// 示例
ut_launch_kernel_MultiThread_test.cc
```

### 3.2 测试类命名规范

1. 公开接口测试

```c++
// 如：ut_HcclReduce_API_test
class HcclReduceApiTest : public ::testing::Test {};
```

2. 文件级别测试

```c++
// 如：ut_HcclCommunicator_Class_test.cc
class HcclCommunicatorClassTest : public ::testing::Test {};
```

### 3.3 测试用例命名规范

HCCL 测试用例命名格式采用 Given_When_Then 格式（BDD 风格）

```c++
TEST_F(HcclReduceApiTest, UT_HcclReduce_When_SendBufIsNull_Expect_ReturnHCCL_E_PTR)
```

### 3.4 变量命名规范

HCCL 测试用例变量命名规范与业务代码保持一致：

1. 进程/线程全局变量名：以 `g_` 开头，剩余变量名首字母小写，驼峰。如：`thread_local s32 g_hcclDeviceId;`
2. 类成员变量名：以 `_` 结尾，首字母小写、驼峰。如：`RankGraph rankGraph_;`
3. 函数局部变量名：首字母小写、驼峰。如：`HcclComm hcclComm = nullptr;`

## 4. 测试类设计规范

```c++
class ExampleTest : public ::testing::Test {
protected:
    // 整个测试类执行前执行一次
    static void SetUpTestSuite() {}

    // 整个测试类执行后执行一次
    static void TearDownTestSuite() {
        Logger::Shutdown();
    }

    // 每个测试用例执行前执行
    void SetUp() override {}

    // 每个测试用例执行后执行
    void TearDown() override {}

    // 公共辅助方法
    void Foo() {}

    // 公共成员变量
    HcclComm comm;

    // 全局变量
    static constexpr int kDefaultTimeout = 1000;
};
```

## 5. 测试用例编写规范

### 5.1 测试用例编写规范

HCCL 测试用例采用 AAA 三段式结构：

```c++
TEST(ExampleTest, UT_When_Normal_Expect_Success) {
    // 1. 准备测试数据（Arrange）
    InitComm(comm);

    // 2. 执行操作（Act）
    HcclResult ret = HcclReduce(comm, ...);

    // 3. 验证结果（Assert）
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
```

### 5.2 异常测试

```c++
TEST(ExampleTest, UT_When_InvalidParam_Expect_ThrowException) {
    // 验证异常类型
    EXPECT_THROW(HcclReduce(nullptr), std::invalid_argument);

    // 验证异常场景，但不校验异常类型
    EXPECT_ANY_THROW(HcclReduce(nullptr));

    // 验证异常消息
    EXPECT_THROW({
        try {
            HcclReduce(nullptr);
        } catch (const std::invalid_argument& e) {
            EXPECT_STREQ("comm is null", e.what());
            throw;
        }
    }, std::invalid_argument);
}
```

### 5.3 断言使用规范

```c++
// 相等检查
EXPECT_EQ(actual, expected);            // 推荐
EXPECT_TRUE(actual == expected);        // 不推荐，优先使用 EXPECT_EQ

// 浮点数比较
EXPECT_FLOAT_EQ(actual, expected);      // 4 ULP
EXPECT_DOUBLE_EQ(actual, expected);     // 4 ULP
EXPECT_NEAR(actual, expected, 1e-6);    // 指定误差

// 字符串比较
EXPECT_STREQ(str1, str2);               // C风格字符串
EXPECT_STRNE(str1, str2);
EXPECT_STRCASEEQ(str1, str2);           // 忽略大小写

// 指针检查
EXPECT_NE(ptr, nullptr);
EXPECT_EQ(ptr, nullptr);

// 数组比较
EXPECT_THAT(actual_vector, ::testing::ElementsAre(1.0, 2.0, 3.0));
EXPECT_THAT(actual_vector, ::testing::ContainerEq(expected_vector));

// 致命检查
ASSERT_EQ(actual, expected);            // 致命，失败即终止当前测试
```

### 5.4 参数化测试

```c++
// 定义参数结构
struct AllReduceTestParam {
    // HcclAllReduce 接口参数
    HcclDataType dataType;
    HcclReduceOp op;
    // 测试用例参数名称
    std::string testcaseName;
};

// 参数化测试类
class HcclAllReduceParamTest : public ::testing::WithParamInterface<AllReduceTestParam> {
protected:
    void SetUp() override {
        // 获取参数句柄
        param_ = GetParam();
    }

    void TearDown() override {
        // 释放资源
        HcclCommDestroy(comm_);
    }

    AllReduceTestParam param_;
    void *sendBuf_;
    void *recvBuf_;
    uint64_t count_;
    HcclComm comm_;
    aclrtStream stream_;
};

// 测试用例
TEST_P(HcclAllReduceParamTest, UT_HcclAllReduce_When_VariousParams_Expect_Success)
{
    // 可选：跳过特定参数
    if (param_.op == HCCL_REDUCE_MIN) {
        GTEST_SKIP() << "Skipping test with HCCL_REDUCE_MIN";
    }

    // 执行接口
    HcclResult ret = HcclAllReduce(
        sendBuf_,
        recvBuf_,
        count_,
        param_.dataType,  // 使用组合参数
        param_.op,        // 使用组合参数
        comm_,
        stream_
    );

    // 验证结果
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 实例化参数
INSTANTIATE_TEST_SUITE_P(
    AllReduceParamTests,          // 测试套件名称
    HcclAllReduceParamTest,       // 测试类名称
    ::testing::Values(
        AllReduceTestParam{HCCL_DATA_TYPE_FP32,  HCCL_REDUCE_SUM, "AllReduce_FP32_Sum"},
        AllReduceTestParam{HCCL_DATA_TYPE_FP16,  HCCL_REDUCE_SUM, "AllReduce_FP16_Sum"},
        AllReduceTestParam{HCCL_DATA_TYPE_INT32, HCCL_REDUCE_SUM, "AllReduce_INT32_Sum"},
        AllReduceTestParam{HCCL_DATA_TYPE_FP32,  HCCL_REDUCE_MAX, "AllReduce_FP32_Max"}
    ),
    [](const ::testing::TestParamInfo<AllReduceTestParam>& info) {
        return info.param.testcaseName;   // 自定义参数化测试用例的参数名称
    }
);
```
