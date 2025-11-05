# HCCL对外接口单线程基础功能测试
## 概述
- **测试对象**：HCCL 对外接口 API  
- **测试方法**：单线程基础功能测试
## 目录结构
hccl_api_base_func_test/  
├── hccl_api_base_test.h  
├── hccl_api_base_test.cc  
└── ut_<API名称>_API_test.cc
- `hccl_api_base_test.h` / `hccl_api_base_test.cc`：用于存放测试中的公共类和通用函数。
- `ut_<API名称>_API_test.cc`：针对各 API 的单元测试文件，文件名包含对应 API 名称，便于区分查找。
## 命名规范
- **测试类命名**：`<API名称>Test`  
  例如：`HcclGetCommNameTest`
- **单个测试用例命名**：`Ut_<API名称>_When_<测试条件>_Expect_<预期行为>`（大驼峰命名）  
  例如：`ut_HcclGetCommName_When_Normal_Expect_ReturnIsHCCL_SUCCESS`

## 测试接口
- **HcclCommInitClusterInfo**
- **HcclCommInitClusterInfoConfig**
- **HcclCreateSubCommConfig**
- **HcclGetRootInfo**
- **HcclCommInitRootInfo**
- **HcclCommInitRootInfoConfig**
- **HcclSetConfig**
- **HcclGetConfig**
- **HcclGetCommName**
- **HcclAllReduce**
- **HcclBroadcast**
- **HcclReduceScatter**
- **HcclReduceScatterV**
- **HcclScatter**
- **HcclAllGather**
- **HcclAllGatherV**
- **HcclGetRankSize**
- **HcclGetRankId**
- **HcclBarrier**
- **HcclRecv**
- **HcclSend**
- **HcclAlltoAllV**
- **HcclAlltoAll**
- **HcclReduce**
- **HcclCommDestroy**
- **HcclCommInitAll**
- **HcclGetCommAsyncError**
- **HcclGetErrorString**
- **HcclBatchSendRecv**
- **HcclGetCommConfigCapability**
- **HcclCommConfigInit**
- **HcclCommSuspend**
- **HcclCommResume**
- **HcclCommSetMemoryRange**
- **HcclCommUnsetMemoryRange**
- **HcclCommActivateCommMemory**
- **HcclCommDeactivateCommMemory**
- **HcclCommWorkingDevNicSet**

