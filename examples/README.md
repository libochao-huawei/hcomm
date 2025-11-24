# HCCL 代码示例

本目录提供了不同场景下使用 HCCL 接口实现集合通信功能的示例代码。

## 通信域管理

- [每个进程管理一个 NPU 设备（基于 root 节点信息初始化通信域）](./01_communicators/01_one_device_per_process/)
- [每个进程管理一个 NPU 设备（基于 rank table 初始化通信域）](./01_communicators/02_one_device_per_process_rank_table/)
- [每个线程管理一个 NPU 设备（基于 root 节点信息初始化通信域）](./01_communicators/03_one_device_per_pthread/)

## 点对点通信

- [HcclSend/HcclRecv（基础收发功能）](./02_point_to_point/01_send_recv/)
- [HcclBatchSendRecv（实现 Ring 环状通信）](./02_point_to_point/02_batch_send_recv_ring/)

## 集合通信

- [AllReduce](./03_collectives/01_allreduce/)
- [Broadcast](./03_collectives/02_broadcast/)
- [AllGather](./03_collectives/03_allgather/)
- [ReduceScatter](./03_collectives/04_reduce_scatter/)
- [Reduce](./03_collectives/05_reduce/)
- [AlltoAll](./03_collectives/06_alltoall/)
- [AlltoAllV](./03_collectives/07_alltoallv/)
- [AlltoAllVC](./03_collectives/08_alltoallvc/)
- [Scatter](./03_collectives/09_scatter/)

## AI 框架

- [PyTorch](./04_ai_framework/01_pytorch/)
- [Tensorflow](./04_ai_framework/02_tensorflow/)

## 依赖安装
mpich下载地址，选择4.1.3版本
```bash
https://www.mpich.org/static/downloads/4.1.3/mpich-4.1.3.tar.gz
```

### 编译安装

```shell
tar -zxvf mpich-4.1.3.tar.gz
cd mpich-4.1.3.tar.gz

./configure --disable-fortran  --prefix=/usr/local/mpich --with-device=ch3:nemesis
--prefix=可以指定安装路径

make -j32 && make install
```

## 用例执行
1、设置MPI安装路径环境变量

```shell
export MPI_HOME=/usr/local/mpich
```

2、设置环境变量
```shell
source /usr/local/Ascend/ascend-toolkit/latest/bin/setenv.bash
该脚本和装包路径有关
```
3、执行前冒烟用例
```shell
bash build.sh --cb_test_verify
```