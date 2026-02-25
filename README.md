# HCOMM

## 🔥Latest News

- [2025/11/30] HCOMM项目开源。

## 🚀 概述

HCOMM（Huawei Communication）是HCCL的通信基础库，提供通信域以及通信资源的管理能力。

HCOMM提供了标准化通信编程接口，具备以下关键特性：

- 支持昇腾设备上的多种通信引擎，充分发挥硬件能力。
- 支持多种通信协议，包括PCIe、HCCS、RDMA等。
- 通信平台与通信算子开发解耦，支持通信算子的独立开发、构建与部署。

<img src="./docs/figures/architecture.png" alt="hccl-architecture" style="width: 65%;  height:65%;" />

HCOMM通信基础库采用分层解耦的设计思路，将通信能力划分为控制面和数据面两部分。

- 控制面：提供拓扑信息查询与通信资源管理功能。
- 数据面：提供本地操作、算子间同步、通信操作等数据搬运和计算功能。

  控制面提供通信资源，数据面提供操作资源的方法，所提供的通信编程接口可以让通信算子开发人员聚焦于业务创新，而无需关注芯片底层复杂的实现细节。

## 🔍 目录结构说明

本项目关键目录如下所示：

```
├── src                               # HCCL源码目录
│   ├── algorithm                     # 通信算法源码目录
|   │   ├── base                      # 通信算法模板
|   │   ├── impl                      # 通信算法实现
|   │   └── pub_inc                   # 通信算法模块头文件
│   ├── framework                     # 通信框架源码目录
|   │   ├── cluster_maintenance       # 集群维护，包括：快照、心跳、算子重执行等
|   │   ├── common                    # 通信框架通用逻辑
|   │   ├── communicator              # 通信域管理
|   │   ├── device                    # AI CPU实现
|   │   ├── hcom                      # hcomm接口实现
|   │   ├── inc                       # 通信框架模块头文件
|   │   ├── nslbdp                    # 数据面网络负载均衡
|   │   └── op_base                   # 通信域、单算子接口入口
│   └── platform                      # 通信平台源码目录
|   │   ├── comm_primitive            # 通信原语
|   │   ├── common                    # 通信平台通用逻辑
|   │   ├── debug                     # 维测
|   │   ├── hccp                      # HCCP集合通信协议栈
|   │   ├── inc                       # 通信平台模块头文件
|   │   ├── ping_mesh                 # 网络探测功能
|   │   ├── resource                  # 通信资源管理
|   │   └── task                      # 下发任务管理
├── python                            # Python 包
├── include                           # 对外头文件
├── pkg_inc                           # 包间接口头文件
├── test                              # 测试代码目录
|   ├── ut                            # 单元测试代码目录
|   └── st                            # 系统测试代码目录
├── docs                              # 资料文档目录
├── examples                          # 样例代码目录
└── build.sh                          # 编译构建脚本
```

## 📝版本配套

本项目源码会跟随CANN软件版本发布，关于CANN软件版本与本项目标签的对应关系请参阅[release仓库](https://gitcode.com/cann/release-management)中的相应版本说明。
请注意，为确保您的源码定制开发顺利进行，请选择配套的CANN版本与GitCode标签源码，使用master分支可能存在版本不匹配的风险。

## ⚡️ 快速开始

若您希望快速构建并体验本项目，请访问如下简易指南。

- [源码构建](./docs/build.md)：了解如何编译、安装本项目，并进行基础测试验证。
- [样例执行](./examples/README.md)：参照详细的示例代码与操作步骤指引，快速体验。

## 📖 学习教程

HCCL提供了使用指南、通信算子开发指南、技术文章、培训视频，详细可参见 [HCCL 参考资料](./docs/README.md)。

## 📝 相关信息

- [贡献指南](CONTRIBUTING.md)
- [安全声明](SECURITY.md)
- [许可证](LICENSE)
