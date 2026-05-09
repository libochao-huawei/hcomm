# 基础资源管理 - 创建通信端点

## 样例介绍

本样例展示如何使用 `HcommEndpointCreate()` 接口创建通信端点，包含以下功能点：

- 初始化 ACL 和设置设备。
- 配置 `EndpointDesc` 结构体，指定通信协议、地址和位置信息。
- 调用 `HcommEndpointCreate()` 创建端点。
- 销毁端点并释放资源。

## 目录结构

```text
├── main.cc           # 样例源文件
├── Makefile          # 编译/构建配置文件
└── endpoint_create   # 编译生成的可执行文件
```

## 环境准备

### 环境要求

本样例支持以下产品：

- **Ascend 950PR / Ascend 950DT**（支持）
- **Atlas A3 训练系列产品 / Atlas A3 推理系列产品**（不支持）
- **Atlas A2 训练系列产品 / Atlas A2 推理系列产品**（不支持）

> **重要提示**：HcommEndpointCreate 接口仅在 Ascend 950PR/Ascend 950DT 设备上支持。如果在不支持的产品上运行此样例，将返回参数错误（HCCL_E_PARA）。

### 软件依赖

本样例运行依赖安装CANN软件包，详细安装步骤可参见 [源码构建](../../../docs/build.md) 中的 "安装CANN软件包" 章节。

### 配置环境变量

```bash
# 设置 CANN 环境变量，以 root 用户默认安装路径为例
source /usr/local/Ascend/cann/set_env.sh
```

## 编译执行样例

在本样例代码目录下执行如下命令：

```bash
make
make test
```

## 结果示例

### 在支持的设备上（Ascend 950PR/Ascend 950DT）

```text
=== HcommEndpointCreate Example ===
This example demonstrates how to create a communication endpoint.

Note: This API is only supported on Ascend 950PR/Ascend 950DT devices.
      It is NOT supported on Atlas A3/A2 series products.

Creating endpoint with:
  Protocol: COMM_PROTOCOL_ROCE
  IP Address: 192.168.1.100
  Device ID: 0
  Location: DEVICE

Endpoint created successfully!
Endpoint handle: 0x...

Endpoint destroyed successfully!

=== Example completed ===
```

### 在不支持的设备上（如 Atlas A2/A3 系列）

```text
=== HcommEndpointCreate Example ===
This example demonstrates how to create a communication endpoint.

Note: This API is only supported on Ascend 950PR/Ascend 950DT devices.
      It is NOT supported on Atlas A3/A2 series products.

Creating endpoint with:
  Protocol: COMM_PROTOCOL_ROCE
  IP Address: 192.168.1.100
  Device ID: 0
  Location: DEVICE

Failed to create endpoint, ret: 1
This may happen if the device does not support this API.
```

> 注意：返回码 1 对应 HCCL_E_PARA（参数错误），表示接口不支持当前设备类型。

## 关键接口说明

### HcommEndpointCreate

用于创建通信设备端点。

```c
HcommResult HcommEndpointCreate(const EndpointDesc *endpoint, EndpointHandle *endpointHandle)
```

参数说明：
- `endpoint`: Endpoint初始化配置信息
- `endpointHandle`: 返回的Endpoint句柄

详细API说明请参考 [HcommEndpointCreate](../../../docs/api_ref/comm_opdev/control_plane_api/basic_resource_mgmt/HcommEndpointCreate.md)。

### HcommEndpointDestroy

用于销毁通信设备端点。

```c
HcommResult HcommEndpointDestroy(EndpointHandle endpointHandle)
```

详细API说明请参考 [HcommEndpointDestroy](../../../docs/api_ref/comm_opdev/control_plane_api/basic_resource_mgmt/HcommEndpointDestroy.md)。

## 约束说明

- 支持的通信协议包括：RoCE、UBC_TP、UBC_CTP、UBoE。
- 需要在调用 `HcommEndpointCreate` 前正确初始化 ACL 和设置设备。
- 创建的端点在使用完毕后需要调用 `HcommEndpointDestroy` 进行销毁。