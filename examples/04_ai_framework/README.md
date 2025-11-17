## 样例介绍

本样例展示如何使用 TensorFlow 框架和 PyTorch 框架调用 HCCL 接口，下发集合通信算子，执行训练。

### 通信域管理

- 每个进程管理一个 NPU 设备  
  - **TensorFlow** 基于 `ranktable.json` 配置文件初始化通信域  
  - **PyTorch** 基于 `rootinfo` 标识信息通过 `HcclCommInitRootInfo()` 接口初始化通信域

---

## TensorFlow 训练

### 环境准备

#### 环境要求

本示例支持以下昇腾产品，集群拓扑为单机 8 卡：

- Atlas 训练系列产品
- Atlas A2 训练系列产品
- Atlas A3 训练系列产品 / Atlas A3 推理系列产品

#### 配置环境变量

# 设置 CANN 环境变量，以 root 用户默认安装路径为例
source /usr/local/Ascend/latest/bin/setenv.bash

#### 导入 ranktable.json 配置文件

export RANK_TABLE_FILE=ranktable.json

#### 执行样例代码

bash run_tensorflow.sh

#### 结果展示

每个 rank 的数据初始化为 2，经过 AllReduce 操作后，每个 rank 的结果是所有 rank 数据的和（8 个 rank 的数据相加）。

rankId: 0, output: [44., 44., 44., 44., 44., 44., 44., 44.]
rankId: 1, output: [44., 44., 44., 44., 44., 44., 44., 44.]
rankId: 2, output: [44., 44., 44., 44., 44., 44., 44., 44.]
rankId: 3, output: [44., 44., 44., 44., 44., 44., 44., 44.]
rankId: 4, output: [44., 44., 44., 44., 44., 44., 44., 44.]
rankId: 5, output: [44., 44., 44., 44., 44., 44., 44., 44.]
rankId: 6, output: [44., 44., 44., 44., 44., 44., 44., 44.]
rankId: 7, output: [44., 44., 44., 44., 44., 44., 44., 44.]

---

## PyTorch 训练

### 环境准备

#### 环境要求

本示例支持以下昇腾产品，集群拓扑为单机 8 卡：

- Atlas 训练系列产品
- Atlas A2 训练系列产品
- Atlas A3 训练系列产品 / Atlas A3 推理系列产品

#### 配置环境变量

# 设置 CANN 环境变量，以 root 用户默认安装路径为例
source /usr/local/Ascend/latest/bin/setenv.bash

#### 执行样例代码

bash run_pytorch.sh

#### 结果展示

每个 rank 的数据初始化为 2，经过 AllReduce 操作后，每个 rank 的结果是所有 rank 数据的和（8 个 rank 的数据相加）。

rankId: 0, output: [44., 44., 44., 44., 44., 44., 44., 44.]
rankId: 1, output: [44., 44., 44., 44., 44., 44., 44., 44.]
rankId: 2, output: [44., 44., 44., 44., 44., 44., 44., 44.]
rankId: 3, output: [44., 44., 44., 44., 44., 44., 44., 44.]
rankId: 4, output: [44., 44., 44., 44., 44., 44., 44., 44.]
rankId: 5, output: [44., 44., 44., 44., 44., 44., 44., 44.]
rankId: 6, output: [44., 44., 44., 44., 44., 44., 44., 44.]
rankId: 7, output: [44., 44., 44., 44., 44., 44., 44., 44.]
