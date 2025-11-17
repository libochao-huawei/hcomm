# cann-hccl

## 概述

集合通信库（Huawei Collective Communication Library，简称HCCL）是基于昇腾AI处理器的高性能集合通信库，提供单机多卡以及多机多卡间的数据并行、模型并行集合通信方案。

HCCL的软件架构如下图所示，分为“通信框架”、“通信算法”与“通信平台”三个模块，本开源仓中包含了其中的“**通信框架**”与“**通信算法**”两个模块的源码。

![hccl-architecture](docs/figures/hccl_architecture.png)



- 通信框架：负责通信域管理，通信算子的业务串联，协同通信算法模块完成算法选择，协同通信平台模块完成资源申请并实现集合通信任务的下发。
- 通信算法：作为集合通信算法的承载模块，提供特性集合通信操作的资源计算，并根据通信域信息完成通信任务编排。
- 通信平台：提供NPU之上与集合通信关联的资源管理，并提供集合通信维测能力。

本开源仓提供了Mesh、Ring、Recursive Halving-Doubling（RHD）、PairWise四种拓扑算法的实现源码。

| 算法  | 描述  |原理与耗时   |
|---|---|---|
| Mesh | Server内通信算法，是Mesh互联拓扑的基础算法。  | [Mesh](docs/Mesh.md) |
| Ring  | Server内和Server间通信算法，是基于环结构的并行调度算法。<br> Server间通信场景下，适用于小规模节点数（<32机，且非2幂）和中大规模通信数据量（\>=256M）的场景。  | [Ring](docs/Ring.md)   |
| RHD | Server间通信算法，递归二分和倍增算法，当通信域内Server个数为2的整数次幂时，此算法具有较好的亲和性。  | [RHD](docs/RHD.md) |
|  Pairwise|Server间通信算法，比较算法，仅用于AllToAll与AlltoAllV算子，适用于数据量较小（<=1M \* RankSize）的场景。   | [PairWise](docs/PairWise.md)  |

HCCL采用α–β模型（Hockney）进行性能评估，算法耗时计算用到的变量定义如下：

-   α：节点间的固定时延。
-   β：每byte数据传输耗时。
-   n：节点间通信的数据大小，单位为byte。
-   γ：每byte数据规约计算耗时。
-   p：通信域节点个数。

传输并规约计算n byte数据的耗时为： D = α + nβ + nγ。


## 目录结构说明

```
  ├── src                       # HCCL相关源代码
  |   ├── algorithm             # 通信算法
  |   ├── framework             # 通信框架
  |   ├── pub_inc
  ├── test   
  ├── docs                      # 相关描述文档                           
```


## 环境准备

HCCL支持源码编译，在源码编译前，请根据如下步骤完成相关环境准备。

1. 获取CANN开发套件包。
   
   请从[Link](https://www.hiascend.com/developer/download/community/result?module=cann)获取配套版本的CANN开发套件包`Ascend-cann-toolkit_<cann_version>_linux-<arch>.run`。
   
   - **开源项目与CANN社区版本的配套关系可参见"[开源项目与CANN版本配套表](https://gitee.com/ascend/cann-community/blob/master/README.md#cannversionmap)"。**
   - 支持的操作系统请参见配套版本的[用户手册](https://hiascend.com/document/redirect/CannCommunityInstSoftware)中“支持的操作系统”章节。
   
2. 安装CANN开发套件包。
   
   执行安装命令时，请确保安装用户对软件包具有可执行权限。
   - 使用默认路径安装
     ```shell
     ./Ascend-cann-toolkit_<soc_version>_linux_<arch>.run --install
     ```
     若使用root用户安装，安装完成后相关软件存储在`/usr/local/Ascend/ascend-toolkit/latest`路径下。

     若使用非root用户安装，安装完成后相关软件存储在`$HOME/Ascend/ascend-toolkit/latest`路径下。
   - 指定路径安装
     ```shell
     ./Ascend-cann-toolkit_<soc_version>_linux_<arch>.run --install --install-path=${install_path}
     ```
     安装完成后，相关软件存储在${install_path}指定路径下。

3. 设置环境变量。

   - 默认路径，root用户安装
     ```shell
     source /usr/local/Ascend/ascend-toolkit/set_env.sh
     ```
   - 默认路径，非root用户安装
     ```shell
     source $HOME/Ascend/ascend-toolkit/set_env.sh
     ```
   - 指定路径安装
     ```shell
     source ${install_path}/ascend-toolkit/set_env.sh
     ```
     **注意：若环境中已安装多个版本的CANN软件包，设置上述环境变量时，请确保${install_path}/ascend-toolkit/latest目录指向的是配套版本的软件包。**

4. 安装nlohmann json头文件。
   
   HCCL源码编译过程中涉及Json文件的解析，编译前需要参见如下步骤下载依赖的nlohmann json头文件。
   
   - 单击[Link](https://github.com/nlohmann/json/releases/download/v3.11.2/include.zip)，下载nlohmann json的头文件压缩包`include.zip`。
   
   - 解压缩`include.zip`。
     
      将`include.zip`解压缩到任意CANN开套件包安装用户具有读写权限的目录，并记录该目录位置，后续编译时会用到。
      例如：
     
       ```
       mkdir /home/nlohmann_json
       cp include.zip /home/nlohmann_json
       cd /home/nlohmann_json
       unzip include.zip
       ```

## 源码下载

开发者可通过如下命令下载本仓源码：

```bash
git clone https://gitee.com/ascend/cann-hccl.git
```

## 编译

HCCL提供一键式编译安装能力，进入本仓代码根目录，执行如下命令：

```shell
sh build.sh --nlohmann_path ${JSON头文件所在目录}
```

例如，环境准备时假设将nlohmann json头文件压缩包`include.zip`解压在了`/home/nlohmann_json`目录，则此处编译命令为：

```
sh build.sh --nlohmann_path /home/nlohmann_json/include
```

编译完成后会在build_out目录下生成`CANN-hccl_alg-linux.x86_64.run`软件包或者`CANN-hccl_alg-linux.aarch64.run`软件包。

## 安装

安装编译生成的HCCL软件包，例如：

```shell
./build_out/CANN-hccl_alg-linux.x86_64.run
```
请注意：编译时需要将上述命令示例中的软件包名称替换为实际编译生成的软件包名称。

## 本地验证（UT/ST）

HCCL软件包安装完成后，开发者可通过HCCL Test工具进行集合通信功能与性能的测试，HCCL Test工具的详细使用方法可参见[昇腾文档中心-HCCL性能测试工具使用指南](https://hiascend.com/document/redirect/CannCommunityToolHcclTest)。

## 贡献指南

HCCL仓欢迎广大开发者体验并参与贡献，在参与社区贡献前，请参见[cann-community](https://gitee.com/ascend/cann-community/blob/master/README.md)了解行为准则，进行CLA协议签署，以及参与开源仓贡献的详细流程。

针对HCCL仓，开发者准备本地代码与提交PR时需要重点关注如下几点：

1. 提交PR时，请按照PR模板仔细填写本次PR的业务背景、目的、方案等信息。
2. 若您的修改不是简单的bug修复，而是涉及到新增特性、新增接口、新增配置参数或者修改代码流程等，请务必先通过Issue进行方案讨论，以避免您的代码被拒绝合入。若您不确定本次修改是否可被归为“简单的bug修复”，亦可通过提交Issue进行方案讨论。

## 许可证

[CANN Open Software License Agreement Version 1.0](LICENSE)